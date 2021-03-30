// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/webgpu_decoder_impl.h"

#include <dawn_native/DawnNative.h>
#include <dawn_platform/DawnPlatform.h>
#include <dawn_wire/WireServer.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/checked_math.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/dawn_platform.h"
#include "gpu/command_buffer/service/dawn_service_memory_transfer_service.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "gpu/config/gpu_preferences.h"
#include "ipc/ipc_channel.h"

#if defined(OS_WIN)
#include <dawn_native/D3D12Backend.h>
#include "ui/gl/gl_angle_util_win.h"
#endif

namespace gpu {
namespace webgpu {

namespace {

constexpr size_t kMaxWireBufferSize =
    std::min(IPC::Channel::kMaximumMessageSize,
             static_cast<size_t>(1024 * 1024));

constexpr size_t kDawnReturnCmdsOffset =
    offsetof(cmds::DawnReturnCommandsInfo, deserialized_buffer);

static_assert(kDawnReturnCmdsOffset < kMaxWireBufferSize, "");

class WireServerCommandSerializer : public dawn_wire::CommandSerializer {
 public:
  explicit WireServerCommandSerializer(DecoderClient* client);
  ~WireServerCommandSerializer() override = default;
  size_t GetMaximumAllocationSize() const final;
  void* GetCmdSpace(size_t size) final;
  bool Flush() final;

 private:
  DecoderClient* client_;
  std::vector<uint8_t> buffer_;
  size_t put_offset_;
};

WireServerCommandSerializer::WireServerCommandSerializer(DecoderClient* client)
    : client_(client),
      buffer_(kMaxWireBufferSize),
      put_offset_(offsetof(cmds::DawnReturnCommandsInfo, deserialized_buffer)) {
  // We prepopulate the message with the header and keep it between flushes so
  // we never need to write it again.
  cmds::DawnReturnCommandsInfoHeader* header =
      reinterpret_cast<cmds::DawnReturnCommandsInfoHeader*>(&buffer_[0]);
  header->return_data_header.return_data_type =
      DawnReturnDataType::kDawnCommands;
}

size_t WireServerCommandSerializer::GetMaximumAllocationSize() const {
  return kMaxWireBufferSize - kDawnReturnCmdsOffset;
}

void* WireServerCommandSerializer::GetCmdSpace(size_t size) {
  // Note: Dawn will never call this function with |size| >
  // GetMaximumAllocationSize().
  DCHECK_LE(put_offset_, kMaxWireBufferSize);
  DCHECK_LE(size, GetMaximumAllocationSize());

  // Statically check that kMaxWireBufferSize + kMaxWireBufferSize is
  // a valid uint32_t. We can add put_offset_ and size without overflow.
  static_assert(base::CheckAdd(kMaxWireBufferSize, kMaxWireBufferSize)
                    .IsValid<uint32_t>(),
                "");
  uint32_t next_offset = put_offset_ + static_cast<uint32_t>(size);
  if (next_offset > buffer_.size()) {
    Flush();
    // TODO(enga): Keep track of how much command space the application is using
    // and adjust the buffer size accordingly.

    DCHECK_EQ(put_offset_, kDawnReturnCmdsOffset);
    next_offset = put_offset_ + static_cast<uint32_t>(size);
  }

  uint8_t* ptr = &buffer_[put_offset_];
  put_offset_ = next_offset;
  return ptr;
}

bool WireServerCommandSerializer::Flush() {
  if (put_offset_ > kDawnReturnCmdsOffset) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "WireServerCommandSerializer::Flush", "bytes", put_offset_);

    static uint32_t return_trace_id = 0;
    TRACE_EVENT_WITH_FLOW0(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                           "DawnReturnCommands", TRACE_EVENT_FLAG_FLOW_OUT,
                           return_trace_id++);

    client_->HandleReturnData(base::make_span(buffer_.data(), put_offset_));
    put_offset_ = kDawnReturnCmdsOffset;
  }
  return true;
}

dawn_native::DeviceType PowerPreferenceToDawnDeviceType(
    PowerPreference power_preference) {
  switch (power_preference) {
    case PowerPreference::kLowPower:
      return dawn_native::DeviceType::IntegratedGPU;
    case PowerPreference::kHighPerformance:
    // Currently for simplicity we always choose discrete GPU as the device
    // related to default power preference.
    case PowerPreference::kDefault:
      return dawn_native::DeviceType::DiscreteGPU;
    default:
      NOTREACHED();
      return dawn_native::DeviceType::CPU;
  }
}

}  // namespace

class WebGPUDecoderImpl final : public WebGPUDecoder {
 public:
  WebGPUDecoderImpl(DecoderClient* client,
                    CommandBufferServiceBase* command_buffer_service,
                    SharedImageManager* shared_image_manager,
                    MemoryTracker* memory_tracker,
                    gles2::Outputter* outputter,
                    const GpuPreferences& gpu_preferences);
  ~WebGPUDecoderImpl() override;

  // WebGPUDecoder implementation
  ContextResult Initialize() override;

  // DecoderContext implementation.
  base::WeakPtr<DecoderContext> AsWeakPtr() override {
    NOTIMPLEMENTED();
    return nullptr;
  }
  const gles2::ContextState* GetContextState() override {
    NOTREACHED();
    return nullptr;
  }
  void Destroy(bool have_context) override;
  bool MakeCurrent() override { return true; }
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
  gles2::GpuFenceManager* GetGpuFenceManager() override {
    NOTREACHED();
    return nullptr;
  }
  bool HasPendingQueries() const override { return false; }
  void ProcessPendingQueries(bool did_finish) override {}
  bool HasMoreIdleWork() const override { return false; }
  void PerformIdleWork() override {}

  bool HasPollingWork() const override { return has_polling_work_; }

  void PerformPollingWork() override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "WebGPUDecoderImpl::PerformPollingWork");
    has_polling_work_ = false;
    if (known_devices_.empty()) {
      return;
    }

    // Prune the list of known devices to remove ones that are no longer valid.
    // And call DeviceTick() on all valid devices.
    auto it =
        std::remove_if(known_devices_.begin(), known_devices_.end(),
                       [this](std::pair<uint32_t, uint32_t> id_generation) {
                         WGPUDevice device = wire_server_->GetDevice(
                             id_generation.first, id_generation.second);
                         if (device != nullptr) {
                           if (dawn_native::DeviceTick(device)) {
                             has_polling_work_ = true;
                           }
                           return false;
                         }
                         return true;
                       });
    known_devices_.erase(it, known_devices_.end());
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
  bool WasContextLostByRobustnessExtension() const override {
    NOTREACHED();
    return false;
  }
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
  void BindImage(uint32_t client_texture_id,
                 uint32_t texture_target,
                 gl::GLImage* image,
                 bool can_bind_to_sampler) override {
    NOTREACHED();
  }
  gles2::ContextGroup* GetContextGroup() override { return nullptr; }
  gles2::ErrorState* GetErrorState() override {
    NOTREACHED();
    return nullptr;
  }
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

  void DiscoverAdapters();

  int32_t GetPreferredAdapterIndex(PowerPreference power_preference) const;

  error::Error InitDawnDevice(
      int32_t requested_adapter_index,
      uint32_t device_id,
      uint32_t device_generation,
      const WGPUDeviceProperties& requested_device_properties);

  void SendAdapterProperties(DawnRequestAdapterSerial request_adapter_serial,
                             int32_t adapter_service_id,
                             const dawn_native::Adapter& adapter,
                             const char* error_message = nullptr);
  void SendRequestedDeviceInfo(DawnRequestDeviceSerial request_device_serial,
                               bool is_request_device_success);

  const GrContextType gr_context_type_;

  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;

  std::unique_ptr<dawn_platform::Platform> dawn_platform_;
  std::unique_ptr<DawnServiceMemoryTransferService> memory_transfer_service_;
  std::unique_ptr<dawn_native::Instance> dawn_instance_;
  std::vector<dawn_native::Adapter> dawn_adapters_;

  std::vector<std::string> force_enabled_toggles_;
  std::vector<std::string> force_disabled_toggles_;

  std::unique_ptr<dawn_wire::WireServer> wire_server_;
  std::unique_ptr<WireServerCommandSerializer> wire_serializer_;

  // Helper struct which holds a representation and its ScopedAccess, ensuring
  // safe destruction order.
  struct SharedImageRepresentationAndAccess {
    std::unique_ptr<SharedImageRepresentationDawn> representation;
    std::unique_ptr<SharedImageRepresentationDawn::ScopedAccess> access;
  };

  // Map from the <ID, generation> pair for a wire texture to the shared image
  // representation and access for it.
  base::flat_map<std::tuple<uint32_t, uint32_t>,
                 std::unique_ptr<SharedImageRepresentationAndAccess>>
      associated_shared_image_map_;

  // A list of (id, generation) device pairs that we've seen on the wire.
  // Not all of them may be valid, so it gets pruned when iterating through it
  // in PerformPollingWork. Dawn will never reuse a previously allocated
  // <ID, generation> pair.
  std::vector<std::pair<uint32_t, uint32_t>> known_devices_;

  bool has_polling_work_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebGPUDecoderImpl);
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

WebGPUDecoder* CreateWebGPUDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    SharedImageManager* shared_image_manager,
    MemoryTracker* memory_tracker,
    gles2::Outputter* outputter,
    const GpuPreferences& gpu_preferences) {
  return new WebGPUDecoderImpl(client, command_buffer_service,
                               shared_image_manager, memory_tracker, outputter,
                               gpu_preferences);
}

WebGPUDecoderImpl::WebGPUDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    SharedImageManager* shared_image_manager,
    MemoryTracker* memory_tracker,
    gles2::Outputter* outputter,
    const GpuPreferences& gpu_preferences)
    : WebGPUDecoder(client, command_buffer_service, outputter),
      gr_context_type_(gpu_preferences.gr_context_type),
      shared_image_representation_factory_(
          std::make_unique<SharedImageRepresentationFactory>(
              shared_image_manager,
              memory_tracker)),
      dawn_platform_(new DawnPlatform()),
      memory_transfer_service_(new DawnServiceMemoryTransferService(this)),
      dawn_instance_(new dawn_native::Instance()),
      wire_serializer_(new WireServerCommandSerializer(client)) {
  dawn_instance_->SetPlatform(dawn_platform_.get());
  dawn_instance_->EnableBackendValidation(
      gpu_preferences.enable_dawn_backend_validation);

  force_enabled_toggles_ = gpu_preferences.enabled_dawn_features_list;
  force_disabled_toggles_ = gpu_preferences.disabled_dawn_features_list;

  dawn_wire::WireServerDescriptor descriptor = {};
  descriptor.procs = &dawn_native::GetProcs();
  descriptor.serializer = wire_serializer_.get();
  descriptor.memoryTransferService = memory_transfer_service_.get();
  wire_server_ = std::make_unique<dawn_wire::WireServer>(descriptor);
}

WebGPUDecoderImpl::~WebGPUDecoderImpl() {
  Destroy(false);
}

void WebGPUDecoderImpl::Destroy(bool have_context) {
  associated_shared_image_map_.clear();
  wire_server_ = nullptr;
}

ContextResult WebGPUDecoderImpl::Initialize() {
  DiscoverAdapters();
  return ContextResult::kSuccess;
}

error::Error WebGPUDecoderImpl::InitDawnDevice(
    int32_t requested_adapter_index,
    uint32_t device_id,
    uint32_t device_generation,
    const WGPUDeviceProperties& request_device_properties) {
  DCHECK_LE(0, requested_adapter_index);

  DCHECK_LT(static_cast<size_t>(requested_adapter_index),
            dawn_adapters_.size());

  dawn_native::DeviceDescriptor device_descriptor;
  if (request_device_properties.textureCompressionBC) {
    device_descriptor.requiredExtensions.push_back("texture_compression_bc");
  }
  if (request_device_properties.shaderFloat16) {
    device_descriptor.requiredExtensions.push_back("shader_float16");
  }
  if (request_device_properties.pipelineStatisticsQuery) {
    device_descriptor.requiredExtensions.push_back("pipeline_statistics_query");
  }
  if (request_device_properties.timestampQuery) {
    device_descriptor.requiredExtensions.push_back("timestamp_query");
  }

  for (const std::string& toggles : force_enabled_toggles_) {
    device_descriptor.forceEnabledToggles.push_back(toggles.c_str());
  }
  for (const std::string& toggles : force_disabled_toggles_) {
    device_descriptor.forceDisabledToggles.push_back(toggles.c_str());
  }

  WGPUDevice wgpu_device =
      dawn_adapters_[requested_adapter_index].CreateDevice(&device_descriptor);
  if (wgpu_device == nullptr) {
    return error::kInvalidArguments;
  }

  if (!wire_server_->InjectDevice(wgpu_device, device_id, device_generation)) {
    return error::kInvalidArguments;
  }

  // Device injection takes a ref. The wire now owns the device so release it.
  dawn_native::GetProcs().deviceRelease(wgpu_device);

  // Save the id and generation of the device. Now, we can query the server for
  // this pair to discover if this device has been destroyed. The list will be
  // checked in PerformPollingWork to tick all the live devices and remove all
  // the dead ones.
  known_devices_.emplace_back(device_id, device_generation);

  return error::kNoError;
}

void WebGPUDecoderImpl::DiscoverAdapters() {
#if defined(OS_WIN)
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    // In the case where the d3d11 device is nullptr, we want to return a null
    // adapter
    return;
  }
  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.As(&dxgi_device);
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(&dxgi_adapter);
  dawn_native::d3d12::AdapterDiscoveryOptions options(std::move(dxgi_adapter));
  dawn_instance_->DiscoverAdapters(&options);
#else
  dawn_instance_->DiscoverDefaultAdapters();
#endif

  std::vector<dawn_native::Adapter> adapters = dawn_instance_->GetAdapters();
  for (const dawn_native::Adapter& adapter : adapters) {
    if (adapter.GetBackendType() != dawn_native::BackendType::Null &&
        adapter.GetBackendType() != dawn_native::BackendType::OpenGL) {
      dawn_adapters_.push_back(adapter);
    }
  }
}

int32_t WebGPUDecoderImpl::GetPreferredAdapterIndex(
    PowerPreference power_preference) const {
  dawn_native::DeviceType preferred_device_type =
      PowerPreferenceToDawnDeviceType(power_preference);

  int32_t discrete_gpu_adapter_index = -1;
  int32_t integrated_gpu_adapter_index = -1;
  int32_t cpu_adapter_index = -1;
  int32_t unknown_adapter_index = -1;

  for (int32_t i = 0; i < static_cast<int32_t>(dawn_adapters_.size()); ++i) {
    const dawn_native::Adapter& adapter = dawn_adapters_[i];
    if (adapter.GetDeviceType() == preferred_device_type) {
      return i;
    }
    switch (adapter.GetDeviceType()) {
      case dawn_native::DeviceType::DiscreteGPU:
        discrete_gpu_adapter_index = i;
        break;
      case dawn_native::DeviceType::IntegratedGPU:
        integrated_gpu_adapter_index = i;
        break;
      case dawn_native::DeviceType::CPU:
        cpu_adapter_index = i;
        break;
      case dawn_native::DeviceType::Unknown:
        unknown_adapter_index = i;
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  // For now, we always prefer the discrete GPU
  if (discrete_gpu_adapter_index >= 0) {
    return discrete_gpu_adapter_index;
  }
  if (integrated_gpu_adapter_index >= 0) {
    return integrated_gpu_adapter_index;
  }
  if (cpu_adapter_index >= 0) {
    return cpu_adapter_index;
  }
  if (unknown_adapter_index >= 0) {
    return unknown_adapter_index;
  }
  return -1;
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
    if (command_index < base::size(command_info)) {
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

void WebGPUDecoderImpl::SendAdapterProperties(
    DawnRequestAdapterSerial request_adapter_serial,
    int32_t adapter_service_id,
    const dawn_native::Adapter& adapter,
    const char* error_message) {
  WGPUDeviceProperties adapter_properties;
  size_t serialized_adapter_properties_size = 0;

  if (adapter) {
    adapter_properties = adapter.GetAdapterProperties();
    serialized_adapter_properties_size =
        dawn_wire::SerializedWGPUDevicePropertiesSize(&adapter_properties);
  } else {
    // If there's no adapter, the adapter_service_id should be -1
    DCHECK_EQ(adapter_service_id, -1);
  }

  size_t error_message_size =
      error_message == nullptr ? 0 : strlen(error_message);

  // Get serialization space for the return struct and variable-length data:
  // The serialized adapter properties, the error message, and null-terminator.
  std::vector<char> serialized_buffer(
      offsetof(cmds::DawnReturnAdapterInfo, deserialized_buffer) +
      serialized_adapter_properties_size + error_message_size + 1);

  cmds::DawnReturnAdapterInfo* return_adapter_info =
      reinterpret_cast<cmds::DawnReturnAdapterInfo*>(serialized_buffer.data());

  // Set Dawn return data header
  return_adapter_info->header = {};
  DCHECK_EQ(DawnReturnDataType::kRequestedDawnAdapterProperties,
            return_adapter_info->header.return_data_header.return_data_type);
  return_adapter_info->header.request_adapter_serial = request_adapter_serial;
  return_adapter_info->header.adapter_service_id = adapter_service_id;

  DCHECK(serialized_adapter_properties_size <=
         std::numeric_limits<uint32_t>::max());

  return_adapter_info->adapter_properties_size =
      static_cast<uint32_t>(serialized_adapter_properties_size);

  if (adapter) {
    // Set serialized adapter properties
    dawn_wire::SerializeWGPUDeviceProperties(
        &adapter_properties, return_adapter_info->deserialized_buffer);
  }

  // Copy the error message
  memcpy(return_adapter_info->deserialized_buffer +
             serialized_adapter_properties_size,
         error_message, error_message_size);

  // Write the null-terminator.
  // We don't copy (error_message_size + 1) above because |error_message| may
  // be nullptr instead of zero-length.
  return_adapter_info->deserialized_buffer[serialized_adapter_properties_size +
                                           error_message_size] = '\0';

  client()->HandleReturnData(base::make_span(
      reinterpret_cast<const uint8_t*>(serialized_buffer.data()),
      serialized_buffer.size()));
}

void WebGPUDecoderImpl::SendRequestedDeviceInfo(
    DawnRequestDeviceSerial request_device_serial,
    bool is_request_device_success) {
  cmds::DawnReturnRequestDeviceInfo return_request_device_info;
  DCHECK_EQ(DawnReturnDataType::kRequestedDeviceReturnInfo,
            return_request_device_info.return_data_header.return_data_type);
  return_request_device_info.request_device_serial = request_device_serial;
  return_request_device_info.is_request_device_success =
      is_request_device_success;

  client()->HandleReturnData(base::make_span(
      reinterpret_cast<const uint8_t*>(&return_request_device_info),
      sizeof(return_request_device_info)));
}

error::Error WebGPUDecoderImpl::HandleRequestAdapter(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::RequestAdapter& c =
      *static_cast<const volatile webgpu::cmds::RequestAdapter*>(cmd_data);
  PowerPreference power_preference =
      static_cast<PowerPreference>(c.power_preference);
  DawnRequestAdapterSerial request_adapter_serial =
      static_cast<DawnRequestAdapterSerial>(c.request_adapter_serial);

  if (gr_context_type_ != GrContextType::kVulkan) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    SendAdapterProperties(request_adapter_serial, -1, nullptr,
                          "WebGPU on Linux requires command-line flag "
                          "--enable-features=Vulkan,UseSkiaRenderer");
    return error::kNoError;
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)
  }

  int32_t requested_adapter_index = GetPreferredAdapterIndex(power_preference);
  if (requested_adapter_index < 0) {
    // There are no adapters to return since webgpu is not supported here
    SendAdapterProperties(request_adapter_serial, requested_adapter_index,
                          nullptr);
    return error::kNoError;
  }

  // Currently we treat the index of the adapter in dawn_adapters_ as the id of
  // the adapter in the server side.
  DCHECK_LT(static_cast<size_t>(requested_adapter_index),
            dawn_adapters_.size());
  const dawn_native::Adapter& adapter = dawn_adapters_[requested_adapter_index];
  SendAdapterProperties(request_adapter_serial, requested_adapter_index,
                        adapter);

  return error::kNoError;
}

error::Error WebGPUDecoderImpl::HandleRequestDevice(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::RequestDevice& c =
      *static_cast<const volatile webgpu::cmds::RequestDevice*>(cmd_data);
  DawnRequestDeviceSerial request_device_serial =
      static_cast<DawnRequestDeviceSerial>(c.request_device_serial);
  uint32_t adapter_service_id = static_cast<uint32_t>(c.adapter_service_id);
  uint32_t device_id = static_cast<uint32_t>(c.device_id);
  uint32_t device_generation = static_cast<uint32_t>(c.device_generation);
  uint32_t request_device_properties_shm_id =
      static_cast<uint32_t>(c.request_device_properties_shm_id);
  uint32_t request_device_properties_shm_offset =
      static_cast<uint32_t>(c.request_device_properties_shm_offset);
  uint32_t request_device_properties_size =
      static_cast<uint32_t>(c.request_device_properties_size);

  WGPUDeviceProperties device_properties = {};
  if (request_device_properties_size) {
    const volatile char* shm_device_properties =
        GetSharedMemoryAs<const volatile char*>(
            request_device_properties_shm_id,
            request_device_properties_shm_offset,
            request_device_properties_size);
    if (!shm_device_properties) {
      return error::kOutOfBounds;
    }

    if (!dawn_wire::DeserializeWGPUDeviceProperties(
            &device_properties, shm_device_properties,
            request_device_properties_size)) {
      return error::kOutOfBounds;
    }
  }

  error::Error init_device_error = InitDawnDevice(
      adapter_service_id, device_id, device_generation, device_properties);
  SendRequestedDeviceInfo(request_device_serial,
                          !error::IsError(init_device_error));
  return init_device_error;
}

error::Error WebGPUDecoderImpl::HandleDawnCommands(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::DawnCommands& c =
      *static_cast<const volatile webgpu::cmds::DawnCommands*>(cmd_data);
  uint32_t size = static_cast<uint32_t>(c.size);
  uint32_t commands_shm_id = static_cast<uint32_t>(c.commands_shm_id);
  uint32_t commands_shm_offset = static_cast<uint32_t>(c.commands_shm_offset);

  const volatile char* shm_commands = GetSharedMemoryAs<const volatile char*>(
      commands_shm_id, commands_shm_offset, size);
  if (shm_commands == nullptr) {
    return error::kOutOfBounds;
  }

  TRACE_EVENT_WITH_FLOW0(
      TRACE_DISABLED_BY_DEFAULT("gpu.dawn"), "DawnCommands",
      TRACE_EVENT_FLAG_FLOW_IN,
      (static_cast<uint64_t>(commands_shm_id) << 32) + commands_shm_offset);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
               "WebGPUDecoderImpl::HandleDawnCommands", "bytes", size);

  if (!wire_server_->HandleCommands(shm_commands, size)) {
    NOTREACHED();
    return error::kLostContext;
  }

  // TODO(crbug.com/1174145): This is O(N) where N is the number of devices.
  // Multiple submits would be O(N*M). We should find a way to more
  // intelligently poll for work on only the devices that need it.
  PerformPollingWork();

  return error::kNoError;
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
  WGPUTextureUsage usage = static_cast<WGPUTextureUsage>(c.usage);

  // Unpack the mailbox
  if (sizeof(Mailbox) > immediate_data_size) {
    return error::kOutOfBounds;
  }
  volatile const GLbyte* mailbox_bytes =
      gles2::GetImmediateDataAs<volatile const GLbyte*>(c, sizeof(Mailbox),
                                                        immediate_data_size);
  if (mailbox_bytes == nullptr) {
    return error::kOutOfBounds;
  }
  Mailbox mailbox = Mailbox::FromVolatile(
      *reinterpret_cast<const volatile Mailbox*>(mailbox_bytes));
  DLOG_IF(ERROR, !mailbox.Verify())
      << "AssociateMailbox was passed an invalid mailbox";

  static constexpr uint32_t kAllowedTextureUsages = static_cast<uint32_t>(
      WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst |
      WGPUTextureUsage_Sampled | WGPUTextureUsage_OutputAttachment);
  if (usage & ~kAllowedTextureUsages) {
    DLOG(ERROR) << "AssociateMailbox: Invalid usage";
    return error::kInvalidArguments;
  }

  WGPUDevice device = wire_server_->GetDevice(device_id, device_generation);
  if (device == nullptr) {
    return error::kInvalidArguments;
  }

  // Create a WGPUTexture from the mailbox.
  std::unique_ptr<SharedImageRepresentationDawn> shared_image =
      shared_image_representation_factory_->ProduceDawn(mailbox, device);
  if (!shared_image) {
    DLOG(ERROR) << "AssociateMailbox: Couldn't produce shared image";
    return error::kInvalidArguments;
  }

  // TODO(cwallez@chromium.org): Handle texture clearing. We should either
  // pre-clear textures, or implement a way to detect whether DAWN has cleared
  // a texture. crbug.com/1036080
  std::unique_ptr<SharedImageRepresentationDawn::ScopedAccess>
      shared_image_access = shared_image->BeginScopedAccess(
          usage, SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!shared_image_access) {
    DLOG(ERROR) << "AssociateMailbox: Couldn't begin shared image access";
    return error::kInvalidArguments;
  }

  // Inject the texture in the dawn_wire::Server and remember which shared image
  // it is associated with.
  if (!wire_server_->InjectTexture(shared_image_access->texture(), id,
                                   generation, device_id, device_generation)) {
    DLOG(ERROR) << "AssociateMailbox: Invalid texture ID";
    return error::kInvalidArguments;
  }

  std::unique_ptr<SharedImageRepresentationAndAccess>
      representation_and_access =
          std::make_unique<SharedImageRepresentationAndAccess>();
  representation_and_access->representation = std::move(shared_image);
  representation_and_access->access = std::move(shared_image_access);

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

}  // namespace webgpu
}  // namespace gpu
