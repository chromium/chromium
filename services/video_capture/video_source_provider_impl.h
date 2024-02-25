// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_PROVIDER_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_PROVIDER_IMPL_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/video_capture/device_factory.h"
#include "services/video_capture/devices_changed_notifier.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"

namespace video_capture {

class VideoSourceImpl;

class VideoSourceProviderImpl : public mojom::VideoSourceProvider {
 public:
  VideoSourceProviderImpl(
      DeviceFactory* device_factory,
      base::RepeatingClosure on_last_client_disconnected_cb);

  VideoSourceProviderImpl(const VideoSourceProviderImpl&) = delete;
  VideoSourceProviderImpl& operator=(const VideoSourceProviderImpl&) = delete;

  ~VideoSourceProviderImpl() override;

  void AddClient(mojo::PendingReceiver<mojom::VideoSourceProvider> receiver);

  // mojom::VideoSourceProvider implementation.
  void GetSourceInfos(GetSourceInfosCallback callback) override;
  void GetVideoSource(
      const std::string& device_id,
      mojo::PendingReceiver<mojom::VideoSource> source_receiver) override;
  void AddSharedMemoryVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingRemote<mojom::Producer> producer,
      mojo::PendingReceiver<mojom::SharedMemoryVirtualDevice>
          virtual_device_receiver) override;
  void AddTextureVirtualDevice(
      const media::VideoCaptureDeviceInfo& device_info,
      mojo::PendingReceiver<mojom::TextureVirtualDevice>
          virtual_device_receiver) override;
  void RegisterVirtualDevicesChangedObserver(
      mojo::PendingRemote<mojom::DevicesChangedObserver> observer,
      bool raise_event_if_virtual_devices_already_present) override;
  void RegisterDevicesChangedObserver(
      mojo::PendingRemote<mojom::DevicesChangedObserver> observer) override;
  void Close(CloseCallback callback) override;

 private:
  void OnClientDisconnected();
  void OnClientDisconnectedOrClosed();
  void OnVideoSourceLastClientDisconnected(const std::string& device_id);

  const raw_ptr<DeviceFactory, DanglingUntriaged> device_factory_;
  base::RepeatingClosure on_last_client_disconnected_cb_;
  int client_count_ = 0;
  int closed_but_not_yet_disconnected_client_count_ = 0;
  mojo::ReceiverSet<mojom::VideoSourceProvider> receivers_;
  std::map<std::string, std::unique_ptr<VideoSourceImpl>> sources_;
  std::optional<DevicesChangedNotifier> devices_changed_notifier_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_SOURCE_PROVIDER_IMPL_H_
