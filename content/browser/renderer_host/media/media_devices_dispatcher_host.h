// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/common/content_export.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom.h"
#include "url/origin.h"

namespace content {

class MediaStreamManager;

class CONTENT_EXPORT MediaDevicesDispatcherHost
    : public blink::mojom::MediaDevicesDispatcherHost {
 public:
  MediaDevicesDispatcherHost(int render_process_id,
                             int render_frame_id,
                             MediaStreamManager* media_stream_manager);
  ~MediaDevicesDispatcherHost() override;

  static void Create(
      int render_process_id,
      int render_frame_id,
      MediaStreamManager* media_stream_manager,
      mojo::PendingReceiver<blink::mojom::MediaDevicesDispatcherHost> receiver);

  // blink::mojom::MediaDevicesDispatcherHost implementation.
  void EnumerateDevices(bool request_audio_input,
                        bool request_video_input,
                        bool request_audio_output,
                        bool request_video_input_capabilities,
                        bool request_audio_input_capabilities,
                        EnumerateDevicesCallback client_callback) override;
  void GetVideoInputCapabilities(
      GetVideoInputCapabilitiesCallback client_callback) override;
  void GetAllVideoInputDeviceFormats(
      const std::string& device_id,
      GetAllVideoInputDeviceFormatsCallback client_callback) override;
  void GetAvailableVideoInputDeviceFormats(
      const std::string& device_id,
      GetAvailableVideoInputDeviceFormatsCallback client_callback) override;
  void GetAudioInputCapabilities(
      GetAudioInputCapabilitiesCallback client_callback) override;
  void AddMediaDevicesListener(
      bool subscribe_audio_input,
      bool subscribe_video_input,
      bool subscribe_audio_output,
      mojo::PendingRemote<blink::mojom::MediaDevicesListener> listener)
      override;

 private:
  using GetVideoInputDeviceFormatsCallback =
      GetAllVideoInputDeviceFormatsCallback;

  void GetDefaultVideoInputDeviceID(
      GetVideoInputCapabilitiesCallback client_callback,
      MediaDeviceSaltAndOrigin salt_and_origin);

  void GotDefaultVideoInputDeviceID(
      GetVideoInputCapabilitiesCallback client_callback,
      MediaDeviceSaltAndOrigin salt_and_origin,
      const std::string& default_device_id);

  void FinalizeGetVideoInputCapabilities(
      GetVideoInputCapabilitiesCallback client_callback,
      const MediaDeviceSaltAndOrigin& salt_and_origin,
      const std::string& default_device_id,
      const MediaDeviceEnumeration& enumeration);

  void GetDefaultAudioInputDeviceID(
      GetAudioInputCapabilitiesCallback client_callback,
      const MediaDeviceSaltAndOrigin& salt_and_origin);

  void GotDefaultAudioInputDeviceID(const std::string& default_device_id);

  void GotAudioInputEnumeration(const std::string& default_device_id,
                                const MediaDeviceEnumeration& enumeration);

  void GotAudioInputParameters(
      size_t index,
      const base::Optional<media::AudioParameters>& parameters);

  void FinalizeGetAudioInputCapabilities();

  void GetVideoInputDeviceFormats(
      const std::string& device_id,
      bool try_in_use_first,
      GetVideoInputDeviceFormatsCallback client_callback);
  void EnumerateVideoDevicesForFormats(
      GetVideoInputDeviceFormatsCallback client_callback,
      const std::string& device_id,
      bool try_in_use_first,
      const MediaDeviceSaltAndOrigin& salt_and_origin);
  void FinalizeGetVideoInputDeviceFormats(
      GetVideoInputDeviceFormatsCallback client_callback,
      const std::string& device_id,
      bool try_in_use_first,
      const std::string& device_id_salt,
      const url::Origin& security_origin,
      const media::VideoCaptureDeviceDescriptors& device_descriptors);

  // The following const fields can be accessed on any thread.
  const int render_process_id_;
  const int render_frame_id_;

  // The following fields can only be accessed on the IO thread.
  MediaStreamManager* media_stream_manager_;

  struct AudioInputCapabilitiesRequest;
  // Queued requests for audio-input capabilities.
  std::vector<AudioInputCapabilitiesRequest>
      pending_audio_input_capabilities_requests_;
  size_t num_pending_audio_input_parameters_;
  std::vector<blink::mojom::AudioInputDeviceCapabilities>
      current_audio_input_capabilities_;

  std::vector<uint32_t> subscription_ids_;

  base::WeakPtrFactory<MediaDevicesDispatcherHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaDevicesDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
