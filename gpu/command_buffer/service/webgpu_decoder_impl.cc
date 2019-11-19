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
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/webgpu_cmd_enums.h"
#include "gpu/command_buffer/common/webgpu_cmd_format.h"
#include "gpu/command_buffer/common/webgpu_cmd_ids.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/dawn_platform.h"
#include "gpu/command_buffer/service/dawn_service_memory_transfer_service.h"
#include "gpu/command_buffer/service/decoder_client.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/webgpu_decoder.h"
#include "ipc/ipc_channel.h"

namespace gpu {
namespace webgpu {

namespace {

constexpr size_t kMaxWireBufferSize =
    std::min(IPC::Channel::kMaximumMessageSize,
             static_cast<size_t>(1024 * 1024));

class WireServerCommandSerializer : public dawn_wire::CommandSerializer {
 public:
  explicit WireServerCommandSerializer(DecoderClient* client);
  ~WireServerCommandSerializer() override = default;
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
      put_offset_(sizeof(cmds::DawnReturnDataHeader)) {
  cmds::DawnReturnDataHeader* return_data_header =
      reinterpret_cast<cmds::DawnReturnDataHeader*>(&buffer_[0]);
  return_data_header->return_data_type = DawnReturnDataType::kDawnCommands;
}

void* WireServerCommandSerializer::GetCmdSpace(size_t size) {
  // TODO(enga): Handle chunking commands if size +
  // sizeof(cmds::DawnReturnDataHeader)> kMaxWireBufferSize.
  if (size + sizeof(cmds::DawnReturnDataHeader) > kMaxWireBufferSize) {
    NOTREACHED();
    return nullptr;
  }

  // |next_offset| should never be more than kMaxWireBufferSize +
  // kMaxWireBufferSize.
  DCHECK_LE(put_offset_, kMaxWireBufferSize);
  DCHECK_LE(size, kMaxWireBufferSize);
  static_assert(base::CheckAdd(kMaxWireBufferSize, kMaxWireBufferSize)
                    .IsValid<uint32_t>(),
                "");
  uint32_t next_offset = put_offset_ + size;

  if (next_offset > buffer_.size()) {
    Flush();
    // TODO(enga): Keep track of how much command space the application is using
    // and adjust the buffer size accordingly.

    DCHECK_EQ(put_offset_, sizeof(cmds::DawnReturnDataHeader));
    next_offset = put_offset_ + size;
  }

  uint8_t* ptr = &buffer_[put_offset_];
  put_offset_ = next_offset;
  return ptr;
}

bool WireServerCommandSerializer::Flush() {
  if (put_offset_ > sizeof(cmds::DawnReturnDataHeader)) {
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "WireServerCommandSerializer::Flush", "bytes", put_offset_);

    static uint32_t return_trace_id = 0;
    TRACE_EVENT_FLOW_BEGIN0(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                            "DawnReturnCommands", return_trace_id++);

    client_->HandleReturnData(base::make_span(buffer_.data(), put_offset_));
    put_offset_ = sizeof(cmds::DawnReturnDataHeader);
  }
  return true;
}

dawn_native::DeviceType PowerPreferenceToDawnDeviceType(
    PowerPreference power_preference) {
  switch (power_preference) {
    case PowerPreference::kLowPower:
      return dawn_native::DeviceType::IntegratedGPU;
    case PowerPreference::kHighPerformance:
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
                    gles2::Outputter* outputter);
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
  void Destroy(bool have_context) override {}
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

  // TODO(crbug.com/940985): Optimize so that this only returns true when
  // deviceTick is needed.
  bool HasPollingWork() const override { return true; }

  void PerformPollingWork() override {
    DCHECK(wgpu_device_);
    DCHECK(wire_serializer_);
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
                 "WebGPUDecoderImpl::PerformPollingWork");
    dawn_procs_.deviceTick(wgpu_device_);
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
  base::StringPiece GetLogPrefix() override {
    NOTIMPLEMENTED();
    return "";
  }
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

  dawn_native::Adapter GetPreferredAdapter(
      PowerPreference power_preference) const;

  error::Error InitDawnDeviceAndSetWireServer(dawn_native::Adapter* adapter);

  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  // Map from the <ID, generation> pair for a wire texture to the shared image
  // representation for it.
  base::flat_map<std::tuple<uint32_t, uint32_t>,
                 std::unique_ptr<SharedImageRepresentationDawn>>
      associated_shared_image_map_;

  std::unique_ptr<dawn_platform::Platform> dawn_platform_;
  std::unique_ptr<WireServerCommandSerializer> wire_serializer_;
  std::unique_ptr<DawnServiceMemoryTransferService> memory_transfer_service_;
  std::unique_ptr<dawn_native::Instance> dawn_instance_;
  std::vector<dawn_native::Adapter> dawn_adapters_;
  DawnProcTable dawn_procs_;
  WGPUDevice wgpu_device_ = nullptr;
  std::unique_ptr<dawn_wire::WireServer> wire_server_;

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
    gles2::Outputter* outputter) {
  return new WebGPUDecoderImpl(client, command_buffer_service,
                               shared_image_manager, memory_tracker, outputter);
}

WebGPUDecoderImpl::WebGPUDecoderImpl(
    DecoderClient* client,
    CommandBufferServiceBase* command_buffer_service,
    SharedImageManager* shared_image_manager,
    MemoryTracker* memory_tracker,
    gles2::Outputter* outputter)
    : WebGPUDecoder(client, command_buffer_service, outputter),
      shared_image_representation_factory_(
          std::make_unique<SharedImageRepresentationFactory>(
              shared_image_manager,
              memory_tracker)),
      dawn_platform_(new DawnPlatform()),
      wire_serializer_(new WireServerCommandSerializer(client)),
      memory_transfer_service_(new DawnServiceMemoryTransferService(this)),
      dawn_instance_(new dawn_native::Instance()),
      dawn_procs_(dawn_native::GetProcs()) {
  dawn_instance_->SetPlatform(dawn_platform_.get());
}

WebGPUDecoderImpl::~WebGPUDecoderImpl() {
  associated_shared_image_map_.clear();

  // Reset the wire server first so all objects are destroyed before the device.
  // TODO(enga): Handle Device/Context lost.
  wire_server_ = nullptr;
  if (wgpu_device_ != nullptr) {
    dawn_procs_.deviceRelease(wgpu_device_);
  }
}

ContextResult WebGPUDecoderImpl::Initialize() {
  DiscoverAdapters();
  return ContextResult::kSuccess;
}

error::Error WebGPUDecoderImpl::InitDawnDeviceAndSetWireServer(
    dawn_native::Adapter* adapter) {
  DCHECK(adapter != nullptr && (*adapter));

  // TODO(jiawei.shao@intel.com): support multiple Dawn devices.
  if (wgpu_device_ != nullptr) {
    DCHECK(wire_server_);
    return error::kNoError;
  }

  wgpu_device_ = adapter->CreateDevice();
  if (wgpu_device_ == nullptr) {
    return error::kLostContext;
  }

  dawn_wire::WireServerDescriptor descriptor = {};
  descriptor.device = wgpu_device_;
  descriptor.procs = &dawn_procs_;
  descriptor.serializer = wire_serializer_.get();
  descriptor.memoryTransferService = memory_transfer_service_.get();

  wire_server_ = std::make_unique<dawn_wire::WireServer>(descriptor);

  return error::kNoError;
}

void WebGPUDecoderImpl::DiscoverAdapters() {
  dawn_instance_->DiscoverDefaultAdapters();
  std::vector<dawn_native::Adapter> adapters = dawn_instance_->GetAdapters();
  for (const dawn_native::Adapter& adapter : adapters) {
#if defined(OS_WIN)
  // On Windows 10, we pick D3D12 backend because the rest of Chromium renders
  // with D3D11. By the same token, we pick the first adapter because ANGLE also
  // picks the first adapter. Later, we'll need to centralize adapter picking
  // such that Dawn and ANGLE are told which adapter to use by Chromium. If we
  // decide to handle multiple adapters, code on the Chromium side will need to
  // change to do appropriate cross adapter copying to make this happen, either
  // manually or by using DirectComposition.
  if (adapter.GetBackendType() == dawn_native::BackendType::D3D12) {
#else
    if (adapter.GetBackendType() != dawn_native::BackendType::Null &&
        adapter.GetBackendType() != dawn_native::BackendType::OpenGL) {
#endif
      dawn_adapters_.push_back(adapter);
#if defined(OS_WIN)
      break;
#endif
  }
  }
}

dawn_native::Adapter WebGPUDecoderImpl::GetPreferredAdapter(
    PowerPreference power_preference) const {
  dawn_native::DeviceType preferred_device_type =
      PowerPreferenceToDawnDeviceType(power_preference);

  dawn_native::Adapter discrete_gpu_adapter = {};
  dawn_native::Adapter integrated_gpu_adapter = {};
  dawn_native::Adapter cpu_adapter = {};
  dawn_native::Adapter unknown_adapter = {};

  for (const dawn_native::Adapter& adapter : dawn_adapters_) {
    if (adapter.GetDeviceType() == preferred_device_type) {
      return adapter;
    }
    switch (adapter.GetDeviceType()) {
      case dawn_native::DeviceType::DiscreteGPU:
        discrete_gpu_adapter = adapter;
        break;
      case dawn_native::DeviceType::IntegratedGPU:
        integrated_gpu_adapter = adapter;
        break;
      case dawn_native::DeviceType::CPU:
        cpu_adapter = adapter;
        break;
      case dawn_native::DeviceType::Unknown:
        unknown_adapter = adapter;
        break;
      default:
        NOTREACHED();
        break;
    }
  }

  // For now, we always prefer the discrete GPU
  if (discrete_gpu_adapter) {
    return discrete_gpu_adapter;
  }
  if (integrated_gpu_adapter) {
    return integrated_gpu_adapter;
  }
  if (cpu_adapter) {
    return cpu_adapter;
  }
  if (unknown_adapter) {
    return unknown_adapter;
  }
  return dawn_native::Adapter();
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

error::Error WebGPUDecoderImpl::HandleRequestAdapter(
    uint32_t immediate_data_size,
    const volatile void* cmd_data) {
  const volatile webgpu::cmds::RequestAdapter& c =
      *static_cast<const volatile webgpu::cmds::RequestAdapter*>(cmd_data);

  PowerPreference power_preference =
      static_cast<PowerPreference>(c.power_preference);
  dawn_native::Adapter requested_adapter =
      GetPreferredAdapter(power_preference);
  if (!requested_adapter) {
    return error::kLostContext;
  }

  // TODO(jiawei.shao@intel.com): support creating device with device descriptor
  return InitDawnDeviceAndSetWireServer(&requested_adapter);
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

  TRACE_EVENT_FLOW_END0(
      TRACE_DISABLED_BY_DEFAULT("gpu.dawn"), "DawnCommands",
      (static_cast<uint64_t>(commands_shm_id) << 32) + commands_shm_offset);

  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("gpu.dawn"),
               "WebGPUDecoderImpl::HandleDawnCommands", "bytes", size);
  if (!wire_server_->HandleCommands(shm_commands, size)) {
    NOTREACHED();
    return error::kLostContext;
  }
  wire_serializer_->Flush();
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
  uint32_t usage = static_cast<WGPUTextureUsage>(c.usage);

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

  // TODO(cwallez@chromium.org): Use device_id/generation when the decoder
  // supports multiple devices.
  if (device_id != 0 || device_generation != 0) {
    DLOG(ERROR) << "AssociateMailbox: Invalid device ID";
    return error::kInvalidArguments;
  }

  static constexpr uint32_t kAllowedTextureUsages = static_cast<uint32_t>(
      WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst |
      WGPUTextureUsage_Sampled | WGPUTextureUsage_OutputAttachment);
  if (usage & ~kAllowedTextureUsages) {
    DLOG(ERROR) << "AssociateMailbox: Invalid usage";
    return error::kInvalidArguments;
  }
  WGPUTextureUsage wgpu_usage = static_cast<WGPUTextureUsage>(usage);

  // Create a WGPUTexture from the mailbox.
  std::unique_ptr<SharedImageRepresentationDawn> shared_image =
      shared_image_representation_factory_->ProduceDawn(mailbox, wgpu_device_);
  if (!shared_image) {
    DLOG(ERROR) << "AssociateMailbox: Couldn't produce shared image";
    return error::kInvalidArguments;
  }

  WGPUTexture texture = shared_image->BeginAccess(wgpu_usage);
  if (!texture) {
    DLOG(ERROR) << "AssociateMailbox: Couldn't begin shared image access";
    return error::kInvalidArguments;
  }

  // Inject the texture in the dawn_wire::Server and remember which shared image
  // it is associated with.
  if (!wire_server_->InjectTexture(texture, id, generation)) {
    DLOG(ERROR) << "AssociateMailbox: Invalid texture ID";
    return error::kInvalidArguments;
  }

  std::tuple<uint32_t, uint32_t> id_and_generation{id, generation};
  auto insertion = associated_shared_image_map_.emplace(
      id_and_generation, std::move(shared_image));

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

  it->second->EndAccess();
  associated_shared_image_map_.erase(it);
  return error::kNoError;
}

}  // namespace webgpu
}  // namespace gpu
