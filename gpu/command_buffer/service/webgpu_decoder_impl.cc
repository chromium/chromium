// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/webgpu_decoder_impl.h"

#include <dawn/native/DawnNative.h>
#include <dawn/native/OpenGLBackend.h>
#include <dawn/platform/DawnPlatform.h>
#include <dawn/webgpu_cpp.h>
#include <dawn/wire/WireServer.h>

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/bits.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/numerics/checked_math.h"
#include "base/power_monitor/power_monitor.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/dawn_caching_interface.h"
#include "gpu/command_buffer/service/dawn_instance.h"
#include "gpu/command_buffer/service/dawn_platform.h"
#include "gpu/command_buffer/service/dawn_service_memory_transfer_service.h"
#include "gpu/command_buffer/service/dawn_service_serializer.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/isolation_key_provider.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/webgpu_blocklist.h"
#include "gpu/webgpu/callback.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/SkSurfaceGanesh.h"
#include "ui/gl/gl_context_egl.h"
#include "ui/gl/gl_surface_egl.h"

#if BUILDFLAG(IS_WIN)
#include <dawn/native/D3D11Backend.h>
#include <dawn/native/D3D12Backend.h>
#include "ui/gl/gl_angle_util_win.h"
#endif

namespace gpu {
namespace webgpu {

namespace {

constexpr wgpu::TextureUsage kAllowedWritableMailboxTextureUsages =
    wgpu::TextureUsage::CopyDst | wgpu::TextureUsage::RenderAttachment |
    wgpu::TextureUsage::StorageBinding;

constexpr wgpu::TextureUsage kAllowedReadableMailboxTextureUsages =
    wgpu::TextureUsage::CopySrc | wgpu::TextureUsage::TextureBinding;

constexpr wgpu::TextureUsage kAllowedMailboxTextureUsages =
    kAllowedWritableMailboxTextureUsages | kAllowedReadableMailboxTextureUsages;

template <typename T1, typename T2>
void ChainStruct(T1& head, T2* struct_to_chain) {
  DCHECK(struct_to_chain->nextInChain == nullptr);
  struct_to_chain->nextInChain = head.nextInChain;
  head.nextInChain = struct_to_chain;
}

class WebGPUDecoderImpl final : public WebGPUDecoder {
 public:
  WebGPUDecoderImpl(
      DecoderClient* client,
      CommandBufferServiceBase* command_buffer_service,
      SharedImageManager* shared_image_manager,
      MemoryTracker* memory_tracker,
      gles2::Outputter* outputter,
      const GpuPreferences& gpu_preferences,
      scoped_refptr<SharedContextState> shared_context_state,
      std::unique_ptr<DawnCachingInterface> dawn_caching_interface_factory,
      IsolationKeyProvider* isolation_key_provider);

  WebGPUDecoderImpl(const WebGPUDecoderImpl&) = delete;
  WebGPUDecoderImpl& operator=(const WebGPUDecoderImpl&) = delete;

  ~WebGPUDecoderImpl() override;

  // WebGPUDecoder implementation
  ContextResult Initialize(const GpuFeatureInfo& gpu_feature_info) override;

  // DecoderContext implementation.
  base::WeakPtr<DecoderContext> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }
  const gles2::ContextState* GetContextState() override {
    NOTREACHED();
    return nullptr;
  }
  void Destroy(bool have_context) override;
  bool MakeCurrent() override {
    if (gl_context_.get()) {
      gl_context_->MakeCurrent(gl_surface_.get());
    }
    return true;
  }
  gl::GLContext* GetGLContext() override { return nullptr; }
  gl::GLSurface* GetGLSurface() override {
    NOTREACHED();
    return nullptr;
  }
  const gles2::FeatureInfo* GetFeatureInfo() const override {
    NOTREACHED();
    return nullptr;
  }
  Capabilities GetCapabilities() override { return {}; }
  void RestoreGlobalState() const override { NOTREACHED(); }
  void ClearAllAttributes() const override { NOTREACHED(); }
  void RestoreAllAttributes() const override { NOTREACHED(); }
  void RestoreState(const gles2::ContextState* prev_state) override {
    NOTREACHED();
  }
  void RestoreActiveTexture() const override { NOTREACHED(); }
  void RestoreAllTextureUnitAndSamplerBindings(
      const gles2::ContextState* prev_state) const override {
    NOTREACHED();
  }
  void RestoreActiveTextureUnitBinding(unsigned int target) const override {
    NOTREACHED();
  }
  void RestoreBufferBinding(unsigned int target) override { NOTREACHED(); }
  void RestoreBufferBindings() const override { NOTREACHED(); }
  void RestoreFramebufferBindings() const override { NOTREACHED(); }
  void RestoreRenderbufferBindings() override { NOTREACHED(); }
  void RestoreProgramBindings() const override { NOTREACHED(); }
  void RestoreTextureState(unsigned service_id) override { NOTREACHED(); }
  void RestoreTextureUnitBindings(unsigned unit) const override {
    NOTREACHED();
  }
  void RestoreVertexAttribArray(unsigned index) override { NOTREACHED(); }
  void RestoreAllExternalTextureBindingsIfNeeded() override { NOTREACHED(); }
  QueryManager* GetQueryManager() override {
    NOTREACHED();
    return nullptr;
  }
  void SetQueryCallback(unsigned int query_client_id,
                        base::OnceClosure callback) override {
    NOTREACHED();
  }
  void CancelAllQueries() override { NOTREACHED(); }
  gles2::GpuFenceManager* GetGpuFenceManager() override {
    NOTREACHED();
    return nullptr;
  }
  bool HasPendingQueries() const override { return false; }
  void ProcessPendingQueries(bool did_finish) override {}
  bool HasMoreIdleWork() const override { return false; }
  void PerformIdleWork() override {}

  bool HasPollingWork() const override {
    return has_polling_work_ || wire_serializer_->NeedsFlush();
  }

  void PerformPollingWork() override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "WebGPUDecoderImpl::PerformPollingWork");
    has_polling_work_ = false;
    if (known_device_metadata_.empty()) {
      wire_serializer_->Flush();
      return;
    }

    // Call DeviceTick() on all known devices, and prune any that are no longer
    // on the wire and do not need a tick.
    for (auto it = known_device_metadata_.begin();
         it != known_device_metadata_.end();) {
      auto& device = it->first;
      const bool known = wire_server_->IsDeviceKnown(device.Get());
      const bool needs_tick = dawn::native::DeviceTick(device.Get());
      if (needs_tick) {
        has_polling_work_ = true;
      }
      if (!known) {
        // The client has dropped all references and the device has been
        // removed from the wire.
        // Release the device and erase it from the map.
        it = known_device_metadata_.erase(it);
      } else {
        ++it;
      }
    }
    wire_serializer_->Flush();
  }

  TextureBase* GetTextureBase(uint32_t client_id) override {
    NOTREACHED();
    return nullptr;
  }
  void SetLevelInfo(uint32_t client_id,
                    int level,
                    unsigned internal_format,
                    unsigned width,
                    unsigned height,
                    unsigned depth,
                    unsigned format,
                    unsigned type,
                    const gfx::Rect& cleared_rect) override {
    NOTREACHED();
  }
  bool WasContextLost() const override {
    NOTIMPLEMENTED();
    return false;
  }
  bool WasContextLostByRobustnessExtension() const override { return false; }
  void MarkContextLost(error::ContextLostReason reason) override {
    NOTIMPLEMENTED();
  }
  bool CheckResetStatus() override {
    NOTREACHED();
    return false;
  }
  void BeginDecoding() override {}
  void EndDecoding() override {}
  const char* GetCommandName(unsigned int command_id) const;
  error::Error DoCommands(unsigned int num_commands,
                          const volatile void* buffer,
                          int num_entries,
                          int* entries_processed) override;
  base::StringPiece GetLogPrefix() override { return "WebGPUDecoderImpl"; }
  gles2::ContextGroup* GetContextGroup() override { return nullptr; }
  gles2::ErrorState* GetErrorState() override {
    NOTREACHED();
    return nullptr;
  }
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<gles2::AbstractTexture> CreateAbstractTexture(
      GLenum target,
      GLenum internal_format,
      GLsizei width,
      GLsizei height,
      GLsizei depth,
      GLint border,
      GLenum format,
      GLenum type) override {
    NOTREACHED();
    return nullptr;
  }
#endif
  bool IsCompressedTextureFormat(unsigned format) override {
    NOTREACHED();
    return false;
  }
  bool ClearLevel(gles2::Texture* texture,
                  unsigned target,
                  int level,
                  unsigned format,
                  unsigned type,
                  int xoffset,
                  int yoffset,
                  int width,
                  int height) override {
    NOTREACHED();
    return false;
  }
  bool ClearCompressedTextureLevel(gles2::Texture* texture,
                                   unsigned target,
                                   int level,
                                   unsigned format,
                                   int width,
                                   int height) override {
    NOTREACHED();
    return false;
  }
  bool ClearCompressedTextureLevel3D(gles2::Texture* texture,
                                     unsigned target,
                                     int level,
                                     unsigned format,
                                     int width,
                                     int height,
                                     int depth) override {
    NOTREACHED();
    return false;
  }
  bool ClearLevel3D(gles2::Texture* texture,
                    unsigned target,
                    int level,
                    unsigned format,
                    unsigned type,
                    int width,
                    int height,
                    int depth) override {
    NOTREACHED();
    return false;
  }
  bool initialized() const override { return true; }
  void SetLogCommands(bool log_commands) override { NOTIMPLEMENTED(); }
  gles2::Outputter* outputter() const override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  int GetRasterDecoderId() const override {
    NOTREACHED();
    return -1;
  }

 private:
  typedef error::Error (WebGPUDecoderImpl::*CmdHandler)(
      uint32_t immediate_data_size,
      const volatile void* data);

  // A struct to hold info about each command.
  struct CommandInfo {
    CmdHandler cmd_handler;
    uint8_t arg_flags;   // How to handle the arguments for this command
    uint8_t cmd_flags;   // How to handle this command
    uint16_t arg_count;  // How many arguments are expected for this command.
  };

  // Since requesting a device may be a deferred operation depending on whether
  // an isolation key has been assigned from the browser process asynchronously,
  // define storable callbacks that invoke device creation to be scheduled
  // later. The argument to the callback should be true iff the device request
  // should be executed. Passing false will effectively cancel the request with
  // RequestDeviceStatus_Unknown. Cancelling is used on destroy to ensure that
  // all callbacks are resolved.
  using QueuedRequestDeviceCallback = base::OnceCallback<void(bool)>;

  // A table of CommandInfo for all the commands.
  static const CommandInfo command_info[kNumCommands - kFirstWebGPUCommand];

// Generate a member function prototype for each command in an automated and
// typesafe way.
#define WEBGPU_CMD_OP(name) \
  Error Handle##name(uint32_t immediate_data_size, const volatile void* data);
  WEBGPU_COMMAND_LIST(WEBGPU_CMD_OP)
#undef WEBGPU_CMD_OP

  // The current decoder error communicates the decoder error through command
  // processing functions that do not return the error value. Should be set
  // only if not returning an error.
  error::Error current_decoder_error_ = error::kNoError;

  wgpu::Adapter CreatePreferredAdapter(wgpu::PowerPreference power_preference,
                                       bool force_fallback,
                                       bool compatibility_mode) const;

  // Decide if a device feature is exposed to render process.
  bool IsFeatureExposed(wgpu::FeatureName feature) const;

  // Dawn wire uses procs which forward their calls to these methods.
  void RequestAdapterImpl(WGPUInstance instance,
                          const WGPURequestAdapterOptions* options,
                          WGPURequestAdapterCallback callback,
                          void* userdata);
  WGPUBool AdapterHasFeatureImpl(WGPUAdapter adapter, WGPUFeatureName feature);
  size_t AdapterEnumerateFeaturesImpl(WGPUAdapter adapter,
                                      WGPUFeatureName* features);
  void RequestDeviceImpl(WGPUAdapter adapter,
                         const WGPUDeviceDescriptor* descriptor,
                         WGPURequestDeviceCallback callback,
                         void* userdata);

  QueuedRequestDeviceCallback CreateQueuedRequestDeviceCallback(
      const wgpu::Adapter& adapter,
      const WGPUDeviceDescriptor* descriptor,
      WGPURequestDeviceCallback callback,
      void* userdata);

  class SharedImageRepresentationAndAccess;

  std::unique_ptr<SharedImageRepresentationAndAccess> AssociateMailboxDawn(
      const Mailbox& mailbox,
      MailboxFlags flags,
      const wgpu::Device& device,
      wgpu::BackendType backendType,
      wgpu::TextureUsage usage,
      std::vector<wgpu::TextureFormat> view_formats);

  std::unique_ptr<SharedImageRepresentationAndAccess>
  AssociateMailboxUsingSkiaFallback(
      const Mailbox& mailbox,
      MailboxFlags flags,
      const wgpu::Device& device,
      wgpu::TextureUsage usage,
      std::vector<wgpu::TextureFormat> view_formats);

  // Device creation requires that an isolation key has been set for the
  // decoder. As a result, this callback also runs all queued device creation
  // calls that were requested and queued before the isolation key was ready.
  void OnGetIsolationKey(const std::string& isolation_key);

  bool use_blocklist() const;

  scoped_refptr<SharedContextState> shared_context_state_;
  const GrContextType gr_context_type_;

  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;

  std::unique_ptr<dawn::platform::Platform> dawn_platform_;
  std::unique_ptr<DawnInstance> dawn_instance_;
  std::unique_ptr<DawnServiceMemoryTransferService> memory_transfer_service_;

  bool enable_unsafe_webgpu_ = false;
  WebGPUAdapterName use_webgpu_adapter_ = WebGPUAdapterName::kDefault;
  WebGPUPowerPreference use_webgpu_power_preference_ =
      WebGPUPowerPreference::kNone;
  bool force_webgpu_compat_ = false;
  std::vector<std::string> require_enabled_toggles_;
  std::vector<std::string> require_disabled_toggles_;
  bool allow_unsafe_apis_;
  bool tiered_adapter_limits_;

  // Isolation key that is necessary for device requests. Optional to
  // differentiate between an empty isolation key, and an unset one.
  absl::optional<std::string> isolation_key_;

  std::unique_ptr<dawn::wire::WireServer> wire_server_;
  std::unique_ptr<DawnServiceSerializer> wire_serializer_;

  // Raw pointer to the isolation key provider because the provider must outlive
  // the decoder. Currently, the only implementation of the provider is
  // GpuChannel which is required to outlive the decoder.
  raw_ptr<IsolationKeyProvider> isolation_key_provider_;

  // A queue of request device calls that were deferred because the decoder had
  // yet to receive a notification from the browser process regarding the
  // isolation key to use for devices created via this decoder.
  std::vector<QueuedRequestDeviceCallback> queued_request_device_calls_;

  // Helper class whose derived implementations holds a representation
  // and its ScopedAccess, ensuring safe destruction order.
  class SharedImageRepresentationAndAccess {
   public:
    virtual ~SharedImageRepresentationAndAccess() = default;
    // Get an unowned reference to the wgpu::Texture for the shared image.
    virtual wgpu::Texture texture() const = 0;
  };

  // Wraps a |DawnImageRepresentation| as a wgpu::Texture.
  class SharedImageRepresentationAndAccessDawn
      : public SharedImageRepresentationAndAccess {
   public:
    SharedImageRepresentationAndAccessDawn(
        std::unique_ptr<DawnImageRepresentation> representation,
        std::unique_ptr<DawnImageRepresentation::ScopedAccess> access)
        : representation_(std::move(representation)),
          access_(std::move(access)) {}

    wgpu::Texture texture() const override { return access_->texture(); }

   private:
    std::unique_ptr<DawnImageRepresentation> representation_;
    std::unique_ptr<DawnImageRepresentation::ScopedAccess> access_;
  };

  // Wraps a |SkiaImageRepresentation| and exposes
  // it as a wgpu::Texture by performing CPU readbacks/uploads.
  class SharedImageRepresentationAndAccessSkiaFallback
      : public SharedImageRepresentationAndAccess {
   public:
    static std::unique_ptr<SharedImageRepresentationAndAccessSkiaFallback>
    Create(scoped_refptr<SharedContextState> shared_context_state,
           std::unique_ptr<SkiaImageRepresentation> representation,
           const wgpu::Device& device,
           wgpu::TextureUsage usage,
           std::vector<wgpu::TextureFormat> view_formats) {
      viz::SharedImageFormat format = representation->format();
      // Include list of formats this is tested to work with.
      // See gpu/command_buffer/tests/webgpu_mailbox_unittest.cc
      if (format != viz::SinglePlaneFormat::kBGRA_8888 &&
// TODO(crbug.com/1241369): Handle additional formats.
#if !BUILDFLAG(IS_MAC)
          format != viz::SinglePlaneFormat::kRGBA_8888 &&
#endif
          format != viz::SinglePlaneFormat::kRGBA_F16) {
        return nullptr;
      }

      // Make sure we can create a WebGPU texture for this format
      if (ToDawnFormat(format) == wgpu::TextureFormat::Undefined) {
        return nullptr;
      }

      const bool is_initialized = representation->IsCleared();
      auto result =
          base::WrapUnique(new SharedImageRepresentationAndAccessSkiaFallback(
              std::move(shared_context_state), std::move(representation),
              device, usage, std::move(view_formats)));
      if (is_initialized && !result->PopulateFromSkia()) {
        return nullptr;
      }
      return result;
    }

    ~SharedImageRepresentationAndAccessSkiaFallback() override {
      // If we have write access, flush any writes by uploading
      // into the SkSurface.
      if ((usage_ & kAllowedWritableMailboxTextureUsages) != 0) {
        // Before using the shared context, ensure it is current if we're on GL.
        if (shared_context_state_->GrContextIsGL()) {
          shared_context_state_->MakeCurrent(/* gl_surface */ nullptr);
        }
        if (UploadContentsToSkia()) {
          // Upload to skia was successful. Mark the contents as initialized.
          representation_->SetCleared();
        } else {
          DLOG(ERROR) << "Failed to write to SkSurface.";
        }
      }

      texture_.Destroy();
    }

    wgpu::Texture texture() const override { return texture_.Get(); }

   private:
    SharedImageRepresentationAndAccessSkiaFallback(
        scoped_refptr<SharedContextState> shared_context_state,
        std::unique_ptr<SkiaImageRepresentation> representation,
        const wgpu::Device& device,
        wgpu::TextureUsage usage,
        std::vector<wgpu::TextureFormat> view_formats)
        : shared_context_state_(std::move(shared_context_state)),
          representation_(std::move(representation)),
          device_(device),
          usage_(usage) {
      // Create a wgpu::Texture to hold the image contents.
      // It should be internally copyable so Chrome can internally perform
      // copies with it, but Javascript cannot (unless |usage| contains copy
      // src/dst).
      // We also need RenderAttachment usage for clears, and TextureBinding for
      // copyTextureForBrowser.
      wgpu::DawnTextureInternalUsageDescriptor internal_usage_desc;
      internal_usage_desc.internalUsage = wgpu::TextureUsage::CopyDst |
                                          wgpu::TextureUsage::CopySrc |
                                          wgpu::TextureUsage::RenderAttachment |
                                          wgpu::TextureUsage::TextureBinding;
      wgpu::TextureDescriptor texture_desc = {
          .nextInChain = &internal_usage_desc,
          .usage = usage,
          .dimension = wgpu::TextureDimension::e2D,
          .size = {static_cast<uint32_t>(representation_->size().width()),
                   static_cast<uint32_t>(representation_->size().height()), 1},
          .format = ToDawnFormat(representation_->format()),
          .mipLevelCount = 1,
          .sampleCount = 1,
          .viewFormatCount = view_formats.size(),
          .viewFormats =
              reinterpret_cast<wgpu::TextureFormat*>(view_formats.data()),
      };

      texture_ = device.CreateTexture(&texture_desc);
      DCHECK(texture_);
    }

    bool ComputeStagingBufferParams(viz::SharedImageFormat format,
                                    const gfx::Size& size,
                                    uint32_t* bytes_per_row,
                                    size_t* buffer_size) const {
      DCHECK(bytes_per_row);
      DCHECK(buffer_size);

      base::CheckedNumeric<uint32_t> checked_bytes_per_row(
          format.BitsPerPixel() / 8);
      checked_bytes_per_row *= size.width();

      uint32_t packed_bytes_per_row;
      if (!checked_bytes_per_row.AssignIfValid(&packed_bytes_per_row)) {
        return false;
      }

      // Align up to 256, required by WebGPU buffer->texture and texture->buffer
      // copies.
      checked_bytes_per_row =
          base::bits::AlignUp(packed_bytes_per_row, uint32_t{256});
      if (!checked_bytes_per_row.AssignIfValid(bytes_per_row)) {
        return false;
      }
      if (*bytes_per_row < packed_bytes_per_row) {
        // Overflow in AlignUp.
        return false;
      }

      base::CheckedNumeric<size_t> checked_buffer_size = checked_bytes_per_row;
      checked_buffer_size *= size.height();

      return checked_buffer_size.AssignIfValid(buffer_size);
    }

    bool ReadPixelsIntoBuffer(void* dst_pointer, uint32_t bytes_per_row) {
      DCHECK(dst_pointer);
      std::vector<GrBackendSemaphore> begin_semaphores;
      std::vector<GrBackendSemaphore> end_semaphores;
      auto scoped_read_access = representation_->BeginScopedReadAccess(
          &begin_semaphores, &end_semaphores);
      if (!scoped_read_access) {
        DLOG(ERROR) << "PopulateFromSkia: Couldn't begin shared image access";
        return false;
      }

      // Wait for any work that previously used the image.
      WaitForSemaphores(std::move(begin_semaphores));

      // Success status will be stored here instead of returning early
      // because proper cleanup up the read access must be done at the
      // end of the function.
      bool success = true;

      // Make an SkImage to read the image contents
      auto sk_image =
          scoped_read_access->CreateSkImage(shared_context_state_.get());
      if (!sk_image) {
        DLOG(ERROR) << "Couldn't make SkImage";
        // Don't return early so we can perform proper cleanup later.
        success = false;
      }

      // Read back the Skia image contents into the staging buffer.
      DCHECK(dst_pointer);
      if (success && !sk_image->readPixels(shared_context_state_->gr_context(),
                                           sk_image->imageInfo(), dst_pointer,
                                           bytes_per_row, 0, 0)) {
        DLOG(ERROR) << "Failed to read from SkImage";
        success = false;
      }

      // Transition the image back to the desired end state. This is used
      // for transitioning the image to the external queue for Vulkan/GL
      // interop.
      scoped_read_access->ApplyBackendSurfaceEndState();
      // Signal the semaphores.
      SignalSemaphores(std::move(end_semaphores));
      return success;
    }

    bool PopulateFromSkia() {
      uint32_t bytes_per_row;
      size_t buffer_size;
      if (!ComputeStagingBufferParams(representation_->format(),
                                      representation_->size(), &bytes_per_row,
                                      &buffer_size)) {
        return false;
      }

      // Create a staging buffer to hold pixel data which will be uploaded into
      // a texture.
      wgpu::BufferDescriptor buffer_desc = {
          .usage = wgpu::BufferUsage::CopySrc,
          .size = buffer_size,
          .mappedAtCreation = true,
      };
      wgpu::Buffer buffer = device_.CreateBuffer(&buffer_desc);
      void* dst_pointer = buffer.GetMappedRange(0, wgpu::kWholeMapSize);

      if (!ReadPixelsIntoBuffer(dst_pointer, bytes_per_row)) {
        return false;
      }
      // Unmap the buffer.
      buffer.Unmap();

      // Copy from the staging WGPUBuffer into the wgpu::Texture.
      wgpu::DawnEncoderInternalUsageDescriptor internal_usage_desc;
      internal_usage_desc.useInternalUsages = true;
      wgpu::CommandEncoderDescriptor command_encoder_desc = {
          .nextInChain = &internal_usage_desc,
      };

      wgpu::CommandEncoder encoder =
          device_.CreateCommandEncoder(&command_encoder_desc);
      wgpu::ImageCopyBuffer buffer_copy = {
          .layout =
              {
                  .bytesPerRow = bytes_per_row,
                  .rowsPerImage = wgpu::kCopyStrideUndefined,
              },
          .buffer = buffer.Get(),
      };
      wgpu::ImageCopyTexture texture_copy = {
          .texture = texture_,
      };
      wgpu::Extent3D extent = {
          static_cast<uint32_t>(representation_->size().width()),
          static_cast<uint32_t>(representation_->size().height()), 1};
      encoder.CopyBufferToTexture(&buffer_copy, &texture_copy, &extent);
      wgpu::CommandBuffer commandBuffer = encoder.Finish();

      wgpu::Queue queue = device_.GetQueue();
      queue.Submit(1, &commandBuffer);

      return true;
    }

    bool UploadContentsToSkia() {
      uint32_t bytes_per_row;
      size_t buffer_size;
      if (!ComputeStagingBufferParams(representation_->format(),
                                      representation_->size(), &bytes_per_row,
                                      &buffer_size)) {
        return false;
      }

      // Create a staging buffer to read back from the texture.
      wgpu::BufferDescriptor buffer_desc = {
          .usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead,
          .size = buffer_size,
      };
      wgpu::Buffer buffer = device_.CreateBuffer(&buffer_desc);

      wgpu::ImageCopyTexture texture_copy = {
          .texture = texture_,
      };
      wgpu::ImageCopyBuffer buffer_copy = {
          .layout =
              {
                  .bytesPerRow = bytes_per_row,
                  .rowsPerImage = wgpu::kCopyStrideUndefined,
              },
          .buffer = buffer,
      };
      wgpu::Extent3D extent = {
          static_cast<uint32_t>(representation_->size().width()),
          static_cast<uint32_t>(representation_->size().height()), 1};

      // Copy from the texture into the staging buffer.
      wgpu::DawnEncoderInternalUsageDescriptor internal_usage_desc;
      internal_usage_desc.useInternalUsages = true;
      wgpu::CommandEncoderDescriptor command_encoder_desc = {
          .nextInChain = &internal_usage_desc,
      };
      wgpu::CommandEncoder encoder =
          device_.CreateCommandEncoder(&command_encoder_desc);
      encoder.CopyTextureToBuffer(&texture_copy, &buffer_copy, &extent);
      wgpu::CommandBuffer commandBuffer = encoder.Finish();

      wgpu::Queue queue = device_.GetQueue();
      queue.Submit(1, &commandBuffer);

      struct Userdata {
        bool map_complete = false;
        WGPUBufferMapAsyncStatus status;
      } userdata;

      // Map the staging buffer for read.
      buffer.MapAsync(
          wgpu::MapMode::Read, 0, wgpu::kWholeMapSize,
          [](WGPUBufferMapAsyncStatus status, void* void_userdata) {
            Userdata* userdata = static_cast<Userdata*>(void_userdata);
            userdata->status = status;
            userdata->map_complete = true;
          },
          &userdata);

      // Poll for the map to complete.
      while (!userdata.map_complete) {
        base::PlatformThread::Sleep(base::Milliseconds(1));
        device_.Tick();
      }

      if (userdata.status != WGPUBufferMapAsyncStatus_Success) {
        return false;
      }

      const void* data = buffer.GetConstMappedRange(0, wgpu::kWholeMapSize);
      DCHECK(data);

      std::vector<GrBackendSemaphore> begin_semaphores;
      std::vector<GrBackendSemaphore> end_semaphores;
      auto scoped_write_access = representation_->BeginScopedWriteAccess(
          &begin_semaphores, &end_semaphores,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
      if (!scoped_write_access) {
        DLOG(ERROR)
            << "UploadContentsToSkia: Couldn't begin shared image access";
        return false;
      }

      auto* surface = scoped_write_access->surface();

      WaitForSemaphores(std::move(begin_semaphores));
      surface->writePixels(SkPixmap(surface->imageInfo(), data, bytes_per_row),
                           /*x*/ 0, /*y*/ 0);

      // It's ok to pass in empty GrFlushInfo here since SignalSemaphores()
      // will populate it with semaphores and call GrDirectContext::flush.
      skgpu::ganesh::Flush(surface);
      // Transition the image back to the desired end state. This is used for
      // transitioning the image to the external queue for Vulkan/GL interop.
      scoped_write_access->ApplyBackendSurfaceEndState();

      SignalSemaphores(std::move(end_semaphores));

      return true;
    }

    void WaitForSemaphores(std::vector<GrBackendSemaphore> semaphores) {
      if (semaphores.empty())
        return;

      bool wait_result = shared_context_state_->gr_context()->wait(
          semaphores.size(), semaphores.data(),
          /*deleteSemaphoresAfterWait=*/false);
      DCHECK(wait_result);
    }

    void SignalSemaphores(std::vector<GrBackendSemaphore> semaphores) {
      if (semaphores.empty())
        return;

      GrFlushInfo flush_info = {
          .fNumSemaphores = semaphores.size(),
          .fSignalSemaphores = semaphores.data(),
      };
      // Note: this is a no-op if vk_context_provider is null.
      AddVulkanCleanupTaskForSkiaFlush(
          shared_context_state_->vk_context_provider(), &flush_info);
      auto flush_result =
          shared_context_state_->gr_context()->flush(flush_info);
      DCHECK(flush_result == GrSemaphoresSubmitted::kYes);
      shared_context_state_->gr_context()->submit();
    }

    scoped_refptr<SharedContextState> shared_context_state_;
    std::unique_ptr<SkiaImageRepresentation> representation_;
    wgpu::Device device_;
    wgpu::Texture texture_;
    wgpu::TextureUsage usage_;
  };

  // Implementation of SharedImageRepresentationAndAccess that yields an error
  // texture.
  class ErrorSharedImageRepresentationAndAccess
      : public SharedImageRepresentationAndAccess {
   public:
    ErrorSharedImageRepresentationAndAccess(const wgpu::Device& device,
                                            wgpu::TextureUsage usage) {
      // Note: the texture descriptor matters little since this texture won't be
      // used for reflection, and all validation check the error state of the
      // texture before the texture attributes.
      wgpu::TextureDescriptor texture_desc = {
          .usage = usage,
          .dimension = wgpu::TextureDimension::e2D,
          .size = {1, 1, 1},
          .format = wgpu::TextureFormat::RGBA8Unorm,
          .mipLevelCount = 1,
          .sampleCount = 1,
      };
      texture_ = device.CreateErrorTexture(&texture_desc);
    }
    ~ErrorSharedImageRepresentationAndAccess() override = default;

    wgpu::Texture texture() const override { return texture_.Get(); }

   private:
    wgpu::Texture texture_;
  };

  // Map from the <ID, generation> pair for a wire texture to the shared image
  // representation and access for it.
  base::flat_map<std::tuple<uint32_t, uint32_t>,
                 std::unique_ptr<SharedImageRepresentationAndAccess>>
      associated_shared_image_map_;

  // A container of devices that we've seen on the wire, and their associated
  // metadata. Not all of them may be valid, so it gets pruned when
  // iterating through it in PerformPollingWork.
  struct DeviceMetadata {
    wgpu::AdapterType adapterType;
    wgpu::BackendType backendType;
  };
  struct DeviceHash {
    size_t operator()(const wgpu::Device& device) const {
      return std::hash<WGPUDevice>()(device.Get());
    }
  };
  struct DeviceEqual {
    bool operator()(const wgpu::Device& lhs, const wgpu::Device& rhs) const {
      return lhs.Get() == rhs.Get();
    }
  };
  std::unordered_map<wgpu::Device, DeviceMetadata, DeviceHash, DeviceEqual>
      known_device_metadata_;

  bool has_polling_work_ = false;
  bool destroyed_ = false;

  scoped_refptr<gl::GLContext> gl_context_;
  scoped_refptr<gl::GLSurface> gl_surface_;

  base::WeakPtrFactory<WebGPUDecoderImpl> weak_ptr_factory_{this};
};

constexpr WebGPUDecoderImpl::CommandInfo WebGPUDecoderImpl::command_info[] = {
#define WEBGPU_CMD_OP(name)                                \
  {                                                        \
      &WebGPUDecoderImpl::Handle##name,                    \
      cmds::name::kArgFlags,                               \
      cmds::name::cmd_flags,                               \
      sizeof(cmds::name) / sizeof(CommandBufferEntry) - 1, \
  }, /* NOLINT */
    WEBGPU_COMMAND_LIST(WEBGPU_CMD_OP)
#undef WEBGPU_CMD_OP
};

// This variable is set to DawnWireServer's parent decoder during execution of
// HandleCommands. It is cleared to nullptr after.
ABSL_CONST_INIT thread_local WebGPUDecoderImpl* parent_decoder = nullptr;

// DawnWireServer is a wrapper around dawn::wire::WireServer which allows
// overriding some of the WGPU procs the server delegates calls to.
// It has a special feature that around HandleDawnCommands, its owning
// WebGPUDecoderImpl is stored in thread-local storage. This enables
// some of the overridden procs to be overridden with a WebGPUDecoderImpl
// member function. The proc will be set to a plain-old C function pointer,
// which loads the WebGPUDecoderImpl from thread-local storage and forwards
// the call to the member function.
class DawnWireServer : public dawn::wire::WireServer {
  // Base template for specialization.
  template <auto Method>
  struct ProcForDecoderMethodImpl;

 public:
  // Helper function for making a Proc from a WebGPUDecoderImpl method.
  template <auto Method>
  static auto ProcForDecoderMethod() {
    return ProcForDecoderMethodImpl<Method>{}();
  }

  template <typename... Procs>
  static std::unique_ptr<DawnWireServer> Create(
      WebGPUDecoderImpl* decoder,
      dawn::wire::CommandSerializer* serializer,
      dawn::wire::server::MemoryTransferService* memory_transfer_service,
      const DawnProcTable& procs) {
    dawn::wire::WireServerDescriptor descriptor = {};
    descriptor.procs = &procs;
    descriptor.serializer = serializer;
    descriptor.memoryTransferService = memory_transfer_service;

    return base::WrapUnique(new DawnWireServer(decoder, descriptor));
  }

  ~DawnWireServer() override = default;

  // Handle Dawn commands. Forward the call to the base class, but
  // set |parent_decoder| around it.
  const volatile char* HandleCommands(const volatile char* commands,
                                      size_t size) override {
    const base::AutoReset<WebGPUDecoderImpl*> resetter_(&parent_decoder,
                                                        decoder_);
    const volatile char* rv =
        dawn::wire::WireServer::HandleCommands(commands, size);
    return rv;
  }

 private:
  DawnWireServer(WebGPUDecoderImpl* decoder,
                 const dawn::wire::WireServerDescriptor& desc)
      : dawn::wire::WireServer(desc), decoder_(decoder) {}

  // Specialization where |Method| is a WebGPUDecoderImpl member function.
  // Returns a proc which loads the decoder from thread-local storage
  // and forwards the call to the member function.
  template <typename R,
            typename... Args,
            R (WebGPUDecoderImpl::*Method)(Args...)>
  struct ProcForDecoderMethodImpl<Method> {
    using Proc = R (*)(Args...);
    Proc operator()() {
      return [](Args... args) -> R {
        DCHECK(parent_decoder);
        return (parent_decoder->*Method)(std::forward<Args>(args)...);
      };
    }
  };

  raw_ptr<WebGPUDecoderImpl> decoder_;
};

}  // namespace

WebGPUDecoder* CreateWebGPUDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    SharedImageManager* shared_image_manager,
    MemoryTracker* memory_tracker,
    gles2::Outputter* outputter,
    const GpuPreferences& gpu_preferences,
    scoped_refptr<SharedContextState> shared_context_state,
    const DawnCacheOptions& dawn_cache_options,
    IsolationKeyProvider* isolation_key_provider) {
  // Construct a Dawn caching interface if the Dawn configurations enables it.
  // If a handle was set, pass the relevant handle and CacheBlob callback so that
  // writing to disk is enabled. Otherwise pass an incognito in-memory version.
  std::unique_ptr<webgpu::DawnCachingInterface> dawn_caching_interface =
      nullptr;
  if (auto* caching_interface_factory =
          dawn_cache_options.caching_interface_factory.get()) {
    if (dawn_cache_options.handle) {
      // The DecoderClient outlives the DawnCachingInterface, so it is safe
      dawn_caching_interface = caching_interface_factory->CreateInstance(
          *dawn_cache_options.handle,
          base::BindRepeating(&DecoderClient::CacheBlob,
                              base::Unretained(client)));
    } else {
      dawn_caching_interface = caching_interface_factory->CreateInstance();
    }
  }

  return new WebGPUDecoderImpl(
      client, command_buffer_service, shared_image_manager, memory_tracker,
      outputter, gpu_preferences, std::move(shared_context_state),
      std::move(dawn_caching_interface), isolation_key_provider);
}

WebGPUDecoderImpl::WebGPUDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    SharedImageManager* shared_image_manager,
    MemoryTracker* memory_tracker,
    gles2::Outputter* outputter,
    const GpuPreferences& gpu_preferences,
    scoped_refptr<SharedContextState> shared_context_state,
    std::unique_ptr<DawnCachingInterface> dawn_caching_interface,
    IsolationKeyProvider* isolation_key_provider)
    : WebGPUDecoder(client, command_buffer_service, outputter),
      shared_context_state_(std::move(shared_context_state)),
      gr_context_type_(gpu_preferences.gr_context_type),
      shared_image_representation_factory_(
          std::make_unique<SharedImageRepresentationFactory>(
              shared_image_manager,
              memory_tracker)),
      dawn_platform_(new DawnPlatform(
          base::FeatureList::IsEnabled(features::kWebGPUBlobCache)
              ? std::move(dawn_caching_interface)
              : nullptr)),
      dawn_instance_(
          DawnInstance::Create(dawn_platform_.get(), gpu_preferences)),
      memory_transfer_service_(new DawnServiceMemoryTransferService(this)),
      wire_serializer_(new DawnServiceSerializer(client)),
      isolation_key_provider_(isolation_key_provider) {
  enable_unsafe_webgpu_ = gpu_preferences.enable_unsafe_webgpu;
  use_webgpu_adapter_ = gpu_preferences.use_webgpu_adapter;
  use_webgpu_power_preference_ = gpu_preferences.use_webgpu_power_preference;
  force_webgpu_compat_ = gpu_preferences.force_webgpu_compat;
  require_enabled_toggles_ = gpu_preferences.enabled_dawn_features_list;
  require_disabled_toggles_ = gpu_preferences.disabled_dawn_features_list;

  // Only allow unsafe APIs if the allow_unsafe_apis toggle is explicitly
  // enabled.
  allow_unsafe_apis_ =
      base::Contains(require_enabled_toggles_, "allow_unsafe_apis");

  // Force adapters to report their limits in predetermined tiers unless the
  // adapter_limit_tiers toggle is explicitly disabled.
  tiered_adapter_limits_ =
      !base::Contains(require_disabled_toggles_, "tiered_adapter_limits");

  DawnProcTable wire_procs = dawn::native::GetProcs();
  wire_procs.createInstance =
      [](const WGPUInstanceDescriptor*) -> WGPUInstance {
    CHECK(false);
    return nullptr;
  };
  wire_procs.instanceRequestAdapter = DawnWireServer::ProcForDecoderMethod<
      &WebGPUDecoderImpl::RequestAdapterImpl>();
  wire_procs.adapterHasFeature = DawnWireServer::ProcForDecoderMethod<
      &WebGPUDecoderImpl::AdapterHasFeatureImpl>();
  wire_procs.adapterEnumerateFeatures = DawnWireServer::ProcForDecoderMethod<
      &WebGPUDecoderImpl::AdapterEnumerateFeaturesImpl>();
  wire_procs.adapterRequestDevice = DawnWireServer::ProcForDecoderMethod<
      &WebGPUDecoderImpl::RequestDeviceImpl>();

  wire_server_ = DawnWireServer::Create(
      this, wire_serializer_.get(), memory_transfer_service_.get(), wire_procs);

  wire_server_->InjectInstance(dawn_instance_->Get(), 1, 0);

  // If there is no isolation key provider we don't want to wait for an
  // isolation key to come when processing device requests. Therefore, we can
  // set the isolation key to an empty string to avoid blocking and disable
  // caching in Dawn. Note that the isolation key provider is not available in
  // some testing scenarios and the in-process command buffer case.
  if (isolation_key_provider_ == nullptr) {
    isolation_key_ = "";
  }
}

WebGPUDecoderImpl::~WebGPUDecoderImpl() {
  Destroy(false);
}

void WebGPUDecoderImpl::Destroy(bool have_context) {
  // Resolve all outstanding callbacks for queued device requests if they
  // exist.
  for (auto& request : queued_request_device_calls_) {
    std::move(request).Run(false);
  }
  queued_request_device_calls_.clear();

  associated_shared_image_map_.clear();
  known_device_metadata_.clear();
  wire_server_ = nullptr;

  destroyed_ = true;
}

ContextResult WebGPUDecoderImpl::Initialize(
    const GpuFeatureInfo& gpu_feature_info) {
  if (kGpuFeatureStatusSoftware ==
      gpu_feature_info.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGPU]) {
    use_webgpu_adapter_ = WebGPUAdapterName::kSwiftShader;
  }

  dawn_instance_->EnableAdapterBlocklist(use_blocklist());
  // Create a Chrome-side EGL context. This isn't actually used by Dawn,
  // but it prevents rendering artifacts in Chrome. This workaround should
  // be revisited once EGL context creation is reworked. See crbug.com/1465911
  if (use_webgpu_adapter_ == WebGPUAdapterName::kOpenGLES) {
    gl_surface_ = new gl::SurfacelessEGL(gl::GLSurfaceEGL::GetGLDisplayEGL(),
                                         gfx::Size(1, 1));
    gl::GLContextAttribs attribs;
    attribs.client_major_es_version = 3;
    attribs.client_minor_es_version = 1;
    gl_context_ = new gl::GLContextEGL(nullptr);
    gl_context_->Initialize(gl_surface_.get(), attribs);
    gl_context_->MakeCurrent(gl_surface_.get());
  }
  return ContextResult::kSuccess;
}

bool WebGPUDecoderImpl::IsFeatureExposed(wgpu::FeatureName feature) const {
  switch (feature) {
    case wgpu::FeatureName::TimestampQuery:
    case wgpu::FeatureName::TimestampQueryInsidePasses:
    case wgpu::FeatureName::PipelineStatisticsQuery:
    case wgpu::FeatureName::ChromiumExperimentalDp4a:
    // TODO(dawn:1664): Enable Float32Filterable by default once it is tested.
    case wgpu::FeatureName::Float32Filterable:
    // TODO(crbug.com/1258986): DawnMultiPlanarFormats is a stable feature in
    // Dawn, but currently we hide it from Render process as unsafe apis, so
    // that 0-copy code path, which explicitly checks this feature, is protected
    // under unsafe apis as well.
    case wgpu::FeatureName::DawnMultiPlanarFormats:
    case wgpu::FeatureName::ShaderF16:
      return allow_unsafe_apis_;
    case wgpu::FeatureName::Depth32FloatStencil8:
    case wgpu::FeatureName::DepthClipControl:
    case wgpu::FeatureName::TextureCompressionBC:
    case wgpu::FeatureName::TextureCompressionETC2:
    case wgpu::FeatureName::TextureCompressionASTC:
    case wgpu::FeatureName::IndirectFirstInstance:
    case wgpu::FeatureName::RG11B10UfloatRenderable:
    case wgpu::FeatureName::BGRA8UnormStorage:
      return true;
    default:
      return false;
  }
}

void WebGPUDecoderImpl::RequestAdapterImpl(
    WGPUInstance instance,
    const WGPURequestAdapterOptions* options,
    WGPURequestAdapterCallback callback,
    void* userdata) {
  WGPURequestAdapterOptions default_options;
  if (options == nullptr) {
    default_options = {};
    options = &default_options;
  }

  bool force_fallback_adapter = options->forceFallbackAdapter;
  if (use_webgpu_adapter_ == WebGPUAdapterName::kSwiftShader) {
    force_fallback_adapter = true;
  }

  if (gr_context_type_ != GrContextType::kVulkan &&
      use_webgpu_adapter_ != WebGPUAdapterName::kOpenGLES) {
#if BUILDFLAG(IS_LINUX)
    callback(WGPURequestAdapterStatus_Unavailable, nullptr,
             "WebGPU on Linux requires command-line flag "
             "--enable-features=Vulkan",
             userdata);
    return;
#endif  // BUILDFLAG(IS_LINUX)
  }

  wgpu::Adapter adapter = CreatePreferredAdapter(
      static_cast<wgpu::PowerPreference>(options->powerPreference),
      force_fallback_adapter,
      options->compatibilityMode || force_webgpu_compat_);

  if (adapter == nullptr) {
    // There are no adapters to return since webgpu is not supported here
    callback(WGPURequestAdapterStatus_Unavailable, nullptr,
             "No available adapters.", userdata);
    return;
  }
  callback(WGPURequestAdapterStatus_Success, adapter.Release(), nullptr,
           userdata);
}

WGPUBool WebGPUDecoderImpl::AdapterHasFeatureImpl(WGPUAdapter adapter,
                                                  WGPUFeatureName feature) {
  if (!IsFeatureExposed(static_cast<wgpu::FeatureName>(feature))) {
    return false;
  }
  wgpu::Adapter adapter_obj(adapter);
  return adapter_obj.HasFeature(static_cast<wgpu::FeatureName>(feature));
}

size_t WebGPUDecoderImpl::AdapterEnumerateFeaturesImpl(
    WGPUAdapter adapter,
    WGPUFeatureName* features_out) {
  wgpu::Adapter adapter_obj(adapter);
  size_t count = adapter_obj.EnumerateFeatures(nullptr);

  std::vector<wgpu::FeatureName> features(count);
  adapter_obj.EnumerateFeatures(features.data());

  auto it =
      std::partition(features.begin(), features.end(),
                     [&](wgpu::FeatureName f) { return IsFeatureExposed(f); });
  count = std::distance(features.begin(), it);

  if (features_out != nullptr) {
    memcpy(features_out, features.data(), sizeof(wgpu::FeatureName) * count);
  }
  return count;
}

void WebGPUDecoderImpl::RequestDeviceImpl(
    WGPUAdapter adapter,
    const WGPUDeviceDescriptor* descriptor,
    WGPURequestDeviceCallback callback,
    void* userdata) {
  wgpu::Adapter adapter_obj(adapter);

  // We can only request a device if we have received an isolation key from an
  // async gpu->browser mojo. As a result, we do a check here and if we don't
  // have the isolation key yet, we queue this request to be handled as soon as
  // we get an isolation key.
  if (!isolation_key_) {
    DCHECK_NE(isolation_key_provider_, nullptr);
    queued_request_device_calls_.emplace_back(CreateQueuedRequestDeviceCallback(
        adapter_obj, descriptor, callback, userdata));
    return;
  }

  // Copy the descriptor so we can modify it.
  wgpu::DeviceDescriptor desc;
  if (descriptor != nullptr) {
    desc = *reinterpret_cast<const wgpu::DeviceDescriptor*>(descriptor);
  }
  DCHECK_EQ(desc.nextInChain, nullptr);

  std::vector<wgpu::FeatureName> required_features;
  if (desc.requiredFeaturesCount) {
    required_features = {
        desc.requiredFeatures,
        desc.requiredFeatures + desc.requiredFeaturesCount,
    };

    // Check that no disallowed features were requested. They should be hidden
    // by AdapterEnumerateFeaturesImpl.
    for (const wgpu::FeatureName& feature : required_features) {
      if (!IsFeatureExposed(feature)) {
        callback(WGPURequestDeviceStatus_Error, nullptr,
                 "Disallowed feature requested.", userdata);
        return;
      }
    }
  }

  // We need to request internal usage to be able to do operations with internal
  // SharedImage / interop methods that would need specific usages.
  required_features.push_back(wgpu::FeatureName::DawnInternalUsages);

  // Always require "multi-planar-formats" as long as supported, although
  // currently this feature is not exposed to render process if unsafe apis
  // disallowed.
  if (adapter_obj.HasFeature(wgpu::FeatureName::DawnMultiPlanarFormats)) {
    required_features.push_back(wgpu::FeatureName::DawnMultiPlanarFormats);
  }

  desc.requiredFeatures = required_features.data();
  desc.requiredFeaturesCount = required_features.size();

  // If a new toggle is added here, ForceDawnTogglesForWebGPU() which collects
  // info for about:gpu should be updated as well.
  wgpu::DawnTogglesDescriptor dawn_device_toggles;
  std::vector<const char*> require_device_enabled_toggles;
  std::vector<const char*> require_device_disabled_toggles;

  // Disallows usage of SPIR-V by default for security (we only ensure that WGSL
  // is secure), unless --enable-unsafe-webgpu is used.
  if (!enable_unsafe_webgpu_) {
    require_device_enabled_toggles.push_back("disallow_spirv");
  }
  // Disable the blob cache if we don't have an isolation key.
  if (isolation_key_->empty()) {
    require_device_enabled_toggles.push_back("disable_blob_cache");
  }

  for (const std::string& toggles : require_enabled_toggles_) {
    require_device_enabled_toggles.push_back(toggles.c_str());
  }
  for (const std::string& toggles : require_disabled_toggles_) {
    require_device_disabled_toggles.push_back(toggles.c_str());
  }
  dawn_device_toggles.enabledToggles = require_device_enabled_toggles.data();
  dawn_device_toggles.enabledTogglesCount =
      require_device_enabled_toggles.size();
  dawn_device_toggles.disabledToggles = require_device_disabled_toggles.data();
  dawn_device_toggles.disabledTogglesCount =
      require_device_disabled_toggles.size();
  ChainStruct(desc, &dawn_device_toggles);

  // Dawn caching isolation key information needs to be passed per device. If an
  // isolation key is empty, we do not pass this extra descriptor, and disable
  // the blob cache via toggles above.
  wgpu::DawnCacheDeviceDescriptor dawn_cache;
  if (!isolation_key_->empty()) {
    dawn_cache.isolationKey = isolation_key_->c_str();
    ChainStruct(desc, &dawn_cache);
  }

  bool called = false;
  auto* bound_callback = BindWGPUOnceCallback(
      [](
          // bound arguments
          bool* called,                                 //
          WebGPUDecoderImpl* decoder,                   //
          const wgpu::Adapter adapter,                  //
          WGPURequestDeviceCallback original_callback,  //
          void* original_userdata,                      //
          // callback arguments, minus userdata
          WGPURequestDeviceStatus status,  //
          WGPUDevice device,               //
          const char* message              //
      ) {
        *called = true;
        // Forward to the original callback.
        original_callback(status, device, message, original_userdata);

        if (device) {
          // Intercept the response so we can add a device ref to the list of
          // known devices that we may need to call DeviceTick on.
          wgpu::AdapterProperties properties;
          adapter.GetProperties(&properties);
          decoder->known_device_metadata_.emplace(
              wgpu::Device(device),
              DeviceMetadata{properties.adapterType, properties.backendType});
        }
      },
      &called, this, adapter_obj, callback, userdata);

  adapter_obj.RequestDevice(&desc, bound_callback->UnboundCallback(),
                            bound_callback->AsUserdata());
  // The callback must have been called synchronously. We could allow async
  // here, but it would require careful handling of the decoder lifetime.
  CHECK(called);
}

namespace {

// A deep copy of WGPUDeviceDescriptor copies owns a copy of all it's members.
// Note that the deep copy does NOT copy/own chained structs.
struct WGPUDeviceDescriptorDeepCopy : WGPUDeviceDescriptor {
  explicit WGPUDeviceDescriptorDeepCopy(const WGPUDeviceDescriptor& desc)
      : WGPUDeviceDescriptor(desc) {
    // Since the deep copy does NOT copy chained structs, CHECK all relevant
    // chained structs for safety.
    CHECK_EQ(desc.nextInChain, nullptr);
    CHECK(desc.requiredLimits == nullptr ||
          desc.requiredLimits->nextInChain == nullptr);
    CHECK_EQ(desc.defaultQueue.nextInChain, nullptr);

    if (desc.label) {
      device_label_ = std::string(desc.label);
      label = device_label_.c_str();
    }
    if (desc.requiredFeatures) {
      required_features_ = std::vector<WGPUFeatureName>(
          desc.requiredFeatures,
          desc.requiredFeatures + desc.requiredFeaturesCount);
      requiredFeatures = required_features_.data();
    }
    if (desc.requiredLimits) {
      required_limits_ = *desc.requiredLimits;
      requiredLimits = &required_limits_;
    }
    if (desc.defaultQueue.label) {
      queue_label_ = std::string(desc.defaultQueue.label);
      defaultQueue.label = queue_label_.c_str();
    }
  }

  // Memory backed members.
  std::string device_label_;
  std::string queue_label_;
  std::vector<WGPUFeatureName> required_features_;
  WGPURequiredLimits required_limits_;
};

}  // namespace

WebGPUDecoderImpl::QueuedRequestDeviceCallback
WebGPUDecoderImpl::CreateQueuedRequestDeviceCallback(
    const wgpu::Adapter& adapter,
    const WGPUDeviceDescriptor* descriptor,
    WGPURequestDeviceCallback callback,
    void* userdata) {
  // We need to create a deep copy of the descriptor for these queued requests
  // since they are a temporary allocation that is freed at the end of
  // RequestDeviceImpl.
  std::unique_ptr<WGPUDeviceDescriptorDeepCopy> desc =
      descriptor != nullptr
          ? std::make_unique<WGPUDeviceDescriptorDeepCopy>(*descriptor)
          : nullptr;

  // Note that it is ok to bind the decoder as unretained in this case because
  // the decoder's dtor explicitly resolves all these callbacks.
  return base::BindOnce(
      [](WebGPUDecoderImpl* decoder, const wgpu::Adapter& adapter,
         std::unique_ptr<WGPUDeviceDescriptor> descriptor,
         WGPURequestDeviceCallback callback, void* userdata, bool run) {
        if (run) {
          DCHECK(decoder->isolation_key_);
          decoder->RequestDeviceImpl(adapter.Get(), descriptor.get(), callback,
                                     userdata);
        } else {
          callback(WGPURequestDeviceStatus_Unknown, nullptr,
                   "Queued device request cancelled.", userdata);
        }
      },
      base::Unretained(this), adapter, std::move(desc), callback, userdata);
}

bool WebGPUDecoderImpl::use_blocklist() const {
  // Enable the blocklist unless --enable-unsafe-webgpu or
  // --disable-dawn-features=adapter_blocklist
  return !(enable_unsafe_webgpu_ ||
           base::Contains(require_disabled_toggles_, "adapter_blocklist"));
}

wgpu::Adapter WebGPUDecoderImpl::CreatePreferredAdapter(
    wgpu::PowerPreference power_preference,
    bool force_fallback,
    bool compatibility_mode) const {
  // Update power_preference based on command-line flag
  // use_webgpu_power_preference_.
  switch (use_webgpu_power_preference_) {
    case WebGPUPowerPreference::kNone:
      if (power_preference == wgpu::PowerPreference::Undefined) {
        // If on battery power, default to the integrated GPU.
        if (!base::PowerMonitor::IsInitialized() ||
            base::PowerMonitor::IsOnBatteryPower()) {
          power_preference = wgpu::PowerPreference::LowPower;
        } else {
          power_preference = wgpu::PowerPreference::HighPerformance;
        }
      }
      break;
    case WebGPUPowerPreference::kDefaultLowPower:
      if (power_preference == wgpu::PowerPreference::Undefined) {
        power_preference = wgpu::PowerPreference::LowPower;
      }
      break;
    case WebGPUPowerPreference::kDefaultHighPerformance:
      if (power_preference == wgpu::PowerPreference::Undefined) {
        power_preference = wgpu::PowerPreference::HighPerformance;
      }
      break;
    case WebGPUPowerPreference::kForceLowPower:
      power_preference = wgpu::PowerPreference::LowPower;
      break;
    case WebGPUPowerPreference::kForceHighPerformance:
      power_preference = wgpu::PowerPreference::HighPerformance;
      break;
  }

  // Prepare wgpu::RequestAdapterOptions.
  wgpu::RequestAdapterOptions adapter_options;
  adapter_options.compatibilityMode = compatibility_mode;
  adapter_options.forceFallbackAdapter = force_fallback;
  adapter_options.powerPreference = power_preference;

  // Prepare adapter toggles descriptor based on required toggles
  wgpu::DawnTogglesDescriptor dawn_adapter_toggles;
  std::vector<const char*> require_adapter_enabled_toggles;
  std::vector<const char*> require_adapter_disabled_toggles;

  for (const std::string& toggles : require_enabled_toggles_) {
    require_adapter_enabled_toggles.push_back(toggles.c_str());
  }
  for (const std::string& toggles : require_disabled_toggles_) {
    require_adapter_disabled_toggles.push_back(toggles.c_str());
  }
  dawn_adapter_toggles.enabledToggles = require_adapter_enabled_toggles.data();
  dawn_adapter_toggles.enabledTogglesCount =
      require_adapter_enabled_toggles.size();
  dawn_adapter_toggles.disabledToggles =
      require_adapter_disabled_toggles.data();
  dawn_adapter_toggles.disabledTogglesCount =
      require_adapter_disabled_toggles.size();
  ChainStruct(adapter_options, &dawn_adapter_toggles);

#if BUILDFLAG(IS_WIN)
  // On Windows, query the LUID of ANGLE's adapter.
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    LOG(ERROR) << "Failed to query ID3D11Device from ANGLE.";
    return nullptr;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  if (!SUCCEEDED(d3d11_device.As(&dxgi_device))) {
    LOG(ERROR) << "Failed to get IDXGIDevice from ANGLE.";
    return nullptr;
  }
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  if (!SUCCEEDED(dxgi_device->GetAdapter(&dxgi_adapter))) {
    LOG(ERROR) << "Failed to get IDXGIAdapter from ANGLE.";
    return nullptr;
  }

  DXGI_ADAPTER_DESC adapter_desc;
  if (!SUCCEEDED(dxgi_adapter->GetDesc(&adapter_desc))) {
    LOG(ERROR) << "Failed to get DXGI_ADAPTER_DESC from ANGLE.";
    return nullptr;
  }

  // Chain the LUID from ANGLE.
  dawn::native::d3d::RequestAdapterOptionsLUID adapter_options_luid = {};
  adapter_options_luid.adapterLUID = adapter_desc.AdapterLuid;
  ChainStruct(adapter_options, &adapter_options_luid);
#endif

  // Build a list of backend types we will search for, in order of preference.
  std::vector<wgpu::BackendType> backend_types;
  switch (use_webgpu_adapter_) {
    case WebGPUAdapterName::kD3D11:
      backend_types = {wgpu::BackendType::D3D11};
      break;
    case WebGPUAdapterName::kOpenGLES:
      backend_types = {wgpu::BackendType::OpenGLES};
      break;
    case WebGPUAdapterName::kSwiftShader:
      backend_types = {wgpu::BackendType::Vulkan};
      break;
    case WebGPUAdapterName::kDefault: {
#if BUILDFLAG(IS_WIN)
      backend_types = {wgpu::BackendType::D3D12};
#elif BUILDFLAG(IS_MAC)
      backend_types = {wgpu::BackendType::Metal};
#else
      backend_types = {wgpu::BackendType::Vulkan, wgpu::BackendType::OpenGLES};
#endif
      break;
    }
  }

  // `CanUseAdapter` is a helper to determine if an adapter is not blocklisted,
  // supports all required features, and matches the requested adapter options
  // (some of which may be set by command-line flags).
  auto CanUseAdapter = [&](const dawn::native::Adapter& adapter) {
    WGPUAdapterProperties adapter_properties = {};
    adapter.GetProperties(&adapter_properties);

    if (use_blocklist() && IsWebGPUAdapterBlocklisted(adapter_properties)) {
      return false;
    }

    const bool is_swiftshader =
        adapter_properties.adapterType == WGPUAdapterType_CPU &&
        adapter_properties.vendorID == 0x1AE0 &&
        adapter_properties.deviceID == 0xC0DE;

    // The adapter must be able to import external images, or it must be a
    // SwiftShader adapter. For SwiftShader, we will perform a manual
    // upload/readback to/from shared images.
    if (!(adapter.SupportsExternalImages() || is_swiftshader)) {
      return false;
    }

    // If the power preference is forced, only accept specific adapter
    // types.
    if (use_webgpu_power_preference_ == WebGPUPowerPreference::kForceLowPower &&
        adapter_properties.adapterType != WGPUAdapterType_IntegratedGPU) {
      return false;
    }
    if (use_webgpu_power_preference_ ==
            WebGPUPowerPreference::kForceHighPerformance &&
        adapter_properties.adapterType != WGPUAdapterType_DiscreteGPU) {
      return false;
    }

    return true;
  };

  // Enumerate adapters in order of the preferred backend type.
  for (wgpu::BackendType backend_type : backend_types) {
    adapter_options.backendType = backend_type;
    for (dawn::native::Adapter& adapter :
         dawn_instance_->EnumerateAdapters(&adapter_options)) {
      adapter.SetUseTieredLimits(tiered_adapter_limits_);

      if (!CanUseAdapter(adapter)) {
        continue;
      }

      return wgpu::Adapter(adapter.Get());
    }
  }

  // If we still don't have an adapter, now try to find the fallback adapter.
  adapter_options.forceFallbackAdapter = true;
  adapter_options.backendType = wgpu::BackendType::Vulkan;
  for (dawn::native::Adapter& adapter :
       dawn_instance_->EnumerateAdapters(&adapter_options)) {
    adapter.SetUseTieredLimits(tiered_adapter_limits_);

    if (!CanUseAdapter(adapter)) {
      continue;
    }

    return wgpu::Adapter(adapter.Get());
  }

  // No adapter could be found.
  return nullptr;
}

const char* WebGPUDecoderImpl::GetCommandName(unsigned int command_id) const {
  if (command_id >= kFirstWebGPUCommand && command_id < kNumCommands) {
    return webgpu::GetCommandName(static_cast<CommandId>(command_id));
  }
  return GetCommonCommandName(static_cast<cmd::CommandId>(command_id));
}

error::Error WebGPUDecoderImpl::DoCommands(unsigned int num_commands,
                                           const volatile void* buffer,
                                           int num_entries,
                                           int* entries_processed) {
  DCHECK(entries_processed);
  int commands_to_process = num_commands;
  error::Error result = error::kNoError;
  const volatile CommandBufferEntry* cmd_data =
      static_cast<const volatile CommandBufferEntry*>(buffer);
  int process_pos = 0;
  CommandId command = static_cast<CommandId>(0);

  while (process_pos < num_entries && result == error::kNoError &&
         commands_to_process--) {
    const unsigned int size = cmd_data->value_header.size;
    command = static_cast<CommandId>(cmd_data->value_header.command);

    if (size == 0) {
      result = error::kInvalidSize;
      break;
    }

    if (static_cast<int>(size) + process_pos > num_entries) {
      result = error::kOutOfBounds;
      break;
    }

    const unsigned int arg_count = size - 1;
    unsigned int command_index = command - kFirstWebGPUCommand;
    if (command_index < std::size(command_info)) {
      // Prevent all further WebGPU commands from being processed if the server
      // is destroyed.
      if (destroyed_) {
        result = error::kLostContext;
        break;
      }
      const CommandInfo& info = command_info[command_index];
      unsigned int info_arg_count = static_cast<unsigned int>(info.arg_count);
      if ((info.arg_flags == cmd::kFixed && arg_count == info_arg_count) ||
          (info.arg_flags == cmd::kAtLeastN && arg_count >= info_arg_count)) {
        uint32_t immediate_data_size = (arg_count - info_arg_count) *
                                       sizeof(CommandBufferEntry);  // NOLINT
        result = (this->*info.cmd_handler)(immediate_data_size, cmd_data);
      } else {
        result = error::kInvalidArguments;
      }
    } else {
      result = DoCommonCommand(command, arg_count, cmd_data);
    }

    if (result == error::kNoError &&
        current_decoder_error_ != error::kNoError) {
      result = current_decoder_error_;
      current_decoder_error_ = error::kNoError;
    }

    if (result != error::kDeferCommandUntilLater) {
      process_pos += size;
      cmd_data += size;
    }
  }

  *entries_processed = process_pos;

  if (error::IsError(result)) {
    LOG(ERROR) << "Error: " << result << " for Command "
               << GetCommandName(command);
  }

  return result;
}

error::Error WebGPUDecoderImpl::HandleDawnCommands(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::DawnCommands& c =
      *static_cast<const volatile webgpu::cmds::DawnCommands*>(cmd_data);
  uint32_t trace_id_high = static_cast<uint32_t>(c.trace_id_high);
  uint32_t trace_id_low = static_cast<uint32_t>(c.trace_id_low);
  uint32_t size = static_cast<uint32_t>(c.size);
  uint32_t commands_shm_id = static_cast<uint32_t>(c.commands_shm_id);
  uint32_t commands_shm_offset = static_cast<uint32_t>(c.commands_shm_offset);

  const volatile char* shm_commands = GetSharedMemoryAs<const volatile char*>(
      commands_shm_id, commands_shm_offset, size);
  if (shm_commands == nullptr) {
    return error::kOutOfBounds;
  }

  uint64_t trace_id =
      (static_cast<uint64_t>(trace_id_high) << 32) + trace_id_low;
  TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"), "DawnCommands",
                         trace_id, TRACE_EVENT_FLAG_FLOW_IN);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
               "WebGPUDecoderImpl::HandleDawnCommands", "bytes", size);

  if (!wire_server_->HandleCommands(shm_commands, size)) {
    return error::kLostContext;
  }

  // TODO(crbug.com/1174145): This is O(N) where N is the number of devices.
  // Multiple submits would be O(N*M). We should find a way to more
  // intelligently poll for work on only the devices that need it.
  PerformPollingWork();

  return error::kNoError;
}

std::unique_ptr<WebGPUDecoderImpl::SharedImageRepresentationAndAccess>
WebGPUDecoderImpl::AssociateMailboxDawn(
    const Mailbox& mailbox,
    MailboxFlags flags,
    const wgpu::Device& device,
    wgpu::BackendType backendType,
    wgpu::TextureUsage usage,
    std::vector<wgpu::TextureFormat> view_formats) {
  std::unique_ptr<DawnImageRepresentation> shared_image =
      shared_image_representation_factory_->ProduceDawn(
          mailbox, device, backendType, std::move(view_formats));

  if (!shared_image) {
    DLOG(ERROR) << "AssociateMailbox: Couldn't produce shared image";
    return nullptr;
  }

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_APPLE) && \
    !BUILDFLAG(IS_ANDROID)
  if (usage & wgpu::TextureUsage::StorageBinding) {
    LOG(ERROR) << "AssociateMailbox: wgpu::TextureUsage::StorageBinding is NOT "
                  "supported yet on this platform.";
    return nullptr;
  }
#endif

  if (flags & WEBGPU_MAILBOX_DISCARD) {
    // Set contents to uncleared.
    shared_image->SetClearedRect(gfx::Rect());
  }

  std::unique_ptr<DawnImageRepresentation::ScopedAccess> scoped_access =
      shared_image->BeginScopedAccess(
          usage, SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!scoped_access) {
    DLOG(ERROR) << "AssociateMailbox: Couldn't begin shared image access";
    return nullptr;
  }

  return std::make_unique<SharedImageRepresentationAndAccessDawn>(
      std::move(shared_image), std::move(scoped_access));
}

std::unique_ptr<WebGPUDecoderImpl::SharedImageRepresentationAndAccess>
WebGPUDecoderImpl::AssociateMailboxUsingSkiaFallback(
    const Mailbox& mailbox,
    MailboxFlags flags,
    const wgpu::Device& device,
    wgpu::TextureUsage usage,
    std::vector<wgpu::TextureFormat> view_formats) {
  // Before using the shared context, ensure it is current if we're on GL.
  if (shared_context_state_->GrContextIsGL()) {
    shared_context_state_->MakeCurrent(/* gl_surface */ nullptr);
  }

  // Produce a Skia image from the mailbox.
  std::unique_ptr<SkiaImageRepresentation> shared_image =
      shared_image_representation_factory_->ProduceSkia(
          mailbox, shared_context_state_.get());

  if (!shared_image) {
    DLOG(ERROR) << "AssociateMailbox: Couldn't produce shared image";
    return nullptr;
  }

  if (flags & WEBGPU_MAILBOX_DISCARD) {
    // Set contents to uncleared.
    shared_image->SetClearedRect(gfx::Rect());
  }

  return SharedImageRepresentationAndAccessSkiaFallback::Create(
      shared_context_state_, std::move(shared_image), device, usage,
      std::move(view_formats));
}

error::Error WebGPUDecoderImpl::HandleAssociateMailboxImmediate(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::AssociateMailboxImmediate& c =
      *static_cast<const volatile webgpu::cmds::AssociateMailboxImmediate*>(
          cmd_data);
  uint32_t device_id = static_cast<uint32_t>(c.device_id);
  uint32_t device_generation = static_cast<uint32_t>(c.device_generation);
  uint32_t id = static_cast<uint32_t>(c.id);
  uint32_t generation = static_cast<uint32_t>(c.generation);
  wgpu::TextureUsage usage = static_cast<wgpu::TextureUsage>(c.usage);
  MailboxFlags flags = static_cast<MailboxFlags>(c.flags);
  uint32_t view_format_count = static_cast<uint32_t>(c.view_format_count);

  GLuint packed_entry_count = c.count;
  // The immediate_data should be uint32_t-sized words that exactly matches
  // the packed_entry_count.
  if (immediate_data_size % sizeof(uint32_t) != 0 ||
      immediate_data_size / sizeof(uint32_t) != packed_entry_count) {
    return error::kOutOfBounds;
  }

  volatile const uint32_t* packed_data =
      gles2::GetImmediateDataAs<volatile const uint32_t*>(
          c, immediate_data_size, immediate_data_size);

  // Compute the expected number of packed entries. Cast to uint64_t to
  // avoid overflow.
  static_assert(sizeof(Mailbox) % sizeof(uint32_t) == 0u);
  constexpr uint32_t kMailboxNumEntries = sizeof(Mailbox) / sizeof(uint32_t);
  uint64_t expected_packed_entries =
      static_cast<uint64_t>(kMailboxNumEntries) + view_format_count;

  // The packed data should be non-empty and exactly match the expected number
  // of entries.
  if (packed_data == nullptr || packed_entry_count != expected_packed_entries) {
    return error::kOutOfBounds;
  }

  // Unpack the mailbox
  Mailbox mailbox = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(packed_data));
  packed_data += kMailboxNumEntries;
  DLOG_IF(ERROR, !mailbox.Verify())
      << "AssociateMailbox was passed an invalid mailbox";

  // Copy the view formats into a vector.
  static_assert(sizeof(wgpu::TextureFormat) == sizeof(uint32_t));
  std::vector<wgpu::TextureFormat> view_formats(view_format_count);
  memcpy(view_formats.data(), const_cast<const uint32_t*>(packed_data),
         view_format_count * sizeof(wgpu::TextureFormat));

  if (usage & ~kAllowedMailboxTextureUsages) {
    DLOG(ERROR) << "AssociateMailbox: Invalid usage";
    return error::kInvalidArguments;
  }

  wgpu::Device device = wire_server_->GetDevice(device_id, device_generation);
  if (device == nullptr) {
    return error::kInvalidArguments;
  }

  std::unique_ptr<SharedImageRepresentationAndAccess> representation_and_access;
  auto it = known_device_metadata_.find(device);
  DCHECK(it != known_device_metadata_.end());
  if (it->second.adapterType == wgpu::AdapterType::CPU) {
    representation_and_access = AssociateMailboxUsingSkiaFallback(
        mailbox, flags, device, usage, std::move(view_formats));
  } else {
    representation_and_access =
        AssociateMailboxDawn(mailbox, flags, device, it->second.backendType,
                             usage, std::move(view_formats));
  }

  if (!representation_and_access) {
    // According to the WebGPU specification, failing to create a wgpu::Texture
    // which wraps a shared image (like the canvas drawing buffer) should yield
    // an error wgpu::Texture. Use an implementation of
    // SharedImageRepresentationAndAccess which always provides an error.
    representation_and_access =
        std::make_unique<ErrorSharedImageRepresentationAndAccess>(device,
                                                                  usage);
  }

  // Inject the texture in the dawn::wire::Server and remember which shared
  // image it is associated with.
  if (!wire_server_->InjectTexture(representation_and_access->texture().Get(),
                                   id, generation, device_id,
                                   device_generation)) {
    DLOG(ERROR) << "AssociateMailbox: Invalid texture ID";
    return error::kInvalidArguments;
  }

  std::tuple<uint32_t, uint32_t> id_and_generation{id, generation};
  auto insertion = associated_shared_image_map_.emplace(
      id_and_generation, std::move(representation_and_access));

  // InjectTexture already validated that the (ID, generation) can't have been
  // registered before.
  DCHECK(insertion.second);
  return error::kNoError;
}

error::Error WebGPUDecoderImpl::HandleDissociateMailbox(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::DissociateMailbox& c =
      *static_cast<const volatile webgpu::cmds::DissociateMailbox*>(cmd_data);
  uint32_t texture_id = static_cast<uint32_t>(c.texture_id);
  uint32_t texture_generation = static_cast<uint32_t>(c.texture_generation);

  std::tuple<uint32_t, uint32_t> id_and_generation{texture_id,
                                                   texture_generation};
  auto it = associated_shared_image_map_.find(id_and_generation);
  if (it == associated_shared_image_map_.end()) {
    DLOG(ERROR) << "DissociateMailbox: Invalid texture ID";
    return error::kInvalidArguments;
  }

  associated_shared_image_map_.erase(it);
  return error::kNoError;
}

error::Error WebGPUDecoderImpl::HandleDissociateMailboxForPresent(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::DissociateMailboxForPresent& c =
      *static_cast<const volatile webgpu::cmds::DissociateMailboxForPresent*>(
          cmd_data);
  uint32_t device_id = static_cast<uint32_t>(c.device_id);
  uint32_t device_generation = static_cast<uint32_t>(c.device_generation);
  uint32_t texture_id = static_cast<uint32_t>(c.texture_id);
  uint32_t texture_generation = static_cast<uint32_t>(c.texture_generation);

  std::tuple<uint32_t, uint32_t> id_and_generation{texture_id,
                                                   texture_generation};
  auto it = associated_shared_image_map_.find(id_and_generation);
  if (it == associated_shared_image_map_.end()) {
    DLOG(ERROR) << "DissociateMailbox: Invalid texture ID";
    return error::kInvalidArguments;
  }

  wgpu::Device device = wire_server_->GetDevice(device_id, device_generation);
  if (!device) {
    return error::kInvalidArguments;
  }

  wgpu::Texture texture = it->second->texture();
  DCHECK(texture);
  if (!dawn::native::IsTextureSubresourceInitialized(texture.Get(), 0, 1, 0,
                                                     1)) {
    // The compositor renders uninitialized textures as red. If the texture is
    // not initialized, we need to explicitly clear its contents to black.
    // This may not successfully initialize the texture if the texture or device
    // was explicitly destroyed, however the client ensures Dissociate is sent
    // before destroy.

    // Push an error scope to capture errors here. The texture may be
    // an error texture, so this code would produce additional errors
    // which should not be visible to the client.
    device.PushErrorScope(wgpu::ErrorFilter::Validation);
    wgpu::TextureView view = texture.CreateView();

    wgpu::RenderPassColorAttachment color_attachment;
    color_attachment.view = view;
    color_attachment.loadOp = wgpu::LoadOp::Clear;
    color_attachment.storeOp = wgpu::StoreOp::Store;
    color_attachment.clearValue = {0.0, 0.0, 0.0, 0.0};

    wgpu::RenderPassDescriptor render_pass_descriptor;
    render_pass_descriptor.colorAttachmentCount = 1;
    render_pass_descriptor.colorAttachments = &color_attachment;

    wgpu::DawnEncoderInternalUsageDescriptor internal_usage_desc;
    internal_usage_desc.useInternalUsages = true;
    wgpu::CommandEncoderDescriptor command_encoder_desc = {
        .nextInChain = &internal_usage_desc,
    };

    wgpu::CommandEncoder encoder =
        device.CreateCommandEncoder(&command_encoder_desc);
    wgpu::RenderPassEncoder pass =
        encoder.BeginRenderPass(&render_pass_descriptor);
    pass.End();
    wgpu::CommandBuffer command_buffer = encoder.Finish();
    device.GetQueue().Submit(1, &command_buffer);

    // Pop the error scope and log errors.
    device.PopErrorScope(
        [](WGPUErrorType, const char* message, void*) {
          if (message) {
            DLOG(ERROR) << "Clear contents to black had error: " << message;
          }
        },
        nullptr);
  }

  associated_shared_image_map_.erase(it);
  return error::kNoError;
}

void WebGPUDecoderImpl::OnGetIsolationKey(const std::string& isolation_key) {
  isolation_key_ = isolation_key;

  // Iterate and run all the potentially queued request device requests.
  for (auto& request : queued_request_device_calls_) {
    std::move(request).Run(true);
  }
  queued_request_device_calls_.clear();

  // The requests have been handled, but they may need to be flushed, so perform
  // polling work.
  PerformPollingWork();
}

error::Error WebGPUDecoderImpl::HandleSetWebGPUExecutionContextToken(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::SetWebGPUExecutionContextToken& c = *static_cast<
      const volatile webgpu::cmds::SetWebGPUExecutionContextToken*>(cmd_data);
  blink::WebGPUExecutionContextToken::Tag type{c.type};
  uint64_t high = uint64_t(c.high_high) << 32 | uint64_t(c.high_low);
  uint64_t low = uint64_t(c.low_high) << 32 | uint64_t(c.low_low);
  absl::optional<base::UnguessableToken> unguessable_token =
      base::UnguessableToken::Deserialize(high, low);
  if (!unguessable_token.has_value()) {
    return error::kInvalidArguments;
  }
  blink::WebGPUExecutionContextToken execution_context_token;
  switch (type) {
    case blink::WebGPUExecutionContextToken::IndexOf<blink::DocumentToken>(): {
      execution_context_token = blink::WebGPUExecutionContextToken(
          blink::DocumentToken(unguessable_token.value()));
      break;
    }
    case blink::WebGPUExecutionContextToken::IndexOf<
        blink::DedicatedWorkerToken>(): {
      execution_context_token = blink::WebGPUExecutionContextToken(
          blink::DedicatedWorkerToken(unguessable_token.value()));
      break;
    }
    default:
      NOTREACHED();
      return error::kInvalidArguments;
  }
  isolation_key_provider_->GetIsolationKey(
      execution_context_token,
      base::BindPostTask(base::SingleThreadTaskRunner::GetCurrentDefault(),
                         base::BindOnce(&WebGPUDecoderImpl::OnGetIsolationKey,
                                        weak_ptr_factory_.GetWeakPtr())));
  return error::kNoError;
}

}  // namespace webgpu
}  // namespace gpu
