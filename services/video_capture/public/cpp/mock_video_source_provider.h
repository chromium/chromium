// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_SOURCE_PROVIDER_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_SOURCE_PROVIDER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/video_capture/public/mojom/devices_changed_observer.mojom.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockVideoSourceProvider
    : public video_capture::mojom::VideoSourceProvider {
 public:
  MockVideoSourceProvider();
  ~MockVideoSourceProvider() override;

  void GetVideoSource(const std::string& device_id,
                      mojo::PendingReceiver<video_capture::mojom::VideoSource>
                          source_receiver) override;

  void GetSourceInfos(GetSourceInfosCallback callback) override;

  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingRemote<video_capture::mojom::Producer> producer,
      mojo::PendingReceiver<video_capture::mojom::SharedMemoryVirtualDevice>
          virtual_device_receiver) override;

  void AddTextureVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingReceiver<video_capture::mojom::TextureVirtualDevice>
          virtual_device_receiver) override;
  void RegisterVirtualDevicesChangedObserver(
      mojo::PendingRemote<video_capture::mojom::DevicesChangedObserver>
          observer,
      bool raise_event_if_virtual_devices_already_present) override {
    virtual_device_observers_.Add(std::move(observer));
  }
  void RegisterDevicesChangedObserver(
      mojo::PendingRemote<video_capture::mojom::DevicesChangedObserver>
          observer) override {
    device_observers_.Add(std::move(observer));
  }

  void RaiseVirtualDeviceChangeEvent() {
    for (const auto& observer : virtual_device_observers_) {
      observer->OnDevicesChanged();
    }
  }

  void RaiseDeviceChangeEvent() {
    for (const auto& observer : device_observers_) {
      observer->OnDevicesChanged();
    }
  }

  void Close(CloseCallback callback) override;

  MOCK_METHOD1(DoGetSourceInfos, void(GetSourceInfosCallback& callback));
  MOCK_METHOD2(
      DoGetVideoSource,
      void(const std::string& device_id,
           mojo::PendingReceiver<video_capture::mojom::VideoSource>* receiver));
  MOCK_METHOD3(
      DoAddVirtualDevice,
      void(
          const media::VideoCaptureDeviceInfo& device_info,
          mojo::PendingRemote<video_capture::mojom::Producer> producer,
          mojo::PendingReceiver<video_capture::mojom::SharedMemoryVirtualDevice>
              virtual_device_receiver));
  MOCK_METHOD2(
      DoAddTextureVirtualDevice,
      void(const media::VideoCaptureDeviceInfo& device_info,
           mojo::PendingReceiver<video_capture::mojom::TextureVirtualDevice>
               virtual_device_receiver));
  MOCK_METHOD1(DoClose, void(CloseCallback& callback));

  mojo::RemoteSet<mojom::DevicesChangedObserver> device_observers_;
  mojo::RemoteSet<mojom::DevicesChangedObserver> virtual_device_observers_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_VIDEO_SOURCE_PROVIDER_H_
