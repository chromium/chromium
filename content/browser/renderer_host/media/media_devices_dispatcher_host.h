// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/media_devices_manager.h"
#include "content/common/content_export.h"
#include "content/public/browser/select_audio_output_request.h"
#include "media/base/scoped_async_trace.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_device_descriptor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom.h"
#include "url/origin.h"

namespace content {

class MediaStreamManager;

class CONTENT_EXPORT MediaDevicesDispatcherHost
    : public blink::mojom::MediaDevicesDispatcherHost {
 public:
  MediaDevicesDispatcherHost(GlobalRenderFrameHostId render_frame_host_id,
                             MediaStreamManager* media_stream_manager);

  MediaDevicesDispatcherHost(const MediaDevicesDispatcherHost&) = delete;
  MediaDevicesDispatcherHost& operator=(const MediaDevicesDispatcherHost&) =
      delete;

  ~MediaDevicesDispatcherHost() override;

  static void Create(
      GlobalRenderFrameHostId render_frame_host_id,
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

  void SelectAudioOutput(const std::string& hashed_device_id,
                         SelectAudioOutputCallback callback) override;

  void AddMediaDevicesListener(
      bool subscribe_audio_input,
      bool subscribe_video_input,
      bool subscribe_audio_output,
      mojo::PendingRemote<blink::mojom::MediaDevicesListener> listener)
      override;
  void SetCaptureHandleConfig(
      blink::mojom::CaptureHandleConfigPtr config) override;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void CloseFocusWindowOfOpportunity(const std::string& label) override;
  void ProduceSubCaptureTargetId(
      media::mojom::SubCaptureTargetType type,
      ProduceSubCaptureTargetIdCallback callback) override;
#endif

 private:
  void OnGotTransientUserActivationResult(const std::string& hashed_device_id,
                                          bool has_user_activation);

  void OnAudioOutputPermissionResult(const std::string& hashed_device_id,
                                     MediaDevicesManager::PermissionDeniedState
                                         speaker_selection_permission_state,
                                     bool has_microphone_permission);

  void OnGotSaltAndOriginForAudioOutput(
      const std::string& hashed_device_id,
      bool has_microphone_permission,
      const MediaDeviceSaltAndOrigin& salt_and_origin);

  void OnEnumeratedAudioOutputDevices(
      const std::string& hashed_device_id,
      bool has_microphone_permission,
      const MediaDeviceSaltAndOrigin& salt_and_origin,
      const MediaDeviceEnumeration& enumeration);

  void OnAvailableAudioOutputDevices(
      const std::string& device_id,
      const MediaDeviceEnumeration& enumeration);

  void OnSelectedDeviceInfo(MediaDeviceEnumeration enumeration,
                            base::expected<std::string, SelectAudioOutputError>
                                selected_device_id_or_error);

  void FinalizeSelectAudioOutput(
      MediaDeviceEnumeration enumeration,
      const std::string& selected_device_id,
      const MediaDeviceSaltAndOrigin& salt_and_origin);

  blink::mojom::SelectAudioOutputResultPtr CreateSelectAudioOutputResult(
      const blink::WebMediaDeviceInfo& device_info,
      const MediaDeviceSaltAndOrigin& salt_and_origin);

  friend class MediaDevicesDispatcherHostTest;

  using GetVideoInputDeviceFormatsCallback =
      GetAllVideoInputDeviceFormatsCallback;

  void OnVideoGotSaltAndOrigin(
      GetVideoInputCapabilitiesCallback client_callback,
      const MediaDeviceSaltAndOrigin& salt_and_origin);

  void FinalizeGetVideoInputCapabilities(
      GetVideoInputCapabilitiesCallback client_callback,
      const MediaDeviceSaltAndOrigin& salt_and_origin,
      const MediaDeviceEnumeration& enumeration);

  void OnAudioGotSaltAndOrigin(
      GetAudioInputCapabilitiesCallback client_callback,
      const MediaDeviceSaltAndOrigin& salt_and_origin);

  void GotAudioInputEnumeration(const MediaDeviceEnumeration& enumeration);

  void GotAudioInputParameters(
      size_t index,
      const std::optional<media::AudioParameters>& parameters);

  void FinalizeGetAudioInputCapabilities();

  using ScopedMediaStreamTrace =
      media::TypedScopedAsyncTrace<media::TraceCategory::kMediaStream>;

  void GetVideoInputDeviceFormats(
      const std::string& hashed_device_id,
      bool try_in_use_first,
      GetVideoInputDeviceFormatsCallback client_callback,
      std::unique_ptr<ScopedMediaStreamTrace> scoped_trace,
      const MediaDeviceSaltAndOrigin& salt_and_origin);

  void GetVideoInputDeviceFormatsWithRawId(
      const std::string& hashed_device_id,
      bool try_in_use_first,
      GetVideoInputDeviceFormatsCallback client_callback,
      std::unique_ptr<ScopedMediaStreamTrace> scoped_trace,
      const std::optional<std::string>& raw_id);

  void ReceivedBadMessage(int render_process_id,
                          bad_message::BadMessageReason reason);

  void SetBadMessageCallbackForTesting(
      base::RepeatingCallback<void(int, bad_message::BadMessageReason)>
          callback);

  void SetCaptureHandleConfigCallbackForTesting(
      base::RepeatingCallback<
          void(int, int, blink::mojom::CaptureHandleConfigPtr)> callback);

  // The following const fields can be accessed on any thread.
  const GlobalRenderFrameHostId render_frame_host_id_;

  // The following fields can only be accessed on the IO thread.
  const raw_ptr<MediaStreamManager> media_stream_manager_;

  SelectAudioOutputCallback select_audio_output_callback_;

  struct AudioInputCapabilitiesRequest;
  // Queued requests for audio-input capabilities.
  std::vector<AudioInputCapabilitiesRequest>
      pending_audio_input_capabilities_requests_;
  size_t num_pending_audio_input_parameters_;
  std::vector<blink::mojom::AudioInputDeviceCapabilities>
      current_audio_input_capabilities_;

  std::vector<uint32_t> subscription_ids_;

  base::RepeatingCallback<void(int, bad_message::BadMessageReason)>
      bad_message_callback_for_testing_;

  base::RepeatingCallback<void(int, int, blink::mojom::CaptureHandleConfigPtr)>
      capture_handle_config_callback_for_testing_;

  base::WeakPtrFactory<MediaDevicesDispatcherHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_DEVICES_DISPATCHER_HOST_H_
