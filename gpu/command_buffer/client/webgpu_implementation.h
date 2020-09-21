// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_H_
#define GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_H_

#include <dawn/webgpu.h>
#include <dawn_wire/WireClient.h>

#include <memory>
#include <utility>
#include <vector>

#include "gpu/command_buffer/client/gpu_control_client.h"
#include "gpu/command_buffer/client/implementation_base.h"
#include "gpu/command_buffer/client/logging.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/client/webgpu_cmd_helper.h"
#include "gpu/command_buffer/client/webgpu_export.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "ui/gl/buildflags.h"

namespace gpu {
namespace webgpu {

class DawnClientMemoryTransferService;

#if BUILDFLAG(USE_DAWN)
class WebGPUCommandSerializer final : public dawn_wire::CommandSerializer {
 public:
  WebGPUCommandSerializer(
      DawnDeviceClientID device_client_id,
      WebGPUCmdHelper* helper,
      DawnClientMemoryTransferService* memory_transfer_service,
      std::unique_ptr<TransferBuffer> c2s_transfer_buffer);
  ~WebGPUCommandSerializer() override;

  // Send WGPUDeviceProperties to the server side
  // Note that this function should only be called once for each
  // WebGPUCommandSerializer object.
  void RequestDeviceCreation(
      uint32_t requested_adapter_id,
      const WGPUDeviceProperties& requested_device_properties);

  // dawn_wire::CommandSerializer implementation
  void* GetCmdSpace(size_t size) final;
  bool Flush() final;

  void SetClientAwaitingFlush(bool awaiting_flush);
  bool ClientAwaitingFlush() const { return client_awaiting_flush_; }

  // Called upon context lost.
  void HandleGpuControlLostContext();

  // For the WebGPUInterface implementation of WebGPUImplementation
  WGPUDevice GetDevice() const;
  ReservedTexture ReserveTexture();
  bool HandleCommands(const char* commands, size_t command_size);

 private:
  DawnDeviceClientID device_client_id_;
  WebGPUCmdHelper* helper_;
  DawnClientMemoryTransferService* memory_transfer_service_;

  std::unique_ptr<dawn_wire::WireClient> wire_client_;

  uint32_t c2s_buffer_default_size_ = 0;
  uint32_t c2s_put_offset_ = 0;
  std::unique_ptr<TransferBuffer> c2s_transfer_buffer_;
  ScopedTransferBufferPtr c2s_buffer_;

  bool client_awaiting_flush_ = false;
};
#endif

class WEBGPU_EXPORT WebGPUImplementation final : public WebGPUInterface,
                                                 public ImplementationBase {
 public:
  explicit WebGPUImplementation(WebGPUCmdHelper* helper,
                                TransferBufferInterface* transfer_buffer,
                                GpuControl* gpu_control);
  ~WebGPUImplementation() override;

  gpu::ContextResult Initialize(const SharedMemoryLimits& limits);

// Include the auto-generated part of this class. We split this because
// it means we can easily edit the non-auto generated parts right here in
// this file instead of having to edit some template or the code generator.
#include "gpu/command_buffer/client/webgpu_implementation_autogen.h"

  // ContextSupport implementation.
  void SetAggressivelyFreeResources(bool aggressively_free_resources) override;
  void Swap(uint32_t flags,
            SwapCompletedCallback complete_callback,
            PresentationCallback presentation_callback) override;
  void SwapWithBounds(const std::vector<gfx::Rect>& rects,
                      uint32_t flags,
                      SwapCompletedCallback swap_completed,
                      PresentationCallback presentation_callback) override;
  void PartialSwapBuffers(const gfx::Rect& sub_buffer,
                          uint32_t flags,
                          SwapCompletedCallback swap_completed,
                          PresentationCallback presentation_callback) override;
  void CommitOverlayPlanes(uint32_t flags,
                           SwapCompletedCallback swap_completed,
                           PresentationCallback presentation_callback) override;
  void ScheduleOverlayPlane(int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            unsigned overlay_texture_id,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& uv_rect,
                            bool enable_blend,
                            unsigned gpu_fence_id) override;
  uint64_t ShareGroupTracingGUID() const override;
  void SetErrorMessageCallback(
      base::RepeatingCallback<void(const char*, int32_t)> callback) override;
  bool ThreadSafeShallowLockDiscardableTexture(uint32_t texture_id) override;
  void CompleteLockDiscardableTexureOnContextThread(
      uint32_t texture_id) override;
  bool ThreadsafeDiscardableTextureIsDeletedForTracing(
      uint32_t texture_id) override;
  void* MapTransferCacheEntry(uint32_t serialized_size) override;
  void UnmapAndCreateTransferCacheEntry(uint32_t type, uint32_t id) override;
  bool ThreadsafeLockTransferCacheEntry(uint32_t type, uint32_t id) override;
  void UnlockTransferCacheEntries(
      const std::vector<std::pair<uint32_t, uint32_t>>& entries) override;
  void DeleteTransferCacheEntry(uint32_t type, uint32_t id) override;
  unsigned int GetTransferBufferFreeSize() const override;
  bool IsJpegDecodeAccelerationSupported() const override;
  bool IsWebPDecodeAccelerationSupported() const override;
  bool CanDecodeWithHardwareAcceleration(
      const cc::ImageHeaderMetadata* image_metadata) const override;

  // InterfaceBase implementation.
  void GenSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void GenUnverifiedSyncTokenCHROMIUM(GLbyte* sync_token) override;
  void VerifySyncTokensCHROMIUM(GLbyte** sync_tokens, GLsizei count) override;
  void WaitSyncTokenCHROMIUM(const GLbyte* sync_token) override;
  bool HasGrContextSupport() const override;

  // ImplementationBase implementation.
  void IssueShallowFlush() override;
  void SetGLError(GLenum error,
                  const char* function_name,
                  const char* msg) override;

  // GpuControlClient implementation.
  void OnGpuControlLostContext() final;
  void OnGpuControlLostContextMaybeReentrant() final;
  void OnGpuControlErrorMessage(const char* message, int32_t id) final;
  void OnGpuControlSwapBuffersCompleted(
      const SwapBuffersCompleteParams& params) final;
  void OnSwapBufferPresented(uint64_t swap_id,
                             const gfx::PresentationFeedback& feedback) final;
  void OnGpuControlReturnData(base::span<const uint8_t> data) final;

  // WebGPUInterface implementation
  const DawnProcTable& GetProcs() const override;
  void FlushCommands() override;
  void FlushCommands(DawnDeviceClientID device_client_id) override;
  void EnsureAwaitingFlush(DawnDeviceClientID device_client_id,
                           bool* needs_flush) override;
  void FlushAwaitingCommands(DawnDeviceClientID device_client_id) override;
  WGPUDevice GetDevice(DawnDeviceClientID device_client_id) override;
  ReservedTexture ReserveTexture(DawnDeviceClientID device_client_id) override;
  bool RequestAdapterAsync(
      PowerPreference power_preference,
      base::OnceCallback<void(int32_t, const WGPUDeviceProperties&)>
          request_adapter_callback) override;
  bool RequestDeviceAsync(
      uint32_t requested_adapter_id,
      const WGPUDeviceProperties& requested_device_properties,
      base::OnceCallback<void(bool, DawnDeviceClientID)>
          request_device_callback) override;
  void RemoveDevice(DawnDeviceClientID device_client_id) override;

 private:
  const char* GetLogPrefix() const { return "webgpu"; }
  void CheckGLError() {}
  DawnRequestAdapterSerial NextRequestAdapterSerial();
  DawnDeviceClientID NextDeviceClientID();

  WebGPUCmdHelper* helper_;
#if BUILDFLAG(USE_DAWN)
  std::unique_ptr<DawnClientMemoryTransferService> memory_transfer_service_;

  WebGPUCommandSerializer* GetCommandSerializerWithDeviceClientID(
      DawnDeviceClientID device_client_id) const;
  void FlushAllCommandSerializers();
  void ClearAllCommandSerializers();
  bool AddNewCommandSerializer(DawnDeviceClientID device_client_id);
  base::flat_map<DawnDeviceClientID, std::unique_ptr<WebGPUCommandSerializer>>
      command_serializers_;
#endif
  DawnProcTable procs_ = {};

  LogSettings log_settings_;

  base::flat_map<DawnRequestAdapterSerial,
                 base::OnceCallback<void(int32_t, const WGPUDeviceProperties&)>>
      request_adapter_callback_map_;
  DawnRequestAdapterSerial request_adapter_serial_ = 0;

  base::flat_map<DawnDeviceClientID,
                 base::OnceCallback<void(bool, DawnDeviceClientID)>>
      request_device_callback_map_;
  DawnDeviceClientID device_client_id_ = 0;

  std::atomic_bool lost_{false};

  DISALLOW_COPY_AND_ASSIGN(WebGPUImplementation);
};

}  // namespace webgpu
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_WEBGPU_IMPLEMENTATION_H_
