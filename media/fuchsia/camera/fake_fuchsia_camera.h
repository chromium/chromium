// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_CAMERA_FAKE_FUCHSIA_CAMERA_H_
#define MEDIA_FUCHSIA_CAMERA_FAKE_FUCHSIA_CAMERA_H_

#include <fuchsia/camera3/cpp/fidl.h>
#include <fuchsia/camera3/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/cpp/outgoing_directory.h>

#include <optional>
#include <vector>

#include "base/message_loop/message_pump_for_io.h"
#include "base/run_loop.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class FakeCameraStream final
    : public fuchsia::camera3::testing::Stream_TestBase,
      public base::MessagePumpForIO::ZxHandleWatcher {
 public:
  static const gfx::Size kMaxFrameSize;
  static const gfx::Size kDefaultFrameSize;

  // Enum used to specify how sysmem collection allocation is expected to fail.
  enum class SysmemFailMode {
    // Don't simulate sysmem failure.
    kNone,

    // Force Sync() failure. Implemented by dropping one of sysmem collection
    // tokens.
    kFailSync,

    // Force buffer allocation failure. Implemented by setting incompatible
    // constraints.
    kFailAllocation,
  };

  // Verifies that the I420 image stored at |data| matches the frame produced
  // by ProduceFrame().
  static void ValidateFrameData(const uint8_t* data,
                                gfx::Size size,
                                uint8_t salt);

  FakeCameraStream();
  ~FakeCameraStream() override;

  FakeCameraStream(const FakeCameraStream&) = delete;
  FakeCameraStream& operator=(const FakeCameraStream&) = delete;

  void Bind(fidl::InterfaceRequest<fuchsia::camera3::Stream> request);

  // Forces the stream to simulate sysmem buffer collection failure for the
  // first buffer collection.
  void SetFirstBufferCollectionFailMode(SysmemFailMode fail_mode);

  void SetFakeResolution(gfx::Size resolution);
  void SetFakeOrientation(fuchsia::camera3::Orientation orientation);

  // Waits for the buffer collection to be allocated. Returns true if the buffer
  // collection was allocated successfully.
  bool WaitBuffersAllocated();

  // Waits until there is at least one free buffer that can be used for the next
  // frame.
  bool WaitFreeBuffer();

  void ProduceFrame(base::TimeTicks timestamp, uint8_t salt);

 private:
  struct Buffer;

  // fuchsia::camera3::Stream implementation.
  void WatchResolution(WatchResolutionCallback callback) override;
  void WatchOrientation(WatchOrientationCallback callback) override;
  void SetBufferCollection(
      fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>
          token_handle) override;
  void WatchBufferCollection(WatchBufferCollectionCallback callback) override;
  void GetNextFrame(GetNextFrameCallback callback) override;

  // fuchsia::camera3::testing::Stream_TestBase override.
  void NotImplemented_(const std::string& name) override;

  void OnBufferCollectionSyncDone(
      fidl::InterfaceHandle<fuchsia::sysmem2::BufferCollectionToken>
          token_for_client,
      fidl::InterfaceHandle<fuchsia::sysmem2::BufferCollectionToken>
          failed_token);

  void OnBufferCollectionError(zx_status_t status);

  void OnBufferCollectionAllocated(
      fuchsia::sysmem2::BufferCollection_WaitForAllBuffersAllocated_Result
          wait_result);

  // Calls callback for the pending WatchResolution() if the call is pending and
  // resolution has been updated.
  void SendResolution();

  // Calls callback for the pending WatchOrientation() if the call is pending
  // and orientation has been updated.
  void SendOrientation();

  // Calls callback for the pending WatchBufferCollection() if we have a new
  // token and the call is pending.
  void SendBufferCollection();

  // Calls callback for the pending GetNextFrame() if we have a new frame and
  // the call is pending.
  void SendNextFrame();

  // ZxHandleWatcher interface. Used to wait for frame release_fences to get
  // notified when the client releases a buffer.
  void OnZxHandleSignalled(zx_handle_t handle, zx_signals_t signals) override;

  fidl::Binding<fuchsia::camera3::Stream> binding_;

  gfx::Size resolution_ = kDefaultFrameSize;
  fuchsia::camera3::Orientation orientation_ =
      fuchsia::camera3::Orientation::UP;

  std::optional<fuchsia::math::Size> resolution_update_ = fuchsia::math::Size{
      kDefaultFrameSize.width(), kDefaultFrameSize.height()};
  WatchResolutionCallback watch_resolution_callback_;

  std::optional<fuchsia::camera3::Orientation> orientation_update_ =
      fuchsia::camera3::Orientation::UP;
  WatchOrientationCallback watch_orientation_callback_;

  fuchsia::sysmem2::BufferCollectionTokenPtr new_buffer_collection_token_;

  std::optional<fidl::InterfaceHandle<fuchsia::sysmem2::BufferCollectionToken>>
      new_buffer_collection_token_for_client_;
  WatchBufferCollectionCallback watch_buffer_collection_callback_;

  std::optional<fuchsia::camera3::FrameInfo> next_frame_;
  GetNextFrameCallback get_next_frame_callback_;

  fuchsia::sysmem2::AllocatorPtr sysmem_allocator_;
  fuchsia::sysmem2::BufferCollectionPtr buffer_collection_;

  std::optional<base::RunLoop> wait_buffers_allocated_run_loop_;
  std::optional<base::RunLoop> wait_free_buffer_run_loop_;

  std::vector<std::unique_ptr<Buffer>> buffers_;
  size_t num_used_buffers_ = 0;

  size_t frame_counter_ = 0;

  SysmemFailMode first_buffer_collection_fail_mode_ = SysmemFailMode::kNone;
};

class FakeCameraDevice final
    : public fuchsia::camera3::testing::Device_TestBase {
 public:
  FakeCameraDevice();
  ~FakeCameraDevice() override;

  FakeCameraDevice(const FakeCameraDevice&) = delete;
  FakeCameraDevice& operator=(const FakeCameraDevice&) = delete;

  void Bind(fidl::InterfaceRequest<fuchsia::camera3::Device> request);

  FakeCameraStream* stream() { return &stream_; }

  // Sets a custom handler for GetIdentifier() messages.
  void SetGetIdentifierHandler(
      base::RepeatingCallback<void(GetIdentifierCallback)>
          get_identifier_handler);

 private:
  // fuchsia::camera3::Device implementation.
  void GetIdentifier(GetIdentifierCallback callback) override;
  void GetConfigurations(GetConfigurationsCallback callback) override;
  void ConnectToStream(
      uint32_t index,
      fidl::InterfaceRequest<fuchsia::camera3::Stream> request) override;

  // fuchsia::camera3::testing::Device_TestBase override.
  void NotImplemented_(const std::string& name) override;

  fidl::BindingSet<fuchsia::camera3::Device> bindings_;

  FakeCameraStream stream_;

  base::RepeatingCallback<void(GetIdentifierCallback)> get_identifier_handler_;
};

class FakeCameraDeviceWatcher {
 public:
  using DevicesMap = std::map<uint64_t, std::unique_ptr<FakeCameraDevice>>;

  explicit FakeCameraDeviceWatcher(sys::OutgoingDirectory* outgoing_directory);
  ~FakeCameraDeviceWatcher();

  FakeCameraDeviceWatcher(const FakeCameraDeviceWatcher&) = delete;
  FakeCameraDeviceWatcher& operator=(const FakeCameraDeviceWatcher&) = delete;

  void DisconnectClients();

  const DevicesMap& devices() const { return devices_; }

  // Removes camera device from the list and returns the corresponding
  // FakeCameraStream and FakeCameraDevice. The caller may want to hold the
  // returned object, e.g. to ensure that the corresponding FIDL connections
  // are not dropped.
  std::unique_ptr<FakeCameraDevice> RemoveDevice(uint64_t device_id);

 private:
  class Client final
      : public fuchsia::camera3::testing::DeviceWatcher_TestBase {
   public:
    explicit Client(FakeCameraDeviceWatcher* device_watcher);
    ~Client() override;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    void QueueEvent(fuchsia::camera3::WatchDevicesEvent event);

    // fuchsia::camera3::testing::DeviceWatcher_TestBase override.
    void NotImplemented_(const std::string& name) override;

    // fuchsia::camera3::DeviceWatcher implementation.
    void WatchDevices(WatchDevicesCallback callback) override;
    void ConnectToDevice(
        uint64_t id,
        fidl::InterfaceRequest<fuchsia::camera3::Device> request) override;

   private:
    bool initial_list_sent_ = false;
    std::vector<fuchsia::camera3::WatchDevicesEvent> event_queue_;

    WatchDevicesCallback watch_devices_callback_;
    FakeCameraDeviceWatcher* const device_watcher_;
  };

  fidl::BindingSet<fuchsia::camera3::DeviceWatcher, std::unique_ptr<Client>>
      bindings_;

  DevicesMap devices_;

  uint64_t next_device_id_ = 1;
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_CAMERA_FAKE_FUCHSIA_CAMERA_H_
