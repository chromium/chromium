// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/raster_implementation.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <set>
#include <sstream>
#include <string>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_cache.h"
#include "cc/paint/paint_op_buffer_serializer.h"
#include "cc/paint/transfer_cache_entry.h"
#include "cc/paint/transfer_cache_serialize_helper.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/client/image_decode_accelerator_interface.h"
#include "gpu/command_buffer/client/query_tracker.h"
#include "gpu/command_buffer/client/raster_cmd_helper.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"

#if defined(GPU_CLIENT_DEBUG)
#define GPU_CLIENT_SINGLE_THREAD_CHECK() SingleThreadChecker checker(this);
#else  // !defined(GPU_CLIENT_DEBUG)
#define GPU_CLIENT_SINGLE_THREAD_CHECK()
#endif  // defined(GPU_CLIENT_DEBUG)

// TODO(backer): Update APIs to always write to the destination? See below.
//
// Check that destination pointers point to initialized memory.
// When the context is lost, calling GL function has no effect so if destination
// pointers point to initialized memory it can often lead to crash bugs. eg.
//
// If it was up to us we'd just always write to the destination but the OpenGL
// spec defines the behavior of OpenGL functions, not us. :-(
#if defined(GPU_DCHECK)
#define GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION_ASSERT(v) GPU_DCHECK(v)
#define GPU_CLIENT_DCHECK(v) GPU_DCHECK(v)
#elif defined(DCHECK)
#define GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION_ASSERT(v) DCHECK(v)
#define GPU_CLIENT_DCHECK(v) DCHECK(v)
#else
#define GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION_ASSERT(v) ASSERT(v)
#define GPU_CLIENT_DCHECK(v) ASSERT(v)
#endif

#define GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(type, ptr) \
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION_ASSERT(          \
      ptr &&                                                     \
      (ptr[0] == static_cast<type>(0) || ptr[0] == static_cast<type>(-1)));

#define GPU_CLIENT_VALIDATE_DESTINATION_OPTIONAL_INITALIZATION(type, ptr) \
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION_ASSERT(                   \
      !ptr ||                                                             \
      (ptr[0] == static_cast<type>(0) || ptr[0] == static_cast<type>(-1)));

using gpu::gles2::GLES2Util;

namespace gpu {
namespace raster {

namespace {

const uint32_t kMaxTransferCacheEntrySizeForTransferBuffer = 1024;

class ScopedSharedMemoryPtr {
 public:
  ScopedSharedMemoryPtr(size_t size,
                        TransferBufferInterface* transfer_buffer,
                        MappedMemoryManager* mapped_memory,
                        RasterCmdHelper* helper) {
    // Prefer transfer buffer but fall back to MappedMemory if there's not
    // enough free space.
    if (transfer_buffer->GetFreeSize() < size) {
      scoped_mapped_ptr_.emplace(size, helper, mapped_memory);
    } else {
      scoped_transfer_ptr_.emplace(size, helper, transfer_buffer);
    }
  }
  ~ScopedSharedMemoryPtr() = default;

  GLuint size() {
    return scoped_transfer_ptr_ ? scoped_transfer_ptr_->size()
                                : scoped_mapped_ptr_->size();
  }

  GLint shm_id() {
    return scoped_transfer_ptr_ ? scoped_transfer_ptr_->shm_id()
                                : scoped_mapped_ptr_->shm_id();
  }

  GLuint offset() {
    return scoped_transfer_ptr_ ? scoped_transfer_ptr_->offset()
                                : scoped_mapped_ptr_->offset();
  }

  void* address() {
    return scoped_transfer_ptr_ ? scoped_transfer_ptr_->address()
                                : scoped_mapped_ptr_->address();
  }

 private:
  base::Optional<ScopedMappedMemoryPtr> scoped_mapped_ptr_;
  base::Optional<ScopedTransferBufferPtr> scoped_transfer_ptr_;
};

}  // namespace

// Helper to copy data to the GPU service over the transfer cache.
class RasterImplementation::TransferCacheSerializeHelperImpl final
    : public cc::TransferCacheSerializeHelper {
 public:
  explicit TransferCacheSerializeHelperImpl(RasterImplementation* ri)
      : ri_(ri) {}
  ~TransferCacheSerializeHelperImpl() final = default;

  uint32_t take_end_offset_of_last_inlined_entry() {
    auto offset = end_offset_of_last_inlined_entry_;
    end_offset_of_last_inlined_entry_ = 0u;
    return offset;
  }

 private:
  bool LockEntryInternal(const EntryKey& key) final {
    return ri_->ThreadsafeLockTransferCacheEntry(
        static_cast<uint32_t>(key.first), key.second);
  }

  uint32_t CreateEntryInternal(const cc::ClientTransferCacheEntry& entry,
                               char* memory) final {
    uint32_t size = entry.SerializedSize();
    // Cap the entries inlined to a specific size.
    if (size <= ri_->max_inlined_entry_size_ && ri_->raster_mapped_buffer_) {
      uint32_t written = InlineEntry(entry, memory);
      if (written > 0u)
        return written;
    }

    void* data = ri_->MapTransferCacheEntry(size);
    if (!data)
      return 0u;

    bool succeeded = entry.Serialize(
        base::make_span(reinterpret_cast<uint8_t*>(data), size));
    DCHECK(succeeded);
    ri_->UnmapAndCreateTransferCacheEntry(entry.UnsafeType(), entry.Id());
    return 0u;
  }

  void FlushEntriesInternal(std::set<EntryKey> entries) final {
    std::vector<std::pair<uint32_t, uint32_t>> transformed;
    transformed.reserve(entries.size());
    for (const auto& e : entries)
      transformed.emplace_back(static_cast<uint32_t>(e.first), e.second);
    ri_->UnlockTransferCacheEntries(transformed);
  }

  // Writes the entry into |memory| if there is enough space. Returns the number
  // of bytes written on success or 0u on failure due to insufficient size.
  uint32_t InlineEntry(const cc::ClientTransferCacheEntry& entry,
                       char* memory) {
    DCHECK(memory);
    DCHECK(SkIsAlign4(reinterpret_cast<uintptr_t>(memory)));

    // The memory passed from the PaintOpWriter for inlining the transfer cache
    // entry must be from the transfer buffer mapped during RasterCHROMIUM.
    const auto& buffer = ri_->raster_mapped_buffer_;
    DCHECK(buffer->BelongsToBuffer(memory));

    DCHECK(base::CheckedNumeric<uint32_t>(memory -
                                          static_cast<char*>(buffer->address()))
               .IsValid());
    uint32_t memory_offset = memory - static_cast<char*>(buffer->address());
    uint32_t bytes_to_write = entry.SerializedSize();
    uint32_t bytes_remaining = buffer->size() - memory_offset;
    DCHECK_GT(bytes_to_write, 0u);

    if (bytes_to_write > bytes_remaining)
      return 0u;

    bool succeeded = entry.Serialize(
        base::make_span(reinterpret_cast<uint8_t*>(memory), bytes_remaining));
    DCHECK(succeeded);
    ri_->transfer_cache_.AddTransferCacheEntry(
        entry.UnsafeType(), entry.Id(), buffer->shm_id(),
        buffer->offset() + memory_offset, bytes_to_write);

    end_offset_of_last_inlined_entry_ = memory_offset + bytes_to_write;
    return bytes_to_write;
  }

  RasterImplementation* const ri_;
  uint32_t end_offset_of_last_inlined_entry_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(TransferCacheSerializeHelperImpl);
};

// Helper to copy PaintOps to the GPU service over the transfer buffer.
class RasterImplementation::PaintOpSerializer {
 public:
  PaintOpSerializer(uint32_t initial_size,
                    RasterImplementation* ri,
                    cc::DecodeStashingImageProvider* stashing_image_provider,
                    TransferCacheSerializeHelperImpl* transfer_cache_helper,
                    ClientFontManager* font_manager,
                    size_t* max_op_size_hint)
      : ri_(ri),
        stashing_image_provider_(stashing_image_provider),
        transfer_cache_helper_(transfer_cache_helper),
        font_manager_(font_manager),
        max_op_size_hint_(max_op_size_hint) {
    buffer_ =
        static_cast<char*>(ri_->MapRasterCHROMIUM(initial_size, &free_bytes_));
  }

  ~PaintOpSerializer() {
    // Need to call SendSerializedData;
    DCHECK(!written_bytes_);
  }

  size_t Serialize(const cc::PaintOp* op,
                   const cc::PaintOp::SerializeOptions& options) {
    if (!valid())
      return 0;

    size_t size = op->Serialize(buffer_ + written_bytes_, free_bytes_, options);
    size_t block_size = *max_op_size_hint_;

    if (!size) {
      // The entries serialized for |op| above will not be transferred since the
      // op will be re-serialized once the buffer is remapped.
      ri_->paint_cache_->AbortPendingEntries();
      SendSerializedData();

      const unsigned int max_size = ri_->transfer_buffer_->GetMaxSize();
      DCHECK_LE(block_size, max_size);
      while (true) {
        buffer_ = static_cast<char*>(
            ri_->MapRasterCHROMIUM(block_size, &free_bytes_));
        if (!buffer_) {
          return 0;
        }

        size = op->Serialize(buffer_ + written_bytes_, free_bytes_, options);
        if (size) {
          *max_op_size_hint_ = std::max(size, *max_op_size_hint_);
          break;
        }

        ri_->paint_cache_->AbortPendingEntries();
        ri_->UnmapRasterCHROMIUM(0u, 0u);

        if (block_size == max_size)
          break;
        block_size = std::min(block_size * 2, static_cast<size_t>(max_size));
      }

      if (!size) {
        LOG(ERROR) << "Failed to serialize op in " << block_size << " bytes.";
        return 0u;
      }
    }

    DCHECK_LE(size, free_bytes_);
    DCHECK(base::CheckAdd<uint32_t>(written_bytes_, size).IsValid());

    ri_->paint_cache_->FinalizePendingEntries();
    written_bytes_ += size;
    free_bytes_ -= size;
    return size;
  }

  void SendSerializedData() {
    if (!valid())
      return;

    // Serialize fonts before sending raster commands.
    font_manager_->Serialize();

    // Check the address of the last inlined entry to figured out whether
    // transfer cache entries were written past the last successfully serialized
    // op.
    uint32_t total_written_size = std::max(
        written_bytes_,
        transfer_cache_helper_->take_end_offset_of_last_inlined_entry());

    // Send the raster command itself now that the commands for its
    // dependencies have been sent.
    ri_->UnmapRasterCHROMIUM(written_bytes_, total_written_size);

    // Now that we've issued the RasterCHROMIUM referencing the stashed
    // images, Reset the |stashing_image_provider_|, causing us to issue
    // unlock commands for these images.
    stashing_image_provider_->Reset();

    // Unlock all the transfer cache entries used (both immediate and deferred).
    transfer_cache_helper_->FlushEntries();
    written_bytes_ = 0;
  }

  bool valid() const { return !!buffer_; }

 private:
  RasterImplementation* const ri_;
  char* buffer_;
  cc::DecodeStashingImageProvider* const stashing_image_provider_;
  TransferCacheSerializeHelperImpl* const transfer_cache_helper_;
  ClientFontManager* font_manager_;

  uint32_t written_bytes_ = 0;
  uint32_t free_bytes_ = 0;

  size_t* max_op_size_hint_;

  DISALLOW_COPY_AND_ASSIGN(PaintOpSerializer);
};

RasterImplementation::SingleThreadChecker::SingleThreadChecker(
    RasterImplementation* raster_implementation)
    : raster_implementation_(raster_implementation) {
  CHECK_EQ(0, raster_implementation_->use_count_);
  ++raster_implementation_->use_count_;
}

RasterImplementation::SingleThreadChecker::~SingleThreadChecker() {
  --raster_implementation_->use_count_;
  CHECK_EQ(0, raster_implementation_->use_count_);
}

RasterImplementation::RasterImplementation(
    RasterCmdHelper* helper,
    TransferBufferInterface* transfer_buffer,
    bool bind_generates_resource,
    bool lose_context_when_out_of_memory,
    GpuControl* gpu_control,
    ImageDecodeAcceleratorInterface* image_decode_accelerator)
    : ImplementationBase(helper, transfer_buffer, gpu_control),
      helper_(helper),
      error_bits_(0),
      lose_context_when_out_of_memory_(lose_context_when_out_of_memory),
      use_count_(0),
      current_trace_stack_(0),
      aggressively_free_resources_(false),
      font_manager_(this, helper->command_buffer()),
      lost_(false),
      max_inlined_entry_size_(kMaxTransferCacheEntrySizeForTransferBuffer),
      transfer_cache_(this),
      image_decode_accelerator_(image_decode_accelerator) {
  DCHECK(helper);
  DCHECK(transfer_buffer);
  DCHECK(gpu_control);

  std::stringstream ss;
  ss << std::hex << this;
  this_in_hex_ = ss.str();
}

gpu::ContextResult RasterImplementation::Initialize(
    const SharedMemoryLimits& limits) {
  TRACE_EVENT0("gpu", "RasterImplementation::Initialize");

  auto result = ImplementationBase::Initialize(limits);
  if (result != gpu::ContextResult::kSuccess) {
    return result;
  }

  return gpu::ContextResult::kSuccess;
}

RasterImplementation::~RasterImplementation() {
  // Make sure the queries are finished otherwise we'll delete the
  // shared memory (mapped_memory_) which will free the memory used
  // by the queries. The GPU process when validating that memory is still
  // shared will fail and abort (ie, it will stop running).
  WaitForCmd();

  query_tracker_.reset();

  // Make sure the commands make it the service.
  WaitForCmd();
}

RasterCmdHelper* RasterImplementation::helper() const {
  return helper_;
}

IdAllocator* RasterImplementation::GetIdAllocator(IdNamespaces namespace_id) {
  DCHECK_EQ(namespace_id, IdNamespaces::kQueries);
  return &query_id_allocator_;
}

void RasterImplementation::OnGpuControlLostContext() {
  OnGpuControlLostContextMaybeReentrant();

  // This should never occur more than once.
  DCHECK(!lost_context_callback_run_);
  lost_context_callback_run_ = true;
  if (!lost_context_callback_.is_null()) {
    std::move(lost_context_callback_).Run();
  }
}

void RasterImplementation::OnGpuControlLostContextMaybeReentrant() {
  {
    base::AutoLock hold(lost_lock_);
    lost_ = true;
  }
}

void RasterImplementation::OnGpuControlErrorMessage(const char* message,
                                                    int32_t id) {
  if (!error_message_callback_.is_null())
    error_message_callback_.Run(message, id);
}

void RasterImplementation::OnGpuControlSwapBuffersCompleted(
    const SwapBuffersCompleteParams& params) {
  NOTREACHED();
}

void RasterImplementation::OnSwapBufferPresented(
    uint64_t swap_id,
    const gfx::PresentationFeedback& feedback) {
  NOTREACHED();
}

void RasterImplementation::OnGpuControlReturnData(
    base::span<const uint8_t> data) {
  NOTIMPLEMENTED();
}

void RasterImplementation::SetAggressivelyFreeResources(
    bool aggressively_free_resources) {
  TRACE_EVENT1("gpu", "RasterImplementation::SetAggressivelyFreeResources",
               "aggressively_free_resources", aggressively_free_resources);
  aggressively_free_resources_ = aggressively_free_resources;

  if (aggressively_free_resources_)
    ClearPaintCache();

  if (aggressively_free_resources_ && helper_->HaveRingBuffer()) {
    // Flush will delete transfer buffer resources if
    // |aggressively_free_resources_| is true.
    Flush();
  } else {
    ShallowFlushCHROMIUM();
  }

  if (aggressively_free_resources_) {
    temp_raster_offsets_.clear();
    temp_raster_offsets_.shrink_to_fit();
  }
}

void RasterImplementation::Swap(
    uint32_t /* flags */,
    SwapCompletedCallback /* swap_completed */,
    PresentationCallback /* presentation_callback */) {
  NOTREACHED();
}

void RasterImplementation::SwapWithBounds(
    const std::vector<gfx::Rect>& /* rects */,
    uint32_t /* flags */,
    SwapCompletedCallback /* swap_completed */,
    PresentationCallback /* presentation_callback */) {
  NOTREACHED();
}

void RasterImplementation::PartialSwapBuffers(
    const gfx::Rect& /* sub_buffer */,
    uint32_t /* flags */,
    SwapCompletedCallback /* swap_completed */,
    PresentationCallback /* presentation_callback */) {
  NOTREACHED();
}

void RasterImplementation::CommitOverlayPlanes(
    uint32_t /* flags */,
    SwapCompletedCallback /* swap_completed */,
    PresentationCallback /* presentation_callback */) {
  NOTREACHED();
}

void RasterImplementation::ScheduleOverlayPlane(
    int /* plane_z_order */,
    gfx::OverlayTransform /* plane_transform */,
    unsigned /* overlay_texture_id */,
    const gfx::Rect& /* display_bounds */,
    const gfx::RectF& /* uv_rect */,
    bool /* enable_blend */,
    unsigned /* gpu_fence_id */) {
  NOTREACHED();
}

uint64_t RasterImplementation::ShareGroupTracingGUID() const {
  NOTREACHED();
  return 0;
}

void RasterImplementation::SetErrorMessageCallback(
    base::RepeatingCallback<void(const char*, int32_t)> callback) {
  error_message_callback_ = std::move(callback);
}

bool RasterImplementation::ThreadSafeShallowLockDiscardableTexture(
    uint32_t texture_id) {
  NOTREACHED();
  return false;
}

void RasterImplementation::CompleteLockDiscardableTexureOnContextThread(
    uint32_t texture_id) {
  NOTREACHED();
}

bool RasterImplementation::ThreadsafeDiscardableTextureIsDeletedForTracing(
    uint32_t texture_id) {
  NOTREACHED();
  return false;
}

void* RasterImplementation::MapTransferCacheEntry(uint32_t serialized_size) {
  // Prefer to use transfer buffer when possible, since transfer buffer
  // allocations are much cheaper.
  if (raster_mapped_buffer_ ||
      transfer_buffer_->GetFreeSize() < serialized_size) {
    return transfer_cache_.MapEntry(mapped_memory_.get(), serialized_size);
  }

  return transfer_cache_.MapTransferBufferEntry(transfer_buffer_,
                                                serialized_size);
}

void RasterImplementation::UnmapAndCreateTransferCacheEntry(uint32_t type,
                                                            uint32_t id) {
  transfer_cache_.UnmapAndCreateEntry(type, id);
}

bool RasterImplementation::ThreadsafeLockTransferCacheEntry(uint32_t type,
                                                            uint32_t id) {
  return transfer_cache_.LockEntry(type, id);
}

void RasterImplementation::UnlockTransferCacheEntries(
    const std::vector<std::pair<uint32_t, uint32_t>>& entries) {
  transfer_cache_.UnlockEntries(entries);
}

void RasterImplementation::DeleteTransferCacheEntry(uint32_t type,
                                                    uint32_t id) {
  transfer_cache_.DeleteEntry(type, id);
}

unsigned int RasterImplementation::GetTransferBufferFreeSize() const {
  return transfer_buffer_->GetFreeSize();
}

bool RasterImplementation::IsJpegDecodeAccelerationSupported() const {
  return image_decode_accelerator_ &&
         image_decode_accelerator_->IsJpegDecodeAccelerationSupported();
}

bool RasterImplementation::IsWebPDecodeAccelerationSupported() const {
  return image_decode_accelerator_ &&
         image_decode_accelerator_->IsWebPDecodeAccelerationSupported();
}

bool RasterImplementation::CanDecodeWithHardwareAcceleration(
    const cc::ImageHeaderMetadata* image_metadata) const {
  return image_decode_accelerator_ &&
         image_decode_accelerator_->IsImageSupported(image_metadata);
}

const std::string& RasterImplementation::GetLogPrefix() const {
  const std::string& prefix(debug_marker_manager_.GetMarker());
  return prefix.empty() ? this_in_hex_ : prefix;
}

GLenum RasterImplementation::GetError() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetError()");
  GLenum err = GetGLError();
  GPU_CLIENT_LOG("returned " << GLES2Util::GetStringError(err));
  return err;
}

void RasterImplementation::IssueBeginQuery(GLenum target,
                                           GLuint id,
                                           uint32_t sync_data_shm_id,
                                           uint32_t sync_data_shm_offset) {
  helper_->BeginQueryEXT(target, id, sync_data_shm_id, sync_data_shm_offset);
}

void RasterImplementation::IssueEndQuery(GLenum target, GLuint submit_count) {
  helper_->EndQueryEXT(target, submit_count);
}

void RasterImplementation::IssueQueryCounter(GLuint id,
                                             GLenum target,
                                             uint32_t sync_data_shm_id,
                                             uint32_t sync_data_shm_offset,
                                             GLuint submit_count) {
  helper_->QueryCounterEXT(id, target, sync_data_shm_id, sync_data_shm_offset,
                           submit_count);
}

void RasterImplementation::IssueSetDisjointValueSync(
    uint32_t sync_data_shm_id,
    uint32_t sync_data_shm_offset) {
  NOTIMPLEMENTED();
}

GLenum RasterImplementation::GetClientSideGLError() {
  if (error_bits_ == 0) {
    return GL_NO_ERROR;
  }

  GLenum error = GL_NO_ERROR;
  for (uint32_t mask = 1; mask != 0; mask = mask << 1) {
    if ((error_bits_ & mask) != 0) {
      error = GLES2Util::GLErrorBitToGLError(mask);
      break;
    }
  }
  error_bits_ &= ~GLES2Util::GLErrorToErrorBit(error);
  return error;
}

CommandBufferHelper* RasterImplementation::cmd_buffer_helper() {
  return helper_;
}

void RasterImplementation::IssueCreateTransferCacheEntry(
    GLuint entry_type,
    GLuint entry_id,
    GLuint handle_shm_id,
    GLuint handle_shm_offset,
    GLuint data_shm_id,
    GLuint data_shm_offset,
    GLuint data_size) {
  helper_->CreateTransferCacheEntryINTERNAL(entry_type, entry_id, handle_shm_id,
                                            handle_shm_offset, data_shm_id,
                                            data_shm_offset, data_size);
}

void RasterImplementation::IssueDeleteTransferCacheEntry(GLuint entry_type,
                                                         GLuint entry_id) {
  helper_->DeleteTransferCacheEntryINTERNAL(entry_type, entry_id);
}

void RasterImplementation::IssueUnlockTransferCacheEntry(GLuint entry_type,
                                                         GLuint entry_id) {
  helper_->UnlockTransferCacheEntryINTERNAL(entry_type, entry_id);
}

CommandBuffer* RasterImplementation::command_buffer() const {
  return helper_->command_buffer();
}

GLenum RasterImplementation::GetGLError() {
  TRACE_EVENT0("gpu", "RasterImplementation::GetGLError");
  // Check the GL error first, then our wrapped error.
  auto result = GetResultAs<cmds::GetError::Result>();
  // If we couldn't allocate a result the context is lost.
  if (!result) {
    return GL_NO_ERROR;
  }
  *result = GL_NO_ERROR;
  helper_->GetError(GetResultShmId(), result.offset());
  WaitForCmd();
  GLenum error = *result;
  if (error == GL_NO_ERROR) {
    error = GetClientSideGLError();
  } else {
    // There was an error, clear the corresponding wrapped error.
    error_bits_ &= ~GLES2Util::GLErrorToErrorBit(error);
  }
  return error;
}

#if defined(RASTER_CLIENT_FAIL_GL_ERRORS)
void RasterImplementation::FailGLError(GLenum error) {
  if (error != GL_NO_ERROR) {
    NOTREACHED() << "Error";
  }
}
// NOTE: Calling GetGLError overwrites data in the result buffer.
void RasterImplementation::CheckGLError() {
  FailGLError(GetGLError());
}
#endif  // defined(RASTER_CLIENT_FAIL_GL_ERRORS)

void RasterImplementation::SetGLError(GLenum error,
                                      const char* function_name,
                                      const char* msg) {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] Client Synthesized Error: "
                     << GLES2Util::GetStringError(error) << ": "
                     << function_name << ": " << msg);
  FailGLError(error);
  if (msg) {
    last_error_ = msg;
  }
  if (!error_message_callback_.is_null()) {
    std::string temp(GLES2Util::GetStringError(error) + " : " + function_name +
                     ": " + (msg ? msg : ""));
    error_message_callback_.Run(temp.c_str(), 0);
  }
  error_bits_ |= GLES2Util::GLErrorToErrorBit(error);

  if (error == GL_OUT_OF_MEMORY && lose_context_when_out_of_memory_) {
    helper_->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                                 GL_UNKNOWN_CONTEXT_RESET_ARB);
  }
}

void RasterImplementation::SetGLErrorInvalidEnum(const char* function_name,
                                                 GLenum value,
                                                 const char* label) {
  SetGLError(
      GL_INVALID_ENUM, function_name,
      (std::string(label) + " was " + GLES2Util::GetStringEnum(value)).c_str());
}

bool RasterImplementation::GetQueryObjectValueHelper(const char* function_name,
                                                     GLuint id,
                                                     GLenum pname,
                                                     GLuint64* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] GetQueryObjectValueHelper(" << id
                     << ", " << GLES2Util::GetStringQueryObjectParameter(pname)
                     << ", " << static_cast<const void*>(params) << ")");

  gles2::QueryTracker::Query* query = query_tracker_->GetQuery(id);
  if (!query) {
    SetGLError(GL_INVALID_OPERATION, function_name, "unknown query id");
    return false;
  }

  if (query->Active()) {
    SetGLError(GL_INVALID_OPERATION, function_name,
               "query active. Did you call glEndQueryEXT?");
    return false;
  }

  if (query->NeverUsed()) {
    SetGLError(GL_INVALID_OPERATION, function_name,
               "Never used. Did you call glBeginQueryEXT?");
    return false;
  }

  bool valid_value = false;
  const bool flush_if_pending =
      pname != GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT;
  switch (pname) {
    case GL_QUERY_RESULT_EXT:
      if (!query->CheckResultsAvailable(helper_, flush_if_pending)) {
        helper_->WaitForToken(query->token());
        if (!query->CheckResultsAvailable(helper_, flush_if_pending)) {
          FinishHelper();
          CHECK(query->CheckResultsAvailable(helper_, flush_if_pending));
        }
      }
      *params = query->GetResult();
      valid_value = true;
      break;
    case GL_QUERY_RESULT_AVAILABLE_EXT:
    case GL_QUERY_RESULT_AVAILABLE_NO_FLUSH_CHROMIUM_EXT:
      *params = query->CheckResultsAvailable(helper_, flush_if_pending);
      valid_value = true;
      break;
    default:
      SetGLErrorInvalidEnum(function_name, pname, "pname");
      break;
  }
  GPU_CLIENT_LOG("  " << *params);
  CheckGLError();
  return valid_value;
}

void RasterImplementation::Flush() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glFlush()");
  // Insert the cmd to call glFlush
  helper_->Flush();
  FlushHelper();
}

// InterfaceBase implementation.
void RasterImplementation::GenSyncTokenCHROMIUM(GLbyte* sync_token) {
  ImplementationBase::GenSyncToken(sync_token);
}
void RasterImplementation::GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) {
  ImplementationBase::GenUnverifiedSyncToken(sync_token);
}
void RasterImplementation::VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                                    GLsizei count) {
  ImplementationBase::VerifySyncTokens(sync_tokens, count);
}
void RasterImplementation::WaitSyncTokenCHROMIUM(const GLbyte* sync_token) {
  ImplementationBase::WaitSyncToken(sync_token);
}

// ImplementationBase implementation.
void RasterImplementation::IssueShallowFlush() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glShallowFlushCHROMIUM()");
  FlushHelper();
}

void RasterImplementation::ShallowFlushCHROMIUM() {
  IssueShallowFlush();
}

void RasterImplementation::FlushHelper() {
  // Flush our command buffer
  // (tell the service to execute up to the flush cmd.)
  helper_->CommandBufferHelper::Flush();

  if (aggressively_free_resources_)
    FreeEverything();
}

void RasterImplementation::OrderingBarrierCHROMIUM() {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glOrderingBarrierCHROMIUM");
  // Flush command buffer at the GPU channel level.  May be implemented as
  // Flush().
  helper_->CommandBufferHelper::OrderingBarrier();
}

void RasterImplementation::Finish() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  FinishHelper();
}

void RasterImplementation::FinishHelper() {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glFinish()");
  TRACE_EVENT0("gpu", "RasterImplementation::Finish");
  // Insert the cmd to call glFinish
  helper_->Finish();
  // Finish our command buffer
  // (tell the service to execute up to the Finish cmd and wait for it to
  // execute.)
  helper_->CommandBufferHelper::Finish();

  if (aggressively_free_resources_)
    FreeEverything();
}

void RasterImplementation::GenQueriesEXTHelper(GLsizei /* n */,
                                               const GLuint* /* queries */) {}

GLenum RasterImplementation::GetGraphicsResetStatusKHR() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetGraphicsResetStatusKHR()");

  base::AutoLock hold(lost_lock_);
  if (lost_)
    return GL_UNKNOWN_CONTEXT_RESET_KHR;
  return GL_NO_ERROR;
}

void RasterImplementation::DeleteQueriesEXTHelper(GLsizei n,
                                                  const GLuint* queries) {
  IdAllocator* id_allocator = GetIdAllocator(IdNamespaces::kQueries);
  for (GLsizei ii = 0; ii < n; ++ii) {
    query_tracker_->RemoveQuery(queries[ii]);
    id_allocator->FreeID(queries[ii]);
  }

  helper_->DeleteQueriesEXTImmediate(n, queries);
}

void RasterImplementation::BeginQueryEXT(GLenum target, GLuint id) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] BeginQueryEXT("
                     << GLES2Util::GetStringQueryTarget(target) << ", " << id
                     << ")");

  switch (target) {
    case GL_COMMANDS_ISSUED_CHROMIUM:
      break;
    case GL_COMMANDS_COMPLETED_CHROMIUM:
      if (!capabilities_.sync_query) {
        SetGLError(GL_INVALID_OPERATION, "glBeginQueryEXT",
                   "not enabled for commands completed queries");
        return;
      }
      break;
    default:
      SetGLError(GL_INVALID_ENUM, "glBeginQueryEXT", "unknown query target");
      return;
  }

  // if any outstanding queries INV_OP
  if (query_tracker_->GetCurrentQuery(target)) {
    SetGLError(GL_INVALID_OPERATION, "glBeginQueryEXT",
               "query already in progress");
    return;
  }

  if (id == 0) {
    SetGLError(GL_INVALID_OPERATION, "glBeginQueryEXT", "id is 0");
    return;
  }

  if (!GetIdAllocator(IdNamespaces::kQueries)->InUse(id)) {
    SetGLError(GL_INVALID_OPERATION, "glBeginQueryEXT", "invalid id");
    return;
  }

  if (query_tracker_->BeginQuery(id, target, this))
    CheckGLError();
}

void RasterImplementation::EndQueryEXT(GLenum target) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] EndQueryEXT("
                     << GLES2Util::GetStringQueryTarget(target) << ")");
  if (query_tracker_->EndQuery(target, this))
    CheckGLError();
}

void RasterImplementation::QueryCounterEXT(GLuint id, GLenum target) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] QueryCounterEXT(" << id << ", "
                     << GLES2Util::GetStringQueryTarget(target) << ")");

  if (target != GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM) {
    SetGLError(GL_INVALID_ENUM, "glQueryCounterEXT", "unknown query target");
    return;
  }

  if (id == 0) {
    SetGLError(GL_INVALID_OPERATION, "glQueryCounterEXT", "id is 0");
    return;
  }

  if (!GetIdAllocator(IdNamespaces::kQueries)->InUse(id)) {
    SetGLError(GL_INVALID_OPERATION, "glQueryCounterEXT", "invalid id");
    return;
  }

  if (query_tracker_->QueryCounter(id, target, this))
    CheckGLError();
}
void RasterImplementation::GetQueryObjectuivEXT(GLuint id,
                                                GLenum pname,
                                                GLuint* params) {
  GLuint64 result = 0;
  if (GetQueryObjectValueHelper("glGetQueryObjectuivEXT", id, pname, &result))
    *params = base::saturated_cast<GLuint>(result);
}

void RasterImplementation::GetQueryObjectui64vEXT(GLuint id,
                                                  GLenum pname,
                                                  GLuint64* params) {
  GLuint64 result = 0;
  if (GetQueryObjectValueHelper("glGetQueryObjectui64vEXT", id, pname, &result))
    *params = result;
}

void* RasterImplementation::MapRasterCHROMIUM(uint32_t size,
                                              uint32_t* size_allocated) {
  *size_allocated = 0u;
  if (raster_mapped_buffer_) {
    SetGLError(GL_INVALID_OPERATION, "glMapRasterCHROMIUM", "already mapped");
    return nullptr;
  }
  raster_mapped_buffer_.emplace(size, helper_, transfer_buffer_);
  if (!raster_mapped_buffer_->valid()) {
    SetGLError(GL_INVALID_OPERATION, "glMapRasterCHROMIUM", "size too big");
    raster_mapped_buffer_ = base::nullopt;
    return nullptr;
  }
  *size_allocated = raster_mapped_buffer_->size();
  return raster_mapped_buffer_->address();
}

void* RasterImplementation::MapFontBuffer(uint32_t size) {
  if (font_mapped_buffer_) {
    SetGLError(GL_INVALID_OPERATION, "glMapFontBufferCHROMIUM",
               "already mapped");
    return nullptr;
  }
  if (!raster_mapped_buffer_) {
    SetGLError(GL_INVALID_OPERATION, "glMapFontBufferCHROMIUM",
               "mapped font buffer with no raster buffer");
    return nullptr;
  }

  font_mapped_buffer_.emplace(size, helper_, mapped_memory_.get());
  if (!font_mapped_buffer_->valid()) {
    SetGLError(GL_INVALID_OPERATION, "glMapFontBufferCHROMIUM", "size too big");
    font_mapped_buffer_ = base::nullopt;
    return nullptr;
  }
  return font_mapped_buffer_->address();
}

void RasterImplementation::UnmapRasterCHROMIUM(uint32_t raster_written_size,
                                               uint32_t total_written_size) {
  if (!raster_mapped_buffer_) {
    SetGLError(GL_INVALID_OPERATION, "glUnmapRasterCHROMIUM", "not mapped");
    return;
  }
  DCHECK(raster_mapped_buffer_->valid());
  if (total_written_size == 0) {
    raster_mapped_buffer_->Discard();
    raster_mapped_buffer_ = base::nullopt;
    return;
  }
  raster_mapped_buffer_->Shrink(total_written_size);

  uint32_t font_shm_id = 0u;
  uint32_t font_shm_offset = 0u;
  uint32_t font_shm_size = 0u;
  if (font_mapped_buffer_) {
    font_shm_id = font_mapped_buffer_->shm_id();
    font_shm_offset = font_mapped_buffer_->offset();
    font_shm_size = font_mapped_buffer_->size();
  }

  if (raster_written_size != 0u) {
    helper_->RasterCHROMIUM(
        raster_mapped_buffer_->shm_id(), raster_mapped_buffer_->offset(),
        raster_written_size, font_shm_id, font_shm_offset, font_shm_size);
  }

  raster_mapped_buffer_ = base::nullopt;
  font_mapped_buffer_ = base::nullopt;
  CheckGLError();
}

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/raster_implementation_impl_autogen.h"

void RasterImplementation::CopySubTexture(const gpu::Mailbox& source_mailbox,
                                          const gpu::Mailbox& dest_mailbox,
                                          GLenum dest_target,
                                          GLint xoffset,
                                          GLint yoffset,
                                          GLint x,
                                          GLint y,
                                          GLsizei width,
                                          GLsizei height,
                                          GLboolean unpack_flip_y,
                                          GLboolean unpack_premultiply_alpha) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glCopySubTexture("
                     << source_mailbox.ToDebugString() << ", "
                     << dest_mailbox.ToDebugString() << ", " << xoffset << ", "
                     << yoffset << ", " << x << ", " << y << ", " << width
                     << ", " << height << ")");
  if (width < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopySubTexture", "width < 0");
    return;
  }
  if (height < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopySubTexture", "height < 0");
    return;
  }
  GLbyte mailboxes[sizeof(source_mailbox.name) * 2];
  memcpy(mailboxes, source_mailbox.name, sizeof(source_mailbox.name));
  memcpy(mailboxes + sizeof(source_mailbox.name), dest_mailbox.name,
         sizeof(dest_mailbox.name));
  helper_->CopySubTextureINTERNALImmediate(xoffset, yoffset, x, y, width,
                                           height, unpack_flip_y, mailboxes);
  CheckGLError();
}

void RasterImplementation::WritePixels(const gpu::Mailbox& dest_mailbox,
                                       int dst_x_offset,
                                       int dst_y_offset,
                                       GLenum texture_target,
                                       GLuint row_bytes,
                                       const SkImageInfo& src_info,
                                       const void* src_pixels) {
  DCHECK_GE(row_bytes, src_info.minRowBytes());

  // Get the size of the SkColorSpace while maintaining 8-byte alignment.
  GLuint pixels_offset = 0;
  if (src_info.colorSpace()) {
    pixels_offset = base::bits::Align(
        src_info.colorSpace()->writeToMemory(nullptr), sizeof(uint64_t));
  }

  GLuint src_size = src_info.computeByteSize(row_bytes);
  GLuint total_size =
      pixels_offset + base::bits::Align(src_size, sizeof(uint64_t));

  ScopedSharedMemoryPtr scoped_shared_memory(total_size, transfer_buffer_,
                                             mapped_memory_.get(), helper());
  GLint shm_id = scoped_shared_memory.shm_id();
  GLuint shm_offset = scoped_shared_memory.offset();
  void* address = scoped_shared_memory.address();

  if (src_info.colorSpace()) {
    size_t bytes_written = src_info.colorSpace()->writeToMemory(address);
    DCHECK_LE(bytes_written, pixels_offset);
  }
  memcpy(static_cast<uint8_t*>(address) + pixels_offset, src_pixels, src_size);

  helper_->WritePixelsINTERNALImmediate(
      dst_x_offset, dst_y_offset, src_info.width(), src_info.height(),
      row_bytes, src_info.colorType(), src_info.alphaType(), shm_id, shm_offset,
      pixels_offset, dest_mailbox.name);
}

namespace {
constexpr size_t kNumMailboxes = SkYUVAInfo::kMaxPlanes + 1;
}  // namespace

void RasterImplementation::ConvertYUVAMailboxesToRGB(
    const gpu::Mailbox& dest_mailbox,
    SkYUVColorSpace planes_yuv_color_space,
    SkYUVAInfo::PlaneConfig plane_config,
    SkYUVAInfo::Subsampling subsampling,
    const gpu::Mailbox yuva_plane_mailboxes[]) {
  gpu::Mailbox mailboxes[kNumMailboxes]{};
  for (int i = 0; i < SkYUVAInfo::NumPlanes(plane_config); ++i) {
    mailboxes[i] = yuva_plane_mailboxes[i];
  }
  mailboxes[kNumMailboxes - 1] = dest_mailbox;
  helper_->ConvertYUVAMailboxesToRGBINTERNALImmediate(
      planes_yuv_color_space, static_cast<GLenum>(plane_config),
      static_cast<GLenum>(subsampling), reinterpret_cast<GLbyte*>(mailboxes));
}

void RasterImplementation::BeginRasterCHROMIUM(
    GLuint sk_color,
    GLuint msaa_sample_count,
    GLboolean can_use_lcd_text,
    const gfx::ColorSpace& color_space,
    const GLbyte* mailbox) {
  DCHECK(!raster_properties_);

  helper_->BeginRasterCHROMIUMImmediate(sk_color, msaa_sample_count,
                                        can_use_lcd_text, mailbox);

  raster_properties_.emplace(sk_color, can_use_lcd_text,
                             color_space.ToSkColorSpace());
}

void RasterImplementation::RasterCHROMIUM(const cc::DisplayItemList* list,
                                          cc::ImageProvider* provider,
                                          const gfx::Size& content_size,
                                          const gfx::Rect& full_raster_rect,
                                          const gfx::Rect& playback_rect,
                                          const gfx::Vector2dF& post_translate,
                                          GLfloat post_scale,
                                          bool requires_clear,
                                          size_t* max_op_size_hint) {
  TRACE_EVENT1("gpu", "RasterImplementation::RasterCHROMIUM",
               "raster_chromium_id", ++raster_chromium_id_);
  DCHECK(max_op_size_hint);

  if (std::abs(post_scale) < std::numeric_limits<float>::epsilon())
    return;

  gfx::Rect query_rect =
      gfx::ScaleToEnclosingRect(playback_rect, 1.f / post_scale);
  list->rtree_.Search(query_rect, &temp_raster_offsets_);
  // We can early out if we have nothing to draw and we don't need a clear. Note
  // that if there is nothing to draw, but a clear is required, then those
  // commands would be serialized in the preamble and it's important to play
  // those back.
  if (temp_raster_offsets_.empty() && !requires_clear)
    return;

  // TODO(enne): Tune these numbers
  static constexpr uint32_t kMinAlloc = 16 * 1024;
  uint32_t free_size = std::max(GetTransferBufferFreeSize(), kMinAlloc);

  // This section duplicates RasterSource::PlaybackToCanvas setup preamble.
  cc::PaintOpBufferSerializer::Preamble preamble;
  preamble.content_size = content_size;
  preamble.full_raster_rect = full_raster_rect;
  preamble.playback_rect = playback_rect;
  preamble.post_translation = post_translate;
  preamble.post_scale = gfx::SizeF(post_scale, post_scale);
  preamble.requires_clear = requires_clear;
  preamble.background_color = raster_properties_->background_color;

  // Wrap the provided provider in a stashing provider so that we can delay
  // unrefing images until we have serialized dependent commands.
  cc::DecodeStashingImageProvider stashing_image_provider(provider);

  // TODO(enne): Don't access private members of DisplayItemList.
  TransferCacheSerializeHelperImpl transfer_cache_serialize_helper(this);
  PaintOpSerializer op_serializer(free_size, this, &stashing_image_provider,
                                  &transfer_cache_serialize_helper,
                                  &font_manager_, max_op_size_hint);
  cc::PaintOpBufferSerializer::SerializeCallback serialize_cb =
      base::BindRepeating(&PaintOpSerializer::Serialize,
                          base::Unretained(&op_serializer));

  cc::PaintOpBufferSerializer serializer(
      serialize_cb, &stashing_image_provider, &transfer_cache_serialize_helper,
      GetOrCreatePaintCache(), font_manager_.strike_server(),
      raster_properties_->color_space, raster_properties_->can_use_lcd_text,
      capabilities().context_supports_distance_field_text,
      capabilities().max_texture_size);
  serializer.Serialize(&list->paint_op_buffer_, &temp_raster_offsets_,
                       preamble);
  // TODO(piman): raise error if !serializer.valid()?
  op_serializer.SendSerializedData();
}

void RasterImplementation::EndRasterCHROMIUM() {
  DCHECK(raster_properties_);

  raster_properties_.reset();
  helper_->EndRasterCHROMIUM();

  if (aggressively_free_resources_)
    ClearPaintCache();
  else
    FlushPaintCachePurgedEntries();
}

SyncToken RasterImplementation::ScheduleImageDecode(
    base::span<const uint8_t> encoded_data,
    const gfx::Size& output_size,
    uint32_t transfer_cache_entry_id,
    const gfx::ColorSpace& target_color_space,
    bool needs_mips) {
  // It's safe to use base::Unretained(this) here because
  // StartTransferCacheEntry() will call the callback before returning.
  SyncToken decode_sync_token;
  transfer_cache_.StartTransferCacheEntry(
      static_cast<uint32_t>(cc::TransferCacheEntryType::kImage),
      transfer_cache_entry_id,
      base::BindOnce(&RasterImplementation::IssueImageDecodeCacheEntryCreation,
                     base::Unretained(this), encoded_data, output_size,
                     transfer_cache_entry_id, target_color_space, needs_mips,
                     &decode_sync_token));
  return decode_sync_token;
}

void RasterImplementation::ReadbackARGBPixelsAsync(
    const gpu::Mailbox& source_mailbox,
    GLenum source_target,
    const gfx::Size& dst_size,
    unsigned char* out,
    GLenum format,
    base::OnceCallback<void(bool)> readback_done) {
  NOTREACHED();
}

void RasterImplementation::ReadbackYUVPixelsAsync(
    const gpu::Mailbox& source_mailbox,
    GLenum source_target,
    const gfx::Size& source_size,
    const gfx::Rect& output_rect,
    bool vertically_flip_texture,
    int y_plane_row_stride_bytes,
    unsigned char* y_plane_data,
    int u_plane_row_stride_bytes,
    unsigned char* u_plane_data,
    int v_plane_row_stride_bytes,
    unsigned char* v_plane_data,
    const gfx::Point& paste_location,
    base::OnceCallback<void()> release_mailbox,
    base::OnceCallback<void(bool)> readback_done) {
  NOTREACHED();
}

void RasterImplementation::ReadbackImagePixels(
    const gpu::Mailbox& source_mailbox,
    const SkImageInfo& dst_info,
    GLuint dst_row_bytes,
    int src_x,
    int src_y,
    void* dst_pixels) {
  DCHECK_GE(dst_row_bytes, dst_info.minRowBytes());

  // Get the size of the SkColorSpace while maintaining 8-byte alignment.
  GLuint pixels_offset = 0;
  if (dst_info.colorSpace()) {
    pixels_offset = base::bits::Align(
        dst_info.colorSpace()->writeToMemory(nullptr), sizeof(uint64_t));
  }

  GLuint dst_size = dst_info.computeByteSize(dst_row_bytes);
  GLuint total_size =
      pixels_offset + base::bits::Align(dst_size, sizeof(uint64_t));

  ScopedSharedMemoryPtr scoped_shared_memory(total_size, transfer_buffer_,
                                             mapped_memory_.get(), helper());
  GLint shm_id = scoped_shared_memory.shm_id();
  GLuint shm_offset = scoped_shared_memory.offset();
  void* address = scoped_shared_memory.address();
  auto result =
      GetResultAs<cmds::ReadbackImagePixelsINTERNALImmediate::Result>();
  *result = 0;

  if (dst_info.colorSpace()) {
    size_t bytes_written = dst_info.colorSpace()->writeToMemory(address);
    DCHECK_LE(bytes_written, pixels_offset);
  }

  helper_->ReadbackImagePixelsINTERNALImmediate(
      src_x, src_y, dst_info.width(), dst_info.height(), dst_row_bytes,
      dst_info.colorType(), dst_info.alphaType(), shm_id, shm_offset,
      pixels_offset, GetResultShmId(), result.offset(), source_mailbox.name);
  WaitForCmd();

  if (!*result)
    return;

  memcpy(dst_pixels, static_cast<uint8_t*>(address) + pixels_offset, dst_size);
}

void RasterImplementation::IssueImageDecodeCacheEntryCreation(
    base::span<const uint8_t> encoded_data,
    const gfx::Size& output_size,
    uint32_t transfer_cache_entry_id,
    const gfx::ColorSpace& target_color_space,
    bool needs_mips,
    SyncToken* decode_sync_token,
    ClientDiscardableHandle handle) {
  DCHECK(gpu_control_);
  DCHECK(image_decode_accelerator_);
  DCHECK(handle.IsValid());

  // Insert a sync token to signal that |handle|'s buffer has been registered.
  SyncToken sync_token;
  GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());

  // Send the decode request to the service.
  *decode_sync_token = image_decode_accelerator_->ScheduleImageDecode(
      encoded_data, output_size, gpu_control_->GetCommandBufferID(),
      transfer_cache_entry_id, handle.shm_id(), handle.byte_offset(),
      sync_token.release_count(), target_color_space, needs_mips);
}

GLuint RasterImplementation::CreateAndConsumeForGpuRaster(
    const gpu::Mailbox& mailbox) {
  NOTREACHED();
  return 0;
}

void RasterImplementation::DeleteGpuRasterTexture(GLuint texture) {
  NOTREACHED();
}

void RasterImplementation::BeginGpuRaster() {
  NOTREACHED();
}
void RasterImplementation::EndGpuRaster() {
  NOTREACHED();
}

void RasterImplementation::BeginSharedImageAccessDirectCHROMIUM(GLuint texture,
                                                                GLenum mode) {
  NOTREACHED();
}

void RasterImplementation::EndSharedImageAccessDirectCHROMIUM(GLuint texture) {
  NOTREACHED();
}

void RasterImplementation::InitializeDiscardableTextureCHROMIUM(
    GLuint texture) {
  NOTREACHED();
}

void RasterImplementation::UnlockDiscardableTextureCHROMIUM(GLuint texture) {
  NOTREACHED();
}

bool RasterImplementation::LockDiscardableTextureCHROMIUM(GLuint texture) {
  NOTREACHED();
  return false;
}

void RasterImplementation::TraceBeginCHROMIUM(const char* category_name,
                                              const char* trace_name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glTraceBeginCHROMIUM("
                     << category_name << ", " << trace_name << ")");
  static constexpr size_t kMaxStrLen = 256;
  DCHECK_LE(strlen(category_name), kMaxStrLen);
  DCHECK_LE(strlen(trace_name), kMaxStrLen);
  SetBucketAsCString(kResultBucketId, category_name);
  SetBucketAsCString(kResultBucketId + 1, trace_name);
  helper_->TraceBeginCHROMIUM(kResultBucketId, kResultBucketId + 1);
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->SetBucketSize(kResultBucketId + 1, 0);
  current_trace_stack_++;
}

void RasterImplementation::TraceEndCHROMIUM() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glTraceEndCHROMIUM("
                     << ")");
  if (current_trace_stack_ == 0) {
    SetGLError(GL_INVALID_OPERATION, "glTraceEndCHROMIUM",
               "missing begin trace");
    return;
  }
  helper_->TraceEndCHROMIUM();
  current_trace_stack_--;
}

void RasterImplementation::SetActiveURLCHROMIUM(const char* url) {
  DCHECK(url);
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glSetActiveURLCHROMIUM(" << url);

  if (last_active_url_ == url)
    return;

  last_active_url_ = url;
  static constexpr uint32_t kMaxStrLen = 1024;
  size_t len = strlen(url);
  if (len == 0)
    return;

  SetBucketContents(kResultBucketId, url,
                    base::CheckMin(len, kMaxStrLen).ValueOrDie());
  helper_->SetActiveURLCHROMIUM(kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
}

cc::ClientPaintCache* RasterImplementation::GetOrCreatePaintCache() {
  if (!paint_cache_) {
    constexpr size_t kNormalPaintCacheBudget = 4 * 1024 * 1024;
    constexpr size_t kLowEndPaintCacheBudget = 256 * 1024;
    size_t paint_cache_budget = 0u;
    if (base::SysInfo::IsLowEndDevice())
      paint_cache_budget = kLowEndPaintCacheBudget;
    else
      paint_cache_budget = kNormalPaintCacheBudget;
    paint_cache_ = std::make_unique<cc::ClientPaintCache>(paint_cache_budget);
  }
  return paint_cache_.get();
}

void RasterImplementation::FlushPaintCachePurgedEntries() {
  if (!paint_cache_)
    return;

  paint_cache_->Purge(&temp_paint_cache_purged_data_);
  for (uint32_t i = static_cast<uint32_t>(cc::PaintCacheDataType::kTextBlob);
       i < cc::PaintCacheDataTypeCount; ++i) {
    auto& ids = temp_paint_cache_purged_data_[i];
    if (ids.empty())
      continue;

    switch (static_cast<cc::PaintCacheDataType>(i)) {
      case cc::PaintCacheDataType::kTextBlob:
        helper_->DeletePaintCacheTextBlobsINTERNALImmediate(ids.size(),
                                                            ids.data());
        break;
      case cc::PaintCacheDataType::kPath:
        helper_->DeletePaintCachePathsINTERNALImmediate(ids.size(), ids.data());
        break;
    }
    ids.clear();
  }
}

void RasterImplementation::ClearPaintCache() {
  if (!paint_cache_ || !paint_cache_->PurgeAll())
    return;

  helper_->ClearPaintCacheINTERNAL();
}

std::unique_ptr<cc::TransferCacheSerializeHelper>
RasterImplementation::CreateTransferCacheHelperForTesting() {
  return std::make_unique<TransferCacheSerializeHelperImpl>(this);
}

void RasterImplementation::SetRasterMappedBufferForTesting(
    ScopedTransferBufferPtr buffer) {
  raster_mapped_buffer_.emplace(std::move(buffer));
}

RasterImplementation::RasterProperties::RasterProperties(
    SkColor background_color,
    bool can_use_lcd_text,
    sk_sp<SkColorSpace> color_space)
    : background_color(background_color),
      can_use_lcd_text(can_use_lcd_text),
      color_space(std::move(color_space)) {}

RasterImplementation::RasterProperties::~RasterProperties() = default;

}  // namespace raster
}  // namespace gpu
