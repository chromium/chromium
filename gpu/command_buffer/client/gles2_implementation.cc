// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// A class to emulate GLES2 over command buffers.

#include "gpu/command_buffer/client/gles2_implementation.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>

#include "base/atomic_sequence_num.h"
#include "base/bits.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/ostream_operators.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/nacl/common/buildflags.h"
#include "gpu/command_buffer/client/buffer_tracker.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/client/program_info_manager.h"
#include "gpu/command_buffer/client/query_tracker.h"
#include "gpu/command_buffer/client/readback_buffer_shadow_tracker.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/client/transfer_buffer_cmd_copy_helpers.h"
#include "gpu/command_buffer/client/vertex_array_object_manager.h"
#include "gpu/command_buffer/common/context_creation_attribs.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/id_allocator.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gl/gpu_preference.h"

#if !defined(__native_client__) && !BUILDFLAG(IS_MINIMAL_TOOLCHAIN)
#include "ui/gfx/color_space.h"                 // nogncheck
#include "ui/gfx/ipc/color/gfx_param_traits.h"  // nogncheck
#endif

#if defined(GPU_CLIENT_DEBUG)
#define GPU_CLIENT_SINGLE_THREAD_CHECK() \
  DeferErrorCallbacks deferrer(this);    \
  SingleThreadChecker checker(this);
#else  // !defined(GPU_CLIENT_DEBUG)
#define GPU_CLIENT_SINGLE_THREAD_CHECK() DeferErrorCallbacks deferrer(this);
#endif  // defined(GPU_CLIENT_DEBUG)

// Check that destination pointers point to initialized memory.
// When the context is lost, calling GL function has no effect so if destination
// pointers point to initialized memory it can often lead to crash bugs. eg.
//
// GLsizei len;
// glGetShaderSource(shader, max_size, &len, buffer);
// std::string src(buffer, buffer + len);  // len can be uninitialized here!!!
//
// Because this check is not official GL this check happens only on Chrome code,
// not Pepper.
//
// If it was up to us we'd just always write to the destination but the OpenGL
// spec defines the behavior of OpenGL functions, not us. :-(
#if defined(__native_client__) || BUILDFLAG(IS_MINIMAL_TOOLCHAIN)
#define GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION_ASSERT(v)
#define GPU_CLIENT_DCHECK(v)
#elif defined(GPU_DCHECK)
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

namespace gpu {
namespace gles2 {

namespace {

void CopyRectToBuffer(const void* pixels,
                      uint32_t height,
                      uint32_t unpadded_row_size,
                      uint32_t pixels_padded_row_size,
                      void* buffer,
                      uint32_t buffer_padded_row_size) {
  if (height == 0)
    return;
  const int8_t* source = static_cast<const int8_t*>(pixels);
  int8_t* dest = static_cast<int8_t*>(buffer);
  if (pixels_padded_row_size != buffer_padded_row_size) {
    for (uint32_t ii = 0; ii < height; ++ii) {
      memcpy(dest, source, unpadded_row_size);
      dest += buffer_padded_row_size;
      source += pixels_padded_row_size;
    }
  } else {
    uint32_t size = (height - 1) * pixels_padded_row_size + unpadded_row_size;
    memcpy(dest, source, size);
  }
}

static base::AtomicSequenceNumber g_flush_id;

uint32_t GenerateNextFlushId() {
  return static_cast<uint32_t>(g_flush_id.GetNext());
}

bool IsReadbackUsage(GLenum usage) {
  return usage == GL_STREAM_READ || usage == GL_DYNAMIC_READ ||
         usage == GL_STATIC_READ;
}

void UpdateProgramInfo(base::span<const uint8_t>& data,
                       ProgramInfoManager* manager,
                       ProgramInfoManager::ProgramInfoType type) {
  DCHECK(data.size() > sizeof(cmds::GLES2ReturnProgramInfo));
  const cmds::GLES2ReturnProgramInfo* return_program_info =
      reinterpret_cast<const cmds::GLES2ReturnProgramInfo*>(data.data());
  uint32_t program = return_program_info->program_client_id;
  base::span<const int8_t> info(
      reinterpret_cast<const int8_t*>(return_program_info->deserialized_buffer),
      data.size() - sizeof(cmds::GLES2ReturnProgramInfo));
  manager->UpdateProgramInfo(program, info, type);
}

}  // anonymous namespace

GLES2Implementation::GLStaticState::GLStaticState() = default;

GLES2Implementation::GLStaticState::~GLStaticState() = default;

GLES2Implementation::DeferErrorCallbacks::DeferErrorCallbacks(
    GLES2Implementation* gles2_implementation)
    : gles2_implementation_(*gles2_implementation) {
  DCHECK_EQ(false, gles2_implementation_.deferring_error_callbacks_);
  gles2_implementation_.deferring_error_callbacks_ = true;
}

GLES2Implementation::DeferErrorCallbacks::~DeferErrorCallbacks() {
  DCHECK_EQ(true, gles2_implementation_.deferring_error_callbacks_);
  gles2_implementation_.deferring_error_callbacks_ = false;
  gles2_implementation_.CallDeferredErrorCallbacks();
}

GLES2Implementation::DeferredErrorCallback::DeferredErrorCallback(
    std::string message,
    int32_t id)
    : message(std::move(message)), id(id) {}

GLES2Implementation::SingleThreadChecker::SingleThreadChecker(
    GLES2Implementation* gles2_implementation)
    : gles2_implementation_(gles2_implementation) {
  CHECK_EQ(0, gles2_implementation_->use_count_);
  ++gles2_implementation_->use_count_;
}

GLES2Implementation::SingleThreadChecker::~SingleThreadChecker() {
  --gles2_implementation_->use_count_;
  CHECK_EQ(0, gles2_implementation_->use_count_);
}

GLES2Implementation::GLES2Implementation(
    GLES2CmdHelper* helper,
    scoped_refptr<ShareGroup> share_group,
    TransferBufferInterface* transfer_buffer,
    bool bind_generates_resource,
    bool lose_context_when_out_of_memory,
    bool support_client_side_arrays,
    GpuControl* gpu_control)
    : ImplementationBase(helper, transfer_buffer, gpu_control),
      helper_(helper),
      chromium_framebuffer_multisample_(kUnknownExtensionStatus),
      gl_capabilities_(gpu_control->GetGLCapabilities()),
      pack_alignment_(4),
      pack_row_length_(0),
      pack_skip_pixels_(0),
      pack_skip_rows_(0),
      unpack_alignment_(4),
      unpack_row_length_(0),
      unpack_image_height_(0),
      unpack_skip_rows_(0),
      unpack_skip_pixels_(0),
      unpack_skip_images_(0),
      active_texture_unit_(0),
      bound_framebuffer_(0),
      bound_read_framebuffer_(0),
      bound_renderbuffer_(0),
      current_program_(0),
      bound_array_buffer_(0),
      bound_atomic_counter_buffer_(0),
      bound_copy_read_buffer_(0),
      bound_copy_write_buffer_(0),
      bound_dispatch_indirect_buffer_(0),
      bound_draw_indirect_buffer_(0),
      bound_pixel_pack_buffer_(0),
      bound_pixel_unpack_buffer_(0),
      bound_shader_storage_buffer_(0),
      bound_transform_feedback_buffer_(0),
      bound_uniform_buffer_(0),
      bound_pixel_pack_transfer_buffer_id_(0),
      bound_pixel_unpack_transfer_buffer_id_(0),
      error_bits_(0),
      lose_context_when_out_of_memory_(lose_context_when_out_of_memory),
      support_client_side_arrays_(support_client_side_arrays),
      use_count_(0),
      flush_id_(0),
      max_extra_transfer_buffer_size_(0),
      current_trace_stack_(0),
      aggressively_free_resources_(false),
      cached_extension_string_(nullptr) {
  DCHECK(helper);

  std::stringstream ss;
  ss << std::hex << this;
  this_in_hex_ = ss.str();

  share_group_ =
      (share_group ? std::move(share_group)
                   : new ShareGroup(
                         bind_generates_resource,
                         gpu_control_->GetCommandBufferID().GetUnsafeValue()));
  DCHECK(share_group_->bind_generates_resource() == bind_generates_resource);

  memset(&reserved_ids_, 0, sizeof(reserved_ids_));
}

gpu::ContextResult GLES2Implementation::Initialize(
    const SharedMemoryLimits& limits) {
  TRACE_EVENT0("gpu", "GLES2Implementation::Initialize");
  auto result = ImplementationBase::Initialize(limits);
  if (result != gpu::ContextResult::kSuccess) {
    return result;
  }

  max_extra_transfer_buffer_size_ = limits.max_mapped_memory_for_texture_upload;

  GLStaticState::ShaderPrecisionMap* shader_precisions =
      &static_state_.shader_precisions;
  gl_capabilities_.VisitPrecisions(
      [shader_precisions](GLenum shader, GLenum type,
                          GLCapabilities::ShaderPrecision* result) {
        const GLStaticState::ShaderPrecisionKey key(shader, type);
        cmds::GetShaderPrecisionFormat::Result cached_result = {
            true, result->min_range, result->max_range, result->precision};
        shader_precisions->insert(std::make_pair(key, cached_result));
      });

  util_.set_num_compressed_texture_formats(
      gl_capabilities_.num_compressed_texture_formats);
  util_.set_num_shader_binary_formats(
      gl_capabilities_.num_shader_binary_formats);

  texture_units_ = std::make_unique<TextureUnit[]>(
      gl_capabilities_.max_combined_texture_image_units);

  buffer_tracker_ = std::make_unique<BufferTracker>(mapped_memory_.get());
  readback_buffer_shadow_tracker_ =
      std::make_unique<ReadbackBufferShadowTracker>(mapped_memory_.get(),
                                                    helper_);

  for (int i = 0; i < static_cast<int>(IdNamespaces::kNumIdNamespaces); ++i)
    id_allocators_[i] = std::make_unique<IdAllocator>();

  if (support_client_side_arrays_) {
    GetIdHandler(SharedIdNamespaces::kBuffers)
        ->MakeIds(this, kClientSideArrayId, std::size(reserved_ids_),
                  &reserved_ids_[0]);
  }

  vertex_array_object_manager_ = std::make_unique<VertexArrayObjectManager>(
      gl_capabilities_.max_vertex_attribs, reserved_ids_[0], reserved_ids_[1],
      support_client_side_arrays_);

  // GL_BIND_GENERATES_RESOURCE_CHROMIUM state must be the same
  // on Client & Service.
  if (gl_capabilities_.bind_generates_resource_chromium !=
      (share_group_->bind_generates_resource() ? 1 : 0)) {
    SetGLError(GL_INVALID_OPERATION, "Initialize",
               "Service bind_generates_resource mismatch.");
    LOG(ERROR) << "ContextResult::kFatalFailure: "
               << "bind_generates_resource mismatch";
    return gpu::ContextResult::kFatalFailure;
  }

  return gpu::ContextResult::kSuccess;
}

GLES2Implementation::~GLES2Implementation() {
  // Assure no DeferErrorCallbacks instances are loose in the wild.
  CHECK(!deferring_error_callbacks_);

  // Make sure the queries are finished otherwise we'll delete the
  // shared memory (mapped_memory_) which will free the memory used
  // by the queries. The GPU process when validating that memory is still
  // shared will fail and abort (ie, it will stop running).
  WaitForCmd();

  query_tracker_.reset();

  // GLES2Implementation::Initialize() could fail before allocating
  // reserved_ids_, so we need delete them carefully.
  if (support_client_side_arrays_ && reserved_ids_[0]) {
    DeleteBuffers(std::size(reserved_ids_), &reserved_ids_[0]);
  }

  // Release remaining BufferRange mem; This is when a MapBufferRange() is
  // called but not the UnmapBuffer() pair.
  ClearMappedBufferRangeMap();

  // Release remaining BufferMap mem; This is when a MapBufferSubData() is
  // called but not the UnmapBufferSubData() pair.
  ClearMappedBufferMap();

  // Release remaining TextureMap mem; This is when a MapTexSubImage2D() is
  // called but not the UnmapTexSubImage2D() pair.
  ClearMappedTextureMap();

  // Release any per-context data in share group.
  share_group_->FreeContext(this);

  buffer_tracker_.reset();
  readback_buffer_shadow_tracker_.reset();

  // Make sure the commands make it the service.
  WaitForCmd();
}

GLES2CmdHelper* GLES2Implementation::helper() const {
  return helper_;
}

IdHandlerInterface* GLES2Implementation::GetIdHandler(
    SharedIdNamespaces namespace_id) const {
  return share_group_->GetIdHandler(namespace_id);
}

RangeIdHandlerInterface* GLES2Implementation::GetRangeIdHandler(
    int namespace_id) const {
  return share_group_->GetRangeIdHandler(namespace_id);
}

IdAllocator* GLES2Implementation::GetIdAllocator(
    IdNamespaces namespace_id) const {
  return id_allocators_[static_cast<int>(namespace_id)].get();
}

void GLES2Implementation::OnGpuControlLostContext() {
  // This should never occur more than once.
  DCHECK(!lost_context_callback_run_);
  lost_context_callback_run_ = true;
  share_group_->Lose();
  if (!lost_context_callback_.is_null()) {
    std::move(lost_context_callback_).Run();
  }
}

void GLES2Implementation::OnGpuControlLostContextMaybeReentrant() {
  // Queries for lost context state should immediately reflect reality,
  // but don't call out to clients yet to avoid them re-entering this
  // class.
  share_group_->Lose();
}

void GLES2Implementation::OnGpuControlErrorMessage(const char* message,
                                                   int32_t id) {
  SendErrorMessage(message, id);
}

void GLES2Implementation::OnGpuSwitched(
    gl::GpuPreference active_gpu_heuristic) {
  gpu_switched_ = true;
  active_gpu_heuristic_ = active_gpu_heuristic;
}

GLboolean GLES2Implementation::DidGpuSwitch(gl::GpuPreference* active_gpu) {
  if (gpu_switched_) {
    *active_gpu = active_gpu_heuristic_;
  }
  GLboolean result = gpu_switched_ ? GL_TRUE : GL_FALSE;
  gpu_switched_ = false;
  return result;
}

void GLES2Implementation::SendErrorMessage(std::string message, int32_t id) {
  if (error_message_callback_.is_null())
    return;

  if (deferring_error_callbacks_) {
    deferred_error_callbacks_.emplace_back(std::move(message), id);
    return;
  }

  error_message_callback_.Run(message.c_str(), id);
}

void GLES2Implementation::CallDeferredErrorCallbacks() {
  if (deferred_error_callbacks_.empty())
    return;

  if (error_message_callback_.is_null()) {
    // User probably cleared this out.
    deferred_error_callbacks_.clear();
    return;
  }

  std::deque<DeferredErrorCallback> local_callbacks;
  std::swap(deferred_error_callbacks_, local_callbacks);
  for (auto c : local_callbacks) {
    error_message_callback_.Run(c.message.c_str(), c.id);
  }
}

void GLES2Implementation::OnGpuControlReturnData(
    base::span<const uint8_t> data) {
  DCHECK(data.size() > sizeof(cmds::GLES2ReturnDataHeader));
  const cmds::GLES2ReturnDataHeader& gles2ReturnDataHeader =
      *reinterpret_cast<const cmds::GLES2ReturnDataHeader*>(data.data());

  switch (gles2ReturnDataHeader.return_data_type) {
    case GLES2ReturnDataType::kES2ProgramInfo: {
      UpdateProgramInfo(data, share_group_->program_info_manager(),
                        ProgramInfoManager::kES2);
    } break;
    case GLES2ReturnDataType::kES3UniformBlocks: {
      UpdateProgramInfo(data, share_group_->program_info_manager(),
                        ProgramInfoManager::kES3UniformBlocks);
    } break;
    case GLES2ReturnDataType::kES3TransformFeedbackVaryings: {
      UpdateProgramInfo(data, share_group_->program_info_manager(),
                        ProgramInfoManager::kES3TransformFeedbackVaryings);
    } break;
    case GLES2ReturnDataType::kES3Uniforms: {
      UpdateProgramInfo(data, share_group_->program_info_manager(),
                        ProgramInfoManager::kES3Uniformsiv);
    } break;

    default:
      NOTREACHED_IN_MIGRATION();
  }
}

void GLES2Implementation::FreeSharedMemory(void* mem) {
  mapped_memory_->FreePendingToken(mem, helper_->InsertToken());
}

GLuint GLES2Implementation::CreateGpuFenceCHROMIUM() {
  GLuint client_id = GetIdAllocator(IdNamespaces::kGpuFences)
                         ->AllocateIDAtOrAbove(last_gpu_fence_id_ + 1);
  // Out of paranoia, don't allow IDs to wrap around to avoid potential
  // collisions on reuse. The space of 2^32 IDs is enough for over a year of
  // allocating two per frame at 60fps. TODO(crbug.com/40552536): Revisit if
  // this is an issue, for example by deferring ID release if they would be
  // reissued too soon.
  CHECK(client_id > last_gpu_fence_id_) << "ID wrap prevented";
  last_gpu_fence_id_ = client_id;
  helper_->CreateGpuFenceINTERNAL(client_id);
  GPU_CLIENT_LOG("returned " << client_id);
  CheckGLError();
  return client_id;
}

GLuint GLES2Implementation::CreateClientGpuFenceCHROMIUM(
    ClientGpuFence source) {
  GLuint client_id = GetIdAllocator(IdNamespaces::kGpuFences)
                         ->AllocateIDAtOrAbove(last_gpu_fence_id_ + 1);
  // See CreateGpuFenceCHROMIUM comment re wraparound.
  CHECK(client_id > last_gpu_fence_id_) << "ID wrap prevented";
  last_gpu_fence_id_ = client_id;

  // Create the service-side GpuFenceEntry via gpu_control. This is guaranteed
  // to arrive before any future GL helper_ commands on this stream, so it's
  // safe to use the client_id generated here in following commands such as
  // WaitGpuFenceCHROMIUM without explicit flushing.
  gpu_control_->CreateGpuFence(client_id, source);

  GPU_CLIENT_LOG("returned " << client_id);
  CheckGLError();
  return client_id;
}

void GLES2Implementation::DestroyGpuFenceCHROMIUMHelper(GLuint client_id) {
  if (GetIdAllocator(IdNamespaces::kGpuFences)->InUse(client_id)) {
    GetIdAllocator(IdNamespaces::kGpuFences)->FreeID(client_id);
    helper_->DestroyGpuFenceCHROMIUM(client_id);
  } else {
    SetGLError(GL_INVALID_VALUE, "glDestroyGpuFenceCHROMIUM",
               "id not created by this context.");
  }
}

void GLES2Implementation::SetAggressivelyFreeResources(
    bool aggressively_free_resources) {
  TRACE_EVENT1("gpu", "GLES2Implementation::SetAggressivelyFreeResources",
               "aggressively_free_resources", aggressively_free_resources);
  aggressively_free_resources_ = aggressively_free_resources;

  if (aggressively_free_resources_ && helper_->HaveRingBuffer()) {
    // Ensure that we clean up as much cache memory as possible and fully flush.
    FlushDriverCachesCHROMIUM();

    // Flush will delete transfer buffer resources if
    // |aggressively_free_resources_| is true.
    Flush();
  } else {
    ShallowFlushCHROMIUM();
  }
}

bool GLES2Implementation::IsExtensionAvailable(const char* ext) {
  const char* extensions =
      reinterpret_cast<const char*>(GetStringHelper(GL_EXTENSIONS));
  if (!extensions)
    return false;

  int length = strlen(ext);
  while (true) {
    int n = strcspn(extensions, " ");
    if (n == length && 0 == strncmp(ext, extensions, length)) {
      return true;
    }
    if ('\0' == extensions[n]) {
      return false;
    }
    extensions += n + 1;
  }
}

bool GLES2Implementation::IsExtensionAvailableHelper(const char* extension,
                                                     ExtensionStatus* status) {
  switch (*status) {
    case kAvailableExtensionStatus:
      return true;
    case kUnavailableExtensionStatus:
      return false;
    default: {
      bool available = IsExtensionAvailable(extension);
      *status =
          available ? kAvailableExtensionStatus : kUnavailableExtensionStatus;
      return available;
    }
  }
}

bool GLES2Implementation::IsChromiumFramebufferMultisampleAvailable() {
  return IsExtensionAvailableHelper("GL_CHROMIUM_framebuffer_multisample",
                                    &chromium_framebuffer_multisample_);
}

const std::string& GLES2Implementation::GetLogPrefix() const {
  const std::string& prefix(debug_marker_manager_.GetMarker());
  return prefix.empty() ? this_in_hex_ : prefix;
}

GLenum GLES2Implementation::GetError() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetError()");
  GLenum err = GetGLError();
  GPU_CLIENT_LOG("returned " << GLES2Util::GetStringError(err));
  return err;
}

GLenum GLES2Implementation::GetGLError() {
  TRACE_EVENT0("gpu", "GLES2::GetGLError");
  // Check the GL error first, then our wrapped error.
  typedef cmds::GetError::Result Result;
  auto result = GetResultAs<Result>();
  // If we couldn't allocate a result the context is lost.
  if (!result) {
    return GL_NO_ERROR;
  }
  *result = GL_NO_ERROR;
  helper_->GetError(GetResultShmId(), result.offset());
  if (!WaitForCmd()) {
    return GL_NO_ERROR;
  }
  GLenum error = *result;
  if (error == GL_NO_ERROR) {
    error = GetClientSideGLError();
  } else {
    // There was an error, clear the corresponding wrapped error.
    error_bits_ &= ~GLES2Util::GLErrorToErrorBit(error);
  }
  return error;
}

#if defined(GL_CLIENT_FAIL_GL_ERRORS)
void GLES2Implementation::FailGLError(GLenum error) {
  if (error != GL_NO_ERROR) {
    NOTREACHED_IN_MIGRATION() << "Error";
  }
}
// NOTE: Calling GetGLError overwrites data in the result buffer.
void GLES2Implementation::CheckGLError() {
  FailGLError(GetGLError());
}
#endif  // defined(GPU_CLIENT_FAIL_GL_ERRORS)

void GLES2Implementation::SetGLError(GLenum error,
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
    SendErrorMessage(temp.c_str(), 0);
  }
  error_bits_ |= GLES2Util::GLErrorToErrorBit(error);

  if (error == GL_OUT_OF_MEMORY && lose_context_when_out_of_memory_) {
    helper_->LoseContextCHROMIUM(GL_GUILTY_CONTEXT_RESET_ARB,
                                 GL_UNKNOWN_CONTEXT_RESET_ARB);
  }
}

void GLES2Implementation::SetGLErrorInvalidEnum(const char* function_name,
                                                GLenum value,
                                                const char* label) {
  SetGLError(
      GL_INVALID_ENUM, function_name,
      (std::string(label) + " was " + GLES2Util::GetStringEnum(value)).c_str());
}

void GLES2Implementation::Disable(GLenum cap) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDisable("
                     << GLES2Util::GetStringCapability(cap) << ")");
  bool changed = false;
  if (!state_.SetCapabilityState(cap, false, &changed) || changed) {
    helper_->Disable(cap);
  }
  CheckGLError();
}

void GLES2Implementation::DisableiOES(GLenum target, GLuint index) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDisableiOES("
                     << GLES2Util::GetStringEnum(target) << ", " << index
                     << ")");
  if (index == 0u && target == GL_BLEND) {
    bool changed = false;
    DCHECK(target == GL_BLEND);
    if (!state_.SetCapabilityState(target, false, &changed) || changed) {
      helper_->DisableiOES(target, index);
    }
  } else {
    helper_->DisableiOES(target, index);
  }

  CheckGLError();
}

void GLES2Implementation::Enable(GLenum cap) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glEnable("
                     << GLES2Util::GetStringCapability(cap) << ")");
  bool changed = false;
  if (!state_.SetCapabilityState(cap, true, &changed) || changed) {
    helper_->Enable(cap);
  }
  CheckGLError();
}

void GLES2Implementation::EnableiOES(GLenum target, GLuint index) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glEnableiOES("
                     << GLES2Util::GetStringEnum(target) << ", " << index
                     << ")");
  if (index == 0u && target == GL_BLEND) {
    bool changed = false;
    DCHECK(target == GL_BLEND);
    if (!state_.SetCapabilityState(target, true, &changed) || changed) {
      helper_->EnableiOES(target, index);
    }
  } else {
    helper_->EnableiOES(target, index);
  }

  CheckGLError();
}

GLboolean GLES2Implementation::IsEnabled(GLenum cap) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glIsEnabled("
                     << GLES2Util::GetStringCapability(cap) << ")");
  bool state = false;
  if (!state_.GetEnabled(cap, &state)) {
    typedef cmds::IsEnabled::Result Result;
    auto result = GetResultAs<Result>();
    if (!result) {
      return GL_FALSE;
    }
    *result = 0;
    helper_->IsEnabled(cap, GetResultShmId(), result.offset());
    if (!WaitForCmd()) {
      return GL_FALSE;
    }
    state = (*result) != 0;
  }

  GPU_CLIENT_LOG("returned " << state);
  CheckGLError();
  return state;
}

GLboolean GLES2Implementation::IsEnablediOES(GLenum target, GLuint index) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glIsEnablediOES("
                     << GLES2Util::GetStringCapability(target) << ", " << index
                     << ")");
  bool state = false;
  typedef cmds::IsEnabled::Result Result;
  // Limit scope of result to avoid overlap with CheckGLError()
  {
    auto result = GetResultAs<Result>();
    *result = 0;
    helper_->IsEnablediOES(target, index, GetResultShmId(), result.offset());
    if (!WaitForCmd()) {
      return GL_FALSE;
    }
    state = (*result) != 0;
  }

  GPU_CLIENT_LOG("returned " << state);
  CheckGLError();
  return state;
}

bool GLES2Implementation::GetHelper(GLenum pname, GLint* params) {
  // TODO(zmo): For all the BINDING points, there is a possibility where
  // resources are shared among multiple contexts, that the cached points
  // are invalid. It is not a problem for now, but once we allow resource
  // sharing in WebGL, we need to implement a mechanism to allow correct
  // client side binding points tracking.  crbug.com/465562.

  // ES2 parameters.
  switch (pname) {
    case GL_ACTIVE_TEXTURE:
      *params = active_texture_unit_ + GL_TEXTURE0;
      return true;
    case GL_ARRAY_BUFFER_BINDING:
      *params = bound_array_buffer_;
      return true;
    case GL_ELEMENT_ARRAY_BUFFER_BINDING:
      *params = vertex_array_object_manager_->bound_element_array_buffer();
      return true;
    case GL_FRAMEBUFFER_BINDING:
      *params = bound_framebuffer_;
      return true;
    case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      *params = gl_capabilities_.max_combined_texture_image_units;
      return true;
    case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
      *params = gl_capabilities_.max_cube_map_texture_size;
      return true;
    case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      *params = gl_capabilities_.max_fragment_uniform_vectors;
      return true;
    case GL_MAX_RENDERBUFFER_SIZE:
      *params = gl_capabilities_.max_renderbuffer_size;
      return true;
    case GL_MAX_TEXTURE_IMAGE_UNITS:
      *params = gl_capabilities_.max_texture_image_units;
      return true;
    case GL_MAX_TEXTURE_SIZE:
      *params = gl_capabilities_.max_texture_size;
      return true;
    case GL_MAX_VARYING_VECTORS:
      *params = gl_capabilities_.max_varying_vectors;
      return true;
    case GL_MAX_VERTEX_ATTRIBS:
      *params = gl_capabilities_.max_vertex_attribs;
      return true;
    case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      *params = gl_capabilities_.max_vertex_texture_image_units;
      return true;
    case GL_MAX_VERTEX_UNIFORM_VECTORS:
      *params = gl_capabilities_.max_vertex_uniform_vectors;
      return true;
    case GL_MAX_VIEWPORT_DIMS:
      if (gl_capabilities_.max_viewport_width > 0 &&
          gl_capabilities_.max_viewport_height > 0) {
        params[0] = gl_capabilities_.max_viewport_width;
        params[1] = gl_capabilities_.max_viewport_height;
        return true;
      }
      // If they are not cached on the client side yet, query the service side.
      return false;
    case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      *params = gl_capabilities_.num_compressed_texture_formats;
      return true;
    case GL_NUM_SHADER_BINARY_FORMATS:
      *params = gl_capabilities_.num_shader_binary_formats;
      return true;
    case GL_RENDERBUFFER_BINDING:
      *params = bound_renderbuffer_;
      return true;
    case GL_TEXTURE_BINDING_2D:
      *params = texture_units_[active_texture_unit_].bound_texture_2d;
      return true;
    case GL_TEXTURE_BINDING_CUBE_MAP:
      *params = texture_units_[active_texture_unit_].bound_texture_cube_map;
      return true;

    // Non-standard parameters.
    case GL_TEXTURE_BINDING_EXTERNAL_OES:
      *params = texture_units_[active_texture_unit_].bound_texture_external_oes;
      return true;
    case GL_TEXTURE_BINDING_RECTANGLE_ARB:
      *params =
          texture_units_[active_texture_unit_].bound_texture_rectangle_arb;
      return true;
    case GL_PIXEL_PACK_TRANSFER_BUFFER_BINDING_CHROMIUM:
      *params = bound_pixel_pack_transfer_buffer_id_;
      return true;
    case GL_PIXEL_UNPACK_TRANSFER_BUFFER_BINDING_CHROMIUM:
      *params = bound_pixel_unpack_transfer_buffer_id_;
      return true;
    case GL_READ_FRAMEBUFFER_BINDING:
      if (gl_capabilities_.major_version >= 3 ||
          IsChromiumFramebufferMultisampleAvailable()) {
        *params = bound_read_framebuffer_;
        return true;
      }
      break;
    case GL_TIMESTAMP_EXT:
      // We convert all GPU timestamps to CPU time.
      *params = base::saturated_cast<GLint>(
          (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds() *
          base::Time::kNanosecondsPerMicrosecond);
      return true;
    case GL_GPU_DISJOINT_EXT:
      *params = static_cast<GLint>(query_tracker_->CheckAndResetDisjoint());
      return true;
    case GL_UNPACK_ALIGNMENT:
      *params = unpack_alignment_;
      return true;
    case GL_VIEWPORT:
      if (state_.viewport_width > 0 && state_.viewport_height > 0 &&
          gl_capabilities_.max_viewport_width > 0 &&
          gl_capabilities_.max_viewport_height > 0) {
        params[0] = state_.viewport_x;
        params[1] = state_.viewport_y;
        params[2] = std::min(state_.viewport_width,
                             gl_capabilities_.max_viewport_width);
        params[3] = std::min(state_.viewport_height,
                             gl_capabilities_.max_viewport_height);
        return true;
      }
      // If they haven't been cached on the client side, go to service side
      // to query the underlying driver.
      return false;

    // Non-cached parameters.
    case GL_ALIASED_LINE_WIDTH_RANGE:
    case GL_ALIASED_POINT_SIZE_RANGE:
    case GL_ALPHA_BITS:
    case GL_BLEND:
    case GL_BLEND_COLOR:
    case GL_BLEND_DST_ALPHA:
    case GL_BLEND_DST_RGB:
    case GL_BLEND_EQUATION_ALPHA:
    case GL_BLEND_EQUATION_RGB:
    case GL_BLEND_SRC_ALPHA:
    case GL_BLEND_SRC_RGB:
    case GL_BLUE_BITS:
    case GL_COLOR_CLEAR_VALUE:
    case GL_COLOR_WRITEMASK:
    case GL_COMPRESSED_TEXTURE_FORMATS:
    case GL_CULL_FACE:
    case GL_CULL_FACE_MODE:
    case GL_CURRENT_PROGRAM:
    case GL_DEPTH_BITS:
    case GL_DEPTH_CLEAR_VALUE:
    case GL_DEPTH_FUNC:
    case GL_DEPTH_RANGE:
    case GL_DEPTH_TEST:
    case GL_DEPTH_WRITEMASK:
    case GL_DITHER:
    case GL_FRONT_FACE:
    case GL_GENERATE_MIPMAP_HINT:
    case GL_GREEN_BITS:
    case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
    case GL_IMPLEMENTATION_COLOR_READ_TYPE:
    case GL_LINE_WIDTH:
    case GL_PACK_ALIGNMENT:
    case GL_POLYGON_OFFSET_FACTOR:
    case GL_POLYGON_OFFSET_FILL:
    case GL_POLYGON_OFFSET_UNITS:
    case GL_RED_BITS:
    case GL_SAMPLE_ALPHA_TO_COVERAGE:
    case GL_SAMPLE_BUFFERS:
    case GL_SAMPLE_COVERAGE:
    case GL_SAMPLE_COVERAGE_INVERT:
    case GL_SAMPLE_COVERAGE_VALUE:
    case GL_SAMPLES:
    case GL_SCISSOR_BOX:
    case GL_SCISSOR_TEST:
    case GL_SHADER_BINARY_FORMATS:
    case GL_SHADER_COMPILER:
    case GL_STENCIL_BACK_FAIL:
    case GL_STENCIL_BACK_FUNC:
    case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
    case GL_STENCIL_BACK_PASS_DEPTH_PASS:
    case GL_STENCIL_BACK_REF:
    case GL_STENCIL_BACK_VALUE_MASK:
    case GL_STENCIL_BACK_WRITEMASK:
    case GL_STENCIL_BITS:
    case GL_STENCIL_CLEAR_VALUE:
    case GL_STENCIL_FAIL:
    case GL_STENCIL_FUNC:
    case GL_STENCIL_PASS_DEPTH_FAIL:
    case GL_STENCIL_PASS_DEPTH_PASS:
    case GL_STENCIL_REF:
    case GL_STENCIL_TEST:
    case GL_STENCIL_VALUE_MASK:
    case GL_STENCIL_WRITEMASK:
    case GL_SUBPIXEL_BITS:
      return false;
    default:
      break;
  }

  if (gl_capabilities_.major_version < 3) {
    return false;
  }

  // ES3 parameters.
  switch (pname) {
    case GL_COPY_READ_BUFFER_BINDING:
      *params = bound_copy_read_buffer_;
      return true;
    case GL_COPY_WRITE_BUFFER_BINDING:
      *params = bound_copy_write_buffer_;
      return true;
    case GL_MAJOR_VERSION:
      *params = gl_capabilities_.major_version;
      return true;
    case GL_MAX_3D_TEXTURE_SIZE:
      *params = gl_capabilities_.max_3d_texture_size;
      return true;
    case GL_MAX_ARRAY_TEXTURE_LAYERS:
      *params = gl_capabilities_.max_array_texture_layers;
      return true;
    case GL_MAX_COLOR_ATTACHMENTS:
      *params = gl_capabilities_.max_color_attachments;
      return true;
    case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
      *params = static_cast<GLint>(
          gl_capabilities_.max_combined_fragment_uniform_components);
      return true;
    case GL_MAX_COMBINED_UNIFORM_BLOCKS:
      *params = gl_capabilities_.max_combined_uniform_blocks;
      return true;
    case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
      *params = static_cast<GLint>(
          gl_capabilities_.max_combined_vertex_uniform_components);
      return true;
    case GL_MAX_DRAW_BUFFERS:
      *params = gl_capabilities_.max_draw_buffers;
      return true;
    case GL_MAX_ELEMENT_INDEX:
      *params = static_cast<GLint>(gl_capabilities_.max_element_index);
      return true;
    case GL_MAX_ELEMENTS_INDICES:
      *params = gl_capabilities_.max_elements_indices;
      return true;
    case GL_MAX_ELEMENTS_VERTICES:
      *params = gl_capabilities_.max_elements_vertices;
      return true;
    case GL_MAX_FRAGMENT_INPUT_COMPONENTS:
      *params = gl_capabilities_.max_fragment_input_components;
      return true;
    case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
      *params = gl_capabilities_.max_fragment_uniform_blocks;
      return true;
    case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
      *params = gl_capabilities_.max_fragment_uniform_components;
      return true;
    case GL_MAX_PROGRAM_TEXEL_OFFSET:
      *params = gl_capabilities_.max_program_texel_offset;
      return true;
    case GL_MAX_SAMPLES:
      *params = gl_capabilities_.max_samples;
      return true;
    case GL_MAX_SERVER_WAIT_TIMEOUT:
      *params = static_cast<GLint>(gl_capabilities_.max_server_wait_timeout);
      return true;
    case GL_MAX_TEXTURE_LOD_BIAS:
      *params = static_cast<GLint>(gl_capabilities_.max_texture_lod_bias);
      return true;
    case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
      *params = gl_capabilities_.max_transform_feedback_interleaved_components;
      return true;
    case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
      *params = gl_capabilities_.max_transform_feedback_separate_attribs;
      return true;
    case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
      *params = gl_capabilities_.max_transform_feedback_separate_components;
      return true;
    case GL_MAX_UNIFORM_BLOCK_SIZE:
      *params = static_cast<GLint>(gl_capabilities_.max_uniform_block_size);
      return true;
    case GL_MAX_UNIFORM_BUFFER_BINDINGS:
      *params = gl_capabilities_.max_uniform_buffer_bindings;
      return true;
    case GL_MAX_VARYING_COMPONENTS:
      *params = gl_capabilities_.max_varying_components;
      return true;
    case GL_MAX_VERTEX_OUTPUT_COMPONENTS:
      *params = gl_capabilities_.max_vertex_output_components;
      return true;
    case GL_MAX_VERTEX_UNIFORM_BLOCKS:
      *params = gl_capabilities_.max_vertex_uniform_blocks;
      return true;
    case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
      *params = gl_capabilities_.max_vertex_uniform_components;
      return true;
    case GL_MIN_PROGRAM_TEXEL_OFFSET:
      *params = gl_capabilities_.min_program_texel_offset;
      return true;
    case GL_MINOR_VERSION:
      *params = gl_capabilities_.minor_version;
      return true;
    case GL_NUM_EXTENSIONS:
      UpdateCachedExtensionsIfNeeded();
      *params = cached_extensions_.size();
      return true;
    case GL_NUM_PROGRAM_BINARY_FORMATS:
      *params = gl_capabilities_.num_program_binary_formats;
      return true;
    case GL_PACK_SKIP_PIXELS:
      *params = pack_skip_pixels_;
      return true;
    case GL_PACK_SKIP_ROWS:
      *params = pack_skip_rows_;
      return true;
    case GL_PIXEL_PACK_BUFFER_BINDING:
      *params = bound_pixel_pack_buffer_;
      return true;
    case GL_PIXEL_UNPACK_BUFFER_BINDING:
      *params = bound_pixel_unpack_buffer_;
      return true;
    case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
      *params = bound_transform_feedback_buffer_;
      return true;
    case GL_UNIFORM_BUFFER_BINDING:
      *params = bound_uniform_buffer_;
      return true;
    case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
      *params = gl_capabilities_.uniform_buffer_offset_alignment;
      return true;
    case GL_UNPACK_SKIP_IMAGES:
      *params = unpack_skip_images_;
      return true;
    case GL_UNPACK_SKIP_PIXELS:
      *params = unpack_skip_pixels_;
      return true;
    case GL_UNPACK_SKIP_ROWS:
      *params = unpack_skip_rows_;
      return true;

    // Non-cached ES3 parameters.
    case GL_DRAW_BUFFER0:
    case GL_DRAW_BUFFER1:
    case GL_DRAW_BUFFER2:
    case GL_DRAW_BUFFER3:
    case GL_DRAW_BUFFER4:
    case GL_DRAW_BUFFER5:
    case GL_DRAW_BUFFER6:
    case GL_DRAW_BUFFER7:
    case GL_DRAW_BUFFER8:
    case GL_DRAW_BUFFER9:
    case GL_DRAW_BUFFER10:
    case GL_DRAW_BUFFER11:
    case GL_DRAW_BUFFER12:
    case GL_DRAW_BUFFER13:
    case GL_DRAW_BUFFER14:
    case GL_DRAW_BUFFER15:
    case GL_DRAW_FRAMEBUFFER_BINDING:
    case GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
    case GL_PACK_ROW_LENGTH:
    case GL_PRIMITIVE_RESTART_FIXED_INDEX:
    case GL_PROGRAM_BINARY_FORMATS:
    case GL_RASTERIZER_DISCARD:
    case GL_READ_BUFFER:
    case GL_READ_FRAMEBUFFER_BINDING:
    case GL_SAMPLER_BINDING:
    case GL_TEXTURE_BINDING_2D_ARRAY:
    case GL_TEXTURE_BINDING_3D:
    case GL_TRANSFORM_FEEDBACK_BINDING:
    case GL_TRANSFORM_FEEDBACK_ACTIVE:
    case GL_TRANSFORM_FEEDBACK_PAUSED:
    case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
    case GL_TRANSFORM_FEEDBACK_BUFFER_START:
    case GL_UNIFORM_BUFFER_SIZE:
    case GL_UNIFORM_BUFFER_START:
    case GL_UNPACK_IMAGE_HEIGHT:
    case GL_UNPACK_ROW_LENGTH:
    case GL_VERTEX_ARRAY_BINDING:
      return false;
    default:
      break;
  }

  if (gl_capabilities_.minor_version < 1) {
    return false;
  }

  // ES31 parameters.
  switch (pname) {
    case GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS:
      *params = gl_capabilities_.max_atomic_counter_buffer_bindings;
      return true;
    case GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS:
      *params = gl_capabilities_.max_shader_storage_buffer_bindings;
      return true;
    case GL_ATOMIC_COUNTER_BUFFER_BINDING:
      *params = bound_atomic_counter_buffer_;
      return true;
    case GL_DISPATCH_INDIRECT_BUFFER_BINDING:
      *params = bound_dispatch_indirect_buffer_;
      return true;
    case GL_DRAW_INDIRECT_BUFFER_BINDING:
      *params = bound_draw_indirect_buffer_;
      return true;
    case GL_SHADER_STORAGE_BUFFER_BINDING:
      *params = bound_shader_storage_buffer_;
      return true;
    case GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT:
      *params = gl_capabilities_.shader_storage_buffer_offset_alignment;
      return true;

    // Non-cached ES31 parameters.
    case GL_ATOMIC_COUNTER_BUFFER_SIZE:
    case GL_ATOMIC_COUNTER_BUFFER_START:
    case GL_SHADER_STORAGE_BUFFER_SIZE:
    case GL_SHADER_STORAGE_BUFFER_START:
      return false;
    default:
      return false;
  }
}

bool GLES2Implementation::GetBooleanvHelper(GLenum pname, GLboolean* params) {
  // TODO(gman): Make this handle pnames that return more than 1 value.
  GLint value;
  if (!GetHelper(pname, &value)) {
    return false;
  }
  *params = static_cast<GLboolean>(value);
  return true;
}

bool GLES2Implementation::GetBooleani_vHelper(GLenum pname,
                                              GLuint index,
                                              GLboolean* data) {
  // TODO(zmo): Implement client side caching.
  return false;
}

bool GLES2Implementation::GetFloatvHelper(GLenum pname, GLfloat* params) {
  // TODO(gman): Make this handle pnames that return more than 1 value.
  switch (pname) {
    case GL_MAX_TEXTURE_LOD_BIAS:
      *params = gl_capabilities_.max_texture_lod_bias;
      return true;
    default:
      break;
  }
  GLint value;
  if (!GetHelper(pname, &value)) {
    return false;
  }
  *params = static_cast<GLfloat>(value);
  return true;
}

bool GLES2Implementation::GetInteger64vHelper(GLenum pname, GLint64* params) {
  switch (pname) {
    case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
      *params = gl_capabilities_.max_combined_fragment_uniform_components;
      return true;
    case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
      *params = gl_capabilities_.max_combined_vertex_uniform_components;
      return true;
    case GL_MAX_ELEMENT_INDEX:
      *params = gl_capabilities_.max_element_index;
      return true;
    case GL_MAX_SERVER_WAIT_TIMEOUT:
      *params = gl_capabilities_.max_server_wait_timeout;
      return true;
    case GL_MAX_UNIFORM_BLOCK_SIZE:
      *params = gl_capabilities_.max_uniform_block_size;
      return true;
    case GL_TIMESTAMP_EXT:
      // We convert all GPU timestamps to CPU time.
      *params = (base::TimeTicks::Now() - base::TimeTicks()).InMicroseconds() *
                base::Time::kNanosecondsPerMicrosecond;
      return true;
    default:
      break;
  }
  GLint value;
  if (!GetHelper(pname, &value)) {
    return false;
  }
  *params = static_cast<GLint64>(value);
  return true;
}

bool GLES2Implementation::GetIntegervHelper(GLenum pname, GLint* params) {
  return GetHelper(pname, params);
}

bool GLES2Implementation::GetIntegeri_vHelper(GLenum pname,
                                              GLuint index,
                                              GLint* data) {
  // TODO(zmo): Implement client side caching.
  return false;
}

bool GLES2Implementation::GetInteger64i_vHelper(GLenum pname,
                                                GLuint index,
                                                GLint64* data) {
  // TODO(zmo): Implement client side caching.
  return false;
}

bool GLES2Implementation::GetInternalformativHelper(GLenum target,
                                                    GLenum format,
                                                    GLenum pname,
                                                    GLsizei bufSize,
                                                    GLint* params) {
  // TODO(zmo): Implement the client side caching.
  return false;
}

bool GLES2Implementation::GetSyncivHelper(GLsync sync,
                                          GLenum pname,
                                          GLsizei bufsize,
                                          GLsizei* length,
                                          GLint* values) {
  GLint value = 0;
  switch (pname) {
    case GL_OBJECT_TYPE:
      value = GL_SYNC_FENCE;
      break;
    case GL_SYNC_CONDITION:
      value = GL_SYNC_GPU_COMMANDS_COMPLETE;
      break;
    case GL_SYNC_FLAGS:
      value = 0;
      break;
    default:
      return false;
  }
  if (bufsize > 0) {
    DCHECK(values);
    *values = value;
  }
  if (length) {
    *length = 1;
  }
  return true;
}

bool GLES2Implementation::GetQueryObjectValueHelper(const char* function_name,
                                                    GLuint id,
                                                    GLenum pname,
                                                    GLuint64* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] GetQueryObjectValueHelper(" << id
                     << ", " << GLES2Util::GetStringQueryObjectParameter(pname)
                     << ", " << static_cast<const void*>(params) << ")");

  QueryTracker::Query* query = query_tracker_->GetQuery(id);
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
      *params = query->CheckResultsAvailable(helper_, flush_if_pending);
      valid_value = true;
      break;
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

GLuint GLES2Implementation::GetMaxValueInBufferCHROMIUMHelper(GLuint buffer_id,
                                                              GLsizei count,
                                                              GLenum type,
                                                              GLuint offset) {
  typedef cmds::GetMaxValueInBufferCHROMIUM::Result Result;
  auto result = GetResultAs<Result>();
  if (!result) {
    return 0;
  }
  *result = 0;
  helper_->GetMaxValueInBufferCHROMIUM(buffer_id, count, type, offset,
                                       GetResultShmId(), result.offset());
  if (!WaitForCmd()) {
    return 0;
  }
  return *result;
}

GLuint GLES2Implementation::GetMaxValueInBufferCHROMIUM(GLuint buffer_id,
                                                        GLsizei count,
                                                        GLenum type,
                                                        GLuint offset) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetMaxValueInBufferCHROMIUM("
                     << buffer_id << ", " << count << ", "
                     << GLES2Util::GetStringGetMaxIndexType(type) << ", "
                     << offset << ")");
  GLuint result =
      GetMaxValueInBufferCHROMIUMHelper(buffer_id, count, type, offset);
  GPU_CLIENT_LOG("returned " << result);
  CheckGLError();
  return result;
}

void GLES2Implementation::RestoreElementAndArrayBuffers(bool restore) {
  if (restore) {
    RestoreArrayBuffer(restore);
    // Restore the element array binding.
    // We only need to restore it if it wasn't a client side array.
    if (vertex_array_object_manager_->bound_element_array_buffer() == 0) {
      helper_->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
  }
}

void GLES2Implementation::RestoreArrayBuffer(bool restore) {
  if (restore) {
    // Restore the user's current binding.
    helper_->BindBuffer(GL_ARRAY_BUFFER, bound_array_buffer_);
  }
}

void GLES2Implementation::DrawElements(GLenum mode,
                                       GLsizei count,
                                       GLenum type,
                                       const void* indices) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDrawElements("
                     << GLES2Util::GetStringDrawMode(mode) << ", " << count
                     << ", " << GLES2Util::GetStringIndexType(type) << ", "
                     << static_cast<const void*>(indices) << ")");
  DrawElementsImpl(mode, count, type, indices, "glDrawElements");
}

void GLES2Implementation::DrawRangeElements(GLenum mode,
                                            GLuint start,
                                            GLuint end,
                                            GLsizei count,
                                            GLenum type,
                                            const void* indices) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDrawRangeElements("
                     << GLES2Util::GetStringDrawMode(mode) << ", " << start
                     << ", " << end << ", " << count << ", "
                     << GLES2Util::GetStringIndexType(type) << ", "
                     << static_cast<const void*>(indices) << ")");
  if (end < start) {
    SetGLError(GL_INVALID_VALUE, "glDrawRangeElements", "end < start");
    return;
  }
  DrawElementsImpl(mode, count, type, indices, "glDrawRangeElements");
}

void GLES2Implementation::DrawElementsImpl(GLenum mode,
                                           GLsizei count,
                                           GLenum type,
                                           const void* indices,
                                           const char* func_name) {
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, func_name, "count < 0");
    return;
  }
  bool simulated = false;
  GLuint offset = ToGLuint(indices);
  if (count > 0) {
    if (vertex_array_object_manager_->bound_element_array_buffer() != 0 &&
        !ValidateOffset(func_name, reinterpret_cast<GLintptr>(indices))) {
      return;
    }
    if (!vertex_array_object_manager_->SetupSimulatedIndexAndClientSideBuffers(
            func_name, this, helper_, count, type, 0, indices, &offset,
            &simulated)) {
      return;
    }
  }
  helper_->DrawElements(mode, count, type, offset);
  RestoreElementAndArrayBuffers(simulated);
  CheckGLError();
}

void GLES2Implementation::DrawElementsIndirect(GLenum mode,
                                               GLenum type,
                                               const void* offset) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDrawElementsIndirect("
                     << GLES2Util::GetStringDrawMode(mode) << ", "
                     << GLES2Util::GetStringIndexType(type) << ", " << offset
                     << ")");
  if (!ValidateOffset("glDrawElementsIndirect",
                      reinterpret_cast<GLintptr>(offset))) {
    return;
  }
  // This is for WebGL 2.0 Compute which doesn't support client side arrays
  if (vertex_array_object_manager_->bound_element_array_buffer() == 0) {
    SetGLError(GL_INVALID_OPERATION, "glDrawElementsIndirect",
               "No element array buffer");
    return;
  }
  if (vertex_array_object_manager_->SupportsClientSideBuffers()) {
    SetGLError(GL_INVALID_OPERATION, "glDrawElementsIndirect",
               "Missing array buffer for vertex attribute");
    return;
  }
  helper_->DrawElementsIndirect(mode, type, ToGLuint(offset));
  CheckGLError();
}

void GLES2Implementation::Flush() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glFlush()");
  flush_id_ = GenerateNextFlushId();
  // Insert the cmd to call glFlush
  helper_->Flush();
  FlushHelper();
}

// InterfaceBase implementation.
void GLES2Implementation::GenSyncTokenCHROMIUM(GLbyte* sync_token) {
  ImplementationBase::GenSyncToken(sync_token);
}
void GLES2Implementation::GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) {
  ImplementationBase::GenUnverifiedSyncToken(sync_token);
}
void GLES2Implementation::VerifySyncTokensCHROMIUM(GLbyte** sync_tokens,
                                                   GLsizei count) {
  ImplementationBase::VerifySyncTokens(sync_tokens, count);
}
void GLES2Implementation::WaitSyncTokenCHROMIUM(const GLbyte* sync_token) {
  ImplementationBase::WaitSyncToken(sync_token);
}
void GLES2Implementation::ShallowFlushCHROMIUM() {
  IssueShallowFlush();
}

// ImplementationBase implementation.
void GLES2Implementation::IssueShallowFlush() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glShallowFlushCHROMIUM()");
  flush_id_ = GenerateNextFlushId();
  FlushHelper();
}

void GLES2Implementation::FlushHelper() {
  // Flush our command buffer
  // (tell the service to execute up to the flush cmd.)
  helper_->CommandBufferHelper::Flush();

  if (aggressively_free_resources_)
    FreeEverything();
}

void GLES2Implementation::OrderingBarrierCHROMIUM() {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glOrderingBarrierCHROMIUM");
  // Flush command buffer at the GPU channel level.  May be implemented as
  // Flush().
  helper_->CommandBufferHelper::OrderingBarrier();
}

void GLES2Implementation::Finish() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  flush_id_ = GenerateNextFlushId();
  FinishHelper();
}

void GLES2Implementation::ShallowFinishCHROMIUM() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  TRACE_EVENT0("gpu", "GLES2::ShallowFinishCHROMIUM");
  flush_id_ = GenerateNextFlushId();
  // Flush our command buffer (tell the service to execute up to the flush cmd
  // and don't return until it completes).
  helper_->CommandBufferHelper::Finish();

  if (aggressively_free_resources_)
    FreeEverything();
}

void GLES2Implementation::FinishHelper() {
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glFinish()");
  TRACE_EVENT0("gpu", "GLES2::Finish");
  // Insert the cmd to call glFinish
  helper_->Finish();
  // Finish our command buffer
  // (tell the service to execute up to the Finish cmd and wait for it to
  // execute.)
  helper_->CommandBufferHelper::Finish();

  if (aggressively_free_resources_)
    FreeEverything();
}

GLuint GLES2Implementation::GetLastFlushIdCHROMIUM() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetLastFlushIdCHROMIUM()");
  return flush_id_;
}

void GLES2Implementation::SwapBuffers(uint64_t swap_id, GLbitfield flags) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glSwapBuffers()");
  // TODO(piman): Strictly speaking we'd want to insert the token after the
  // swap, but the state update with the updated token might not have happened
  // by the time the SwapBuffer callback gets called, forcing us to synchronize
  // with the GPU process more than needed. So instead, make it happen before.
  // All it means is that we could be slightly looser on the kMaxSwapBuffers
  // semantics if the client doesn't use the callback mechanism, and by chance
  // the scheduler yields between the InsertToken and the SwapBuffers.
  swap_buffers_tokens_.push(helper_->InsertToken());
  helper_->SwapBuffers(swap_id, flags);
  helper_->CommandBufferHelper::Flush();
  // Wait if we added too many swap buffers. Add 1 to kMaxSwapBuffers to
  // compensate for TODO above.
  if (swap_buffers_tokens_.size() > kMaxSwapBuffers + 1) {
    helper_->WaitForToken(swap_buffers_tokens_.front());
    swap_buffers_tokens_.pop();
  }
}

void GLES2Implementation::BindAttribLocation(GLuint program,
                                             GLuint index,
                                             const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindAttribLocation(" << program
                     << ", " << index << ", " << name << ")");
  SetBucketAsString(kResultBucketId, name);
  helper_->BindAttribLocationBucket(program, index, kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
  CheckGLError();
}

void GLES2Implementation::BindFragDataLocationEXT(GLuint program,
                                                  GLuint colorName,
                                                  const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindFragDataLocationEXT("
                     << program << ", " << colorName << ", " << name << ")");
  SetBucketAsString(kResultBucketId, name);
  helper_->BindFragDataLocationEXTBucket(program, colorName, kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
  CheckGLError();
}

void GLES2Implementation::BindFragDataLocationIndexedEXT(GLuint program,
                                                         GLuint colorName,
                                                         GLuint index,
                                                         const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindFragDataLocationEXT("
                     << program << ", " << colorName << ", " << index << ", "
                     << name << ")");
  SetBucketAsString(kResultBucketId, name);
  helper_->BindFragDataLocationIndexedEXTBucket(program, colorName, index,
                                                kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
  CheckGLError();
}

void GLES2Implementation::BindUniformLocationCHROMIUM(GLuint program,
                                                      GLint location,
                                                      const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBindUniformLocationCHROMIUM("
                     << program << ", " << location << ", " << name << ")");
  SetBucketAsString(kResultBucketId, name);
  helper_->BindUniformLocationCHROMIUMBucket(program, location,
                                             kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
  CheckGLError();
}

void GLES2Implementation::GetVertexAttribPointerv(GLuint index,
                                                  GLenum pname,
                                                  void** ptr) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetVertexAttribPointer(" << index
                     << ", " << GLES2Util::GetStringVertexPointer(pname) << ", "
                     << static_cast<void*>(ptr) << ")");
  GPU_CLIENT_LOG_CODE_BLOCK(int32_t num_results = 1);
  if (!vertex_array_object_manager_->GetAttribPointer(index, pname, ptr)) {
    TRACE_EVENT0("gpu", "GLES2::GetVertexAttribPointerv");
    typedef cmds::GetVertexAttribPointerv::Result Result;
    auto result = GetResultAs<Result>();
    if (!result) {
      return;
    }
    result->SetNumResults(0);
    helper_->GetVertexAttribPointerv(index, pname, GetResultShmId(),
                                     result.offset());
    if (!WaitForCmd()) {
      return;
    }
    result->CopyResult(ptr);
    GPU_CLIENT_LOG_CODE_BLOCK(num_results = result->GetNumResults());
  }
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < num_results; ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << ptr[i]);
    }
  });
  CheckGLError();
}

bool GLES2Implementation::DeleteProgramHelper(GLuint program) {
  if (!GetIdHandler(SharedIdNamespaces::kProgramsAndShaders)
           ->FreeIds(this, 1, &program,
                     &GLES2Implementation::DeleteProgramStub)) {
    SetGLError(GL_INVALID_VALUE, "glDeleteProgram",
               "id not created by this context.");
    return false;
  }
  if (program == current_program_) {
    current_program_ = 0;
  }
  return true;
}

void GLES2Implementation::DeleteProgramStub(GLsizei n, const GLuint* programs) {
  DCHECK_EQ(1, n);
  share_group_->program_info_manager()->DeleteInfo(programs[0]);
  helper_->DeleteProgram(programs[0]);
}

bool GLES2Implementation::DeleteShaderHelper(GLuint shader) {
  if (!GetIdHandler(SharedIdNamespaces::kProgramsAndShaders)
           ->FreeIds(this, 1, &shader,
                     &GLES2Implementation::DeleteShaderStub)) {
    SetGLError(GL_INVALID_VALUE, "glDeleteShader",
               "id not created by this context.");
    return false;
  }
  return true;
}

void GLES2Implementation::DeleteShaderStub(GLsizei n, const GLuint* shaders) {
  DCHECK_EQ(1, n);
  share_group_->program_info_manager()->DeleteInfo(shaders[0]);
  helper_->DeleteShader(shaders[0]);
}

void GLES2Implementation::DeleteSyncHelper(GLsync sync) {
  GLuint sync_uint = ToGLuint(sync);
  if (!GetIdHandler(SharedIdNamespaces::kSyncs)
           ->FreeIds(this, 1, &sync_uint,
                     &GLES2Implementation::DeleteSyncStub)) {
    SetGLError(GL_INVALID_VALUE, "glDeleteSync",
               "id not created by this context.");
  }
}

void GLES2Implementation::DeleteSyncStub(GLsizei n, const GLuint* syncs) {
  DCHECK_EQ(1, n);
  helper_->DeleteSync(syncs[0]);
}

GLint GLES2Implementation::GetAttribLocationHelper(GLuint program,
                                                   const char* name) {
  typedef cmds::GetAttribLocation::Result Result;
  SetBucketAsCString(kResultBucketId, name);
  auto result = GetResultAs<Result>();
  if (!result) {
    return -1;
  }
  *result = -1;
  helper_->GetAttribLocation(program, kResultBucketId, GetResultShmId(),
                             result.offset());
  if (!WaitForCmd()) {
    return -1;
  }
  helper_->SetBucketSize(kResultBucketId, 0);
  return *result;
}

GLint GLES2Implementation::GetAttribLocation(GLuint program, const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetAttribLocation(" << program
                     << ", " << name << ")");
  TRACE_EVENT0("gpu", "GLES2::GetAttribLocation");
  GLint loc = share_group_->program_info_manager()->GetAttribLocation(
      this, program, name);
  GPU_CLIENT_LOG("returned " << loc);
  CheckGLError();
  return loc;
}

GLint GLES2Implementation::GetUniformLocationHelper(GLuint program,
                                                    const char* name) {
  typedef cmds::GetUniformLocation::Result Result;
  SetBucketAsCString(kResultBucketId, name);
  auto result = GetResultAs<Result>();
  if (!result) {
    return -1;
  }
  *result = -1;
  helper_->GetUniformLocation(program, kResultBucketId, GetResultShmId(),
                              result.offset());
  if (!WaitForCmd()) {
    return -1;
  }
  helper_->SetBucketSize(kResultBucketId, 0);
  return *result;
}

GLint GLES2Implementation::GetUniformLocation(GLuint program,
                                              const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetUniformLocation(" << program
                     << ", " << name << ")");
  TRACE_EVENT0("gpu", "GLES2::GetUniformLocation");
  GLint loc = share_group_->program_info_manager()->GetUniformLocation(
      this, program, name);
  GPU_CLIENT_LOG("returned " << loc);
  CheckGLError();
  return loc;
}

bool GLES2Implementation::GetUniformIndicesHelper(GLuint program,
                                                  GLsizei count,
                                                  const char* const* names,
                                                  GLuint* indices) {
  if (!PackStringsToBucket(count, names, nullptr, "glGetUniformIndices")) {
    return false;
  }
  typedef cmds::GetUniformIndices::Result Result;
  auto result = GetResultAs<Result>();
  if (!result) {
    return false;
  }
  result->SetNumResults(0);
  helper_->GetUniformIndices(program, kResultBucketId, GetResultShmId(),
                             result.offset());
  if (!WaitForCmd()) {
    return false;
  }
  if (result->GetNumResults() != count) {
    return false;
  }
  result->CopyResult(indices);
  return true;
}

void GLES2Implementation::GetUniformIndices(GLuint program,
                                            GLsizei count,
                                            const char* const* names,
                                            GLuint* indices) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetUniformIndices(" << program
                     << ", " << count << ", " << names << ", " << indices
                     << ")");
  TRACE_EVENT0("gpu", "GLES2::GetUniformIndices");
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetUniformIndices", "count < 0");
    return;
  }
  if (count == 0) {
    return;
  }
  bool success = share_group_->program_info_manager()->GetUniformIndices(
      this, program, count, names, indices);
  if (success) {
    GPU_CLIENT_LOG_CODE_BLOCK({
      for (GLsizei ii = 0; ii < count; ++ii) {
        GPU_CLIENT_LOG("  " << ii << ": " << indices[ii]);
      }
    });
  }
  CheckGLError();
}

bool GLES2Implementation::GetProgramivHelper(GLuint program,
                                             GLenum pname,
                                             GLint* params) {
  bool got_value = share_group_->program_info_manager()->GetProgramiv(
      this, program, pname, params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    if (got_value) {
      GPU_CLIENT_LOG("  0: " << *params);
    }
  });
  return got_value;
}

GLint GLES2Implementation::GetFragDataIndexEXTHelper(GLuint program,
                                                     const char* name) {
  typedef cmds::GetFragDataIndexEXT::Result Result;
  SetBucketAsCString(kResultBucketId, name);
  auto result = GetResultAs<Result>();
  if (!result) {
    return -1;
  }
  *result = -1;
  helper_->GetFragDataIndexEXT(program, kResultBucketId, GetResultShmId(),
                               result.offset());
  if (!WaitForCmd()) {
    return -1;
  }
  helper_->SetBucketSize(kResultBucketId, 0);
  return *result;
}

GLint GLES2Implementation::GetFragDataIndexEXT(GLuint program,
                                               const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetFragDataIndexEXT(" << program
                     << ", " << name << ")");
  TRACE_EVENT0("gpu", "GLES2::GetFragDataIndexEXT");
  GLint loc = share_group_->program_info_manager()->GetFragDataIndex(
      this, program, name);
  GPU_CLIENT_LOG("returned " << loc);
  CheckGLError();
  return loc;
}

GLint GLES2Implementation::GetFragDataLocationHelper(GLuint program,
                                                     const char* name) {
  typedef cmds::GetFragDataLocation::Result Result;
  SetBucketAsCString(kResultBucketId, name);
  auto result = GetResultAs<Result>();
  if (!result) {
    return -1;
  }
  *result = -1;
  helper_->GetFragDataLocation(program, kResultBucketId, GetResultShmId(),
                               result.offset());
  if (!WaitForCmd()) {
    return -1;
  }
  helper_->SetBucketSize(kResultBucketId, 0);
  return *result;
}

GLint GLES2Implementation::GetFragDataLocation(GLuint program,
                                               const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetFragDataLocation(" << program
                     << ", " << name << ")");
  TRACE_EVENT0("gpu", "GLES2::GetFragDataLocation");
  GLint loc = share_group_->program_info_manager()->GetFragDataLocation(
      this, program, name);
  GPU_CLIENT_LOG("returned " << loc);
  CheckGLError();
  return loc;
}

GLuint GLES2Implementation::GetUniformBlockIndexHelper(GLuint program,
                                                       const char* name) {
  typedef cmds::GetUniformBlockIndex::Result Result;
  SetBucketAsCString(kResultBucketId, name);
  auto result = GetResultAs<Result>();
  if (!result) {
    return GL_INVALID_INDEX;
  }
  *result = GL_INVALID_INDEX;
  helper_->GetUniformBlockIndex(program, kResultBucketId, GetResultShmId(),
                                result.offset());
  if (!WaitForCmd()) {
    return GL_INVALID_INDEX;
  }
  helper_->SetBucketSize(kResultBucketId, 0);
  return *result;
}

GLuint GLES2Implementation::GetUniformBlockIndex(GLuint program,
                                                 const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetUniformBlockIndex(" << program
                     << ", " << name << ")");
  TRACE_EVENT0("gpu", "GLES2::GetUniformBlockIndex");
  GLuint index = share_group_->program_info_manager()->GetUniformBlockIndex(
      this, program, name);
  GPU_CLIENT_LOG("returned " << index);
  CheckGLError();
  return index;
}

bool GLES2Implementation::GetProgramInterfaceivHelper(GLuint program,
                                                      GLenum program_interface,
                                                      GLenum pname,
                                                      GLint* params) {
  bool success = share_group_->program_info_manager()->GetProgramInterfaceiv(
      this, program, program_interface, pname, params);
  GPU_CLIENT_LOG_CODE_BLOCK({
    if (success) {
      GPU_CLIENT_LOG("  0: " << *params);
    }
  });
  return success;
}

GLuint GLES2Implementation::GetProgramResourceIndexHelper(
    GLuint program,
    GLenum program_interface,
    const char* name) {
  typedef cmds::GetProgramResourceIndex::Result Result;
  SetBucketAsCString(kResultBucketId, name);
  auto result = GetResultAs<Result>();
  if (!result) {
    return GL_INVALID_INDEX;
  }
  *result = GL_INVALID_INDEX;
  helper_->GetProgramResourceIndex(program, program_interface, kResultBucketId,
                                   GetResultShmId(), result.offset());
  if (!WaitForCmd()) {
    return GL_INVALID_INDEX;
  }
  helper_->SetBucketSize(kResultBucketId, 0);
  return *result;
}

GLuint GLES2Implementation::GetProgramResourceIndex(
    GLuint program,
    GLenum program_interface,
    const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetProgramResourceIndex("
                     << program << ", " << program_interface << ", " << name
                     << ")");
  TRACE_EVENT0("gpu", "GLES2::GetProgramResourceIndex");
  GLuint index = share_group_->program_info_manager()->GetProgramResourceIndex(
      this, program, program_interface, name);
  GPU_CLIENT_LOG("returned " << index);
  CheckGLError();
  return index;
}

bool GLES2Implementation::GetProgramResourceNameHelper(GLuint program,
                                                       GLenum program_interface,
                                                       GLuint index,
                                                       GLsizei bufsize,
                                                       GLsizei* length,
                                                       char* name) {
  DCHECK_LE(0, bufsize);
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  bool success = false;
  {
    // The Result pointer must be scoped to this block because it can be
    // invalidated below if getting result name causes the transfer buffer to be
    // reallocated.
    typedef cmds::GetProgramResourceName::Result Result;
    auto result = GetResultAs<Result>();
    if (!result) {
      return false;
    }
    // Set as failed so if the command fails we'll recover.
    *result = 0;
    helper_->GetProgramResourceName(program, program_interface, index,
                                    kResultBucketId, GetResultShmId(),
                                    result.offset());
    if (!WaitForCmd()) {
      return false;
    }
    success = !!*result;
  }
  if (success) {
    GetResultNameHelper(bufsize, length, name);
  }
  return success;
}

void GLES2Implementation::GetProgramResourceName(GLuint program,
                                                 GLenum program_interface,
                                                 GLuint index,
                                                 GLsizei bufsize,
                                                 GLsizei* length,
                                                 char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetProgramResourceName("
                     << program << ", " << program_interface << ", " << index
                     << ", " << bufsize << ", " << static_cast<void*>(length)
                     << ", " << static_cast<void*>(name) << ")");
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetProgramResourceName", "bufsize < 0");
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetProgramResourceName");
  bool success = share_group_->program_info_manager()->GetProgramResourceName(
      this, program, program_interface, index, bufsize, length, name);
  if (success && name) {
    GPU_CLIENT_LOG("  name: " << name);
  }
  CheckGLError();
}

bool GLES2Implementation::GetProgramResourceivHelper(GLuint program,
                                                     GLenum program_interface,
                                                     GLuint index,
                                                     GLsizei prop_count,
                                                     const GLenum* props,
                                                     GLsizei bufsize,
                                                     GLsizei* length,
                                                     GLint* params) {
  DCHECK_LE(0, prop_count);
  DCHECK_LE(0, bufsize);
  base::CheckedNumeric<uint32_t> bytes = prop_count;
  bytes *= sizeof(GLenum);
  if (!bytes.IsValid()) {
    SetGLError(GL_INVALID_VALUE, "glGetProgramResourceiv", "count overflow");
    return false;
  }
  SetBucketContents(kResultBucketId, props, bytes.ValueOrDefault(0));
  typedef cmds::GetProgramResourceiv::Result Result;
  auto result = GetResultAs<Result>();
  if (!result) {
    return false;
  }
  result->SetNumResults(0);
  helper_->GetProgramResourceiv(program, program_interface, index,
                                kResultBucketId, GetResultShmId(),
                                result.offset());
  if (!WaitForCmd()) {
    return false;
  }
  if (length) {
    *length = result->GetNumResults();
  }
  if (result->GetNumResults() > 0) {
    if (params) {
      result->CopyResult(params);
    }
    GPU_CLIENT_LOG_CODE_BLOCK({
      for (int32_t i = 0; i < result->GetNumResults(); ++i) {
        GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
      }
    });
    return true;
  }
  return false;
}

void GLES2Implementation::GetProgramResourceiv(GLuint program,
                                               GLenum program_interface,
                                               GLuint index,
                                               GLsizei prop_count,
                                               const GLenum* props,
                                               GLsizei bufsize,
                                               GLsizei* length,
                                               GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetProgramResourceiv(" << program
                     << ", " << program_interface << ", " << index << ", "
                     << prop_count << ", " << static_cast<const void*>(props)
                     << ", " << bufsize << ", " << static_cast<void*>(length)
                     << ", " << static_cast<void*>(params) << ")");
  if (prop_count < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetProgramResourceiv", "prop_count < 0");
    return;
  }
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetProgramResourceiv", "bufsize < 0");
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetProgramResourceiv");
  GLsizei param_count = 0;
  bool success = share_group_->program_info_manager()->GetProgramResourceiv(
      this, program, program_interface, index, prop_count, props, bufsize,
      &param_count, params);
  if (length) {
    *length = param_count;
  }
  if (success && params) {
    GPU_CLIENT_LOG_CODE_BLOCK({
      for (GLsizei ii = 0; ii < param_count; ++ii) {
        GPU_CLIENT_LOG("  " << ii << ": " << params[ii]);
      }
    });
  }
  CheckGLError();
}

GLint GLES2Implementation::GetProgramResourceLocationHelper(
    GLuint program,
    GLenum program_interface,
    const char* name) {
  typedef cmds::GetProgramResourceLocation::Result Result;
  SetBucketAsCString(kResultBucketId, name);
  auto result = GetResultAs<Result>();
  if (!result) {
    return -1;
  }
  *result = -1;
  helper_->GetProgramResourceLocation(program, program_interface,
                                      kResultBucketId, GetResultShmId(),
                                      result.offset());
  if (!WaitForCmd()) {
    return -1;
  }
  helper_->SetBucketSize(kResultBucketId, 0);
  return *result;
}

GLint GLES2Implementation::GetProgramResourceLocation(
    GLuint program,
    GLenum program_interface,
    const char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetProgramResourceLocation("
                     << program << ", " << program_interface << ", " << name
                     << ")");
  TRACE_EVENT0("gpu", "GLES2::GetProgramResourceLocation");
  GLint location =
      share_group_->program_info_manager()->GetProgramResourceLocation(
          this, program, program_interface, name);
  GPU_CLIENT_LOG("returned " << location);
  CheckGLError();
  return location;
}

void GLES2Implementation::LinkProgram(GLuint program) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glLinkProgram(" << program << ")");
  helper_->LinkProgram(program);
  share_group_->program_info_manager()->CreateInfo(program);
  CheckGLError();
}

void GLES2Implementation::ShaderBinary(GLsizei n,
                                       const GLuint* shaders,
                                       GLenum binaryformat,
                                       const void* binary,
                                       GLsizei length) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glShaderBinary(" << n << ", "
                     << static_cast<const void*>(shaders) << ", "
                     << GLES2Util::GetStringEnum(binaryformat) << ", "
                     << static_cast<const void*>(binary) << ", " << length
                     << ")");
  if (n < 0) {
    SetGLError(GL_INVALID_VALUE, "glShaderBinary", "n < 0.");
    return;
  }
  if (length < 0) {
    SetGLError(GL_INVALID_VALUE, "glShaderBinary", "length < 0.");
    return;
  }
  // TODO(gman): ShaderBinary should use buckets.
  unsigned int shader_id_size = n * sizeof(*shaders);
  ScopedTransferBufferArray<GLint> buffer(shader_id_size + length, helper_,
                                          transfer_buffer_);
  if (!buffer.valid() || buffer.num_elements() != shader_id_size + length) {
    SetGLError(GL_OUT_OF_MEMORY, "glShaderBinary", "out of memory.");
    return;
  }
  void* shader_ids = buffer.elements();
  void* shader_data = buffer.elements() + shader_id_size;
  memcpy(shader_ids, shaders, shader_id_size);
  memcpy(shader_data, binary, length);
  helper_->ShaderBinary(n, buffer.shm_id(), buffer.offset(), binaryformat,
                        buffer.shm_id(), buffer.offset() + shader_id_size,
                        length);
  CheckGLError();
}

void GLES2Implementation::PixelStorei(GLenum pname, GLint param) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glPixelStorei("
                     << GLES2Util::GetStringPixelStore(pname) << ", " << param
                     << ")");
  // We have to validate before caching these parameters because we use them
  // to compute image sizes on the client side.
  switch (pname) {
    case GL_PACK_ALIGNMENT:
    case GL_UNPACK_ALIGNMENT:
      if (param != 1 && param != 2 && param != 4 && param != 8) {
        SetGLError(GL_INVALID_VALUE, "glPixelStorei", "invalid param");
        return;
      }
      break;
    case GL_PACK_ROW_LENGTH:
    case GL_PACK_SKIP_PIXELS:
    case GL_PACK_SKIP_ROWS:
    case GL_UNPACK_IMAGE_HEIGHT:
    case GL_UNPACK_SKIP_IMAGES:
      if (gl_capabilities_.major_version < 3) {
        SetGLError(GL_INVALID_ENUM, "glPixelStorei", "invalid pname");
        return;
      }
      if (param < 0) {
        SetGLError(GL_INVALID_VALUE, "glPixelStorei", "invalid param");
        return;
      }
      break;
    case GL_UNPACK_ROW_LENGTH:
    case GL_UNPACK_SKIP_ROWS:
    case GL_UNPACK_SKIP_PIXELS:
      // These parameters are always enabled in ES2 by EXT_unpack_subimage.
      if (param < 0) {
        SetGLError(GL_INVALID_VALUE, "glPixelStorei", "invalid param");
        return;
      }
      break;
    default:
      SetGLError(GL_INVALID_ENUM, "glPixelStorei", "invalid pname");
      return;
  }
  // Do not send SKIP parameters to the service side.
  // Handle them on the client side.
  switch (pname) {
    case GL_PACK_ALIGNMENT:
      pack_alignment_ = param;
      break;
    case GL_PACK_ROW_LENGTH:
      pack_row_length_ = param;
      break;
    case GL_PACK_SKIP_PIXELS:
      pack_skip_pixels_ = param;
      return;
    case GL_PACK_SKIP_ROWS:
      pack_skip_rows_ = param;
      return;
    case GL_UNPACK_ALIGNMENT:
      unpack_alignment_ = param;
      break;
    case GL_UNPACK_ROW_LENGTH:
      unpack_row_length_ = param;
      if (gl_capabilities_.major_version < 3) {
        // In ES2 with EXT_unpack_subimage, it's handled on the client side
        // and there is no need to send it to the service side.
        return;
      }
      break;
    case GL_UNPACK_IMAGE_HEIGHT:
      unpack_image_height_ = param;
      break;
    case GL_UNPACK_SKIP_ROWS:
      unpack_skip_rows_ = param;
      return;
    case GL_UNPACK_SKIP_PIXELS:
      unpack_skip_pixels_ = param;
      return;
    case GL_UNPACK_SKIP_IMAGES:
      unpack_skip_images_ = param;
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
  helper_->PixelStorei(pname, param);
  CheckGLError();
}

void GLES2Implementation::VertexAttribIPointer(GLuint index,
                                               GLint size,
                                               GLenum type,
                                               GLsizei stride,
                                               const void* ptr) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttribIPointer(" << index
                     << ", " << size << ", "
                     << GLES2Util::GetStringVertexAttribIType(type) << ", "
                     << stride << ", " << ptr << ")");
  // Record the info on the client side.
  if (!vertex_array_object_manager_->SetAttribPointer(
          bound_array_buffer_, index, size, type, GL_FALSE, stride, ptr,
          GL_TRUE)) {
    SetGLError(GL_INVALID_OPERATION, "glVertexAttribIPointer",
               "client side arrays are not allowed in vertex array objects.");
    return;
  }
  if (!support_client_side_arrays_ || bound_array_buffer_ != 0) {
    // Only report NON client side buffers to the service.
    if (!ValidateOffset("glVertexAttribIPointer",
                        reinterpret_cast<GLintptr>(ptr))) {
      return;
    }
    helper_->VertexAttribIPointer(index, size, type, stride, ToGLuint(ptr));
  }
  CheckGLError();
}

void GLES2Implementation::VertexAttribPointer(GLuint index,
                                              GLint size,
                                              GLenum type,
                                              GLboolean normalized,
                                              GLsizei stride,
                                              const void* ptr) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttribPointer(" << index
                     << ", " << size << ", "
                     << GLES2Util::GetStringVertexAttribType(type) << ", "
                     << GLES2Util::GetStringBool(normalized) << ", " << stride
                     << ", " << ptr << ")");
  // Record the info on the client side.
  if (!vertex_array_object_manager_->SetAttribPointer(
          bound_array_buffer_, index, size, type, normalized, stride, ptr,
          GL_FALSE)) {
    SetGLError(GL_INVALID_OPERATION, "glVertexAttribPointer",
               "client side arrays are not allowed in vertex array objects.");
    return;
  }
  if (!support_client_side_arrays_ || bound_array_buffer_ != 0) {
    // Only report NON client side buffers to the service.
    if (!ValidateOffset("glVertexAttribPointer",
                        reinterpret_cast<GLintptr>(ptr))) {
      return;
    }
    helper_->VertexAttribPointer(index, size, type, normalized, stride,
                                 ToGLuint(ptr));
  }
  CheckGLError();
}

void GLES2Implementation::VertexAttribDivisorANGLE(GLuint index,
                                                   GLuint divisor) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glVertexAttribDivisorANGLE("
                     << index << ", " << divisor << ") ");
  // Record the info on the client side.
  vertex_array_object_manager_->SetAttribDivisor(index, divisor);
  helper_->VertexAttribDivisorANGLE(index, divisor);
  CheckGLError();
}

void GLES2Implementation::BufferDataHelper(GLenum target,
                                           GLsizeiptr size,
                                           const void* data,
                                           GLenum usage) {
  if (!ValidateSize("glBufferData", size))
    return;

#if defined(MEMORY_SANITIZER) && !BUILDFLAG(IS_NACL)
  // Do not upload uninitialized data. Even if it's not a bug, it can cause a
  // bogus MSan report during a readback later. This is because MSan doesn't
  // understand shared memory and would assume we were reading back the same
  // unintialized data.
  if (data)
    __msan_check_mem_is_initialized(data, size);
#endif

  GLuint buffer_id;
  if (GetBoundPixelTransferBuffer(target, "glBufferData", &buffer_id)) {
    if (!buffer_id) {
      return;
    }

    BufferTracker::Buffer* buffer = buffer_tracker_->GetBuffer(buffer_id);
    if (buffer)
      RemoveTransferBuffer(buffer);

    // Create new buffer.
    buffer = buffer_tracker_->CreateBuffer(buffer_id, size);
    DCHECK(buffer);
    if (buffer->address() && data)
      memcpy(buffer->address(), data, size);
    return;
  }

  if (IsReadbackUsage(usage)) {
    GLuint id = GetBoundBufferHelper(target);
    readback_buffer_shadow_tracker_->GetOrCreateBuffer(id, size);
  }

  RemoveMappedBufferRangeByTarget(target);

  // If there is no data just send BufferData
  if (size == 0 || !data) {
    helper_->BufferData(target, size, 0, 0, usage);
    return;
  }

  // See if we can send all at once.
  ScopedTransferBufferPtr buffer(size, helper_, transfer_buffer_);
  if (!buffer.valid()) {
    return;
  }

  if (buffer.size() >= static_cast<unsigned int>(size)) {
    memcpy(buffer.address(), data, size);
    helper_->BufferData(target, size, buffer.shm_id(), buffer.offset(), usage);
    return;
  }

  // Make the buffer with BufferData then send via BufferSubData
  helper_->BufferData(target, size, 0, 0, usage);
  BufferSubDataHelperImpl(target, 0, size, data, &buffer);
  CheckGLError();
}

void GLES2Implementation::BufferData(GLenum target,
                                     GLsizeiptr size,
                                     const void* data,
                                     GLenum usage) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBufferData("
                     << GLES2Util::GetStringBufferTarget(target) << ", " << size
                     << ", " << static_cast<const void*>(data) << ", "
                     << GLES2Util::GetStringBufferUsage(usage) << ")");
  BufferDataHelper(target, size, data, usage);
  CheckGLError();
}

void GLES2Implementation::BufferSubDataHelper(GLenum target,
                                              GLintptr offset,
                                              GLsizeiptr size,
                                              const void* data) {
  if (size == 0) {
    return;
  }

  if (!ValidateSize("glBufferSubData", size) ||
      !ValidateOffset("glBufferSubData", offset)) {
    return;
  }

  GLuint buffer_id;
  if (GetBoundPixelTransferBuffer(target, "glBufferSubData", &buffer_id)) {
    if (!buffer_id) {
      return;
    }
    BufferTracker::Buffer* buffer = buffer_tracker_->GetBuffer(buffer_id);
    if (!buffer) {
      SetGLError(GL_INVALID_VALUE, "glBufferSubData", "unknown buffer");
      return;
    }

    int32_t end = 0;
    int32_t buffer_size = buffer->size();
    if (!base::CheckAdd(offset, size).AssignIfValid(&end) ||
        end > buffer_size) {
      SetGLError(GL_INVALID_VALUE, "glBufferSubData", "out of range");
      return;
    }

    if (buffer->address() && data)
      memcpy(static_cast<uint8_t*>(buffer->address()) + offset, data, size);
    return;
  }

  ScopedTransferBufferPtr buffer(size, helper_, transfer_buffer_);
  BufferSubDataHelperImpl(target, offset, size, data, &buffer);
}

void GLES2Implementation::BufferSubDataHelperImpl(
    GLenum target,
    GLintptr offset,
    GLsizeiptr size,
    const void* data,
    ScopedTransferBufferPtr* buffer) {
  DCHECK(buffer);
  DCHECK_GT(size, 0);

  auto DoBufferSubData = [&](const std::array<uint32_t, 1>&,
                             uint32_t copy_offset, uint32_t) {
    helper_->BufferSubData(target, offset + copy_offset, buffer->size(),
                           buffer->shm_id(), buffer->offset());
    InvalidateReadbackBufferShadowDataCHROMIUM(GetBoundBufferHelper(target));
  };

  if (!TransferArraysAndExecute(size, buffer, DoBufferSubData,
                                static_cast<const int8_t*>(data))) {
    SetGLError(GL_OUT_OF_MEMORY, "glBufferSubData", "out of memory");
  }
}

void GLES2Implementation::BufferSubData(GLenum target,
                                        GLintptr offset,
                                        GLsizeiptr size,
                                        const void* data) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glBufferSubData("
                     << GLES2Util::GetStringBufferTarget(target) << ", "
                     << offset << ", " << size << ", "
                     << static_cast<const void*>(data) << ")");
  BufferSubDataHelper(target, offset, size, data);
  CheckGLError();
}

void GLES2Implementation::MultiDrawArraysWEBGLHelper(GLenum mode,
                                                     const GLint* firsts,
                                                     const GLsizei* counts,
                                                     GLsizei drawcount) {
  DCHECK_GT(drawcount, 0);

  uint32_t buffer_size = ComputeCombinedCopySize(drawcount, firsts, counts);
  ScopedTransferBufferPtr buffer(buffer_size, helper_, transfer_buffer_);

  helper_->MultiDrawBeginCHROMIUM(drawcount);
  auto DoMultiDraw = [&](const std::array<uint32_t, 2>& offsets, uint32_t,
                         uint32_t copy_count) {
    helper_->MultiDrawArraysCHROMIUM(
        mode, buffer.shm_id(), buffer.offset() + offsets[0], buffer.shm_id(),
        buffer.offset() + offsets[1], copy_count);
  };
  if (!TransferArraysAndExecute(drawcount, &buffer, DoMultiDraw, firsts,
                                counts)) {
    SetGLError(GL_OUT_OF_MEMORY, "glMultiDrawArraysWEBGL", "out of memory");
  }
  helper_->MultiDrawEndCHROMIUM();
}

void GLES2Implementation::MultiDrawArraysInstancedWEBGLHelper(
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* instance_counts,
    GLsizei drawcount) {
  DCHECK_GT(drawcount, 0);

  uint32_t buffer_size =
      ComputeCombinedCopySize(drawcount, firsts, counts, instance_counts);
  ScopedTransferBufferPtr buffer(buffer_size, helper_, transfer_buffer_);

  helper_->MultiDrawBeginCHROMIUM(drawcount);
  auto DoMultiDraw = [&](const std::array<uint32_t, 3>& offsets, uint32_t,
                         uint32_t copy_count) {
    helper_->MultiDrawArraysInstancedCHROMIUM(
        mode, buffer.shm_id(), buffer.offset() + offsets[0], buffer.shm_id(),
        buffer.offset() + offsets[1], buffer.shm_id(),
        buffer.offset() + offsets[2], copy_count);
  };
  if (!TransferArraysAndExecute(drawcount, &buffer, DoMultiDraw, firsts, counts,
                                instance_counts)) {
    SetGLError(GL_OUT_OF_MEMORY, "glMultiDrawArraysInstancedWEBGL",
               "out of memory");
  }
  helper_->MultiDrawEndCHROMIUM();
}

void GLES2Implementation::MultiDrawArraysInstancedBaseInstanceWEBGLHelper(
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* instance_counts,
    const GLuint* baseinstances,
    GLsizei drawcount) {
  DCHECK_GT(drawcount, 0);

  uint32_t buffer_size = ComputeCombinedCopySize(
      drawcount, firsts, counts, instance_counts, baseinstances);
  ScopedTransferBufferPtr buffer(buffer_size, helper_, transfer_buffer_);

  helper_->MultiDrawBeginCHROMIUM(drawcount);
  auto DoMultiDraw = [&](const std::array<uint32_t, 4>& offsets, uint32_t,
                         uint32_t copy_count) {
    helper_->MultiDrawArraysInstancedBaseInstanceCHROMIUM(
        mode, buffer.shm_id(), buffer.offset() + offsets[0], buffer.shm_id(),
        buffer.offset() + offsets[1], buffer.shm_id(),
        buffer.offset() + offsets[2], buffer.shm_id(),
        buffer.offset() + offsets[3], copy_count);
  };
  if (!TransferArraysAndExecute(drawcount, &buffer, DoMultiDraw, firsts, counts,
                                instance_counts, baseinstances)) {
    SetGLError(GL_OUT_OF_MEMORY, "glMultiDrawArraysInstancedBaseInstanceWEBGL",
               "out of memory");
  }
  helper_->MultiDrawEndCHROMIUM();
}

void GLES2Implementation::MultiDrawElementsWEBGLHelper(GLenum mode,
                                                       const GLsizei* counts,
                                                       GLenum type,
                                                       const GLsizei* offsets,
                                                       GLsizei drawcount) {
  DCHECK_GT(drawcount, 0);

  uint32_t buffer_size = ComputeCombinedCopySize(drawcount, counts, offsets);
  ScopedTransferBufferPtr buffer(buffer_size, helper_, transfer_buffer_);

  helper_->MultiDrawBeginCHROMIUM(drawcount);
  auto DoMultiDraw = [&](const std::array<uint32_t, 2>& offsets, uint32_t,
                         uint32_t copy_count) {
    helper_->MultiDrawElementsCHROMIUM(
        mode, buffer.shm_id(), buffer.offset() + offsets[0], type,
        buffer.shm_id(), buffer.offset() + offsets[1], copy_count);
  };
  if (!TransferArraysAndExecute(drawcount, &buffer, DoMultiDraw, counts,
                                offsets)) {
    SetGLError(GL_OUT_OF_MEMORY, "glMultiDrawElementsWEBGL", "out of memory");
  }
  helper_->MultiDrawEndCHROMIUM();
}

void GLES2Implementation::MultiDrawElementsInstancedWEBGLHelper(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLsizei* offsets,
    const GLsizei* instance_counts,
    GLsizei drawcount) {
  DCHECK_GT(drawcount, 0);

  uint32_t buffer_size =
      ComputeCombinedCopySize(drawcount, counts, offsets, instance_counts);
  ScopedTransferBufferPtr buffer(buffer_size, helper_, transfer_buffer_);

  helper_->MultiDrawBeginCHROMIUM(drawcount);
  auto DoMultiDraw = [&](const std::array<uint32_t, 3>& offsets, uint32_t,
                         uint32_t copy_count) {
    helper_->MultiDrawElementsInstancedCHROMIUM(
        mode, buffer.shm_id(), buffer.offset() + offsets[0], type,
        buffer.shm_id(), buffer.offset() + offsets[1], buffer.shm_id(),
        buffer.offset() + offsets[2], copy_count);
  };
  if (!TransferArraysAndExecute(drawcount, &buffer, DoMultiDraw, counts,
                                offsets, instance_counts)) {
    SetGLError(GL_OUT_OF_MEMORY, "glMultiDrawElementsInstancedWEBGL",
               "out of memory");
  }
  helper_->MultiDrawEndCHROMIUM();
}

void GLES2Implementation::
    MultiDrawElementsInstancedBaseVertexBaseInstanceWEBGLHelper(
        GLenum mode,
        const GLsizei* counts,
        GLenum type,
        const GLsizei* offsets,
        const GLsizei* instance_counts,
        const GLint* basevertices,
        const GLuint* baseinstances,
        GLsizei drawcount) {
  DCHECK_GT(drawcount, 0);

  uint32_t buffer_size = ComputeCombinedCopySize(
      drawcount, counts, offsets, instance_counts, basevertices, baseinstances);
  ScopedTransferBufferPtr buffer(buffer_size, helper_, transfer_buffer_);

  helper_->MultiDrawBeginCHROMIUM(drawcount);
  auto DoMultiDraw = [&](const std::array<uint32_t, 5>& offsets, uint32_t,
                         uint32_t copy_count) {
    helper_->MultiDrawElementsInstancedBaseVertexBaseInstanceCHROMIUM(
        mode, buffer.shm_id(), buffer.offset() + offsets[0], type,
        buffer.shm_id(), buffer.offset() + offsets[1], buffer.shm_id(),
        buffer.offset() + offsets[2], buffer.shm_id(),
        buffer.offset() + offsets[3], buffer.shm_id(),
        buffer.offset() + offsets[4], copy_count);
  };
  if (!TransferArraysAndExecute(drawcount, &buffer, DoMultiDraw, counts,
                                offsets, instance_counts, basevertices,
                                baseinstances)) {
    SetGLError(GL_OUT_OF_MEMORY,
               "glMultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL",
               "out of memory");
  }
  helper_->MultiDrawEndCHROMIUM();
}

void GLES2Implementation::MultiDrawArraysWEBGL(GLenum mode,
                                               const GLint* firsts,
                                               const GLsizei* counts,
                                               GLsizei drawcount) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glMultiDrawArraysWEBGL("
                     << GLES2Util::GetStringDrawMode(mode) << ", " << firsts
                     << ", " << counts << ", " << drawcount << ")");
  if (drawcount < 0) {
    SetGLError(GL_INVALID_VALUE, "glMultiDrawArraysWEBGL", "drawcount < 0");
    return;
  }
  if (drawcount == 0) {
    return;
  }
  // This is for an extension for WebGL which doesn't support client side arrays
  if (vertex_array_object_manager_->SupportsClientSideBuffers()) {
    SetGLError(GL_INVALID_OPERATION, "glMultiDrawArraysWEBGL",
               "Missing array buffer for vertex attribute");
    return;
  }
  MultiDrawArraysWEBGLHelper(mode, firsts, counts, drawcount);
  CheckGLError();
}

void GLES2Implementation::MultiDrawArraysInstancedWEBGL(
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* instance_counts,
    GLsizei drawcount) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glMultiDrawArraysInstancedWEBGL("
                     << GLES2Util::GetStringDrawMode(mode) << ", " << firsts
                     << ", " << counts << ", " << instance_counts << ", "
                     << drawcount << ")");
  if (drawcount < 0) {
    SetGLError(GL_INVALID_VALUE, "glMultiDrawArraysWEBGLInstanced",
               "drawcount < 0");
    return;
  }
  if (drawcount == 0) {
    return;
  }
  // This is for an extension for WebGL which doesn't support client side arrays
  if (vertex_array_object_manager_->SupportsClientSideBuffers()) {
    SetGLError(GL_INVALID_OPERATION, "glMultiDrawArraysWEBGLInstanced",
               "Missing array buffer for vertex attribute");
    return;
  }
  MultiDrawArraysInstancedWEBGLHelper(mode, firsts, counts, instance_counts,
                                      drawcount);
  CheckGLError();
}

void GLES2Implementation::MultiDrawArraysInstancedBaseInstanceWEBGL(
    GLenum mode,
    const GLint* firsts,
    const GLsizei* counts,
    const GLsizei* instance_counts,
    const GLuint* baseinstances,
    GLsizei drawcount) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glMultiDrawArraysInstancedBaseInstanceWEBGL("
                     << GLES2Util::GetStringDrawMode(mode) << ", " << firsts
                     << ", " << counts << ", " << instance_counts << ", "
                     << baseinstances << ", " << drawcount << ")");
  if (drawcount < 0) {
    SetGLError(GL_INVALID_VALUE, "glMultiDrawArraysInstancedBaseInstanceWEBGL",
               "drawcount < 0");
    return;
  }
  if (drawcount == 0) {
    return;
  }
  // This is for an extension for WebGL which doesn't support client side arrays
  if (vertex_array_object_manager_->SupportsClientSideBuffers()) {
    SetGLError(GL_INVALID_OPERATION,
               "glMultiDrawArraysInstancedBaseInstanceWEBGL",
               "Missing array buffer for vertex attribute");
    return;
  }
  MultiDrawArraysInstancedBaseInstanceWEBGLHelper(
      mode, firsts, counts, instance_counts, baseinstances, drawcount);
  CheckGLError();
}

void GLES2Implementation::MultiDrawElementsWEBGL(GLenum mode,
                                                 const GLsizei* counts,
                                                 GLenum type,
                                                 const GLsizei* offsets,
                                                 GLsizei drawcount) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glMultiDrawElementsWEBGL("
                     << GLES2Util::GetStringDrawMode(mode) << ", " << counts
                     << ", " << GLES2Util::GetStringIndexType(type) << ", "
                     << offsets << ", " << drawcount << ")");
  if (drawcount < 0) {
    SetGLError(GL_INVALID_VALUE, "glMultiDrawElementsWEBGL", "drawcount < 0");
    return;
  }
  if (drawcount == 0) {
    return;
  }
  // This is for an extension for WebGL which doesn't support client side arrays
  if (vertex_array_object_manager_->bound_element_array_buffer() == 0) {
    SetGLError(GL_INVALID_OPERATION, "glMultiDrawElementsWEBGL",
               "No element array buffer");
    return;
  }
  if (vertex_array_object_manager_->SupportsClientSideBuffers()) {
    SetGLError(GL_INVALID_OPERATION, "glMultiDrawElementsWEBGL",
               "Missing array buffer for vertex attribute");
    return;
  }
  MultiDrawElementsWEBGLHelper(mode, counts, type, offsets, drawcount);
  CheckGLError();
}

void GLES2Implementation::MultiDrawElementsInstancedWEBGL(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLsizei* offsets,
    const GLsizei* instance_counts,
    GLsizei drawcount) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glMultiDrawElementsInstancedWEBGL("
                     << GLES2Util::GetStringDrawMode(mode) << ", " << counts
                     << ", " << GLES2Util::GetStringIndexType(type) << ", "
                     << offsets << ", " << instance_counts << ", " << drawcount
                     << ")");
  if (drawcount < 0) {
    SetGLError(GL_INVALID_VALUE, "glMultiDrawElementsInstancedWEBGL",
               "drawcount < 0");
    return;
  }
  if (drawcount == 0) {
    return;
  }
  // This is for an extension for WebGL which doesn't support client side arrays
  if (vertex_array_object_manager_->bound_element_array_buffer() == 0) {
    SetGLError(GL_INVALID_OPERATION, "glMultiDrawElementsInstancedWEBGL",
               "No element array buffer");
    return;
  }
  if (vertex_array_object_manager_->SupportsClientSideBuffers()) {
    SetGLError(GL_INVALID_OPERATION, "glMultiDrawElementsInstancedWEBGL",
               "Missing array buffer for vertex attribute");
    return;
  }
  MultiDrawElementsInstancedWEBGLHelper(mode, counts, type, offsets,
                                        instance_counts, drawcount);
  CheckGLError();
}

void GLES2Implementation::MultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL(
    GLenum mode,
    const GLsizei* counts,
    GLenum type,
    const GLsizei* offsets,
    const GLsizei* instance_counts,
    const GLint* basevertices,
    const GLuint* baseinstances,
    GLsizei drawcount) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix()
          << "] glMultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL("
          << GLES2Util::GetStringDrawMode(mode) << ", " << counts << ", "
          << GLES2Util::GetStringIndexType(type) << ", " << offsets << ", "
          << instance_counts << ", " << basevertices << ", " << baseinstances
          << drawcount << ")");
  if (drawcount < 0) {
    SetGLError(GL_INVALID_VALUE, "glMultiDrawElementsInstancedWEBGL",
               "drawcount < 0");
    return;
  }
  if (drawcount == 0) {
    return;
  }
  // This is for an extension for WebGL which doesn't support client side arrays
  if (vertex_array_object_manager_->bound_element_array_buffer() == 0) {
    SetGLError(GL_INVALID_OPERATION,
               "glMultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL",
               "No element array buffer");
    return;
  }
  if (vertex_array_object_manager_->SupportsClientSideBuffers()) {
    SetGLError(GL_INVALID_OPERATION,
               "glMultiDrawElementsInstancedBaseVertexBaseInstanceWEBGL",
               "Missing array buffer for vertex attribute");
    return;
  }
  MultiDrawElementsInstancedBaseVertexBaseInstanceWEBGLHelper(
      mode, counts, type, offsets, instance_counts, basevertices, baseinstances,
      drawcount);
  CheckGLError();
}

void GLES2Implementation::RemoveTransferBuffer(BufferTracker::Buffer* buffer) {
  int32_t token = buffer->last_usage_token();

  if (token) {
    if (helper_->HasTokenPassed(token))
      buffer_tracker_->Free(buffer);
    else
      buffer_tracker_->FreePendingToken(buffer, token);
  } else {
    buffer_tracker_->Free(buffer);
  }

  buffer_tracker_->RemoveBuffer(buffer->id());
}

bool GLES2Implementation::GetBoundPixelTransferBuffer(GLenum target,
                                                      const char* function_name,
                                                      GLuint* buffer_id) {
  *buffer_id = 0;

  switch (target) {
    case GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM:
      *buffer_id = bound_pixel_pack_transfer_buffer_id_;
      break;
    case GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM:
      *buffer_id = bound_pixel_unpack_transfer_buffer_id_;
      break;
    default:
      // Unknown target
      return false;
  }
  if (!*buffer_id) {
    SetGLError(GL_INVALID_OPERATION, function_name, "no buffer bound");
  }
  return true;
}

BufferTracker::Buffer* GLES2Implementation::GetBoundPixelTransferBufferIfValid(
    GLuint buffer_id,
    const char* function_name,
    GLuint offset,
    GLsizei size) {
  DCHECK(buffer_id);
  BufferTracker::Buffer* buffer = buffer_tracker_->GetBuffer(buffer_id);
  if (!buffer) {
    SetGLError(GL_INVALID_OPERATION, function_name, "invalid buffer");
    return nullptr;
  }
  if (buffer->mapped()) {
    SetGLError(GL_INVALID_OPERATION, function_name, "buffer mapped");
    return nullptr;
  }
  base::CheckedNumeric<uint32_t> buffer_offset = buffer->shm_offset();
  buffer_offset += offset;
  if (!buffer_offset.IsValid()) {
    SetGLError(GL_INVALID_VALUE, function_name, "offset to large");
    return nullptr;
  }
  base::CheckedNumeric<uint32_t> required_size = offset;
  required_size += size;
  if (!required_size.IsValid() ||
      buffer->size() < required_size.ValueOrDefault(0)) {
    SetGLError(GL_INVALID_VALUE, function_name, "unpack size to large");
    return nullptr;
  }
  return buffer;
}

void GLES2Implementation::InvalidateReadbackBufferShadowDataCHROMIUM(
    GLuint buffer_id) {
  readback_buffer_shadow_tracker_->OnBufferWrite(buffer_id);
}

void GLES2Implementation::CompressedTexImage2D(GLenum target,
                                               GLint level,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLint border,
                                               GLsizei image_size,
                                               const void* data) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glCompressedTexImage2D("
          << GLES2Util::GetStringTextureTarget(target) << ", " << level << ", "
          << GLES2Util::GetStringCompressedTextureFormat(internalformat) << ", "
          << width << ", " << height << ", " << border << ", " << image_size
          << ", " << static_cast<const void*>(data) << ")");
  if (width < 0 || height < 0 || level < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexImage2D", "dimension < 0");
    return;
  }
  if (border != 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexImage2D", "border != 0");
    return;
  }
  // If there's a pixel unpack buffer bound use it when issuing
  // CompressedTexImage2D.
  if (bound_pixel_unpack_transfer_buffer_id_) {
    GLuint offset = ToGLuint(data);
    BufferTracker::Buffer* buffer = GetBoundPixelTransferBufferIfValid(
        bound_pixel_unpack_transfer_buffer_id_, "glCompressedTexImage2D",
        offset, image_size);
    if (buffer && buffer->shm_id() != -1) {
      helper_->CompressedTexImage2D(target, level, internalformat, width,
                                    height, image_size, buffer->shm_id(),
                                    buffer->shm_offset() + offset);
      buffer->set_last_usage_token(helper_->InsertToken());
    }
    return;
  }
  if (bound_pixel_unpack_buffer_) {
    helper_->CompressedTexImage2D(target, level, internalformat, width, height,
                                  image_size, 0, ToGLuint(data));
  } else if (data) {
    SetBucketContents(kResultBucketId, data, image_size);
    helper_->CompressedTexImage2DBucket(target, level, internalformat, width,
                                        height, kResultBucketId);
    // Free the bucket. This is not required but it does free up the memory.
    // and we don't have to wait for the result so from the client's perspective
    // it's cheap.
    helper_->SetBucketSize(kResultBucketId, 0);
  } else {
    helper_->CompressedTexImage2D(target, level, internalformat, width, height,
                                  image_size, 0, 0);
  }
  CheckGLError();
}

void GLES2Implementation::CompressedTexSubImage2D(GLenum target,
                                                  GLint level,
                                                  GLint xoffset,
                                                  GLint yoffset,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLenum format,
                                                  GLsizei image_size,
                                                  const void* data) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glCompressedTexSubImage2D("
          << GLES2Util::GetStringTextureTarget(target) << ", " << level << ", "
          << xoffset << ", " << yoffset << ", " << width << ", " << height
          << ", " << GLES2Util::GetStringCompressedTextureFormat(format) << ", "
          << image_size << ", " << static_cast<const void*>(data) << ")");
  if (width < 0 || height < 0 || level < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage2D", "dimension < 0");
    return;
  }
  // If there's a pixel unpack buffer bound use it when issuing
  // CompressedTexSubImage2D.
  if (bound_pixel_unpack_transfer_buffer_id_) {
    GLuint offset = ToGLuint(data);
    BufferTracker::Buffer* buffer = GetBoundPixelTransferBufferIfValid(
        bound_pixel_unpack_transfer_buffer_id_, "glCompressedTexSubImage2D",
        offset, image_size);
    if (buffer && buffer->shm_id() != -1) {
      helper_->CompressedTexSubImage2D(
          target, level, xoffset, yoffset, width, height, format, image_size,
          buffer->shm_id(), buffer->shm_offset() + offset);
      buffer->set_last_usage_token(helper_->InsertToken());
      CheckGLError();
    }
    return;
  }
  if (bound_pixel_unpack_buffer_) {
    helper_->CompressedTexSubImage2D(target, level, xoffset, yoffset, width,
                                     height, format, image_size, 0,
                                     ToGLuint(data));
  } else if (data) {
    SetBucketContents(kResultBucketId, data, image_size);
    helper_->CompressedTexSubImage2DBucket(target, level, xoffset, yoffset,
                                           width, height, format,
                                           kResultBucketId);
    // Free the bucket. This is not required but it does free up the memory.
    // and we don't have to wait for the result so from the client's perspective
    // it's cheap.
    helper_->SetBucketSize(kResultBucketId, 0);
  } else {
    helper_->CompressedTexSubImage2D(target, level, xoffset, yoffset, width,
                                     height, format, image_size, 0, 0);
  }
  CheckGLError();
}

void GLES2Implementation::CompressedTexImage3D(GLenum target,
                                               GLint level,
                                               GLenum internalformat,
                                               GLsizei width,
                                               GLsizei height,
                                               GLsizei depth,
                                               GLint border,
                                               GLsizei image_size,
                                               const void* data) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glCompressedTexImage3D("
          << GLES2Util::GetStringTexture3DTarget(target) << ", " << level
          << ", " << GLES2Util::GetStringCompressedTextureFormat(internalformat)
          << ", " << width << ", " << height << ", " << depth << ", " << border
          << ", " << image_size << ", " << static_cast<const void*>(data)
          << ")");
  if (width < 0 || height < 0 || depth < 0 || level < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexImage3D", "dimension < 0");
    return;
  }
  if (border != 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexImage3D", "border != 0");
    return;
  }
  // If there's a pixel unpack buffer bound use it when issuing
  // CompressedTexImage3D.
  if (bound_pixel_unpack_transfer_buffer_id_) {
    GLuint offset = ToGLuint(data);
    BufferTracker::Buffer* buffer = GetBoundPixelTransferBufferIfValid(
        bound_pixel_unpack_transfer_buffer_id_, "glCompressedTexImage3D",
        offset, image_size);
    if (buffer && buffer->shm_id() != -1) {
      helper_->CompressedTexImage3D(target, level, internalformat, width,
                                    height, depth, image_size, buffer->shm_id(),
                                    buffer->shm_offset() + offset);
      buffer->set_last_usage_token(helper_->InsertToken());
    }
    return;
  }
  if (bound_pixel_unpack_buffer_) {
    helper_->CompressedTexImage3D(target, level, internalformat, width, height,
                                  depth, image_size, 0, ToGLuint(data));
  } else if (data) {
    SetBucketContents(kResultBucketId, data, image_size);
    helper_->CompressedTexImage3DBucket(target, level, internalformat, width,
                                        height, depth, kResultBucketId);
    // Free the bucket. This is not required but it does free up the memory.
    // and we don't have to wait for the result so from the client's perspective
    // it's cheap.
    helper_->SetBucketSize(kResultBucketId, 0);
  } else {
    helper_->CompressedTexImage3D(target, level, internalformat, width, height,
                                  depth, image_size, 0, 0);
  }
  CheckGLError();
}

void GLES2Implementation::CompressedTexSubImage3D(GLenum target,
                                                  GLint level,
                                                  GLint xoffset,
                                                  GLint yoffset,
                                                  GLint zoffset,
                                                  GLsizei width,
                                                  GLsizei height,
                                                  GLsizei depth,
                                                  GLenum format,
                                                  GLsizei image_size,
                                                  const void* data) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glCompressedTexSubImage3D("
          << GLES2Util::GetStringTextureTarget(target) << ", " << level << ", "
          << xoffset << ", " << yoffset << ", " << zoffset << ", " << width
          << ", " << height << ", " << depth << ", "
          << GLES2Util::GetStringCompressedTextureFormat(format) << ", "
          << image_size << ", " << static_cast<const void*>(data) << ")");
  if (width < 0 || height < 0 || depth < 0 || level < 0) {
    SetGLError(GL_INVALID_VALUE, "glCompressedTexSubImage3D", "dimension < 0");
    return;
  }
  // If there's a pixel unpack buffer bound use it when issuing
  // CompressedTexSubImage3D.
  if (bound_pixel_unpack_transfer_buffer_id_) {
    GLuint offset = ToGLuint(data);
    BufferTracker::Buffer* buffer = GetBoundPixelTransferBufferIfValid(
        bound_pixel_unpack_transfer_buffer_id_, "glCompressedTexSubImage3D",
        offset, image_size);
    if (buffer && buffer->shm_id() != -1) {
      helper_->CompressedTexSubImage3D(
          target, level, xoffset, yoffset, zoffset, width, height, depth,
          format, image_size, buffer->shm_id(), buffer->shm_offset() + offset);
      buffer->set_last_usage_token(helper_->InsertToken());
      CheckGLError();
    }
    return;
  }
  if (bound_pixel_unpack_buffer_) {
    helper_->CompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                     width, height, depth, format, image_size,
                                     0, ToGLuint(data));
  } else if (data) {
    SetBucketContents(kResultBucketId, data, image_size);
    helper_->CompressedTexSubImage3DBucket(target, level, xoffset, yoffset,
                                           zoffset, width, height, depth,
                                           format, kResultBucketId);
    // Free the bucket. This is not required but it does free up the memory.
    // and we don't have to wait for the result so from the client's perspective
    // it's cheap.
    helper_->SetBucketSize(kResultBucketId, 0);
  } else {
    helper_->CompressedTexSubImage3D(target, level, xoffset, yoffset, zoffset,
                                     width, height, depth, format, image_size,
                                     0, 0);
  }
  CheckGLError();
}

PixelStoreParams GLES2Implementation::GetUnpackParameters(Dimension dimension) {
  PixelStoreParams params;
  params.alignment = unpack_alignment_;
  params.row_length = unpack_row_length_;
  params.skip_pixels = unpack_skip_pixels_;
  params.skip_rows = unpack_skip_rows_;
  if (dimension == k3D) {
    params.image_height = unpack_image_height_;
    params.skip_images = unpack_skip_images_;
  }
  return params;
}

void GLES2Implementation::TexImage2D(GLenum target,
                                     GLint level,
                                     GLint internalformat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLint border,
                                     GLenum format,
                                     GLenum type,
                                     const void* pixels) {
  const char* func_name = "glTexImage2D";
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glTexImage2D("
          << GLES2Util::GetStringTextureTarget(target) << ", " << level << ", "
          << GLES2Util::GetStringTextureInternalFormat(internalformat) << ", "
          << width << ", " << height << ", " << border << ", "
          << GLES2Util::GetStringTextureFormat(format) << ", "
          << GLES2Util::GetStringPixelType(type) << ", "
          << static_cast<const void*>(pixels) << ")");
  if (level < 0 || height < 0 || width < 0) {
    SetGLError(GL_INVALID_VALUE, func_name, "dimension < 0");
    return;
  }
  if (border != 0) {
    SetGLError(GL_INVALID_VALUE, func_name, "border != 0");
    return;
  }
  if ((bound_pixel_unpack_buffer_ || pixels) &&
      (unpack_skip_pixels_ + width >
       (unpack_row_length_ ? unpack_row_length_ : width))) {
    // This is WebGL 2 specific constraints, but we do it for all ES3 contexts.
    SetGLError(GL_INVALID_OPERATION, func_name,
               "invalid unpack params combination");
    return;
  }

  uint32_t size;
  uint32_t unpadded_row_size;
  uint32_t padded_row_size;
  uint32_t skip_size;
  PixelStoreParams params = GetUnpackParameters(k2D);

  if (!GLES2Util::ComputeImageDataSizesES3(
          width, height, 1, format, type, params, &size, &unpadded_row_size,
          &padded_row_size, &skip_size, nullptr)) {
    SetGLError(GL_INVALID_VALUE, func_name, "image size too large");
    return;
  }

  if (bound_pixel_unpack_buffer_) {
    base::CheckedNumeric<uint32_t> offset = ToGLuint(pixels);
    offset += skip_size;
    if (!offset.IsValid()) {
      SetGLError(GL_INVALID_VALUE, func_name, "skip size too large");
      return;
    }
    helper_->TexImage2D(target, level, internalformat, width, height, format,
                        type, 0, offset.ValueOrDefault(0));
    CheckGLError();
    return;
  }

  // If there's a pixel unpack buffer bound use it when issuing TexImage2D.
  if (bound_pixel_unpack_transfer_buffer_id_) {
    if (unpack_row_length_ > 0 || unpack_image_height_ > 0 ||
        unpack_skip_pixels_ > 0 || unpack_skip_rows_ > 0 ||
        unpack_skip_images_ > 0) {
      SetGLError(GL_INVALID_OPERATION, func_name,
                 "No ES3 pack parameters with pixel unpack transfer buffer.");
      return;
    }
    DCHECK_EQ(0u, skip_size);
    GLuint offset = ToGLuint(pixels);
    BufferTracker::Buffer* buffer = GetBoundPixelTransferBufferIfValid(
        bound_pixel_unpack_transfer_buffer_id_, func_name, offset, size);
    if (buffer && buffer->shm_id() != -1) {
      helper_->TexImage2D(target, level, internalformat, width, height, format,
                          type, buffer->shm_id(),
                          buffer->shm_offset() + offset);
      buffer->set_last_usage_token(helper_->InsertToken());
      CheckGLError();
    }
    return;
  }

  // If there's no data just issue TexImage2D
  if (!pixels || width == 0 || height == 0) {
    helper_->TexImage2D(target, level, internalformat, width, height, format,
                        type, 0, 0);
    CheckGLError();
    return;
  }

  // Compute the advance bytes per row on the service side.
  // Note |size| is recomputed here if needed.
  uint32_t service_padded_row_size;
  if (unpack_row_length_ > 0 && unpack_row_length_ != width) {
    // All parameters have been applied to the data that are sent to the
    // service side except UNPACK_ALIGNMENT.
    PixelStoreParams service_params;
    service_params.alignment = unpack_alignment_;
    if (!GLES2Util::ComputeImageDataSizesES3(
            width, height, 1, format, type, service_params, &size, nullptr,
            &service_padded_row_size, nullptr, nullptr)) {
      SetGLError(GL_INVALID_VALUE, func_name, "image size too large");
      return;
    }
  } else {
    service_padded_row_size = padded_row_size;
  }

  // advance pixels pointer past the skip rows and skip pixels
  pixels = reinterpret_cast<const int8_t*>(pixels) + skip_size;

  // Check if we can send it all at once.
  int32_t shm_id = 0;
  uint32_t shm_offset = 0;
  void* buffer_pointer = nullptr;

  ScopedTransferBufferPtr transfer_alloc(size, helper_, transfer_buffer_);
  ScopedMappedMemoryPtr mapped_alloc(0, helper_, mapped_memory_.get());

  if (transfer_alloc.valid() && transfer_alloc.size() >= size) {
    shm_id = transfer_alloc.shm_id();
    shm_offset = transfer_alloc.offset();
    buffer_pointer = transfer_alloc.address();
  } else if (size < max_extra_transfer_buffer_size_) {
    mapped_alloc.Reset(size);
    if (mapped_alloc.valid()) {
      transfer_alloc.Discard();

      mapped_alloc.SetFlushAfterRelease(true);
      shm_id = mapped_alloc.shm_id();
      shm_offset = mapped_alloc.offset();
      buffer_pointer = mapped_alloc.address();
    }
  }

  if (buffer_pointer) {
    CopyRectToBuffer(pixels, height, unpadded_row_size, padded_row_size,
                     buffer_pointer, service_padded_row_size);
    helper_->TexImage2D(target, level, internalformat, width, height, format,
                        type, shm_id, shm_offset);
    CheckGLError();
    return;
  }

  // No, so send it using TexSubImage2D.
  helper_->TexImage2D(target, level, internalformat, width, height, format,
                      type, 0, 0);
  TexSubImage2DImpl(target, level, 0, 0, width, height, format, type,
                    unpadded_row_size, pixels, padded_row_size, GL_TRUE,
                    &transfer_alloc, service_padded_row_size);
  CheckGLError();
}

void GLES2Implementation::TexImage3D(GLenum target,
                                     GLint level,
                                     GLint internalformat,
                                     GLsizei width,
                                     GLsizei height,
                                     GLsizei depth,
                                     GLint border,
                                     GLenum format,
                                     GLenum type,
                                     const void* pixels) {
  const char* func_name = "glTexImage3D";
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glTexImage3D("
          << GLES2Util::GetStringTextureTarget(target) << ", " << level << ", "
          << GLES2Util::GetStringTextureInternalFormat(internalformat) << ", "
          << width << ", " << height << ", " << depth << ", " << border << ", "
          << GLES2Util::GetStringTextureFormat(format) << ", "
          << GLES2Util::GetStringPixelType(type) << ", "
          << static_cast<const void*>(pixels) << ")");
  if (level < 0 || height < 0 || width < 0 || depth < 0) {
    SetGLError(GL_INVALID_VALUE, func_name, "dimension < 0");
    return;
  }
  if (border != 0) {
    SetGLError(GL_INVALID_VALUE, func_name, "border != 0");
    return;
  }
  if ((bound_pixel_unpack_buffer_ || pixels) &&
      ((unpack_skip_pixels_ + width >
        (unpack_row_length_ ? unpack_row_length_ : width)) ||
       (unpack_skip_rows_ + height >
        (unpack_image_height_ ? unpack_image_height_ : height)))) {
    // This is WebGL 2 specific constraints, but we do it for all ES3 contexts.
    SetGLError(GL_INVALID_OPERATION, func_name,
               "invalid unpack params combination");
    return;
  }

  uint32_t size;
  uint32_t unpadded_row_size;
  uint32_t padded_row_size;
  uint32_t skip_size;
  PixelStoreParams params = GetUnpackParameters(k3D);
  if (!GLES2Util::ComputeImageDataSizesES3(
          width, height, depth, format, type, params, &size, &unpadded_row_size,
          &padded_row_size, &skip_size, nullptr)) {
    SetGLError(GL_INVALID_VALUE, func_name, "image size too large");
    return;
  }

  if (bound_pixel_unpack_buffer_) {
    base::CheckedNumeric<uint32_t> offset = ToGLuint(pixels);
    offset += skip_size;
    if (!offset.IsValid()) {
      SetGLError(GL_INVALID_VALUE, func_name, "skip size too large");
      return;
    }
    helper_->TexImage3D(target, level, internalformat, width, height, depth,
                        format, type, 0, offset.ValueOrDefault(0));
    CheckGLError();
    return;
  }

  // If there's a pixel unpack buffer bound use it when issuing TexImage3D.
  if (bound_pixel_unpack_transfer_buffer_id_) {
    if (unpack_row_length_ > 0 || unpack_image_height_ > 0 ||
        unpack_skip_pixels_ > 0 || unpack_skip_rows_ > 0 ||
        unpack_skip_images_ > 0) {
      SetGLError(GL_INVALID_OPERATION, func_name,
                 "No ES3 pack parameters with pixel unpack transfer buffer.");
      return;
    }
    DCHECK_EQ(0u, skip_size);
    GLuint offset = ToGLuint(pixels);
    BufferTracker::Buffer* buffer = GetBoundPixelTransferBufferIfValid(
        bound_pixel_unpack_transfer_buffer_id_, func_name, offset, size);
    if (buffer && buffer->shm_id() != -1) {
      helper_->TexImage3D(target, level, internalformat, width, height, depth,
                          format, type, buffer->shm_id(),
                          buffer->shm_offset() + offset);
      buffer->set_last_usage_token(helper_->InsertToken());
      CheckGLError();
    }
    return;
  }

  // If there's no data just issue TexImage3D
  if (!pixels || width == 0 || height == 0 || depth == 0) {
    helper_->TexImage3D(target, level, internalformat, width, height, depth,
                        format, type, 0, 0);
    CheckGLError();
    return;
  }

  // Compute the advance bytes per row on the service side.
  // Note |size| is recomputed here if needed.
  uint32_t service_padded_row_size;
  if ((unpack_row_length_ > 0 && unpack_row_length_ != width) ||
      (unpack_image_height_ > 0 && unpack_image_height_ != height)) {
    // All parameters have been applied to the data that are sent to the
    // service side except UNPACK_ALIGNMENT.
    PixelStoreParams service_params;
    service_params.alignment = unpack_alignment_;
    if (!GLES2Util::ComputeImageDataSizesES3(
            width, height, depth, format, type, service_params, &size, nullptr,
            &service_padded_row_size, nullptr, nullptr)) {
      SetGLError(GL_INVALID_VALUE, func_name, "image size too large");
      return;
    }
  } else {
    service_padded_row_size = padded_row_size;
  }
  uint32_t src_height =
      unpack_image_height_ > 0 ? unpack_image_height_ : height;

  // advance pixels pointer past the skip images/rows/pixels
  pixels = reinterpret_cast<const int8_t*>(pixels) + skip_size;

  // Check if we can send it all at once.
  int32_t shm_id = 0;
  uint32_t shm_offset = 0;
  void* buffer_pointer = nullptr;

  ScopedTransferBufferPtr transfer_alloc(size, helper_, transfer_buffer_);
  ScopedMappedMemoryPtr mapped_alloc(0, helper_, mapped_memory_.get());

  if (transfer_alloc.valid() && transfer_alloc.size() >= size) {
    shm_id = transfer_alloc.shm_id();
    shm_offset = transfer_alloc.offset();
    buffer_pointer = transfer_alloc.address();
  } else if (size < max_extra_transfer_buffer_size_) {
    mapped_alloc.Reset(size);
    if (mapped_alloc.valid()) {
      transfer_alloc.Discard();

      mapped_alloc.SetFlushAfterRelease(true);
      shm_id = mapped_alloc.shm_id();
      shm_offset = mapped_alloc.offset();
      buffer_pointer = mapped_alloc.address();
    }
  }

  if (buffer_pointer) {
    for (GLsizei z = 0; z < depth; ++z) {
      CopyRectToBuffer(pixels, height, unpadded_row_size, padded_row_size,
                       buffer_pointer, service_padded_row_size);
      pixels = reinterpret_cast<const int8_t*>(pixels) +
               padded_row_size * src_height;
      buffer_pointer = reinterpret_cast<int8_t*>(buffer_pointer) +
                       service_padded_row_size * height;
    }
    helper_->TexImage3D(target, level, internalformat, width, height, depth,
                        format, type, shm_id, shm_offset);
    CheckGLError();
    return;
  }

  // No, so send it using TexSubImage3D.
  helper_->TexImage3D(target, level, internalformat, width, height, depth,
                      format, type, 0, 0);
  TexSubImage3DImpl(target, level, 0, 0, 0, width, height, depth, format, type,
                    unpadded_row_size, pixels, padded_row_size, GL_TRUE,
                    &transfer_alloc, service_padded_row_size);
  CheckGLError();
}

void GLES2Implementation::TexSubImage2D(GLenum target,
                                        GLint level,
                                        GLint xoffset,
                                        GLint yoffset,
                                        GLsizei width,
                                        GLsizei height,
                                        GLenum format,
                                        GLenum type,
                                        const void* pixels) {
  const char* func_name = "glTexSubImage2D";
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glTexSubImage2D("
                     << GLES2Util::GetStringTextureTarget(target) << ", "
                     << level << ", " << xoffset << ", " << yoffset << ", "
                     << width << ", " << height << ", "
                     << GLES2Util::GetStringTextureFormat(format) << ", "
                     << GLES2Util::GetStringPixelType(type) << ", "
                     << static_cast<const void*>(pixels) << ")");

  if (level < 0 || height < 0 || width < 0 || xoffset < 0 || yoffset < 0) {
    SetGLError(GL_INVALID_VALUE, func_name, "dimension < 0");
    return;
  }
  if (unpack_skip_pixels_ + width >
      (unpack_row_length_ ? unpack_row_length_ : width)) {
    // This is WebGL 2 specific constraints, but we do it for all ES3 contexts.
    SetGLError(GL_INVALID_OPERATION, func_name,
               "invalid unpack params combination");
    return;
  }

  uint32_t size;
  uint32_t unpadded_row_size;
  uint32_t padded_row_size;
  uint32_t skip_size;
  PixelStoreParams params = GetUnpackParameters(k2D);
  if (!GLES2Util::ComputeImageDataSizesES3(
          width, height, 1, format, type, params, &size, &unpadded_row_size,
          &padded_row_size, &skip_size, nullptr)) {
    SetGLError(GL_INVALID_VALUE, func_name, "image size to large");
    return;
  }

  if (bound_pixel_unpack_buffer_) {
    base::CheckedNumeric<uint32_t> offset = ToGLuint(pixels);
    offset += skip_size;
    if (!offset.IsValid()) {
      SetGLError(GL_INVALID_VALUE, func_name, "skip size too large");
      return;
    }
    helper_->TexSubImage2D(target, level, xoffset, yoffset, width, height,
                           format, type, 0, offset.ValueOrDefault(0), false);
    CheckGLError();
    return;
  }

  // If there's a pixel unpack buffer bound use it when issuing TexSubImage2D.
  if (bound_pixel_unpack_transfer_buffer_id_) {
    if (unpack_row_length_ > 0 || unpack_image_height_ > 0 ||
        unpack_skip_pixels_ > 0 || unpack_skip_rows_ > 0 ||
        unpack_skip_images_ > 0) {
      SetGLError(GL_INVALID_OPERATION, func_name,
                 "No ES3 pack parameters with pixel unpack transfer buffer.");
      return;
    }
    DCHECK_EQ(0u, skip_size);
    GLuint offset = ToGLuint(pixels);
    BufferTracker::Buffer* buffer = GetBoundPixelTransferBufferIfValid(
        bound_pixel_unpack_transfer_buffer_id_, func_name, offset, size);
    if (buffer && buffer->shm_id() != -1) {
      helper_->TexSubImage2D(target, level, xoffset, yoffset, width, height,
                             format, type, buffer->shm_id(),
                             buffer->shm_offset() + offset, false);
      buffer->set_last_usage_token(helper_->InsertToken());
      CheckGLError();
    }
    return;
  }

  if (width == 0 || height == 0) {
    // No need to worry about pixel data.
    helper_->TexSubImage2D(target, level, xoffset, yoffset, width, height,
                           format, type, 0, 0, false);
    CheckGLError();
    return;
  }

  // Compute the advance bytes per row on the service side.
  // Note |size| is recomputed here if needed.
  uint32_t service_padded_row_size;
  if (unpack_row_length_ > 0 && unpack_row_length_ != width) {
    // All parameters have been applied to the data that are sent to the
    // service side except UNPACK_ALIGNMENT.
    PixelStoreParams service_params;
    service_params.alignment = unpack_alignment_;
    if (!GLES2Util::ComputeImageDataSizesES3(
            width, height, 1, format, type, service_params, &size, nullptr,
            &service_padded_row_size, nullptr, nullptr)) {
      SetGLError(GL_INVALID_VALUE, func_name, "image size too large");
      return;
    }
  } else {
    service_padded_row_size = padded_row_size;
  }

  // advance pixels pointer past the skip rows and skip pixels
  pixels = reinterpret_cast<const int8_t*>(pixels) + skip_size;

  ScopedTransferBufferPtr buffer(size, helper_, transfer_buffer_);
  base::CheckedNumeric<GLint> checked_xoffset = xoffset;
  checked_xoffset += width;
  if (!checked_xoffset.IsValid()) {
    SetGLError(GL_INVALID_VALUE, "TexSubImage2D", "xoffset + width overflows");
    return;
  }
  base::CheckedNumeric<GLint> checked_yoffset = yoffset;
  checked_yoffset += height;
  if (!checked_yoffset.IsValid()) {
    SetGLError(GL_INVALID_VALUE, "TexSubImage2D", "yoffset + height overflows");
    return;
  }
  TexSubImage2DImpl(target, level, xoffset, yoffset, width, height, format,
                    type, unpadded_row_size, pixels, padded_row_size, GL_FALSE,
                    &buffer, service_padded_row_size);
  CheckGLError();
}

void GLES2Implementation::TexSubImage3D(GLenum target,
                                        GLint level,
                                        GLint xoffset,
                                        GLint yoffset,
                                        GLint zoffset,
                                        GLsizei width,
                                        GLsizei height,
                                        GLsizei depth,
                                        GLenum format,
                                        GLenum type,
                                        const void* pixels) {
  const char* func_name = "glTexSubImage3D";
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glTexSubImage3D("
                     << GLES2Util::GetStringTextureTarget(target) << ", "
                     << level << ", " << xoffset << ", " << yoffset << ", "
                     << zoffset << ", " << width << ", " << height << ", "
                     << depth << ", "
                     << GLES2Util::GetStringTextureFormat(format) << ", "
                     << GLES2Util::GetStringPixelType(type) << ", "
                     << static_cast<const void*>(pixels) << ")");

  if (level < 0 || height < 0 || width < 0 || depth < 0 || xoffset < 0 ||
      yoffset < 0 || zoffset < 0) {
    SetGLError(GL_INVALID_VALUE, func_name, "dimension < 0");
    return;
  }
  if ((unpack_skip_pixels_ + width >
       (unpack_row_length_ ? unpack_row_length_ : width)) ||
      (unpack_skip_rows_ + height >
       (unpack_image_height_ ? unpack_image_height_ : height))) {
    // This is WebGL 2 specific constraints, but we do it for all ES3 contexts.
    SetGLError(GL_INVALID_OPERATION, func_name,
               "invalid unpack params combination");
    return;
  }

  uint32_t size;
  uint32_t unpadded_row_size;
  uint32_t padded_row_size;
  uint32_t skip_size;
  PixelStoreParams params = GetUnpackParameters(k3D);
  if (!GLES2Util::ComputeImageDataSizesES3(
          width, height, depth, format, type, params, &size, &unpadded_row_size,
          &padded_row_size, &skip_size, nullptr)) {
    SetGLError(GL_INVALID_VALUE, func_name, "image size to large");
    return;
  }

  if (bound_pixel_unpack_buffer_) {
    base::CheckedNumeric<uint32_t> offset = ToGLuint(pixels);
    offset += skip_size;
    if (!offset.IsValid()) {
      SetGLError(GL_INVALID_VALUE, func_name, "skip size too large");
      return;
    }
    helper_->TexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                           height, depth, format, type, 0,
                           offset.ValueOrDefault(0), false);
    CheckGLError();
    return;
  }

  // If there's a pixel unpack buffer bound use it when issuing TexSubImage2D.
  if (bound_pixel_unpack_transfer_buffer_id_) {
    if (unpack_row_length_ > 0 || unpack_image_height_ > 0 ||
        unpack_skip_pixels_ > 0 || unpack_skip_rows_ > 0 ||
        unpack_skip_images_ > 0) {
      SetGLError(GL_INVALID_OPERATION, func_name,
                 "No ES3 pack parameters with pixel unpack transfer buffer.");
      return;
    }
    DCHECK_EQ(0u, skip_size);
    GLuint offset = ToGLuint(pixels);
    BufferTracker::Buffer* buffer = GetBoundPixelTransferBufferIfValid(
        bound_pixel_unpack_transfer_buffer_id_, func_name, offset, size);
    if (buffer && buffer->shm_id() != -1) {
      helper_->TexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                             height, depth, format, type, buffer->shm_id(),
                             buffer->shm_offset() + offset, false);
      buffer->set_last_usage_token(helper_->InsertToken());
      CheckGLError();
    }
    return;
  }

  if (width == 0 || height == 0 || depth == 0) {
    // No need to worry about pixel data.
    helper_->TexSubImage3D(target, level, xoffset, yoffset, zoffset, width,
                           height, depth, format, type, 0, 0, false);
    CheckGLError();
    return;
  }

  // Compute the advance bytes per row on the service side
  // Note |size| is recomputed here if needed.
  uint32_t service_padded_row_size;
  if ((unpack_row_length_ > 0 && unpack_row_length_ != width) ||
      (unpack_image_height_ > 0 && unpack_image_height_ != height)) {
    PixelStoreParams service_params;
    service_params.alignment = unpack_alignment_;
    if (!GLES2Util::ComputeImageDataSizesES3(
            width, height, depth, format, type, service_params, &size, nullptr,
            &service_padded_row_size, nullptr, nullptr)) {
      SetGLError(GL_INVALID_VALUE, func_name, "image size too large");
      return;
    }
  } else {
    service_padded_row_size = padded_row_size;
  }

  // advance pixels pointer past the skip images/rows/pixels
  pixels = reinterpret_cast<const int8_t*>(pixels) + skip_size;

  ScopedTransferBufferPtr buffer(size, helper_, transfer_buffer_);
  base::CheckedNumeric<GLint> checked_xoffset = xoffset;
  checked_xoffset += width;
  if (!checked_xoffset.IsValid()) {
    SetGLError(GL_INVALID_VALUE, "TexSubImage3D", "xoffset + width overflows");
    return;
  }
  base::CheckedNumeric<GLint> checked_yoffset = yoffset;
  checked_yoffset += height;
  if (!checked_yoffset.IsValid()) {
    SetGLError(GL_INVALID_VALUE, "TexSubImage3D", "yoffset + height overflows");
    return;
  }
  base::CheckedNumeric<GLint> checked_zoffset = zoffset;
  checked_zoffset += depth;
  if (!checked_zoffset.IsValid()) {
    SetGLError(GL_INVALID_VALUE, "TexSubImage3D", "zoffset + depth overflows");
    return;
  }
  TexSubImage3DImpl(target, level, xoffset, yoffset, zoffset, width, height,
                    depth, format, type, unpadded_row_size, pixels,
                    padded_row_size, GL_FALSE, &buffer,
                    service_padded_row_size);
  CheckGLError();
}

static GLint ComputeNumRowsThatFitInBuffer(uint32_t padded_row_size,
                                           uint32_t unpadded_row_size,
                                           unsigned int size,
                                           GLsizei remaining_rows) {
  DCHECK_GE(unpadded_row_size, 0u);
  if (padded_row_size == 0) {
    return 1;
  }
  GLint num_rows = size / padded_row_size;
  if (num_rows + 1 == remaining_rows &&
      size - num_rows * padded_row_size >= unpadded_row_size) {
    num_rows++;
  }
  return num_rows;
}

void GLES2Implementation::TexSubImage2DImpl(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLenum type,
                                            uint32_t unpadded_row_size,
                                            const void* pixels,
                                            uint32_t pixels_padded_row_size,
                                            GLboolean internal,
                                            ScopedTransferBufferPtr* buffer,
                                            uint32_t buffer_padded_row_size) {
  DCHECK(buffer);
  DCHECK_GE(level, 0);
  DCHECK_GT(height, 0);
  DCHECK_GT(width, 0);
  DCHECK_GE(xoffset, 0);
  DCHECK_GE(yoffset, 0);

  const int8_t* source = reinterpret_cast<const int8_t*>(pixels);
  // Transfer by rows.
  while (height) {
    unsigned int desired_size =
        buffer_padded_row_size * (height - 1) + unpadded_row_size;
    if (!buffer->valid() || buffer->size() == 0) {
      buffer->Reset(desired_size);
      if (!buffer->valid()) {
        return;
      }
    }

    GLint num_rows = ComputeNumRowsThatFitInBuffer(
        buffer_padded_row_size, unpadded_row_size, buffer->size(), height);
    num_rows = std::min(num_rows, height);
    CopyRectToBuffer(source, num_rows, unpadded_row_size,
                     pixels_padded_row_size, buffer->address(),
                     buffer_padded_row_size);
    helper_->TexSubImage2D(target, level, xoffset, yoffset, width, num_rows,
                           format, type, buffer->shm_id(), buffer->offset(),
                           internal);
    buffer->Release();
    yoffset += num_rows;
    source += num_rows * pixels_padded_row_size;
    height -= num_rows;
  }
}

void GLES2Implementation::TexSubImage3DImpl(GLenum target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLsizei zoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLsizei depth,
                                            GLenum format,
                                            GLenum type,
                                            uint32_t unpadded_row_size,
                                            const void* pixels,
                                            uint32_t pixels_padded_row_size,
                                            GLboolean internal,
                                            ScopedTransferBufferPtr* buffer,
                                            uint32_t buffer_padded_row_size) {
  DCHECK(buffer);
  DCHECK_GE(level, 0);
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);
  DCHECK_GT(depth, 0);
  DCHECK_GE(xoffset, 0);
  DCHECK_GE(yoffset, 0);
  DCHECK_GE(zoffset, 0);
  const int8_t* source = reinterpret_cast<const int8_t*>(pixels);
  GLsizei total_rows = height * depth;
  GLint row_index = 0, depth_index = 0;
  while (total_rows) {
    // Each time, we either copy one or more images, or copy one or more rows
    // within a single image, depending on the buffer size limit.
    GLsizei max_rows;
    unsigned int desired_size;
    if (row_index > 0) {
      // We are in the middle of an image. Send the remaining of the image.
      max_rows = height - row_index;
      if (total_rows <= height) {
        // Last image, so last row is unpadded.
        desired_size =
            buffer_padded_row_size * (max_rows - 1) + unpadded_row_size;
      } else {
        desired_size = buffer_padded_row_size * max_rows;
      }
    } else {
      // Send all the remaining data if possible.
      max_rows = total_rows;
      desired_size =
          buffer_padded_row_size * (max_rows - 1) + unpadded_row_size;
    }
    if (!buffer->valid() || buffer->size() == 0) {
      buffer->Reset(desired_size);
      if (!buffer->valid()) {
        return;
      }
    }
    GLint num_rows = ComputeNumRowsThatFitInBuffer(
        buffer_padded_row_size, unpadded_row_size, buffer->size(), total_rows);
    num_rows = std::min(num_rows, max_rows);
    GLint num_images = num_rows / height;
    GLsizei my_height, my_depth;
    if (num_images > 0) {
      num_rows = num_images * height;
      my_height = height;
      my_depth = num_images;
    } else {
      my_height = num_rows;
      my_depth = 1;
    }

    if (num_images > 0) {
      int8_t* buffer_pointer = reinterpret_cast<int8_t*>(buffer->address());
      uint32_t src_height =
          unpack_image_height_ > 0 ? unpack_image_height_ : height;
      uint32_t image_size_dst = buffer_padded_row_size * height;
      uint32_t image_size_src = pixels_padded_row_size * src_height;
      for (GLint ii = 0; ii < num_images; ++ii) {
        CopyRectToBuffer(source + ii * image_size_src, my_height,
                         unpadded_row_size, pixels_padded_row_size,
                         buffer_pointer + ii * image_size_dst,
                         buffer_padded_row_size);
      }
    } else {
      CopyRectToBuffer(source, my_height, unpadded_row_size,
                       pixels_padded_row_size, buffer->address(),
                       buffer_padded_row_size);
    }
    helper_->TexSubImage3D(target, level, xoffset, yoffset + row_index,
                           zoffset + depth_index, width, my_height, my_depth,
                           format, type, buffer->shm_id(), buffer->offset(),
                           internal);
    buffer->Release();

    total_rows -= num_rows;
    if (total_rows > 0) {
      GLint num_image_paddings;
      if (num_images > 0) {
        DCHECK_EQ(row_index, 0);
        depth_index += num_images;
        num_image_paddings = num_images;
      } else {
        row_index = (row_index + my_height) % height;
        num_image_paddings = 0;
        if (my_height > 0 && row_index == 0) {
          depth_index++;
          num_image_paddings++;
        }
      }
      source += num_rows * pixels_padded_row_size;
      if (unpack_image_height_ > height && num_image_paddings > 0) {
        source += num_image_paddings * (unpack_image_height_ - height) *
                  pixels_padded_row_size;
      }
    }
  }
}

void GLES2Implementation::GetResultNameHelper(GLsizei bufsize,
                                              GLsizei* length,
                                              char* name) {
  // Length of string (without final \0) that we will write to the buffer.
  GLsizei max_length = 0;
  if (name && (bufsize > 0)) {
    std::vector<int8_t> str;
    GetBucketContents(kResultBucketId, &str);
    if (!str.empty()) {
      DCHECK_LE(str.size(), static_cast<size_t>(INT_MAX));
      // Note: both bufsize and str.size() count/include the terminating \0.
      max_length = std::min(bufsize, static_cast<GLsizei>(str.size())) - 1;
    }
    memcpy(name, str.data(), max_length);
    name[max_length] = '\0';
  }
  if (length) {
    *length = max_length;
  }
}

bool GLES2Implementation::GetActiveAttribHelper(GLuint program,
                                                GLuint index,
                                                GLsizei bufsize,
                                                GLsizei* length,
                                                GLint* size,
                                                GLenum* type,
                                                char* name) {
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  typedef cmds::GetActiveAttrib::Result Result;
  auto result = GetResultAs<Result>();
  if (!result) {
    return false;
  }
  // Set as failed so if the command fails we'll recover.
  result->success = false;
  helper_->GetActiveAttrib(program, index, kResultBucketId, GetResultShmId(),
                           result.offset());
  if (!WaitForCmd()) {
    return false;
  }
  bool success = !!result->success;
  if (success) {
    if (size) {
      *size = result->size;
    }
    if (type) {
      *type = result->type;
    }
    // Note: this can invalidate |result|.
    GetResultNameHelper(bufsize, length, name);
  }
  return success;
}

void GLES2Implementation::GetActiveAttrib(GLuint program,
                                          GLuint index,
                                          GLsizei bufsize,
                                          GLsizei* length,
                                          GLint* size,
                                          GLenum* type,
                                          char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetActiveAttrib(" << program
                     << ", " << index << ", " << bufsize << ", "
                     << static_cast<const void*>(length) << ", "
                     << static_cast<const void*>(size) << ", "
                     << static_cast<const void*>(type) << ", "
                     << static_cast<const void*>(name) << ", ");
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveAttrib", "bufsize < 0");
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetActiveAttrib");
  bool success = share_group_->program_info_manager()->GetActiveAttrib(
      this, program, index, bufsize, length, size, type, name);
  if (success) {
    if (size) {
      GPU_CLIENT_LOG("  size: " << *size);
    }
    if (type) {
      GPU_CLIENT_LOG("  type: " << GLES2Util::GetStringEnum(*type));
    }
    if (name) {
      GPU_CLIENT_LOG("  name: " << name);
    }
  }
  CheckGLError();
}

bool GLES2Implementation::GetActiveUniformHelper(GLuint program,
                                                 GLuint index,
                                                 GLsizei bufsize,
                                                 GLsizei* length,
                                                 GLint* size,
                                                 GLenum* type,
                                                 char* name) {
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  typedef cmds::GetActiveUniform::Result Result;
  auto result = GetResultAs<Result>();
  if (!result) {
    return false;
  }
  // Set as failed so if the command fails we'll recover.
  result->success = false;
  helper_->GetActiveUniform(program, index, kResultBucketId, GetResultShmId(),
                            result.offset());
  if (!WaitForCmd()) {
    return false;
  }
  bool success = !!result->success;
  if (success) {
    if (size) {
      *size = result->size;
    }
    if (type) {
      *type = result->type;
    }
    // Note: this can invalidate |result|.
    GetResultNameHelper(bufsize, length, name);
  }
  return success;
}

void GLES2Implementation::GetActiveUniform(GLuint program,
                                           GLuint index,
                                           GLsizei bufsize,
                                           GLsizei* length,
                                           GLint* size,
                                           GLenum* type,
                                           char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetActiveUniform(" << program
                     << ", " << index << ", " << bufsize << ", "
                     << static_cast<const void*>(length) << ", "
                     << static_cast<const void*>(size) << ", "
                     << static_cast<const void*>(type) << ", "
                     << static_cast<const void*>(name) << ", ");
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveUniform", "bufsize < 0");
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetActiveUniform");
  bool success = share_group_->program_info_manager()->GetActiveUniform(
      this, program, index, bufsize, length, size, type, name);
  if (success) {
    if (size) {
      GPU_CLIENT_LOG("  size: " << *size);
    }
    if (type) {
      GPU_CLIENT_LOG("  type: " << GLES2Util::GetStringEnum(*type));
    }
    if (name) {
      GPU_CLIENT_LOG("  name: " << name);
    }
  }
  CheckGLError();
}

bool GLES2Implementation::GetActiveUniformBlockNameHelper(GLuint program,
                                                          GLuint index,
                                                          GLsizei bufsize,
                                                          GLsizei* length,
                                                          char* name) {
  DCHECK_LE(0, bufsize);
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  typedef cmds::GetActiveUniformBlockName::Result Result;
  auto result = GetResultAs<Result>();
  if (!result) {
    return false;
  }
  // Set as failed so if the command fails we'll recover.
  *result = 0;
  helper_->GetActiveUniformBlockName(program, index, kResultBucketId,
                                     GetResultShmId(), result.offset());
  if (!WaitForCmd()) {
    return false;
  }
  bool success = !!result;
  if (success) {
    // Note: this can invalidate |result|.
    GetResultNameHelper(bufsize, length, name);
  }
  return success;
}

void GLES2Implementation::GetActiveUniformBlockName(GLuint program,
                                                    GLuint index,
                                                    GLsizei bufsize,
                                                    GLsizei* length,
                                                    char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetActiveUniformBlockName("
                     << program << ", " << index << ", " << bufsize << ", "
                     << static_cast<const void*>(length) << ", "
                     << static_cast<const void*>(name) << ")");
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveUniformBlockName", "bufsize < 0");
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetActiveUniformBlockName");
  bool success =
      share_group_->program_info_manager()->GetActiveUniformBlockName(
          this, program, index, bufsize, length, name);
  if (success) {
    if (name) {
      GPU_CLIENT_LOG("  name: " << name);
    }
  }
  CheckGLError();
}

bool GLES2Implementation::GetActiveUniformBlockivHelper(GLuint program,
                                                        GLuint index,
                                                        GLenum pname,
                                                        GLint* params) {
  typedef cmds::GetActiveUniformBlockiv::Result Result;
  auto result = GetResultAs<Result>();
  if (!result) {
    return false;
  }
  result->SetNumResults(0);
  helper_->GetActiveUniformBlockiv(program, index, pname, GetResultShmId(),
                                   result.offset());
  if (!WaitForCmd()) {
    return false;
  }
  if (result->GetNumResults() > 0) {
    if (params) {
      result->CopyResult(params);
    }
    GPU_CLIENT_LOG_CODE_BLOCK({
      for (int32_t i = 0; i < result->GetNumResults(); ++i) {
        GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
      }
    });
    return true;
  }
  return false;
}

void GLES2Implementation::GetActiveUniformBlockiv(GLuint program,
                                                  GLuint index,
                                                  GLenum pname,
                                                  GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetActiveUniformBlockiv("
                     << program << ", " << index << ", "
                     << GLES2Util::GetStringUniformBlockParameter(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2::GetActiveUniformBlockiv");
  bool success = share_group_->program_info_manager()->GetActiveUniformBlockiv(
      this, program, index, pname, params);
  if (success) {
    if (params) {
      // TODO(zmo): For GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, there will
      // be more than one value returned in params.
      GPU_CLIENT_LOG("  params: " << params[0]);
    }
  }
  CheckGLError();
}

bool GLES2Implementation::GetActiveUniformsivHelper(GLuint program,
                                                    GLsizei count,
                                                    const GLuint* indices,
                                                    GLenum pname,
                                                    GLint* params) {
  base::CheckedNumeric<uint32_t> bytes = count;
  bytes *= sizeof(GLuint);
  if (!bytes.IsValid()) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveUniformsiv", "count overflow");
    return false;
  }
  SetBucketContents(kResultBucketId, indices, bytes.ValueOrDefault(0));
  typedef cmds::GetActiveUniformsiv::Result Result;
  auto result = GetResultAs<Result>();
  if (!result) {
    return false;
  }
  result->SetNumResults(0);
  helper_->GetActiveUniformsiv(program, kResultBucketId, pname,
                               GetResultShmId(), result.offset());
  if (!WaitForCmd()) {
    return false;
  }
  bool success = result->GetNumResults() == count;
  if (success) {
    if (params) {
      result->CopyResult(params);
    }
    GPU_CLIENT_LOG_CODE_BLOCK({
      for (int32_t i = 0; i < result->GetNumResults(); ++i) {
        GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
      }
    });
  }
  helper_->SetBucketSize(kResultBucketId, 0);
  return success;
}

void GLES2Implementation::GetActiveUniformsiv(GLuint program,
                                              GLsizei count,
                                              const GLuint* indices,
                                              GLenum pname,
                                              GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetActiveUniformsiv(" << program
                     << ", " << count << ", "
                     << static_cast<const void*>(indices) << ", "
                     << GLES2Util::GetStringUniformParameter(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2::GetActiveUniformsiv");
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetActiveUniformsiv", "count < 0");
    return;
  }
  bool success = share_group_->program_info_manager()->GetActiveUniformsiv(
      this, program, count, indices, pname, params);
  if (success) {
    if (params) {
      GPU_CLIENT_LOG_CODE_BLOCK({
        for (GLsizei ii = 0; ii < count; ++ii) {
          GPU_CLIENT_LOG("  " << ii << ": " << params[ii]);
        }
      });
    }
  }
  CheckGLError();
}

void GLES2Implementation::GetAttachedShaders(GLuint program,
                                             GLsizei maxcount,
                                             GLsizei* count,
                                             GLuint* shaders) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetAttachedShaders(" << program
                     << ", " << maxcount << ", "
                     << static_cast<const void*>(count) << ", "
                     << static_cast<const void*>(shaders) << ", ");
  if (maxcount < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetAttachedShaders", "maxcount < 0");
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetAttachedShaders");
  typedef cmds::GetAttachedShaders::Result Result;
  uint32_t checked_size = 0;
  if (!Result::ComputeSize(maxcount).AssignIfValid(&checked_size)) {
    SetGLError(GL_OUT_OF_MEMORY, "glGetAttachedShaders",
               "allocation too large");
    return;
  }
  Result* result = static_cast<Result*>(transfer_buffer_->Alloc(checked_size));
  if (!result) {
    return;
  }
  result->SetNumResults(0);
  helper_->GetAttachedShaders(program, transfer_buffer_->GetShmId(),
                              transfer_buffer_->GetOffset(result),
                              checked_size);
  int32_t token = helper_->InsertToken();
  if (!WaitForCmd()) {
    return;
  }
  if (count) {
    *count = result->GetNumResults();
  }
  result->CopyResult(shaders);
  GPU_CLIENT_LOG_CODE_BLOCK({
    for (int32_t i = 0; i < result->GetNumResults(); ++i) {
      GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
    }
  });
  transfer_buffer_->FreePendingToken(result, token);
  CheckGLError();
}

void GLES2Implementation::GetShaderPrecisionFormat(GLenum shadertype,
                                                   GLenum precisiontype,
                                                   GLint* range,
                                                   GLint* precision) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetShaderPrecisionFormat("
                     << GLES2Util::GetStringShaderType(shadertype) << ", "
                     << GLES2Util::GetStringShaderPrecision(precisiontype)
                     << ", " << static_cast<const void*>(range) << ", "
                     << static_cast<const void*>(precision) << ", ");
  TRACE_EVENT0("gpu", "GLES2::GetShaderPrecisionFormat");
  typedef cmds::GetShaderPrecisionFormat::Result Result;
  // Limit scope of result to avoid overlap with CheckGLError()
  {
    auto result = GetResultAs<Result>();
    if (!result) {
      return;
    }

    GLStaticState::ShaderPrecisionKey key(shadertype, precisiontype);
    GLStaticState::ShaderPrecisionMap::iterator i =
        static_state_.shader_precisions.find(key);
    if (i != static_state_.shader_precisions.end()) {
      *result = i->second;
    } else {
      result->success = false;
      helper_->GetShaderPrecisionFormat(shadertype, precisiontype,
                                        GetResultShmId(), result.offset());
      if (!WaitForCmd()) {
        return;
      }
      if (result->success)
        static_state_.shader_precisions[key] = *result;
    }

    if (result->success) {
      if (range) {
        range[0] = result->min_range;
        range[1] = result->max_range;
        GPU_CLIENT_LOG("  min_range: " << range[0]);
        GPU_CLIENT_LOG("  min_range: " << range[1]);
      }
      if (precision) {
        precision[0] = result->precision;
        GPU_CLIENT_LOG("  min_range: " << precision[0]);
      }
    }
  }
  CheckGLError();
}

const GLubyte* GLES2Implementation::GetStringHelper(GLenum name) {
  if (name == GL_EXTENSIONS && cached_extension_string_) {
    return reinterpret_cast<const GLubyte*>(cached_extension_string_);
  }
  const char* result = nullptr;
  // Clears the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetString(name, kResultBucketId);
  std::string str;
  if (GetBucketAsString(kResultBucketId, &str)) {
    // Adds extensions implemented on client side only.
    if (name == GL_EXTENSIONS) {
      str += std::string(str.empty() ? "" : " ") +
             "GL_CHROMIUM_map_sub "
             "GL_CHROMIUM_ordering_barrier "
             "GL_CHROMIUM_sync_point "
             "GL_EXT_unpack_subimage";
    }

    // Because of WebGL the extensions can change. We have to cache each unique
    // result since we don't know when the client will stop referring to a
    // previous one it queries.
    // TODO: Here we could save memory by defining RequestExtensions
    // invalidating the GL_EXTENSIONS string. http://crbug.com/586414
    const std::string& cache = *gl_strings_.insert(str).first;
    result = cache.c_str();

    if (name == GL_EXTENSIONS) {
      cached_extension_string_ = result;
      std::vector<std::string> extensions =
          base::SplitString(cache, base::kWhitespaceASCII,
                            base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      for (const std::string& extension : extensions) {
        cached_extensions_.push_back(
            gl_strings_.insert(extension).first->c_str());
      }
    }
  }
  return reinterpret_cast<const GLubyte*>(result);
}

const GLubyte* GLES2Implementation::GetString(GLenum name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetString("
                     << GLES2Util::GetStringStringType(name) << ")");
  TRACE_EVENT0("gpu", "GLES2::GetString");
  const GLubyte* result = GetStringHelper(name);
  GPU_CLIENT_LOG("  returned " << reinterpret_cast<const char*>(result));
  CheckGLError();
  return result;
}

const GLubyte* GLES2Implementation::GetStringi(GLenum name, GLuint index) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetStringi("
                     << GLES2Util::GetStringStringType(name) << "," << index
                     << ")");
  TRACE_EVENT0("gpu", "GLES2::GetStringi");
  UpdateCachedExtensionsIfNeeded();
  if (name != GL_EXTENSIONS) {
    SetGLError(GL_INVALID_ENUM, "glGetStringi", "name");
    return nullptr;
  }
  if (index >= cached_extensions_.size()) {
    SetGLError(GL_INVALID_VALUE, "glGetStringi", "index too large");
    return nullptr;
  }

  const char* result = cached_extensions_[index];
  GPU_CLIENT_LOG("  returned " << result);
  CheckGLError();
  return reinterpret_cast<const GLubyte*>(result);
}

bool GLES2Implementation::GetTransformFeedbackVaryingHelper(GLuint program,
                                                            GLuint index,
                                                            GLsizei bufsize,
                                                            GLsizei* length,
                                                            GLint* size,
                                                            GLenum* type,
                                                            char* name) {
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  typedef cmds::GetTransformFeedbackVarying::Result Result;
  auto result = GetResultAs<Result>();
  if (!result) {
    return false;
  }
  // Set as failed so if the command fails we'll recover.
  result->success = false;
  helper_->GetTransformFeedbackVarying(program, index, kResultBucketId,
                                       GetResultShmId(), result.offset());
  if (!WaitForCmd()) {
    return false;
  }
  if (result->success) {
    if (size) {
      *size = result->size;
    }
    if (type) {
      *type = result->type;
    }
    // Note: this can invalidate |result|.
    GetResultNameHelper(bufsize, length, name);
  }
  return result->success != 0;
}

void GLES2Implementation::GetTransformFeedbackVarying(GLuint program,
                                                      GLuint index,
                                                      GLsizei bufsize,
                                                      GLsizei* length,
                                                      GLint* size,
                                                      GLenum* type,
                                                      char* name) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetTransformFeedbackVarying("
                     << program << ", " << index << ", " << bufsize << ", "
                     << static_cast<const void*>(length) << ", "
                     << static_cast<const void*>(size) << ", "
                     << static_cast<const void*>(type) << ", "
                     << static_cast<const void*>(name) << ", ");
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetTransformFeedbackVarying",
               "bufsize < 0");
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetTransformFeedbackVarying");
  bool success =
      share_group_->program_info_manager()->GetTransformFeedbackVarying(
          this, program, index, bufsize, length, size, type, name);
  if (success) {
    if (size) {
      GPU_CLIENT_LOG("  size: " << *size);
    }
    if (type) {
      GPU_CLIENT_LOG("  type: " << GLES2Util::GetStringEnum(*type));
    }
    if (name) {
      GPU_CLIENT_LOG("  name: " << name);
    }
  }
  CheckGLError();
}

void GLES2Implementation::GetUniformfv(GLuint program,
                                       GLint location,
                                       GLfloat* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetUniformfv(" << program << ", "
                     << location << ", " << static_cast<const void*>(params)
                     << ")");
  TRACE_EVENT0("gpu", "GLES2::GetUniformfv");
  typedef cmds::GetUniformfv::Result Result;
  // Limit scope of result to avoid overlap with CheckGLError()
  {
    auto result = GetResultAs<Result>();
    if (!result) {
      return;
    }
    result->SetNumResults(0);
    helper_->GetUniformfv(program, location, GetResultShmId(), result.offset());
    if (!WaitForCmd()) {
      return;
    }
    result->CopyResult(params);
    GPU_CLIENT_LOG_CODE_BLOCK({
      for (int32_t i = 0; i < result->GetNumResults(); ++i) {
        GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
      }
    });
  }
  CheckGLError();
}

void GLES2Implementation::GetUniformiv(GLuint program,
                                       GLint location,
                                       GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetUniformiv(" << program << ", "
                     << location << ", " << static_cast<const void*>(params)
                     << ")");
  TRACE_EVENT0("gpu", "GLES2::GetUniformiv");
  typedef cmds::GetUniformiv::Result Result;
  // Limit scope of result to avoid overlap with CheckGLError()
  {
    auto result = GetResultAs<Result>();
    if (!result) {
      return;
    }
    result->SetNumResults(0);
    helper_->GetUniformiv(program, location, GetResultShmId(), result.offset());
    if (!WaitForCmd()) {
      return;
    }
    result->CopyResult(params);
    GPU_CLIENT_LOG_CODE_BLOCK({
      for (int32_t i = 0; i < result->GetNumResults(); ++i) {
        GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
      }
    });
  }
  CheckGLError();
}

void GLES2Implementation::GetUniformuiv(GLuint program,
                                        GLint location,
                                        GLuint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetUniformuiv(" << program
                     << ", " << location << ", "
                     << static_cast<const void*>(params) << ")");
  TRACE_EVENT0("gpu", "GLES2::GetUniformuiv");
  typedef cmds::GetUniformuiv::Result Result;
  // Limit scope of result to avoid overlap with CheckGLError()
  {
    auto result = GetResultAs<Result>();
    if (!result) {
      return;
    }
    result->SetNumResults(0);
    helper_->GetUniformuiv(program, location, GetResultShmId(),
                           result.offset());
    if (!WaitForCmd()) {
      return;
    }
    result->CopyResult(params);
    GPU_CLIENT_LOG_CODE_BLOCK({
      for (int32_t i = 0; i < result->GetNumResults(); ++i) {
        GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
      }
    });
  }
  CheckGLError();
}

void GLES2Implementation::WritePixelsYUVINTERNAL(
    const GLbyte* mailbox,
    GLuint src_size_plane1,
    GLuint src_size_plane2,
    GLuint src_size_plane3,
    GLuint src_size_plane4,
    GLuint src_width,
    GLuint src_height,
    GLuint src_plane_config,
    GLuint src_subsampling,
    GLuint src_datatype,
    GLuint src_row_bytes_plane1,
    GLuint src_row_bytes_plane2,
    GLuint src_row_bytes_plane3,
    GLuint src_row_bytes_plane4,
    const void* src_pixels_plane1,
    const void* src_pixels_plane2,
    const void* src_pixels_plane3,
    const void* src_pixels_plane4) {
  CHECK(mailbox);
  // We have the following stored at offsets from `shm_address`:
  // 0: stores the destination mailbox,
  // pixels_offset_plane1: stores source pixels for plane 1,
  // pixels_offset_plane2: stores source pixels for plane 2,
  // pixels_offset_plane3: stores source pixels for plane 3,
  // pixels_offset_plane4: stores source pixels for plane 4

  const int kMaxPlanes = 4;
  GLuint src_sizes[kMaxPlanes] = {src_size_plane1, src_size_plane2,
                                  src_size_plane3, src_size_plane4};
  const void* src_pixels[kMaxPlanes] = {src_pixels_plane1, src_pixels_plane2,
                                        src_pixels_plane3, src_pixels_plane4};

  GLuint total_size =
      base::bits::AlignUp(sizeof(gpu::Mailbox), sizeof(uint64_t));
  for (int plane = 0; plane < kMaxPlanes; plane++) {
    if (!src_pixels[plane]) {
      // If pixels don't exist for a plane, we've copied all planes of src
      // image.
      CHECK_EQ(src_sizes[plane], 0u);
      break;
    }
    total_size += base::bits::AlignUp(src_sizes[plane],
                                      static_cast<GLuint>(sizeof(uint64_t)));
  }

  ScopedMappedMemoryPtr scoped_shared_memory(total_size, helper(),
                                             mapped_memory_.get());
  if (!scoped_shared_memory.valid()) {
    SetGLError(GL_INVALID_OPERATION, "WritePixelsYUV", "size too big");
    return;
  }

  GLint shm_id = scoped_shared_memory.shm_id();
  GLuint shm_offset = scoped_shared_memory.offset();
  void* address = scoped_shared_memory.address();

  // Copy the mailbox at `address`.
  GLuint mailbox_offset = 0;
  memcpy(static_cast<uint8_t*>(address), mailbox, sizeof(gpu::Mailbox));

  GLuint pixel_offsets[kMaxPlanes] = {};
  // Calculate first plane offset based on mailbox.
  pixel_offsets[0] =
      mailbox_offset + static_cast<GLuint>(base::bits::AlignUp(
                           sizeof(gpu::Mailbox), sizeof(uint64_t)));
  CHECK(src_pixels[0]);
  memcpy(static_cast<uint8_t*>(address) + pixel_offsets[0], src_pixels[0],
         src_sizes[0]);

  for (int plane = 1; plane < kMaxPlanes; plane++) {
    if (!src_pixels[plane]) {
      // If pixels don't exist for a plane, we've copied all planes of src
      // image.
      break;
    }
    // Calculate the offset based on previous plane offset and previous plane
    // size, and copy pixels for current plane starting at current plane
    // offset.
    pixel_offsets[plane] =
        pixel_offsets[plane - 1] +
        base::bits::AlignUp(src_sizes[plane - 1],
                            static_cast<GLuint>(sizeof(uint64_t)));
    memcpy(static_cast<uint8_t*>(address) + pixel_offsets[plane],
           src_pixels[plane], src_sizes[plane]);
  }

  helper_->WritePixelsYUVINTERNAL(
      src_width, src_height, src_row_bytes_plane1, src_row_bytes_plane2,
      src_row_bytes_plane3, src_row_bytes_plane4, src_plane_config,
      src_subsampling, src_datatype, shm_id, shm_offset, pixel_offsets[0],
      pixel_offsets[1], pixel_offsets[2], pixel_offsets[3]);
}

GLboolean GLES2Implementation::ReadbackARGBImagePixelsINTERNAL(
    const GLbyte* mailbox,
    const void* dst_sk_color_space,
    GLuint dst_color_space_size,
    GLuint dst_size,
    GLuint dst_width,
    GLuint dst_height,
    GLuint dst_sk_color_type,
    GLuint dst_sk_alpha_type,
    GLuint dst_row_bytes,
    GLint src_x,
    GLint src_y,
    GLint plane_index,
    void* pixels) {
  DCHECK(mailbox);
  // We can't use GetResultAs<>() to store our result because it uses
  // TransferBuffer under the hood and this function is potentially
  // asynchronous. Instead, store the result at the beginning of the shared
  // memory we allocate to transfer pixels.
  // We have the following stored at offsets from `shm_address`:
  // 0: stores ReadbackARGBImagePixelsINTERNAL::Result,
  // color_space_offset: stores destination SkColorSpace,
  // mailbox_offset: stores the gpu::Mailbox,
  // pixels_offset: stores the pixels
  GLuint color_space_offset = base::bits::AlignUp(
      sizeof(cmds::ReadbackARGBImagePixelsINTERNAL::Result), sizeof(uint64_t));

  // Add the size of the SkColorSpace while maintaining 8-byte alignment.
  GLuint mailbox_offset = color_space_offset;
  if (dst_sk_color_space) {
    mailbox_offset =
        base::bits::AlignUp(color_space_offset + dst_color_space_size,
                            static_cast<GLuint>(sizeof(uint64_t)));
  }

  // Add the size of the mailbox while maintaining 8-byte alignment.
  GLuint pixels_offset = base::bits::AlignUp(
      mailbox_offset + sizeof(gpu::Mailbox), sizeof(uint64_t));

  GLuint total_size =
      pixels_offset +
      base::bits::AlignUp(dst_size, static_cast<GLuint>(sizeof(uint64_t)));

  ScopedMappedMemoryPtr scoped_shared_memory(total_size, helper(),
                                             mapped_memory_.get());
  if (!scoped_shared_memory.valid()) {
    return GL_FALSE;
  }

  GLint shm_id = scoped_shared_memory.shm_id();
  GLuint shm_offset = scoped_shared_memory.offset();
  void* shm_address = scoped_shared_memory.address();

  // Readback success/failure result is stored at the beginning of the shared
  // memory region. Client is responsible for initialization so we do so here.
  auto* readback_result =
      static_cast<cmds::ReadbackARGBImagePixelsINTERNAL::Result*>(shm_address);
  *readback_result = 0;

  if (dst_sk_color_space) {
    // Copy destination color space to the destination color space offset.
    memcpy(static_cast<uint8_t*>(shm_address) + color_space_offset,
           dst_sk_color_space, dst_color_space_size);
  }
  // Copy shared image mailbox to the mailbox offset.
  memcpy(static_cast<uint8_t*>(shm_address) + mailbox_offset, mailbox,
         sizeof(gpu::Mailbox));

  helper_->ReadbackARGBImagePixelsINTERNAL(
      src_x, src_y, plane_index, dst_width, dst_height, dst_row_bytes,
      dst_sk_color_type, dst_sk_alpha_type, shm_id, shm_offset,
      color_space_offset, pixels_offset, mailbox_offset);

  if (!WaitForCmd()) {
    return GL_FALSE;
  }
  if (!*readback_result) {
    return GL_FALSE;
  }
  memcpy(pixels, static_cast<uint8_t*>(shm_address) + pixels_offset, dst_size);
  return GL_TRUE;
}

void GLES2Implementation::ReadPixels(GLint xoffset,
                                     GLint yoffset,
                                     GLsizei width,
                                     GLsizei height,
                                     GLenum format,
                                     GLenum type,
                                     void* pixels) {
  const char* func_name = "glReadPixels";
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glReadPixels(" << xoffset << ", "
                     << yoffset << ", " << width << ", " << height << ", "
                     << GLES2Util::GetStringReadPixelFormat(format) << ", "
                     << GLES2Util::GetStringPixelType(type) << ", "
                     << static_cast<const void*>(pixels) << ")");
  if (width < 0 || height < 0) {
    SetGLError(GL_INVALID_VALUE, func_name, "dimensions < 0");
    return;
  }

  if (pack_skip_pixels_ + width >
      (pack_row_length_ ? pack_row_length_ : width)) {
    // This is WebGL 2 specific constraints, but we do it for all ES3 contexts.
    SetGLError(GL_INVALID_OPERATION, func_name,
               "invalid pack params combination");
    return;
  }

  // glReadPixel pads the size of each row of pixels by an amount specified by
  // glPixelStorei. So, we have to take that into account both in the fact that
  // the pixels returned from the ReadPixel command will include that padding
  // and that when we copy the results to the user's buffer we need to not
  // write those padding bytes but leave them as they are.

  TRACE_EVENT0("gpu", "GLES2::ReadPixels");
  typedef cmds::ReadPixels::Result Result;

  uint32_t size;
  uint32_t unpadded_row_size;
  uint32_t padded_row_size;
  uint32_t skip_size;
  PixelStoreParams params;
  params.alignment = pack_alignment_;
  params.row_length = pack_row_length_;
  params.skip_pixels = pack_skip_pixels_;
  params.skip_rows = pack_skip_rows_;
  if (!GLES2Util::ComputeImageDataSizesES3(
          width, height, 1, format, type, params, &size, &unpadded_row_size,
          &padded_row_size, &skip_size, nullptr)) {
    SetGLError(GL_INVALID_VALUE, func_name, "size too large.");
    return;
  }

  if (bound_pixel_pack_buffer_) {
    base::CheckedNumeric<GLuint> offset = ToGLuint(pixels);
    offset += skip_size;
    if (!offset.IsValid()) {
      SetGLError(GL_INVALID_VALUE, func_name, "skip size too large.");
      return;
    }
    helper_->ReadPixels(xoffset, yoffset, width, height, format, type, 0,
                        offset.ValueOrDefault(0), 0, 0, false);
    InvalidateReadbackBufferShadowDataCHROMIUM(bound_pixel_pack_buffer_);

    CheckGLError();
    return;
  }

  uint32_t service_padded_row_size = 0;
  if (pack_row_length_ > 0 && pack_row_length_ != width) {
    if (!GLES2Util::ComputeImagePaddedRowSize(
            width, format, type, pack_alignment_, &service_padded_row_size)) {
      SetGLError(GL_INVALID_VALUE, func_name, "size too large.");
      return;
    }
  } else {
    service_padded_row_size = padded_row_size;
  }

  if (bound_pixel_pack_transfer_buffer_id_) {
    if (pack_row_length_ > 0 || pack_skip_pixels_ > 0 || pack_skip_rows_ > 0) {
      SetGLError(GL_INVALID_OPERATION, func_name,
                 "No ES3 pack parameters with pixel pack transfer buffer.");
      return;
    }
    DCHECK_EQ(0u, skip_size);
    GLuint offset = ToGLuint(pixels);
    BufferTracker::Buffer* buffer = GetBoundPixelTransferBufferIfValid(
        bound_pixel_pack_transfer_buffer_id_, func_name, offset, size);
    if (buffer && buffer->shm_id() != -1) {
      helper_->ReadPixels(xoffset, yoffset, width, height, format, type,
                          buffer->shm_id(), buffer->shm_offset() + offset, 0, 0,
                          true);
      CheckGLError();
    }
    return;
  }

  if (!pixels) {
    SetGLError(GL_INVALID_OPERATION, func_name, "pixels = NULL");
    return;
  }

  int8_t* dest = reinterpret_cast<int8_t*>(pixels);
  // Advance pixels pointer past the skip rows and skip pixels
  dest += skip_size;

  // Transfer by rows.
  // The max rows we can transfer.
  GLsizei remaining_rows = height;
  GLint y_index = yoffset;
  uint32_t group_size = GLES2Util::ComputeImageGroupSize(format, type);
  uint32_t skip_row_bytes = 0;
  if (xoffset < 0) {
    skip_row_bytes = static_cast<uint32_t>(-xoffset) * group_size;
  }
  do {
    // Even if height == 0, we still need to trigger the service side handling
    // in case invalid args are passed in and a GL errro needs to be generated.
    GLsizei desired_size =
        remaining_rows == 0 ? 0
                            : service_padded_row_size * (remaining_rows - 1) +
                                  unpadded_row_size;
    ScopedTransferBufferPtr buffer(desired_size, helper_, transfer_buffer_);
    if (!buffer.valid()) {
      break;
    }
    GLint num_rows = ComputeNumRowsThatFitInBuffer(
        service_padded_row_size, unpadded_row_size, buffer.size(),
        remaining_rows);
    // NOTE: We must look up the address of the result area AFTER allocation
    // of the transfer buffer since the transfer buffer may be reallocated.
    auto result = GetResultAs<Result>();
    if (!result) {
      break;
    }
    result->success = 0;  // mark as failed.
    result->row_length = 0;
    result->num_rows = 0;
    helper_->ReadPixels(xoffset, y_index, width, num_rows, format, type,
                        buffer.shm_id(), buffer.offset(), GetResultShmId(),
                        result.offset(), false);
    if (!WaitForCmd()) {
      break;
    }
    // If it was not marked as successful exit.
    if (!result->success) {
      break;
    }
    if (remaining_rows == 0) {
      break;
    }
    const uint8_t* src = static_cast<const uint8_t*>(buffer.address());
    if (padded_row_size == unpadded_row_size &&
        (pack_row_length_ == 0 || pack_row_length_ == width) &&
        result->row_length == width && result->num_rows == num_rows) {
      // The pixels are tightly packed.
      uint32_t copy_size = unpadded_row_size * num_rows;
      memcpy(dest, src, copy_size);
      dest += copy_size;
    } else if (result->row_length > 0 && result->num_rows > 0) {
      uint32_t copy_row_size = result->row_length * group_size;
      uint32_t copy_last_row_size = copy_row_size;
      if (copy_row_size + skip_row_bytes > padded_row_size) {
        // We need to avoid writing into next row in case the leading pixels
        // are out-of-bounds and they need to be left untouched.
        copy_row_size = padded_row_size - skip_row_bytes;
      }
      // We have to copy 1 row at a time to avoid writing padding bytes.
      GLint copied_rows = 0;
      for (GLint yy = 0; yy < num_rows; ++yy) {
        if (y_index + yy >= 0 && copied_rows < result->num_rows) {
          if (yy + 1 == num_rows && remaining_rows == num_rows) {
            memcpy(dest + skip_row_bytes, src + skip_row_bytes,
                   copy_last_row_size);
          } else {
            memcpy(dest + skip_row_bytes, src + skip_row_bytes, copy_row_size);
          }
          ++copied_rows;
        }
        dest += padded_row_size;
        src += service_padded_row_size;
      }
      DCHECK_EQ(result->num_rows, copied_rows);
    }
    y_index += num_rows;
    remaining_rows -= num_rows;
  } while (remaining_rows);
  CheckGLError();
}

void GLES2Implementation::ActiveTexture(GLenum texture) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glActiveTexture("
                     << GLES2Util::GetStringEnum(texture) << ")");
  GLuint texture_index = texture - GL_TEXTURE0;
  if (texture_index >=
      static_cast<GLuint>(gl_capabilities_.max_combined_texture_image_units)) {
    SetGLErrorInvalidEnum("glActiveTexture", texture, "texture");
    return;
  }

  active_texture_unit_ = texture_index;
  helper_->ActiveTexture(texture);
  CheckGLError();
}

void GLES2Implementation::GenBuffersHelper(GLsizei /* n */,
                                           const GLuint* /* buffers */) {}

void GLES2Implementation::GenFramebuffersHelper(
    GLsizei /* n */,
    const GLuint* /* framebuffers */) {}

void GLES2Implementation::GenRenderbuffersHelper(
    GLsizei /* n */,
    const GLuint* /* renderbuffers */) {}

void GLES2Implementation::GenTexturesHelper(GLsizei /* n */,
                                            const GLuint* /* textures */) {}

void GLES2Implementation::GenVertexArraysOESHelper(GLsizei n,
                                                   const GLuint* arrays) {
  vertex_array_object_manager_->GenVertexArrays(n, arrays);
}

void GLES2Implementation::GenQueriesEXTHelper(GLsizei /* n */,
                                              const GLuint* /* queries */) {}

void GLES2Implementation::GenSamplersHelper(GLsizei /* n */,
                                            const GLuint* /* samplers */) {}

void GLES2Implementation::GenTransformFeedbacksHelper(
    GLsizei /* n */,
    const GLuint* /* transformfeedbacks */) {}

// NOTE #1: On old versions of OpenGL, calling glBindXXX with an unused id
// generates a new resource. On newer versions of OpenGL they don't. The code
// related to binding below will need to change if we switch to the new OpenGL
// model. Specifically it assumes a bind will succeed which is always true in
// the old model but possibly not true in the new model if another context has
// deleted the resource.

// NOTE #2: There is a bug in some BindXXXHelpers, that IDs might be marked as
// used even when Bind has failed. However, the bug is minor compared to the
// overhead & duplicated checking in client side.

void GLES2Implementation::BindBufferHelper(GLenum target, GLuint buffer_id) {
  // TODO(gman): See note #1 above.
  bool changed = false;
  switch (target) {
    case GL_ARRAY_BUFFER:
      if (bound_array_buffer_ != buffer_id) {
        bound_array_buffer_ = buffer_id;
        changed = true;
      }
      break;
    case GL_ATOMIC_COUNTER_BUFFER:
      if (bound_atomic_counter_buffer_ != buffer_id) {
        bound_atomic_counter_buffer_ = buffer_id;
        changed = true;
      }
      break;
    case GL_COPY_READ_BUFFER:
      if (bound_copy_read_buffer_ != buffer_id) {
        bound_copy_read_buffer_ = buffer_id;
        changed = true;
      }
      break;
    case GL_COPY_WRITE_BUFFER:
      if (bound_copy_write_buffer_ != buffer_id) {
        bound_copy_write_buffer_ = buffer_id;
        changed = true;
      }
      break;
    case GL_DISPATCH_INDIRECT_BUFFER:
      if (bound_dispatch_indirect_buffer_ != buffer_id) {
        bound_dispatch_indirect_buffer_ = buffer_id;
        changed = true;
      }
      break;
    case GL_DRAW_INDIRECT_BUFFER:
      if (bound_draw_indirect_buffer_ != buffer_id) {
        bound_draw_indirect_buffer_ = buffer_id;
        changed = true;
      }
      break;
    case GL_ELEMENT_ARRAY_BUFFER:
      changed = vertex_array_object_manager_->BindElementArray(buffer_id);
      break;
    case GL_PIXEL_PACK_BUFFER:
      if (bound_pixel_pack_buffer_ != buffer_id) {
        bound_pixel_pack_buffer_ = buffer_id;
        changed = true;
      }
      break;
    case GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM:
      bound_pixel_pack_transfer_buffer_id_ = buffer_id;
      break;
    case GL_PIXEL_UNPACK_BUFFER:
      if (bound_pixel_unpack_buffer_ != buffer_id) {
        bound_pixel_unpack_buffer_ = buffer_id;
        changed = true;
      }
      break;
    case GL_PIXEL_UNPACK_TRANSFER_BUFFER_CHROMIUM:
      bound_pixel_unpack_transfer_buffer_id_ = buffer_id;
      break;
    case GL_SHADER_STORAGE_BUFFER:
      if (bound_shader_storage_buffer_ != buffer_id) {
        bound_shader_storage_buffer_ = buffer_id;
        changed = true;
      }
      break;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
      if (bound_transform_feedback_buffer_ != buffer_id) {
        bound_transform_feedback_buffer_ = buffer_id;
        changed = true;
      }
      break;
    case GL_UNIFORM_BUFFER:
      if (bound_uniform_buffer_ != buffer_id) {
        bound_uniform_buffer_ = buffer_id;
        changed = true;
      }
      break;
    default:
      changed = true;
      break;
  }
  // TODO(gman): See note #2 above.
  if (changed) {
    GetIdHandler(SharedIdNamespaces::kBuffers)
        ->MarkAsUsedForBind(this, target, buffer_id,
                            &GLES2Implementation::BindBufferStub);
  }
}

void GLES2Implementation::BindBufferStub(GLenum target, GLuint buffer) {
  helper_->BindBuffer(target, buffer);
  if (share_group_->bind_generates_resource())
    helper_->CommandBufferHelper::OrderingBarrier();
}

bool GLES2Implementation::UpdateIndexedBufferState(GLenum target,
                                                   GLuint index,
                                                   GLuint buffer_id,
                                                   const char* function_name) {
  switch (target) {
    case GL_ATOMIC_COUNTER_BUFFER:
      if (index >= static_cast<GLuint>(
                       gl_capabilities_.max_atomic_counter_buffer_bindings)) {
        SetGLError(GL_INVALID_VALUE, function_name, "index out of range");
        return false;
      }
      bound_atomic_counter_buffer_ = buffer_id;
      break;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
      if (index >=
          static_cast<GLuint>(
              gl_capabilities_.max_transform_feedback_separate_attribs)) {
        SetGLError(GL_INVALID_VALUE, function_name, "index out of range");
        return false;
      }
      bound_transform_feedback_buffer_ = buffer_id;
      break;
    case GL_SHADER_STORAGE_BUFFER:
      if (index >= static_cast<GLuint>(
                       gl_capabilities_.max_shader_storage_buffer_bindings)) {
        SetGLError(GL_INVALID_VALUE, function_name, "index out of range");
        return false;
      }
      bound_shader_storage_buffer_ = buffer_id;
      break;
    case GL_UNIFORM_BUFFER:
      if (index >=
          static_cast<GLuint>(gl_capabilities_.max_uniform_buffer_bindings)) {
        SetGLError(GL_INVALID_VALUE, function_name, "index out of range");
        return false;
      }
      bound_uniform_buffer_ = buffer_id;
      break;
    default:
      SetGLError(GL_INVALID_ENUM, function_name, "invalid target");
      return false;
  }
  return true;
}

void GLES2Implementation::BindBufferBaseHelper(GLenum target,
                                               GLuint index,
                                               GLuint buffer_id) {
  // TODO(zmo): See note #1 above.
  // TODO(zmo): See note #2 above.
  if (UpdateIndexedBufferState(target, index, buffer_id, "glBindBufferBase")) {
    GetIdHandler(SharedIdNamespaces::kBuffers)
        ->MarkAsUsedForBind(this, target, index, buffer_id,
                            &GLES2Implementation::BindBufferBaseStub);
  }
}

void GLES2Implementation::BindBufferBaseStub(GLenum target,
                                             GLuint index,
                                             GLuint buffer) {
  helper_->BindBufferBase(target, index, buffer);
  if (share_group_->bind_generates_resource())
    helper_->CommandBufferHelper::Flush();
}

void GLES2Implementation::BindBufferRangeHelper(GLenum target,
                                                GLuint index,
                                                GLuint buffer_id,
                                                GLintptr offset,
                                                GLsizeiptr size) {
  // TODO(zmo): See note #1 above.
  // TODO(zmo): See note #2 above.
  if (UpdateIndexedBufferState(target, index, buffer_id, "glBindBufferRange")) {
    GetIdHandler(SharedIdNamespaces::kBuffers)
        ->MarkAsUsedForBind(this, target, index, buffer_id, offset, size,
                            &GLES2Implementation::BindBufferRangeStub);
  }
}

void GLES2Implementation::BindBufferRangeStub(GLenum target,
                                              GLuint index,
                                              GLuint buffer,
                                              GLintptr offset,
                                              GLsizeiptr size) {
  helper_->BindBufferRange(target, index, buffer, offset, size);
  if (share_group_->bind_generates_resource())
    helper_->CommandBufferHelper::Flush();
}

void GLES2Implementation::BindFramebufferHelper(GLenum target,
                                                GLuint framebuffer) {
  // TODO(gman): See note #1 above.
  bool changed = false;
  switch (target) {
    case GL_FRAMEBUFFER:
      if (bound_framebuffer_ != framebuffer ||
          bound_read_framebuffer_ != framebuffer) {
        bound_framebuffer_ = framebuffer;
        bound_read_framebuffer_ = framebuffer;
        changed = true;
      }
      break;
    case GL_READ_FRAMEBUFFER:
#if EXPENSIVE_DCHECKS_ARE_ON()
      DCHECK(gl_capabilities_.major_version >= 3 ||
             IsChromiumFramebufferMultisampleAvailable());
#endif
      if (bound_read_framebuffer_ != framebuffer) {
        bound_read_framebuffer_ = framebuffer;
        changed = true;
      }
      break;
    case GL_DRAW_FRAMEBUFFER:
#if EXPENSIVE_DCHECKS_ARE_ON()
      DCHECK(gl_capabilities_.major_version >= 3 ||
             IsChromiumFramebufferMultisampleAvailable());
#endif
      if (bound_framebuffer_ != framebuffer) {
        bound_framebuffer_ = framebuffer;
        changed = true;
      }
      break;
    default:
      SetGLErrorInvalidEnum("glBindFramebuffer", target, "target");
      return;
  }

  if (changed) {
    if (framebuffer != 0)
      GetIdAllocator(IdNamespaces::kFramebuffers)->MarkAsUsed(framebuffer);
    helper_->BindFramebuffer(target, framebuffer);
  }
}

void GLES2Implementation::BindRenderbufferHelper(GLenum target,
                                                 GLuint renderbuffer) {
  // TODO(gman): See note #1 above.
  bool changed = false;
  switch (target) {
    case GL_RENDERBUFFER:
      if (bound_renderbuffer_ != renderbuffer) {
        bound_renderbuffer_ = renderbuffer;
        changed = true;
      }
      break;
    default:
      changed = true;
      break;
  }
  // TODO(zmo): See note #2 above.
  if (changed) {
    GetIdHandler(SharedIdNamespaces::kRenderbuffers)
        ->MarkAsUsedForBind(this, target, renderbuffer,
                            &GLES2Implementation::BindRenderbufferStub);
  }
}

void GLES2Implementation::BindRenderbufferStub(GLenum target,
                                               GLuint renderbuffer) {
  helper_->BindRenderbuffer(target, renderbuffer);
  if (share_group_->bind_generates_resource())
    helper_->CommandBufferHelper::OrderingBarrier();
}

void GLES2Implementation::BindSamplerHelper(GLuint unit, GLuint sampler) {
  helper_->BindSampler(unit, sampler);
}

void GLES2Implementation::BindTextureHelper(GLenum target, GLuint texture) {
  // TODO(gman): See note #1 above.
  bool changed = false;
  TextureUnit& unit = texture_units_[active_texture_unit_];
  switch (target) {
    case GL_TEXTURE_2D:
      if (unit.bound_texture_2d != texture) {
        unit.bound_texture_2d = texture;
        changed = true;
      }
      break;
    case GL_TEXTURE_CUBE_MAP:
      if (unit.bound_texture_cube_map != texture) {
        unit.bound_texture_cube_map = texture;
        changed = true;
      }
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      if (unit.bound_texture_external_oes != texture) {
        unit.bound_texture_external_oes = texture;
        changed = true;
      }
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      if (unit.bound_texture_rectangle_arb != texture) {
        unit.bound_texture_rectangle_arb = texture;
        changed = true;
      }
      break;
    default:
      changed = true;
      break;
  }
  // TODO(gman): See note #2 above.
  if (changed) {
    GetIdHandler(SharedIdNamespaces::kTextures)
        ->MarkAsUsedForBind(this, target, texture,
                            &GLES2Implementation::BindTextureStub);
  }
}

void GLES2Implementation::BindTextureStub(GLenum target, GLuint texture) {
  helper_->BindTexture(target, texture);
  if (share_group_->bind_generates_resource())
    helper_->CommandBufferHelper::OrderingBarrier();
}

void GLES2Implementation::BindTransformFeedbackHelper(
    GLenum target,
    GLuint transformfeedback) {
  helper_->BindTransformFeedback(target, transformfeedback);
}

void GLES2Implementation::BindVertexArrayOESHelper(GLuint array) {
  bool changed = false;
  if (vertex_array_object_manager_->BindVertexArray(array, &changed)) {
    if (changed) {
      // Unlike other BindXXXHelpers we don't call MarkAsUsedForBind
      // because unlike other resources VertexArrayObject ids must
      // be generated by GenVertexArrays. A random id to Bind will not
      // generate a new object.
      helper_->BindVertexArrayOES(array);
    }
  } else {
    SetGLError(GL_INVALID_OPERATION, "glBindVertexArrayOES",
               "id was not generated with glGenVertexArrayOES");
  }
}

void GLES2Implementation::UseProgramHelper(GLuint program) {
  if (current_program_ != program) {
    current_program_ = program;
    helper_->UseProgram(program);
  }
}

bool GLES2Implementation::IsBufferReservedId(GLuint id) {
  return vertex_array_object_manager_->IsReservedId(id);
}

void GLES2Implementation::DeleteBuffersHelper(GLsizei n,
                                              const GLuint* buffers) {
  if (!GetIdHandler(SharedIdNamespaces::kBuffers)
           ->FreeIds(this, n, buffers,
                     &GLES2Implementation::DeleteBuffersStub)) {
    SetGLError(GL_INVALID_VALUE, "glDeleteBuffers",
               "id not created by this context.");
    return;
  }
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (buffers[ii] == bound_array_buffer_) {
      bound_array_buffer_ = 0;
    }
    if (buffers[ii] == bound_atomic_counter_buffer_) {
      bound_atomic_counter_buffer_ = 0;
    }
    if (buffers[ii] == bound_copy_read_buffer_) {
      bound_copy_read_buffer_ = 0;
    }
    if (buffers[ii] == bound_copy_write_buffer_) {
      bound_copy_write_buffer_ = 0;
    }
    if (buffers[ii] == bound_dispatch_indirect_buffer_) {
      bound_dispatch_indirect_buffer_ = 0;
    }
    if (buffers[ii] == bound_draw_indirect_buffer_) {
      bound_draw_indirect_buffer_ = 0;
    }
    if (buffers[ii] == bound_pixel_pack_buffer_) {
      bound_pixel_pack_buffer_ = 0;
    }
    if (buffers[ii] == bound_pixel_unpack_buffer_) {
      bound_pixel_unpack_buffer_ = 0;
    }
    if (buffers[ii] == bound_shader_storage_buffer_) {
      bound_shader_storage_buffer_ = 0;
    }
    if (buffers[ii] == bound_transform_feedback_buffer_) {
      bound_transform_feedback_buffer_ = 0;
    }
    if (buffers[ii] == bound_uniform_buffer_) {
      bound_uniform_buffer_ = 0;
    }
    vertex_array_object_manager_->UnbindBuffer(buffers[ii]);

    BufferTracker::Buffer* buffer = buffer_tracker_->GetBuffer(buffers[ii]);
    if (buffer)
      RemoveTransferBuffer(buffer);

    readback_buffer_shadow_tracker_->RemoveBuffer(buffers[ii]);

    if (buffers[ii] == bound_pixel_unpack_transfer_buffer_id_) {
      bound_pixel_unpack_transfer_buffer_id_ = 0;
    }

    RemoveMappedBufferRangeById(buffers[ii]);
  }
}

void GLES2Implementation::DeleteBuffersStub(GLsizei n, const GLuint* buffers) {
  helper_->DeleteBuffersImmediate(n, buffers);
}

void GLES2Implementation::DeleteFramebuffersHelper(GLsizei n,
                                                   const GLuint* framebuffers) {
  helper_->DeleteFramebuffersImmediate(n, framebuffers);
  IdAllocator* id_allocator = GetIdAllocator(IdNamespaces::kFramebuffers);
  for (GLsizei ii = 0; ii < n; ++ii) {
    id_allocator->FreeID(framebuffers[ii]);
    if (framebuffers[ii] == bound_framebuffer_) {
      bound_framebuffer_ = 0;
    }
    if (framebuffers[ii] == bound_read_framebuffer_) {
      bound_read_framebuffer_ = 0;
    }
  }
}

void GLES2Implementation::DeleteRenderbuffersHelper(
    GLsizei n,
    const GLuint* renderbuffers) {
  if (!GetIdHandler(SharedIdNamespaces::kRenderbuffers)
           ->FreeIds(this, n, renderbuffers,
                     &GLES2Implementation::DeleteRenderbuffersStub)) {
    SetGLError(GL_INVALID_VALUE, "glDeleteRenderbuffers",
               "id not created by this context.");
    return;
  }
  for (GLsizei ii = 0; ii < n; ++ii) {
    if (renderbuffers[ii] == bound_renderbuffer_) {
      bound_renderbuffer_ = 0;
    }
  }
}

void GLES2Implementation::DeleteRenderbuffersStub(GLsizei n,
                                                  const GLuint* renderbuffers) {
  helper_->DeleteRenderbuffersImmediate(n, renderbuffers);
}

void GLES2Implementation::DeleteTexturesHelper(GLsizei n,
                                               const GLuint* textures) {
  if (!GetIdHandler(SharedIdNamespaces::kTextures)
           ->FreeIds(this, n, textures,
                     &GLES2Implementation::DeleteTexturesStub)) {
    SetGLError(GL_INVALID_VALUE, "glDeleteTextures",
               "id not created by this context.");
    return;
  }
  for (GLsizei ii = 0; ii < n; ++ii) {
    share_group_->discardable_texture_manager()->FreeTexture(textures[ii]);
  }
  UnbindTexturesHelper(n, textures);
}

void GLES2Implementation::UnbindTexturesHelper(GLsizei n,
                                               const GLuint* textures) {
  for (GLsizei ii = 0; ii < n; ++ii) {
    for (GLint tt = 0; tt < gl_capabilities_.max_combined_texture_image_units;
         ++tt) {
      TextureUnit& unit = texture_units_[tt];
      if (textures[ii] == unit.bound_texture_2d) {
        unit.bound_texture_2d = 0;
      }
      if (textures[ii] == unit.bound_texture_cube_map) {
        unit.bound_texture_cube_map = 0;
      }
      if (textures[ii] == unit.bound_texture_external_oes) {
        unit.bound_texture_external_oes = 0;
      }
      if (textures[ii] == unit.bound_texture_rectangle_arb) {
        unit.bound_texture_rectangle_arb = 0;
      }
    }
  }
}

void GLES2Implementation::DeleteTexturesStub(GLsizei n,
                                             const GLuint* textures) {
  helper_->DeleteTexturesImmediate(n, textures);
}

void GLES2Implementation::DeleteVertexArraysOESHelper(GLsizei n,
                                                      const GLuint* arrays) {
  vertex_array_object_manager_->DeleteVertexArrays(n, arrays);
  helper_->DeleteVertexArraysOESImmediate(n, arrays);
  IdAllocator* id_allocator = GetIdAllocator(IdNamespaces::kVertexArrays);
  for (GLsizei ii = 0; ii < n; ++ii)
    id_allocator->FreeID(arrays[ii]);
}

void GLES2Implementation::DeleteSamplersStub(GLsizei n,
                                             const GLuint* samplers) {
  helper_->DeleteSamplersImmediate(n, samplers);
}

void GLES2Implementation::DeleteSamplersHelper(GLsizei n,
                                               const GLuint* samplers) {
  if (!GetIdHandler(SharedIdNamespaces::kSamplers)
           ->FreeIds(this, n, samplers,
                     &GLES2Implementation::DeleteSamplersStub)) {
    SetGLError(GL_INVALID_VALUE, "glDeleteSamplers",
               "id not created by this context.");
    return;
  }
}

void GLES2Implementation::DeleteTransformFeedbacksHelper(
    GLsizei n,
    const GLuint* transformfeedbacks) {
  helper_->DeleteTransformFeedbacksImmediate(n, transformfeedbacks);
  IdAllocator* id_allocator = GetIdAllocator(IdNamespaces::kTransformFeedbacks);
  for (GLsizei ii = 0; ii < n; ++ii)
    id_allocator->FreeID(transformfeedbacks[ii]);
}

void GLES2Implementation::DisableVertexAttribArray(GLuint index) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDisableVertexAttribArray("
                     << index << ")");
  vertex_array_object_manager_->SetAttribEnable(index, false);
  helper_->DisableVertexAttribArray(index);
  CheckGLError();
}

void GLES2Implementation::EnableVertexAttribArray(GLuint index) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glEnableVertexAttribArray("
                     << index << ")");
  vertex_array_object_manager_->SetAttribEnable(index, true);
  helper_->EnableVertexAttribArray(index);
  CheckGLError();
}

void GLES2Implementation::DrawArrays(GLenum mode, GLint first, GLsizei count) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDrawArrays("
                     << GLES2Util::GetStringDrawMode(mode) << ", " << first
                     << ", " << count << ")");
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawArrays", "count < 0");
    return;
  }
  bool simulated = false;
  if (vertex_array_object_manager_->SupportsClientSideBuffers()) {
    GLsizei num_elements;
    if (!base::CheckAdd(first, count).AssignIfValid(&num_elements)) {
      SetGLError(GL_INVALID_VALUE, "glDrawArrays", "first+count overflow");
      return;
    }
    if (!vertex_array_object_manager_->SetupSimulatedClientSideBuffers(
            "glDrawArrays", this, helper_, num_elements, 0, &simulated)) {
      return;
    }
  }
  helper_->DrawArrays(mode, first, count);
  RestoreArrayBuffer(simulated);
  CheckGLError();
}

void GLES2Implementation::DrawArraysIndirect(GLenum mode, const void* offset) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDrawArraysIndirect("
                     << GLES2Util::GetStringDrawMode(mode) << ", " << offset
                     << ")");
  if (!ValidateOffset("glDrawArraysIndirect",
                      reinterpret_cast<GLintptr>(offset))) {
    return;
  }
  // This is for WebGL 2.0 Compute which doesn't support client side arrays
  if (vertex_array_object_manager_->SupportsClientSideBuffers()) {
    SetGLError(GL_INVALID_OPERATION, "glDrawArraysIndirect",
               "Missing array buffer for vertex attribute");
    return;
  }
  helper_->DrawArraysIndirect(mode, ToGLuint(offset));
  CheckGLError();
}

void GLES2Implementation::GetVertexAttribfv(GLuint index,
                                            GLenum pname,
                                            GLfloat* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetVertexAttribfv(" << index
                     << ", " << GLES2Util::GetStringVertexAttribute(pname)
                     << ", " << static_cast<const void*>(params) << ")");
  uint32_t value = 0;
  if (vertex_array_object_manager_->GetVertexAttrib(index, pname, &value)) {
    *params = static_cast<GLfloat>(value);
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetVertexAttribfv");
  typedef cmds::GetVertexAttribfv::Result Result;
  // Limit scope of result to avoid overlap with CheckGLError()
  {
    auto result = GetResultAs<Result>();
    if (!result) {
      return;
    }
    result->SetNumResults(0);
    helper_->GetVertexAttribfv(index, pname, GetResultShmId(), result.offset());
    if (!WaitForCmd()) {
      return;
    }
    result->CopyResult(params);
    GPU_CLIENT_LOG_CODE_BLOCK({
      for (int32_t i = 0; i < result->GetNumResults(); ++i) {
        GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
      }
    });
  }
  CheckGLError();
}

void GLES2Implementation::GetVertexAttribiv(GLuint index,
                                            GLenum pname,
                                            GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetVertexAttribiv(" << index
                     << ", " << GLES2Util::GetStringVertexAttribute(pname)
                     << ", " << static_cast<const void*>(params) << ")");
  uint32_t value = 0;
  if (vertex_array_object_manager_->GetVertexAttrib(index, pname, &value)) {
    *params = static_cast<GLint>(value);
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetVertexAttribiv");
  typedef cmds::GetVertexAttribiv::Result Result;
  // Limit scope of result to avoid overlap with CheckGLError()
  {
    auto result = GetResultAs<Result>();
    if (!result) {
      return;
    }
    result->SetNumResults(0);
    helper_->GetVertexAttribiv(index, pname, GetResultShmId(), result.offset());
    if (!WaitForCmd()) {
      return;
    }
    result->CopyResult(params);
    GPU_CLIENT_LOG_CODE_BLOCK({
      for (int32_t i = 0; i < result->GetNumResults(); ++i) {
        GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
      }
    });
  }
  CheckGLError();
}

void GLES2Implementation::GetVertexAttribIiv(GLuint index,
                                             GLenum pname,
                                             GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetVertexAttribIiv(" << index
                     << ", " << GLES2Util::GetStringVertexAttribute(pname)
                     << ", " << static_cast<const void*>(params) << ")");
  uint32_t value = 0;
  if (vertex_array_object_manager_->GetVertexAttrib(index, pname, &value)) {
    *params = static_cast<GLint>(value);
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetVertexAttribIiv");
  typedef cmds::GetVertexAttribiv::Result Result;
  // Limit scope of result to avoid overlap with CheckGLError()
  {
    auto result = GetResultAs<Result>();
    if (!result) {
      return;
    }
    result->SetNumResults(0);
    helper_->GetVertexAttribIiv(index, pname, GetResultShmId(),
                                result.offset());
    if (!WaitForCmd()) {
      return;
    }
    result->CopyResult(params);
    GPU_CLIENT_LOG_CODE_BLOCK({
      for (int32_t i = 0; i < result->GetNumResults(); ++i) {
        GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
      }
    });
  }
  CheckGLError();
}

void GLES2Implementation::GetVertexAttribIuiv(GLuint index,
                                              GLenum pname,
                                              GLuint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetVertexAttribIuiv(" << index
                     << ", " << GLES2Util::GetStringVertexAttribute(pname)
                     << ", " << static_cast<const void*>(params) << ")");
  uint32_t value = 0;
  if (vertex_array_object_manager_->GetVertexAttrib(index, pname, &value)) {
    *params = static_cast<GLuint>(value);
    return;
  }
  TRACE_EVENT0("gpu", "GLES2::GetVertexAttribIuiv");
  typedef cmds::GetVertexAttribiv::Result Result;
  // Limit scope of result to avoid overlap with CheckGLError()
  {
    auto result = GetResultAs<Result>();
    if (!result) {
      return;
    }
    result->SetNumResults(0);
    helper_->GetVertexAttribIuiv(index, pname, GetResultShmId(),
                                 result.offset());
    if (!WaitForCmd()) {
      return;
    }
    result->CopyResult(params);
    GPU_CLIENT_LOG_CODE_BLOCK({
      for (int32_t i = 0; i < result->GetNumResults(); ++i) {
        GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
      }
    });
  }
  CheckGLError();
}

GLenum GLES2Implementation::GetGraphicsResetStatusKHR() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetGraphicsResetStatusKHR()");
  // If any context (including ourselves) has seen itself become lost,
  // then it will have told the ShareGroup, so just report its status.
  if (share_group_->IsLost())
    return GL_UNKNOWN_CONTEXT_RESET_KHR;
  return GL_NO_ERROR;
}

GLboolean GLES2Implementation::EnableFeatureCHROMIUM(const char* feature) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glEnableFeatureCHROMIUM("
                     << feature << ")");
  TRACE_EVENT0("gpu", "GLES2::EnableFeatureCHROMIUM");
  typedef cmds::EnableFeatureCHROMIUM::Result Result;
  SetBucketAsCString(kResultBucketId, feature);
  auto result = GetResultAs<Result>();
  if (!result) {
    return false;
  }
  *result = 0;
  helper_->EnableFeatureCHROMIUM(kResultBucketId, GetResultShmId(),
                                 result.offset());
  if (!WaitForCmd()) {
    return false;
  }
  helper_->SetBucketSize(kResultBucketId, 0);
  GPU_CLIENT_LOG("   returned " << GLES2Util::GetStringBool(*result));
  return *result != 0;
}

void* GLES2Implementation::MapBufferSubDataCHROMIUM(GLuint target,
                                                    GLintptr offset,
                                                    GLsizeiptr size,
                                                    GLenum access) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glMapBufferSubDataCHROMIUM("
                     << target << ", " << offset << ", " << size << ", "
                     << GLES2Util::GetStringEnum(access) << ")");
  // NOTE: target is NOT checked because the service will check it
  // and we don't know what targets are valid.
  if (access != GL_WRITE_ONLY) {
    SetGLErrorInvalidEnum("glMapBufferSubDataCHROMIUM", access, "access");
    return nullptr;
  }
  if (!ValidateSize("glMapBufferSubDataCHROMIUM", size) ||
      !ValidateOffset("glMapBufferSubDataCHROMIUM", offset)) {
    return nullptr;
  }

  int32_t shm_id;
  unsigned int shm_offset;
  void* mem = mapped_memory_->Alloc(size, &shm_id, &shm_offset);
  if (!mem) {
    SetGLError(GL_OUT_OF_MEMORY, "glMapBufferSubDataCHROMIUM", "out of memory");
    return nullptr;
  }

  std::pair<MappedBufferMap::iterator, bool> result = mapped_buffers_.insert(
      std::make_pair(mem, MappedBuffer(access, shm_id, mem, shm_offset, target,
                                       offset, size)));
  DCHECK(result.second);
  GPU_CLIENT_LOG("  returned " << mem);
  return mem;
}

void GLES2Implementation::UnmapBufferSubDataCHROMIUM(const void* mem) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUnmapBufferSubDataCHROMIUM("
                     << mem << ")");
  MappedBufferMap::iterator it = mapped_buffers_.find(mem);
  if (it == mapped_buffers_.end()) {
    SetGLError(GL_INVALID_VALUE, "UnmapBufferSubDataCHROMIUM",
               "buffer not mapped");
    return;
  }
  const MappedBuffer& mb = it->second;
  helper_->BufferSubData(mb.target, mb.offset, mb.size, mb.shm_id,
                         mb.shm_offset);
  InvalidateReadbackBufferShadowDataCHROMIUM(GetBoundBufferHelper(mb.target));
  mapped_memory_->FreePendingToken(mb.shm_memory, helper_->InsertToken());
  mapped_buffers_.erase(it);
  CheckGLError();
}

GLuint GLES2Implementation::GetBoundBufferHelper(GLenum target) {
  GLenum binding = GLES2Util::MapBufferTargetToBindingEnum(target);
  GLint id = 0;
  bool cached = GetHelper(binding, &id);
  DCHECK(cached);
  return static_cast<GLuint>(id);
}

void GLES2Implementation::RemoveMappedBufferRangeByTarget(GLenum target) {
  GLuint buffer = GetBoundBufferHelper(target);
  RemoveMappedBufferRangeById(buffer);
}

void GLES2Implementation::RemoveMappedBufferRangeById(GLuint buffer) {
  if (buffer > 0) {
    auto iter = mapped_buffer_range_map_.find(buffer);
    if (iter != mapped_buffer_range_map_.end() && iter->second.shm_memory) {
      mapped_memory_->FreePendingToken(iter->second.shm_memory,
                                       helper_->InsertToken());
      mapped_buffer_range_map_.erase(iter);
    }
  }
}

void GLES2Implementation::ClearMappedBufferRangeMap() {
  for (auto& buffer_range : mapped_buffer_range_map_) {
    if (buffer_range.second.shm_memory) {
      mapped_memory_->FreePendingToken(buffer_range.second.shm_memory,
                                       helper_->InsertToken());
    }
  }
  mapped_buffer_range_map_.clear();
}

void GLES2Implementation::ClearMappedBufferMap() {
  for (auto& buffer : mapped_buffers_) {
    if (buffer.second.shm_memory) {
      mapped_memory_->FreePendingToken(buffer.second.shm_memory,
                                       helper_->InsertToken());
    }
  }
  mapped_buffers_.clear();
}

void GLES2Implementation::ClearMappedTextureMap() {
  for (auto& texture : mapped_textures_) {
    if (texture.second.shm_memory) {
      mapped_memory_->FreePendingToken(texture.second.shm_memory,
                                       helper_->InsertToken());
    }
  }
  mapped_textures_.clear();
}

void* GLES2Implementation::MapBufferRange(GLenum target,
                                          GLintptr offset,
                                          GLsizeiptr size,
                                          GLbitfield access) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glMapBufferRange("
                     << GLES2Util::GetStringEnum(target) << ", " << offset
                     << ", " << size << ", " << access << ")");
  if (!ValidateSize("glMapBufferRange", size) ||
      !ValidateOffset("glMapBufferRange", offset)) {
    return nullptr;
  }

  GLuint buffer = GetBoundBufferHelper(target);

  void* mem = nullptr;

  // Early return if we have a valid shadow copy for readback
  if (access == GL_MAP_READ_BIT) {
    // This will return an incorrect result if the client does the following:
    // * Writes into a buffer
    // * Issues query (GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM)
    // * Writes into the buffer using transform feedback (but doesn't issue
    //   InvalidateReadbackBufferShadowDataCHROMIUM correctly)
    // * Waits on the query
    // * Reads from the buffer (may return results from before the transfom
    //   feedback operation).
    // Therefore, if (and only if) a client uses the
    // GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM query, it must also correctly
    // use InvalidateReadbackBufferShadowDataCHROMIUM. WebGL (at the time of
    // this writing) is expected to be the only client which uses
    // GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM.
    if (auto* buffer_object =
            readback_buffer_shadow_tracker_->GetBuffer(buffer)) {
      mem = buffer_object->MapReadbackShm(offset, size);
      if (!mem) {
        // (If there's no valid shadow copy, warn and fall back to usual logic.)
        SendErrorMessage(
            "performance warning: READ-usage buffer was read back without "
            "waiting on a fence. This caused a graphics pipeline stall.",
            0);
      }
    }
  }

  // Usual, round-trip path if we're not doing a shadow-copy readback
  int32_t shm_id = 0;
  unsigned int shm_offset = 0;
  if (!mem) {
    mem = mapped_memory_->Alloc(size, &shm_id, &shm_offset);
    auto result = GetResultAs<cmds::MapBufferRange::Result>();
    if (!mem || !result) {
      SetGLError(GL_OUT_OF_MEMORY, "glMapBufferRange", "out of memory");
      return nullptr;
    }

    *result = 0;
    helper_->MapBufferRange(target, offset, size, access, shm_id, shm_offset,
                            GetResultShmId(), result.offset());
    // TODO(zmo): For write only mode with MAP_INVALID_*_BIT, we should
    // consider an early return without WaitForCmd(). crbug.com/465804.
    if (!WaitForCmd()) {
      return nullptr;
    }
    if (*result) {
      const GLbitfield kInvalidateBits =
          GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_INVALIDATE_RANGE_BIT;
      if ((access & kInvalidateBits) != 0) {
        // We do not read back from the buffer, therefore, we set the client
        // side memory to zero to avoid uninitialized data.
        memset(mem, 0, size);
      }
    } else {
      mapped_memory_->Free(mem);
      mem = nullptr;
    }
  }

  // Track this mapping regardless of which path was taken above.
  if (mem) {
    DCHECK_NE(0u, buffer);
    // glMapBufferRange fails on an already mapped buffer.
    DCHECK(mapped_buffer_range_map_.find(buffer) ==
           mapped_buffer_range_map_.end());
    auto iter = mapped_buffer_range_map_.insert(std::make_pair(
        buffer,
        MappedBuffer(access, shm_id, mem, shm_offset, target, offset, size)));
    DCHECK(iter.second);
  }

  GPU_CLIENT_LOG("  returned " << mem);
  CheckGLError();
  return mem;
}

GLboolean GLES2Implementation::UnmapBuffer(GLenum target) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUnmapBuffer("
                     << GLES2Util::GetStringEnum(target) << ")");
  switch (target) {
    case GL_ARRAY_BUFFER:
    case GL_ATOMIC_COUNTER_BUFFER:
    case GL_ELEMENT_ARRAY_BUFFER:
    case GL_COPY_READ_BUFFER:
    case GL_COPY_WRITE_BUFFER:
    case GL_DISPATCH_INDIRECT_BUFFER:
    case GL_DRAW_INDIRECT_BUFFER:
    case GL_PIXEL_PACK_BUFFER:
    case GL_PIXEL_UNPACK_BUFFER:
    case GL_SHADER_STORAGE_BUFFER:
    case GL_TRANSFORM_FEEDBACK_BUFFER:
    case GL_UNIFORM_BUFFER:
      break;
    default:
      SetGLError(GL_INVALID_ENUM, "glUnmapBuffer", "invalid target");
      return GL_FALSE;
  }
  GLuint buffer = GetBoundBufferHelper(target);
  if (buffer == 0) {
    SetGLError(GL_INVALID_OPERATION, "glUnmapBuffer", "no buffer bound");
    return GL_FALSE;
  }
  auto iter = mapped_buffer_range_map_.find(buffer);
  if (iter == mapped_buffer_range_map_.end()) {
    SetGLError(GL_INVALID_OPERATION, "glUnmapBuffer", "buffer is unmapped");
    return GL_FALSE;
  }

  bool was_mapped_by_readback_tracker = false;
  if (auto* buffer_object =
          readback_buffer_shadow_tracker_->GetBuffer(buffer)) {
    was_mapped_by_readback_tracker = buffer_object->UnmapReadbackShm();
  }
  if (!was_mapped_by_readback_tracker) {
    helper_->UnmapBuffer(target);
    InvalidateReadbackBufferShadowDataCHROMIUM(GetBoundBufferHelper(target));
  }
  RemoveMappedBufferRangeById(buffer);

  // TODO(zmo): There is a rare situation that data might be corrupted and
  // GL_FALSE should be returned. We lose context on that sitatuon, so we
  // don't have to WaitForCmd().
  GPU_CLIENT_LOG("  returned " << GL_TRUE);
  CheckGLError();
  return GL_TRUE;
}

void* GLES2Implementation::MapTexSubImage2DCHROMIUM(GLenum target,
                                                    GLint level,
                                                    GLint xoffset,
                                                    GLint yoffset,
                                                    GLsizei width,
                                                    GLsizei height,
                                                    GLenum format,
                                                    GLenum type,
                                                    GLenum access) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glMapTexSubImage2DCHROMIUM("
                     << target << ", " << level << ", " << xoffset << ", "
                     << yoffset << ", " << width << ", " << height << ", "
                     << GLES2Util::GetStringTextureFormat(format) << ", "
                     << GLES2Util::GetStringPixelType(type) << ", "
                     << GLES2Util::GetStringEnum(access) << ")");
  if (access != GL_WRITE_ONLY) {
    SetGLErrorInvalidEnum("glMapTexSubImage2DCHROMIUM", access, "access");
    return nullptr;
  }
  // NOTE: target is NOT checked because the service will check it
  // and we don't know what targets are valid.
  if (level < 0 || xoffset < 0 || yoffset < 0 || width < 0 || height < 0) {
    SetGLError(GL_INVALID_VALUE, "glMapTexSubImage2DCHROMIUM",
               "bad dimensions");
    return nullptr;
  }
  uint32_t size;
  if (!GLES2Util::ComputeImageDataSizes(width, height, 1, format, type,
                                        unpack_alignment_, &size, nullptr,
                                        nullptr)) {
    SetGLError(GL_INVALID_VALUE, "glMapTexSubImage2DCHROMIUM",
               "image size too large");
    return nullptr;
  }
  int32_t shm_id;
  unsigned int shm_offset;
  void* mem = mapped_memory_->Alloc(size, &shm_id, &shm_offset);
  if (!mem) {
    SetGLError(GL_OUT_OF_MEMORY, "glMapTexSubImage2DCHROMIUM", "out of memory");
    return nullptr;
  }

  std::pair<MappedTextureMap::iterator, bool> result =
      mapped_textures_.insert(std::make_pair(
          mem, MappedTexture(access, shm_id, mem, shm_offset, target, level,
                             xoffset, yoffset, width, height, format, type)));
  DCHECK(result.second);
  GPU_CLIENT_LOG("  returned " << mem);
  return mem;
}

void GLES2Implementation::UnmapTexSubImage2DCHROMIUM(const void* mem) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUnmapTexSubImage2DCHROMIUM("
                     << mem << ")");
  MappedTextureMap::iterator it = mapped_textures_.find(mem);
  if (it == mapped_textures_.end()) {
    SetGLError(GL_INVALID_VALUE, "UnmapTexSubImage2DCHROMIUM",
               "texture not mapped");
    return;
  }
  const MappedTexture& mt = it->second;
  helper_->TexSubImage2D(mt.target, mt.level, mt.xoffset, mt.yoffset, mt.width,
                         mt.height, mt.format, mt.type, mt.shm_id,
                         mt.shm_offset, GL_FALSE);
  mapped_memory_->FreePendingToken(mt.shm_memory, helper_->InsertToken());
  mapped_textures_.erase(it);
  CheckGLError();
}

void GLES2Implementation::ResizeCHROMIUM(GLuint width,
                                         GLuint height,
                                         float scale_factor,
                                         GLcolorSpace gl_color_space,
                                         GLboolean alpha) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glResizeCHROMIUM(" << width << ", "
                     << height << ", " << scale_factor << ", " << alpha << ")");
  // Including gfx::ColorSpace would bring Skia and a lot of other code into
  // NaCl's IRT, so just leave the color space unspecified.
#if !defined(__native_client__) && !BUILDFLAG(IS_MINIMAL_TOOLCHAIN)
  if (gl_color_space) {
    gfx::ColorSpace gfx_color_space =
        *reinterpret_cast<const gfx::ColorSpace*>(gl_color_space);
    base::Pickle color_space_data;
    IPC::ParamTraits<gfx::ColorSpace>::Write(&color_space_data,
                                             gfx_color_space);
    ScopedTransferBufferPtr buffer(color_space_data.size(), helper_,
                                   transfer_buffer_);
    if (!buffer.valid() || buffer.size() < color_space_data.size()) {
      SetGLError(GL_OUT_OF_MEMORY, "GLES2::ResizeCHROMIUM", "out of memory");
      return;
    }
    memcpy(buffer.address(), color_space_data.data(), color_space_data.size());
    helper_->ResizeCHROMIUM(width, height, scale_factor, alpha, buffer.shm_id(),
                            buffer.offset(), color_space_data.size());
    CheckGLError();
    return;
  }
#endif
  helper_->ResizeCHROMIUM(width, height, scale_factor, alpha, 0, 0, 0);
  CheckGLError();
}

const GLchar* GLES2Implementation::GetRequestableExtensionsCHROMIUM() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glGetRequestableExtensionsCHROMIUM()");
  TRACE_EVENT0("gpu",
               "GLES2Implementation::GetRequestableExtensionsCHROMIUM()");
  const char* result = nullptr;
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetRequestableExtensionsCHROMIUM(kResultBucketId);
  std::string str;
  if (GetBucketAsString(kResultBucketId, &str)) {
    // The set of requestable extensions shrinks as we enable
    // them. Because we don't know when the client will stop referring
    // to a previous one it queries (see GetString) we need to cache
    // the unique results.
    // TODO: Here we could save memory by defining RequestExtensions
    // invalidating the GL_EXTENSIONS string. http://crbug.com/586414
    result = gl_strings_.insert(str).first->c_str();
  }
  GPU_CLIENT_LOG("  returned " << result);
  return reinterpret_cast<const GLchar*>(result);
}

// TODO(gman): Remove this command. It's here for WebGL but is incompatible
// with VirtualGL contexts.
void GLES2Implementation::RequestExtensionCHROMIUM(const char* extension) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glRequestExtensionCHROMIUM("
                     << extension << ")");
  InvalidateCachedExtensions();
  SetBucketAsCString(kResultBucketId, extension);
  helper_->RequestExtensionCHROMIUM(kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);

  struct ExtensionCheck {
    const char* extension;
    raw_ptr<ExtensionStatus> status;
  };
  const ExtensionCheck checks[] = {
      {
          "GL_CHROMIUM_framebuffer_multisample",
          &chromium_framebuffer_multisample_,
      },
  };
  const size_t kNumChecks = sizeof(checks) / sizeof(checks[0]);
  for (size_t ii = 0; ii < kNumChecks; ++ii) {
    const ExtensionCheck& check = checks[ii];
    if (*check.status == kUnavailableExtensionStatus &&
        !strcmp(extension, check.extension)) {
      *check.status = kUnknownExtensionStatus;
    }
  }
}

void GLES2Implementation::GetProgramInfoCHROMIUMHelper(
    GLuint program,
    std::vector<int8_t>* result) {
  DCHECK(result);
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetProgramInfoCHROMIUM(program, kResultBucketId);
  GetBucketContents(kResultBucketId, result);
}

void GLES2Implementation::GetProgramInfoCHROMIUM(GLuint program,
                                                 GLsizei bufsize,
                                                 GLsizei* size,
                                                 void* info) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glProgramInfoCHROMIUM",
               "bufsize less than 0.");
    return;
  }
  if (size == nullptr) {
    SetGLError(GL_INVALID_VALUE, "glProgramInfoCHROMIUM", "size is null.");
    return;
  }
  // Make sure they've set size to 0 else the value will be undefined on
  // lost context.
  DCHECK_EQ(0, *size);
  std::vector<int8_t> result;
  GetProgramInfoCHROMIUMHelper(program, &result);
  if (result.empty()) {
    return;
  }
  *size = result.size();
  if (!info) {
    return;
  }
  if (static_cast<size_t>(bufsize) < result.size()) {
    SetGLError(GL_INVALID_OPERATION, "glProgramInfoCHROMIUM",
               "bufsize is too small for result.");
    return;
  }
  memcpy(info, &result[0], result.size());
}

void GLES2Implementation::GetUniformBlocksCHROMIUMHelper(
    GLuint program,
    std::vector<int8_t>* result) {
  DCHECK(result);
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetUniformBlocksCHROMIUM(program, kResultBucketId);
  GetBucketContents(kResultBucketId, result);
}

void GLES2Implementation::GetUniformBlocksCHROMIUM(GLuint program,
                                                   GLsizei bufsize,
                                                   GLsizei* size,
                                                   void* info) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetUniformBlocksCHROMIUM",
               "bufsize less than 0.");
    return;
  }
  if (size == nullptr) {
    SetGLError(GL_INVALID_VALUE, "glGetUniformBlocksCHROMIUM", "size is null.");
    return;
  }
  // Make sure they've set size to 0 else the value will be undefined on
  // lost context.
  DCHECK_EQ(0, *size);
  std::vector<int8_t> result;
  GetUniformBlocksCHROMIUMHelper(program, &result);
  if (result.empty()) {
    return;
  }
  *size = result.size();
  if (!info) {
    return;
  }
  if (static_cast<size_t>(bufsize) < result.size()) {
    SetGLError(GL_INVALID_OPERATION, "glGetUniformBlocksCHROMIUM",
               "bufsize is too small for result.");
    return;
  }
  memcpy(info, &result[0], result.size());
}

void GLES2Implementation::GetUniformsES3CHROMIUMHelper(
    GLuint program,
    std::vector<int8_t>* result) {
  DCHECK(result);
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetUniformsES3CHROMIUM(program, kResultBucketId);
  GetBucketContents(kResultBucketId, result);
}

void GLES2Implementation::GetUniformsES3CHROMIUM(GLuint program,
                                                 GLsizei bufsize,
                                                 GLsizei* size,
                                                 void* info) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetUniformsES3CHROMIUM",
               "bufsize less than 0.");
    return;
  }
  if (size == nullptr) {
    SetGLError(GL_INVALID_VALUE, "glGetUniformsES3CHROMIUM", "size is null.");
    return;
  }
  // Make sure they've set size to 0 else the value will be undefined on
  // lost context.
  DCHECK_EQ(0, *size);
  std::vector<int8_t> result;
  GetUniformsES3CHROMIUMHelper(program, &result);
  if (result.empty()) {
    return;
  }
  *size = result.size();
  if (!info) {
    return;
  }
  if (static_cast<size_t>(bufsize) < result.size()) {
    SetGLError(GL_INVALID_OPERATION, "glGetUniformsES3CHROMIUM",
               "bufsize is too small for result.");
    return;
  }
  memcpy(info, &result[0], result.size());
}

void GLES2Implementation::GetTransformFeedbackVaryingsCHROMIUMHelper(
    GLuint program,
    std::vector<int8_t>* result) {
  DCHECK(result);
  // Clear the bucket so if the command fails nothing will be in it.
  helper_->SetBucketSize(kResultBucketId, 0);
  helper_->GetTransformFeedbackVaryingsCHROMIUM(program, kResultBucketId);
  GetBucketContents(kResultBucketId, result);
}

void GLES2Implementation::GetTransformFeedbackVaryingsCHROMIUM(GLuint program,
                                                               GLsizei bufsize,
                                                               GLsizei* size,
                                                               void* info) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  if (bufsize < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetTransformFeedbackVaryingsCHROMIUM",
               "bufsize less than 0.");
    return;
  }
  if (size == nullptr) {
    SetGLError(GL_INVALID_VALUE, "glGetTransformFeedbackVaryingsCHROMIUM",
               "size is null.");
    return;
  }
  // Make sure they've set size to 0 else the value will be undefined on
  // lost context.
  DCHECK_EQ(0, *size);
  std::vector<int8_t> result;
  GetTransformFeedbackVaryingsCHROMIUMHelper(program, &result);
  if (result.empty()) {
    return;
  }
  *size = result.size();
  if (!info) {
    return;
  }
  if (static_cast<size_t>(bufsize) < result.size()) {
    SetGLError(GL_INVALID_OPERATION, "glGetTransformFeedbackVaryingsCHROMIUM",
               "bufsize is too small for result.");
    return;
  }
  memcpy(info, &result[0], result.size());
}

void GLES2Implementation::DeleteQueriesEXTHelper(GLsizei n,
                                                 const GLuint* queries) {
  IdAllocator* id_allocator = GetIdAllocator(IdNamespaces::kQueries);
  for (GLsizei ii = 0; ii < n; ++ii) {
    query_tracker_->RemoveQuery(queries[ii]);
    id_allocator->FreeID(queries[ii]);
  }

  helper_->DeleteQueriesEXTImmediate(n, queries);
}

GLboolean GLES2Implementation::IsQueryEXT(GLuint id) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] IsQueryEXT(" << id << ")");

  // TODO(gman): To be spec compliant IDs from other contexts sharing
  // resources need to return true here even though you can't share
  // queries across contexts?
  return query_tracker_->GetQuery(id) != nullptr;
}

void GLES2Implementation::BeginQueryEXT(GLenum target, GLuint id) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] BeginQueryEXT("
                     << GLES2Util::GetStringQueryTarget(target) << ", " << id
                     << ")");

  switch (target) {
    case GL_COMMANDS_ISSUED_CHROMIUM:
    case GL_ASYNC_PIXEL_PACK_COMPLETED_CHROMIUM:
    case GL_GET_ERROR_QUERY_CHROMIUM:
    case GL_PROGRAM_COMPLETION_QUERY_CHROMIUM:
      break;
    case GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM:
    case GL_COMMANDS_COMPLETED_CHROMIUM:
      if (!gl_capabilities_.sync_query) {
        SetGLError(GL_INVALID_OPERATION, "glBeginQueryEXT",
                   "not enabled for commands completed queries");
        return;
      }
      break;
    case GL_SAMPLES_PASSED_ARB:
      SetGLError(GL_INVALID_OPERATION, "glBeginQueryEXT",
                 "not enabled for occlusion queries");
      return;
    case GL_ANY_SAMPLES_PASSED:
    case GL_ANY_SAMPLES_PASSED_CONSERVATIVE:
      if (!gl_capabilities_.occlusion_query_boolean) {
        SetGLError(GL_INVALID_OPERATION, "glBeginQueryEXT",
                   "not enabled for boolean occlusion queries");
        return;
      }
      break;
    case GL_TIME_ELAPSED_EXT:
      if (!gl_capabilities_.timer_queries) {
        SetGLError(GL_INVALID_OPERATION, "glBeginQueryEXT",
                   "not enabled for timing queries");
        return;
      }
      break;
    case GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN:
      if (gl_capabilities_.major_version >= 3) {
        break;
      }
      [[fallthrough]];
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

  // Extra setups some targets might need.
  switch (target) {
    case GL_TIME_ELAPSED_EXT:
      if (!query_tracker_->SetDisjointSync(this)) {
        SetGLError(GL_OUT_OF_MEMORY, "glBeginQueryEXT",
                   "buffer allocation failed");
        return;
      }
      break;
    default:
      break;
  }

  if (query_tracker_->BeginQuery(id, target, this))
    CheckGLError();

  if (target == GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM) {
    AllocateShadowCopiesForReadback();
  }
}

void GLES2Implementation::AllocateShadowCopiesForReadback() {
  for (auto buffer : readback_buffer_shadow_tracker_->GetUnfencedBufferList()) {
    if (!buffer) {
      continue;
    }
    int32_t shm_id = 0;
    uint32_t shm_offset = 0;
    bool already_allocated = false;
    uint32_t size = buffer->Alloc(&shm_id, &shm_offset, &already_allocated);
    if (already_allocated) {
      SendErrorMessage(
          "performance warning: READ-usage buffer was written, then "
          "fenced, but written again before being read back. This discarded "
          "the shadow copy that was created to accelerate readback.",
          0);
    }
    helper_->SetReadbackBufferShadowAllocationINTERNAL(buffer->id(), shm_id,
                                                       shm_offset, size);
  }
}

void GLES2Implementation::BufferShadowWrittenCallback(
    const ReadbackBufferShadowTracker::BufferList& buffers,
    uint64_t serial) {
  for (const auto& buffer : buffers) {
    if (buffer) {
      buffer->UpdateSerialTo(serial);
    }
  }
}

void GLES2Implementation::EndQueryEXT(GLenum target) {
  QueryTracker::Query* query = nullptr;
  {
    GPU_CLIENT_SINGLE_THREAD_CHECK();
    GPU_CLIENT_LOG("[" << GetLogPrefix() << "] EndQueryEXT("
                       << GLES2Util::GetStringQueryTarget(target) << ")");
    query = query_tracker_->GetCurrentQuery(target);
    if (!query_tracker_->EndQuery(target, this)) {
      return;
    }
    CheckGLError();
  }  // GPU_CLIENT_SINGLE_THREAD_CHECK ends here

  if (target == GL_READBACK_SHADOW_COPIES_UPDATED_CHROMIUM) {
    DCHECK(query);
    auto serial = readback_buffer_shadow_tracker_->buffer_shadow_serial();
    readback_buffer_shadow_tracker_->IncrementSerial();
    auto buffers = readback_buffer_shadow_tracker_->TakeUnfencedBufferList();
    query->SetCompletedCallback(
        base::BindOnce(&GLES2Implementation::BufferShadowWrittenCallback,
                       std::move(buffers), serial));
  }
}

void GLES2Implementation::QueryCounterEXT(GLuint id, GLenum target) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] QueryCounterEXT(" << id << ", "
                     << GLES2Util::GetStringQueryTarget(target) << ")");

  switch (target) {
    case GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM:
      break;
    case GL_TIMESTAMP_EXT:
      if (!gl_capabilities_.timer_queries) {
        SetGLError(GL_INVALID_OPERATION, "glQueryCounterEXT",
                   "not enabled for timing queries");
        return;
      }
      break;
    default:
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

  // Extra setups some targets might need.
  switch (target) {
    case GL_TIMESTAMP_EXT:
      if (!query_tracker_->SetDisjointSync(this)) {
        SetGLError(GL_OUT_OF_MEMORY, "glQueryCounterEXT",
                   "buffer allocation failed");
        return;
      }
      break;
    default:
      break;
  }

  if (query_tracker_->QueryCounter(id, target, this))
    CheckGLError();
}

void GLES2Implementation::GetQueryivEXT(GLenum target,
                                        GLenum pname,
                                        GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] GetQueryivEXT("
                     << GLES2Util::GetStringQueryTarget(target) << ", "
                     << GLES2Util::GetStringQueryParameter(pname) << ", "
                     << static_cast<const void*>(params) << ")");
  if (pname == GL_QUERY_COUNTER_BITS_EXT) {
    switch (target) {
      case GL_TIMESTAMP_EXT:
        // Overall reliable driver support for timestamps is limited, so we
        // disable the timestamp portion of this extension to encourage use of
        // the better supported time elapsed queries.
        // TODO(crbug.com/40254878): Check the underlying driver's capability
        // instead of disabling it directly.
        *params = 0;
        break;
      case GL_COMMANDS_ISSUED_TIMESTAMP_CHROMIUM:
      case GL_TIME_ELAPSED_EXT:
        // We convert all queries to CPU time so we support 64 bits.
        *params = 64;
        break;
      default:
        SetGLErrorInvalidEnum("glGetQueryivEXT", target, "target");
        break;
    }
    return;
  } else if (pname != GL_CURRENT_QUERY_EXT) {
    SetGLErrorInvalidEnum("glGetQueryivEXT", pname, "pname");
    return;
  }
  QueryTracker::Query* query = query_tracker_->GetCurrentQuery(target);
  *params = query ? query->id() : 0;
  GPU_CLIENT_LOG("  " << *params);
  CheckGLError();
}

void GLES2Implementation::GetQueryObjectivEXT(GLuint id,
                                              GLenum pname,
                                              GLint* params) {
  GLuint64 result = 0;
  if (GetQueryObjectValueHelper("glGetQueryObjectivEXT", id, pname, &result))
    *params = base::saturated_cast<GLint>(result);
}

void GLES2Implementation::GetQueryObjectuivEXT(GLuint id,
                                               GLenum pname,
                                               GLuint* params) {
  GLuint64 result = 0;
  if (GetQueryObjectValueHelper("glGetQueryObjectuivEXT", id, pname, &result))
    *params = base::saturated_cast<GLuint>(result);
}

void GLES2Implementation::GetQueryObjecti64vEXT(GLuint id,
                                                GLenum pname,
                                                GLint64* params) {
  GLuint64 result = 0;
  if (GetQueryObjectValueHelper("glGetQueryObjectiv64vEXT", id, pname, &result))
    *params = base::saturated_cast<GLint64>(result);
}

void GLES2Implementation::GetQueryObjectui64vEXT(GLuint id,
                                                 GLenum pname,
                                                 GLuint64* params) {
  GLuint64 result = 0;
  if (GetQueryObjectValueHelper("glGetQueryObjectui64vEXT", id, pname, &result))
    *params = result;
}

void GLES2Implementation::SetDisjointValueSyncCHROMIUM() {
  query_tracker_->SetDisjointSync(this);
}

void GLES2Implementation::DrawArraysInstancedANGLE(GLenum mode,
                                                   GLint first,
                                                   GLsizei count,
                                                   GLsizei primcount) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDrawArraysInstancedANGLE("
                     << GLES2Util::GetStringDrawMode(mode) << ", " << first
                     << ", " << count << ", " << primcount << ")");
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawArraysInstancedANGLE", "count < 0");
    return;
  }
  if (primcount < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawArraysInstancedANGLE", "primcount < 0");
    return;
  }
  if (primcount == 0) {
    return;
  }
  bool simulated = false;
  if (vertex_array_object_manager_->SupportsClientSideBuffers()) {
    GLsizei num_elements;
    if (!base::CheckAdd(first, count).AssignIfValid(&num_elements)) {
      SetGLError(GL_INVALID_VALUE, "glDrawArraysInstancedANGLE",
                 "first+count overflow");
      return;
    }
    if (!vertex_array_object_manager_->SetupSimulatedClientSideBuffers(
            "glDrawArraysInstancedANGLE", this, helper_, num_elements,
            primcount, &simulated)) {
      return;
    }
  }
  helper_->DrawArraysInstancedANGLE(mode, first, count, primcount);
  RestoreArrayBuffer(simulated);
  CheckGLError();
}

void GLES2Implementation::DrawArraysInstancedBaseInstanceANGLE(
    GLenum mode,
    GLint first,
    GLsizei count,
    GLsizei primcount,
    GLuint baseinstance) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG(
      "[" << GetLogPrefix() << "] glDrawArraysInstancedBaseInstanceANGLE("
          << GLES2Util::GetStringDrawMode(mode) << ", " << first << ", "
          << count << ", " << primcount << ", " << baseinstance << ")");
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawArraysInstancedBaseInstanceANGLE",
               "count < 0");
    return;
  }
  if (primcount < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawArraysInstancedBaseInstanceANGLE",
               "primcount < 0");
    return;
  }
  if (primcount == 0) {
    return;
  }
  bool simulated = false;
  if (vertex_array_object_manager_->SupportsClientSideBuffers()) {
    GLsizei num_elements;
    if (!base::CheckAdd(first, count).AssignIfValid(&num_elements)) {
      SetGLError(GL_INVALID_VALUE, "glDrawArraysInstancedBaseInstanceANGLE",
                 "first+count overflow");
      return;
    }
    // Client side buffer is not used by WebGL so leave it as is.
    if (!vertex_array_object_manager_->SetupSimulatedClientSideBuffers(
            "glDrawArraysInstancedBaseInstanceANGLE", this, helper_,
            num_elements, primcount, &simulated)) {
      return;
    }
  }
  helper_->DrawArraysInstancedBaseInstanceANGLE(mode, first, count, primcount,
                                                baseinstance);
  RestoreArrayBuffer(simulated);
  CheckGLError();
}

void GLES2Implementation::DrawElementsInstancedANGLE(GLenum mode,
                                                     GLsizei count,
                                                     GLenum type,
                                                     const void* indices,
                                                     GLsizei primcount) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glDrawElementsInstancedANGLE("
                     << GLES2Util::GetStringDrawMode(mode) << ", " << count
                     << ", " << GLES2Util::GetStringIndexType(type) << ", "
                     << static_cast<const void*>(indices) << ", " << primcount
                     << ")");
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawElementsInstancedANGLE",
               "count less than 0.");
    return;
  }
  if (primcount < 0) {
    SetGLError(GL_INVALID_VALUE, "glDrawElementsInstancedANGLE",
               "primcount < 0");
    return;
  }
  GLuint offset = 0;
  bool simulated = false;
  if (count > 0 && primcount > 0) {
    if (vertex_array_object_manager_->bound_element_array_buffer() != 0 &&
        !ValidateOffset("glDrawElementsInstancedANGLE",
                        reinterpret_cast<GLintptr>(indices))) {
      return;
    }
    if (!vertex_array_object_manager_->SetupSimulatedIndexAndClientSideBuffers(
            "glDrawElementsInstancedANGLE", this, helper_, count, type,
            primcount, indices, &offset, &simulated)) {
      return;
    }
  }
  helper_->DrawElementsInstancedANGLE(mode, count, type, offset, primcount);
  RestoreElementAndArrayBuffers(simulated);
  CheckGLError();
}

void GLES2Implementation::DrawElementsInstancedBaseVertexBaseInstanceANGLE(
    GLenum mode,
    GLsizei count,
    GLenum type,
    const void* indices,
    GLsizei primcount,
    GLint basevertex,
    GLuint baseinstance) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] glDrawElementsInstancedBaseVertexBaseInstanceANGLE("
                     << GLES2Util::GetStringDrawMode(mode) << ", " << count
                     << ", " << GLES2Util::GetStringIndexType(type) << ", "
                     << static_cast<const void*>(indices) << ", " << primcount
                     << ", " << basevertex << ", " << baseinstance << ")");
  if (count < 0) {
    SetGLError(GL_INVALID_VALUE,
               "glDrawElementsInstancedBaseVertexBaseInstanceANGLE",
               "count less than 0.");
    return;
  }
  if (primcount < 0) {
    SetGLError(GL_INVALID_VALUE,
               "glDrawElementsInstancedBaseVertexBaseInstanceANGLE",
               "primcount < 0");
    return;
  }
  GLuint offset = 0;
  bool simulated = false;
  if (count > 0 && primcount > 0) {
    if (vertex_array_object_manager_->bound_element_array_buffer() != 0 &&
        !ValidateOffset("glDrawElementsInstancedBaseVertexBaseInstanceANGLE",
                        reinterpret_cast<GLintptr>(indices))) {
      return;
    }
    // Client side buffer is not used by WebGL so leave it as is.
    if (!vertex_array_object_manager_->SetupSimulatedIndexAndClientSideBuffers(
            "glDrawElementsInstancedBaseVertexBaseInstanceANGLE", this, helper_,
            count, type, primcount, indices, &offset, &simulated)) {
      return;
    }
  }
  helper_->DrawElementsInstancedBaseVertexBaseInstanceANGLE(
      mode, count, type, offset, primcount, basevertex, baseinstance);
  RestoreElementAndArrayBuffers(simulated);
  CheckGLError();
}

GLuint GLES2Implementation::CreateAndTexStorage2DSharedImageCHROMIUM(
    const GLbyte* mailbox_data) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix()
                     << "] CreateAndTexStorage2DSharedImageCHROMIUM("
                     << static_cast<const void*>(mailbox_data) << ")");
  const Mailbox& mailbox = *reinterpret_cast<const Mailbox*>(mailbox_data);
  DCHECK(mailbox.Verify()) << "CreateAndTexStorage2DSharedImageCHROMIUM was "
                              "passed an invalid mailbox.";
  GLuint client_id;
  GetIdHandler(SharedIdNamespaces::kTextures)->MakeIds(this, 0, 1, &client_id);
  helper_->CreateAndTexStorage2DSharedImageINTERNALImmediate(client_id,
                                                             mailbox_data);
  if (share_group_->bind_generates_resource())
    helper_->CommandBufferHelper::OrderingBarrier();
  CheckGLError();
  return client_id;
}

void GLES2Implementation::PushGroupMarkerEXT(GLsizei length,
                                             const GLchar* marker) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glPushGroupMarkerEXT(" << length
                     << ", " << marker << ")");
  if (!marker) {
    marker = "";
  }
  SetBucketAsString(kResultBucketId, (length ? std::string(marker, length)
                                             : std::string(marker)));
  helper_->PushGroupMarkerEXT(kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
  debug_marker_manager_.PushGroup(length ? std::string(marker, length)
                                         : std::string(marker));
}

void GLES2Implementation::InsertEventMarkerEXT(GLsizei length,
                                               const GLchar* marker) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glInsertEventMarkerEXT(" << length
                     << ", " << marker << ")");
  if (!marker) {
    marker = "";
  }
  SetBucketAsString(kResultBucketId, (length ? std::string(marker, length)
                                             : std::string(marker)));
  helper_->InsertEventMarkerEXT(kResultBucketId);
  helper_->SetBucketSize(kResultBucketId, 0);
  debug_marker_manager_.SetMarker(length ? std::string(marker, length)
                                         : std::string(marker));
}

void GLES2Implementation::PopGroupMarkerEXT() {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glPopGroupMarkerEXT()");
  helper_->PopGroupMarkerEXT();
  debug_marker_manager_.PopGroup();
}

void GLES2Implementation::TraceBeginCHROMIUM(const char* category_name,
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

void GLES2Implementation::TraceEndCHROMIUM() {
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

void* GLES2Implementation::MapBufferCHROMIUM(GLuint target, GLenum access) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glMapBufferCHROMIUM(" << target
                     << ", " << GLES2Util::GetStringEnum(access) << ")");
  switch (target) {
    case GL_PIXEL_PACK_TRANSFER_BUFFER_CHROMIUM:
      if (access != GL_READ_ONLY) {
        SetGLError(GL_INVALID_ENUM, "glMapBufferCHROMIUM", "bad access mode");
        return nullptr;
      }
      break;
    default:
      SetGLError(GL_INVALID_ENUM, "glMapBufferCHROMIUM", "invalid target");
      return nullptr;
  }
  GLuint buffer_id;
  GetBoundPixelTransferBuffer(target, "glMapBufferCHROMIUM", &buffer_id);
  if (!buffer_id) {
    return nullptr;
  }
  BufferTracker::Buffer* buffer = buffer_tracker_->GetBuffer(buffer_id);
  if (!buffer) {
    SetGLError(GL_INVALID_OPERATION, "glMapBufferCHROMIUM", "invalid buffer");
    return nullptr;
  }
  if (buffer->mapped()) {
    SetGLError(GL_INVALID_OPERATION, "glMapBufferCHROMIUM", "already mapped");
    return nullptr;
  }
  // Here we wait for previous transfer operations to be finished.
  if (buffer->last_usage_token()) {
    helper_->WaitForToken(buffer->last_usage_token());
    buffer->set_last_usage_token(0);
  }
  buffer->set_mapped(true);

  GPU_CLIENT_LOG("  returned " << buffer->address());
  CheckGLError();
  return buffer->address();
}

GLboolean GLES2Implementation::UnmapBufferCHROMIUM(GLuint target) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUnmapBufferCHROMIUM(" << target
                     << ")");
  GLuint buffer_id;
  if (!GetBoundPixelTransferBuffer(target, "glMapBufferCHROMIUM", &buffer_id)) {
    SetGLError(GL_INVALID_ENUM, "glUnmapBufferCHROMIUM", "invalid target");
  }
  if (!buffer_id) {
    return false;
  }
  BufferTracker::Buffer* buffer = buffer_tracker_->GetBuffer(buffer_id);
  if (!buffer) {
    SetGLError(GL_INVALID_OPERATION, "glUnmapBufferCHROMIUM", "invalid buffer");
    return false;
  }
  if (!buffer->mapped()) {
    SetGLError(GL_INVALID_OPERATION, "glUnmapBufferCHROMIUM", "not mapped");
    return false;
  }
  buffer->set_mapped(false);
  CheckGLError();
  return true;
}

uint64_t GLES2Implementation::ShareGroupTracingGUID() const {
  return share_group_->TracingGUID();
}

void GLES2Implementation::SetErrorMessageCallback(
    base::RepeatingCallback<void(const char*, int32_t)> callback) {
  error_message_callback_ = std::move(callback);
}

bool GLES2Implementation::ThreadSafeShallowLockDiscardableTexture(
    uint32_t texture_id) {
  ClientDiscardableTextureManager* manager =
      share_group()->discardable_texture_manager();
  return manager->TextureIsValid(texture_id) &&
         manager->LockTexture(texture_id);
}

void GLES2Implementation::CompleteLockDiscardableTexureOnContextThread(
    uint32_t texture_id) {
  helper_->LockDiscardableTextureCHROMIUM(texture_id);
}

bool GLES2Implementation::ThreadsafeDiscardableTextureIsDeletedForTracing(
    uint32_t texture_id) {
  ClientDiscardableTextureManager* manager =
      share_group()->discardable_texture_manager();
  return manager->TextureIsDeletedForTracing(texture_id);
}

void* GLES2Implementation::MapTransferCacheEntry(uint32_t serialized_size) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void GLES2Implementation::UnmapAndCreateTransferCacheEntry(uint32_t type,
                                                           uint32_t id) {
  NOTREACHED_IN_MIGRATION();
}

bool GLES2Implementation::ThreadsafeLockTransferCacheEntry(uint32_t type,
                                                           uint32_t id) {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void GLES2Implementation::UnlockTransferCacheEntries(
    const std::vector<std::pair<uint32_t, uint32_t>>& entries) {
  NOTREACHED_IN_MIGRATION();
}

void GLES2Implementation::DeleteTransferCacheEntry(uint32_t type, uint32_t id) {
  NOTREACHED_IN_MIGRATION();
}

unsigned int GLES2Implementation::GetTransferBufferFreeSize() const {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

bool GLES2Implementation::IsJpegDecodeAccelerationSupported() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool GLES2Implementation::IsWebPDecodeAccelerationSupported() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool GLES2Implementation::CanDecodeWithHardwareAcceleration(
    const cc::ImageHeaderMetadata* image_metadata) const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool GLES2Implementation::ValidateSize(const char* func, GLsizeiptr size) {
  if (size < 0) {
    SetGLError(GL_INVALID_VALUE, func, "size < 0");
    return false;
  }
  if (!base::IsValueInRangeForNumericType<int32_t>(size)) {
    SetGLError(GL_INVALID_OPERATION, func, "size more than 32-bit");
    return false;
  }
  return true;
}

bool GLES2Implementation::ValidateOffset(const char* func, GLintptr offset) {
  if (offset < 0) {
    SetGLError(GL_INVALID_VALUE, func, "offset < 0");
    return false;
  }
  if (!base::IsValueInRangeForNumericType<int32_t>(offset)) {
    SetGLError(GL_INVALID_OPERATION, func, "offset more than 32-bit");
    return false;
  }
  return true;
}

bool GLES2Implementation::GetSamplerParameterfvHelper(GLuint /* sampler */,
                                                      GLenum /* pname */,
                                                      GLfloat* /* params */) {
  // TODO(zmo): Implement client side caching.
  return false;
}

bool GLES2Implementation::GetSamplerParameterivHelper(GLuint /* sampler */,
                                                      GLenum /* pname */,
                                                      GLint* /* params */) {
  // TODO(zmo): Implement client side caching.
  return false;
}

bool GLES2Implementation::PackStringsToBucket(GLsizei count,
                                              const char* const* str,
                                              const GLint* length,
                                              const char* func_name) {
  DCHECK_LE(0, count);
  // Compute the total size.
  base::CheckedNumeric<uint32_t> total_size = count;
  total_size += 1;
  total_size *= sizeof(GLint);
  uint32_t header_size = 0;
  if (!total_size.AssignIfValid(&header_size)) {
    SetGLError(GL_INVALID_VALUE, func_name, "overflow");
    return false;
  }
  std::vector<GLint> header(count + 1);
  header[0] = static_cast<GLint>(count);
  for (GLsizei ii = 0; ii < count; ++ii) {
    GLint len = 0;
    if (str[ii]) {
      len = (length && length[ii] >= 0)
                ? length[ii]
                : base::checked_cast<GLint>(strlen(str[ii]));
    }
    total_size += len;
    total_size += 1;  // NULL at the end of each char array.
    header[ii + 1] = len;
  }
  // Pack data into a bucket on the service.
  uint32_t validated_size = 0;
  if (!total_size.AssignIfValid(&validated_size)) {
    SetGLError(GL_INVALID_VALUE, func_name, "overflow");
    return false;
  }
  helper_->SetBucketSize(kResultBucketId, validated_size);
  uint32_t offset = 0;
  for (GLsizei ii = 0; ii <= count; ++ii) {
    const char* src =
        (ii == 0) ? reinterpret_cast<const char*>(&header[0]) : str[ii - 1];
    uint32_t size = (ii == 0) ? header_size : header[ii];
    if (ii > 0) {
      size += 1;  // NULL in the end.
    }
    while (size) {
      ScopedTransferBufferPtr buffer(size, helper_, transfer_buffer_);
      if (!buffer.valid() || buffer.size() == 0) {
        SetGLError(GL_OUT_OF_MEMORY, func_name, "too large");
        return false;
      }
      uint32_t copy_size = buffer.size();
      if (ii > 0 && buffer.size() == size)
        --copy_size;
      if (copy_size)
        memcpy(buffer.address(), src, copy_size);
      if (copy_size < buffer.size()) {
        // Append NULL in the end.
        DCHECK(copy_size + 1 == buffer.size());
        reinterpret_cast<char*>(buffer.address())[copy_size] = 0;
      }
      helper_->SetBucketData(kResultBucketId, offset, buffer.size(),
                             buffer.shm_id(), buffer.offset());
      offset += buffer.size();
      src += buffer.size();
      size -= buffer.size();
    }
  }
  DCHECK_EQ(total_size.ValueOrDefault(0), offset);
  return true;
}

void GLES2Implementation::UniformBlockBinding(GLuint program,
                                              GLuint index,
                                              GLuint binding) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glUniformBlockBinding(" << program
                     << ", " << index << ", " << binding << ")");
  share_group_->program_info_manager()->UniformBlockBinding(this, program,
                                                            index, binding);
  helper_->UniformBlockBinding(program, index, binding);
  CheckGLError();
}

GLenum GLES2Implementation::ClientWaitSync(GLsync sync,
                                           GLbitfield flags,
                                           GLuint64 timeout) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glClientWaitSync(" << sync << ", "
                     << flags << ", " << timeout << ")");
  typedef cmds::ClientWaitSync::Result Result;
  // Limit scope of result to avoid overlap with CheckGLError()
  Result localResult;
  {
    auto result = GetResultAs<Result>();
    if (!result) {
      SetGLError(GL_OUT_OF_MEMORY, "ClientWaitSync", "");
      return GL_WAIT_FAILED;
    }
    *result = GL_WAIT_FAILED;
    helper_->ClientWaitSync(ToGLuint(sync), flags, timeout, GetResultShmId(),
                            result.offset());
    if (!WaitForCmd()) {
      return GL_WAIT_FAILED;
    }
    localResult = *result;
    GPU_CLIENT_LOG("returned " << localResult);
  }
  CheckGLError();
  return localResult;
}

void GLES2Implementation::CopyBufferSubData(GLenum readtarget,
                                            GLenum writetarget,
                                            GLintptr readoffset,
                                            GLintptr writeoffset,
                                            GLsizeiptr size) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glCopyBufferSubData("
                     << GLES2Util::GetStringBufferTarget(readtarget) << ", "
                     << GLES2Util::GetStringBufferTarget(writetarget) << ", "
                     << readoffset << ", " << writeoffset << ", " << size
                     << ")");
  if (readoffset < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyBufferSubData", "readoffset < 0");
    return;
  }
  if (writeoffset < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyBufferSubData", "writeoffset < 0");
    return;
  }
  if (size < 0) {
    SetGLError(GL_INVALID_VALUE, "glCopyBufferSubData", "size < 0");
    return;
  }
  helper_->CopyBufferSubData(readtarget, writetarget, readoffset, writeoffset,
                             size);
  InvalidateReadbackBufferShadowDataCHROMIUM(GetBoundBufferHelper(writetarget));
  CheckGLError();
}

void GLES2Implementation::WaitSync(GLsync sync,
                                   GLbitfield flags,
                                   GLuint64 timeout) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glWaitSync(" << sync << ", "
                     << flags << ", " << timeout << ")");
  helper_->WaitSync(ToGLuint(sync), flags, timeout);
  CheckGLError();
}

void GLES2Implementation::GetInternalformativ(GLenum target,
                                              GLenum format,
                                              GLenum pname,
                                              GLsizei buf_size,
                                              GLint* params) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_VALIDATE_DESTINATION_INITALIZATION(GLint, params);
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glGetInternalformativ("
                     << GLES2Util::GetStringRenderBufferTarget(target) << ", "
                     << GLES2Util::GetStringRenderBufferFormat(format) << ", "
                     << GLES2Util::GetStringInternalFormatParameter(pname)
                     << ", " << buf_size << ", "
                     << static_cast<const void*>(params) << ")");
  if (buf_size < 0) {
    SetGLError(GL_INVALID_VALUE, "glGetInternalformativ", "bufSize < 0");
    return;
  }
  TRACE_EVENT0("gpu", "GLES2Implementation::GetInternalformativ");
  if (GetInternalformativHelper(target, format, pname, buf_size, params)) {
    return;
  }
  typedef cmds::GetInternalformativ::Result Result;
  // Limit scope of result to avoid overlap with CheckGLError()
  {
    auto result = GetResultAs<Result>();
    if (!result) {
      return;
    }
    result->SetNumResults(0);
    helper_->GetInternalformativ(target, format, pname, GetResultShmId(),
                                 result.offset());
    if (!WaitForCmd()) {
      return;
    }
    GPU_CLIENT_LOG_CODE_BLOCK({
      for (int32_t i = 0; i < result->GetNumResults(); ++i) {
        GPU_CLIENT_LOG("  " << i << ": " << result->GetData()[i]);
      }
    });
    if (buf_size > 0 && params) {
      GLint* data = result->GetData();
      if (buf_size >= result->GetNumResults()) {
        buf_size = result->GetNumResults();
      }
      for (GLsizei ii = 0; ii < buf_size; ++ii) {
        params[ii] = data[ii];
      }
    }
  }
  CheckGLError();
}

void GLES2Implementation::InitializeDiscardableTextureCHROMIUM(
    GLuint texture_id) {
  ClientDiscardableTextureManager* manager =
      share_group()->discardable_texture_manager();
  if (manager->TextureIsValid(texture_id)) {
    SetGLError(GL_INVALID_VALUE, "glInitializeDiscardableTextureCHROMIUM",
               "Texture ID already initialized");
    return;
  }
  ClientDiscardableHandle handle =
      manager->InitializeTexture(helper_->command_buffer(), texture_id);
  if (!handle.IsValid())
    return;

  helper_->InitializeDiscardableTextureCHROMIUM(texture_id, handle.shm_id(),
                                                handle.byte_offset());
}

void GLES2Implementation::UnlockDiscardableTextureCHROMIUM(GLuint texture_id) {
  ClientDiscardableTextureManager* manager =
      share_group()->discardable_texture_manager();
  if (!manager->TextureIsValid(texture_id)) {
    SetGLError(GL_INVALID_VALUE, "glUnlockDiscardableTextureCHROMIUM",
               "Texture ID not initialized");
    return;
  }

  // |should_unbind_texture| will be set to true if the texture has been fully
  // unlocked. In this case, ensure the texture is unbound.
  bool should_unbind_texture = false;
  manager->UnlockTexture(texture_id, &should_unbind_texture);
  if (should_unbind_texture)
    UnbindTexturesHelper(1, &texture_id);

  helper_->UnlockDiscardableTextureCHROMIUM(texture_id);
}

bool GLES2Implementation::LockDiscardableTextureCHROMIUM(GLuint texture_id) {
  ClientDiscardableTextureManager* manager =
      share_group()->discardable_texture_manager();
  if (!manager->TextureIsValid(texture_id)) {
    SetGLError(GL_INVALID_VALUE, "glLockDiscardableTextureCHROMIUM",
               "Texture ID not initialized");
    return false;
  }
  if (!manager->LockTexture(texture_id)) {
    // Failure to lock means that this texture has been deleted on the service
    // side. Delete it here as well.
    DeleteTexturesHelper(1, &texture_id);
    return false;
  }
  helper_->LockDiscardableTextureCHROMIUM(texture_id);
  return true;
}

void GLES2Implementation::UpdateCachedExtensionsIfNeeded() {
  if (cached_extension_string_) {
    return;
  }
  GetStringHelper(GL_EXTENSIONS);
}

void GLES2Implementation::InvalidateCachedExtensions() {
  cached_extension_string_ = nullptr;
  cached_extensions_.clear();
}

void GLES2Implementation::Viewport(GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height) {
  GPU_CLIENT_SINGLE_THREAD_CHECK();
  GPU_CLIENT_LOG("[" << GetLogPrefix() << "] glViewport(" << x << ", " << y
                     << ", " << width << ", " << height << ")");
  if (width < 0 || height < 0) {
    SetGLError(GL_INVALID_VALUE, "glViewport", "negative width/height");
    return;
  }
  state_.SetViewport(x, y, width, height);
  helper_->Viewport(x, y, width, height);
  CheckGLError();
}

void GLES2Implementation::IssueBeginQuery(GLenum target,
                                          GLuint id,
                                          uint32_t sync_data_shm_id,
                                          uint32_t sync_data_shm_offset) {
  helper_->BeginQueryEXT(target, id, sync_data_shm_id, sync_data_shm_offset);
}

void GLES2Implementation::IssueEndQuery(GLenum target, GLuint submit_count) {
  helper_->EndQueryEXT(target, submit_count);
}

void GLES2Implementation::IssueQueryCounter(GLuint id,
                                            GLenum target,
                                            uint32_t sync_data_shm_id,
                                            uint32_t sync_data_shm_offset,
                                            GLuint submit_count) {
  helper_->QueryCounterEXT(id, target, sync_data_shm_id, sync_data_shm_offset,
                           submit_count);
}

void GLES2Implementation::IssueSetDisjointValueSync(
    uint32_t sync_data_shm_id,
    uint32_t sync_data_shm_offset) {
  helper_->SetDisjointValueSyncCHROMIUM(sync_data_shm_id, sync_data_shm_offset);
}

GLenum GLES2Implementation::GetClientSideGLError() {
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

CommandBufferHelper* GLES2Implementation::cmd_buffer_helper() {
  return helper_;
}

CommandBuffer* GLES2Implementation::command_buffer() const {
  return helper_->command_buffer();
}

void GLES2Implementation::SetActiveURLCHROMIUM(const char* url) {
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

// Include the auto-generated part of this file. We split this because it means
// we can easily edit the non-auto generated parts right here in this file
// instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/gles2_implementation_impl_autogen.h"

}  // namespace gles2
}  // namespace gpu
