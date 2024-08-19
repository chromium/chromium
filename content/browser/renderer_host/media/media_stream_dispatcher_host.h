// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_

#include <string>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/media/media_stream_web_contents_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/mediastream/media_stream_controls.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {

class MediaStreamManager;

// MediaStreamDispatcherHost is a delegate for Media Stream API messages used by
// MediaStreamImpl.  There is one MediaStreamDispatcherHost per
// RenderFrameHost, the former owned by the latter.
class CONTENT_EXPORT MediaStreamDispatcherHost
    : public blink::mojom::MediaStreamDispatcherHost {
 public:
  MediaStreamDispatcherHost(GlobalRenderFrameHostId render_frame_host_id,
                            MediaStreamManager* media_stream_manager);

  MediaStreamDispatcherHost(const MediaStreamDispatcherHost&) = delete;
  MediaStreamDispatcherHost& operator=(const MediaStreamDispatcherHost&) =
      delete;

  ~MediaStreamDispatcherHost() override;
  static void Create(
      GlobalRenderFrameHostId render_frame_host_id,
      MediaStreamManager* media_stream_manager,
      mojo::PendingReceiver<blink::mojom::MediaStreamDispatcherHost> receiver);

  void OnWebContentsFocused();
  void set_get_salt_and_origin_cb_for_testing(
      GetMediaDeviceSaltAndOriginCallback callback) {
    get_salt_and_origin_cb_ = std::move(callback);
  }
  void SetMediaStreamDeviceObserverForTesting(
      mojo::PendingRemote<blink::mojom::MediaStreamDeviceObserver> observer) {
    media_stream_device_observer_.Bind(std::move(observer));
  }

 private:
  friend class MediaStreamDispatcherHostTest;
  friend class MockMediaStreamDispatcherHost;
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherHostMultiCaptureTest,
                           NoRenderFrameHostMultiCaptureNotAllowed);
  FRIEND_TEST_ALL_PREFIXES(
      MediaStreamDispatcherHostMultiCaptureTest,
      RenderFrameHostExistsButNoPolicySetMultiCaptureNotAllowed);
  FRIEND_TEST_ALL_PREFIXES(MediaStreamDispatcherHostMultiCaptureTest,
                           PolicySetMultiCaptureAllowed);

  struct GenerateStreamsUIThreadCheckResult {
    bool request_allowed = false;
    MediaDeviceSaltAndOrigin salt_and_origin;
  };

  struct PendingAccessRequest;
  using RequestsQueue =
      base::circular_deque<std::unique_ptr<PendingAccessRequest>>;
  RequestsQueue pending_requests_;

  // Performs checks / computations that need to be done on the UI
  // thread (i.e. if a select all screens request is permitted and
  // the computation of the device salt and origin).
  static void GenerateStreamsChecksOnUIThread(
      GlobalRenderFrameHostId render_frame_host_id,
      bool request_all_screens,
      base::OnceCallback<void(MediaDeviceSaltAndOriginCallback)>
          get_salt_and_origin_cb,
      base::OnceCallback<void(GenerateStreamsUIThreadCheckResult)>
          result_callback);

  static void CheckRequestAllScreensAllowed(
      base::OnceCallback<void(MediaDeviceSaltAndOriginCallback)>
          get_salt_and_origin_cb,
      base::OnceCallback<void(GenerateStreamsUIThreadCheckResult)>
          result_callback,
      GlobalRenderFrameHostId render_frame_host_id);

  static void CheckStreamsPermissionResultReceived(
      base::OnceCallback<void(MediaDeviceSaltAndOriginCallback)>
          get_salt_and_origin_cb,
      base::OnceCallback<void(GenerateStreamsUIThreadCheckResult)>
          result_callback,
      bool result);

  const mojo::Remote<blink::mojom::MediaStreamDeviceObserver>&
  GetMediaStreamDeviceObserver();
  void OnMediaStreamDeviceObserverConnectionError();
  void CancelAllRequests();

  // mojom::MediaStreamDispatcherHost implementation
  void GenerateStreams(
      int32_t request_id,
      const blink::StreamControls& controls,
      bool user_gesture,
      blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
      GenerateStreamsCallback callback) override;
  void CancelRequest(int32_t request_id) override;
  void StopStreamDevice(
      const std::string& device_id,
      const std::optional<base::UnguessableToken>& session_id) override;
  void OpenDevice(int32_t request_id,
                  const std::string& device_id,
                  blink::mojom::MediaStreamType type,
                  OpenDeviceCallback callback) override;
  void CloseDevice(const std::string& label) override;
  void SetCapturingLinkSecured(
      const std::optional<base::UnguessableToken>& session_id,
      blink::mojom::MediaStreamType type,
      bool is_secure) override;
  using KeepDeviceAliveForTransferCallback =
      base::OnceCallback<void(bool device_found)>;
  void KeepDeviceAliveForTransfer(
      const base::UnguessableToken& session_id,
      const base::UnguessableToken& transfer_id,
      KeepDeviceAliveForTransferCallback callback) override;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  void FocusCapturedSurface(const std::string& label, bool focus) override;
  void ApplySubCaptureTarget(const base::UnguessableToken& session_id,
                             media::mojom::SubCaptureTargetType type,
                             const base::Token& sub_capture_target,
                             uint32_t sub_capture_target_version,
                             ApplySubCaptureTargetCallback callback) override;
  void SendWheel(const base::UnguessableToken& session_id,
                 blink::mojom::CapturedWheelActionPtr action,
                 SendWheelCallback callback) override;
  void SetZoomLevel(const base::UnguessableToken& session_id,
                    int32_t zoom_level,
                    SetZoomLevelCallback callback) override;
  void RequestCapturedSurfaceControlPermission(
      const base::UnguessableToken& session_id,
      RequestCapturedSurfaceControlPermissionCallback callback) override;
  void OnSubCaptureTargetValidationComplete(
      const base::UnguessableToken& session_id,
      media::mojom::SubCaptureTargetType type,
      const base::Token& target,
      uint32_t sub_capture_target_version,
      ApplySubCaptureTargetCallback callback,
      bool target_passed_validation);
#endif
  void GetOpenDevice(int32_t page_request_id,
                     const base::UnguessableToken& session_id,
                     const base::UnguessableToken& transfer_id,
                     GetOpenDeviceCallback callback) override;
  void DoGetOpenDevice(int32_t page_request_id,
                       const base::UnguessableToken& session_id,
                       const base::UnguessableToken& transfer_id,
                       GetOpenDeviceCallback callback,
                       const MediaDeviceSaltAndOrigin& salt_and_origin);
  void DoGenerateStreams(
      int32_t request_id,
      const blink::StreamControls& controls,
      bool user_gesture,
      blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
      GenerateStreamsCallback callback,
      GenerateStreamsUIThreadCheckResult ui_check_result);
  void DoOpenDevice(int32_t request_id,
                    const std::string& device_id,
                    blink::mojom::MediaStreamType type,
                    OpenDeviceCallback callback,
                    const MediaDeviceSaltAndOrigin& salt_and_origin);

  void OnDeviceStopped(const std::string& label,
                       const blink::MediaStreamDevice& device);
  void OnDeviceChanged(const std::string& label,
                       const blink::MediaStreamDevice& old_device,
                       const blink::MediaStreamDevice& new_device);
  void OnDeviceRequestStateChange(
      const std::string& label,
      const blink::MediaStreamDevice& device,
      const blink::mojom::MediaStreamStateChange new_state);
  void OnDeviceCaptureConfigurationChange(
      const std::string& label,
      const blink::MediaStreamDevice& device);
  void OnDeviceCaptureHandleChange(const std::string& label,
                                   const blink::MediaStreamDevice& device);
  void OnZoomLevelChange(const std::string& label,
                         const blink::MediaStreamDevice& device,
                         int zoom_level);

  void SetWebContentsObserver(
      std::unique_ptr<MediaStreamWebContentsObserver,
                      BrowserThread::DeleteOnUIThread> web_contents_observer);

  // If valid, std::nullopt is returned.
  // If invalid, the relevant BadMessageReason is returned.
  std::optional<bad_message::BadMessageReason>
  ValidateControlsForGenerateStreams(const blink::StreamControls& controls);

  void ReceivedBadMessage(int render_process_id,
                          bad_message::BadMessageReason reason);

  void SetBadMessageCallbackForTesting(
      base::RepeatingCallback<void(int, bad_message::BadMessageReason)>
          callback);

  static int next_requester_id_;

  const GlobalRenderFrameHostId render_frame_host_id_;
  const int requester_id_;
  raw_ptr<MediaStreamManager> media_stream_manager_;
  mojo::Remote<blink::mojom::MediaStreamDeviceObserver>
      media_stream_device_observer_;
  GetMediaDeviceSaltAndOriginCallback get_salt_and_origin_cb_;

  std::unique_ptr<MediaStreamWebContentsObserver,
                  BrowserThread::DeleteOnUIThread>
      web_contents_observer_;

  base::RepeatingCallback<void(int, bad_message::BadMessageReason)>
      bad_message_callback_for_testing_;

  base::WeakPtrFactory<MediaStreamDispatcherHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_MEDIA_STREAM_DISPATCHER_HOST_H_
