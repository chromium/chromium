// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"

#include <memory>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/bind_post_task.h"
#include "build/build_config.h"
#include "content/browser/media/media_devices_util.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/mojom/mediastream/media_devices.mojom.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/browser/media/capture/sub_capture_target_id_web_contents_helper.h"
#endif

namespace content {

namespace {

using ::blink::mojom::CapturedSurfaceControlResult;

void BindMediaStreamDeviceObserverReceiver(
    GlobalRenderFrameHostId render_frame_host_id,
    mojo::PendingReceiver<blink::mojom::MediaStreamDeviceObserver> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromID(render_frame_host_id);
  if (render_frame_host && render_frame_host->IsRenderFrameLive()) {
    render_frame_host->GetRemoteInterfaces()->GetInterface(std::move(receiver));
  }
}

std::unique_ptr<MediaStreamWebContentsObserver, BrowserThread::DeleteOnUIThread>
StartObservingWebContents(GlobalRenderFrameHostId render_frame_host_id,
                          base::RepeatingClosure focus_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* const web_contents = WebContents::FromRenderFrameHost(
      RenderFrameHost::FromID(render_frame_host_id));
  std::unique_ptr<MediaStreamWebContentsObserver,
                  BrowserThread::DeleteOnUIThread>
      web_contents_observer;
  if (web_contents) {
    web_contents_observer.reset(new MediaStreamWebContentsObserver(
        web_contents, base::BindPostTask(GetIOThreadTaskRunner({}),
                                         std::move(focus_callback))));
  }
  return web_contents_observer;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Checks whether a track living in the WebContents indicated by
// (render_process_id, render_frame_id) may be cropped or restricted
// to the target indicated by |target|.
bool MayApplySubCaptureTarget(GlobalRenderFrameHostId capturing_id,
                              GlobalRenderFrameHostId captured_id,
                              media::mojom::SubCaptureTargetType type,
                              const base::Token& target) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContents* const capturing_wc =
      SubCaptureTargetIdWebContentsHelper::GetRelevantWebContents(capturing_id);
  if (!capturing_wc) {
    return false;
  }

  WebContents* const captured_wc =
      SubCaptureTargetIdWebContentsHelper::GetRelevantWebContents(captured_id);
  if (capturing_wc != captured_wc) {  // Null or not-same-tab.
    return false;
  }

  SubCaptureTargetIdWebContentsHelper* const helper =
      SubCaptureTargetIdWebContentsHelper::FromWebContents(captured_wc);
  if (!helper) {
    // No sub-capture target IDs of this type were produced on this WebContents.
    // Any non-zero ID should be rejected on account of being invalid.
    // A zero ID would ultimately be rejected on account of the track
    // being uncropped/unrestricted, so we can unconditionally reject.
    return false;
  }

  // * target.is_zero() = uncrop-request.
  // * !target.is_zero() = crop-request.
  // TODO(crbug.com/1418194): Extend to support other types.
  return target.is_zero() || helper->IsAssociatedWith(target, type);
}

MediaStreamDispatcherHost::ApplySubCaptureTargetCallback
WrapApplySubCaptureTarget(
    MediaStreamDispatcherHost::ApplySubCaptureTargetCallback callback,
    mojo::ReportBadMessageCallback bad_message_callback) {
  return base::BindOnce(
      [](MediaStreamDispatcherHost::ApplySubCaptureTargetCallback callback,
         mojo::ReportBadMessageCallback bad_message_callback,
         media::mojom::ApplySubCaptureTargetResult result) {
        if (result ==
            media::mojom::ApplySubCaptureTargetResult::kNonIncreasingVersion) {
          std::move(bad_message_callback)
              .Run("Non-increasing sub-capture-target-version.");
          // Intentionally avoid returning. Instead, continue execution and
          // invoke the callback. If the callback were allowed to "drop" that
          // would trigger a DCHECK in the mojom pipe.
          // TODO(crbug.com/40823292): Avoid the necessity for this.
        }
        std::move(callback).Run(result);
      },
      std::move(callback), std::move(bad_message_callback));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

bool AllowedStreamTypeCombination(
    blink::mojom::MediaStreamType audio_stream_type,
    blink::mojom::MediaStreamType video_stream_type) {
  switch (audio_stream_type) {
    // TODO(crbug.com/40211480): Disallow video_stream_type == NO_SERVICE when
    // {video=false} is no longer allowed.
    case blink::mojom::MediaStreamType::NO_SERVICE:
      return blink::IsVideoInputMediaType(video_stream_type) ||
             video_stream_type == blink::mojom::MediaStreamType::NO_SERVICE;
    case blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE:
      return video_stream_type ==
                 blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE ||
             video_stream_type == blink::mojom::MediaStreamType::NO_SERVICE;
    case blink::mojom::MediaStreamType::GUM_TAB_AUDIO_CAPTURE:
      return video_stream_type ==
                 blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE ||
             video_stream_type == blink::mojom::MediaStreamType::NO_SERVICE;
    case blink::mojom::MediaStreamType::GUM_DESKTOP_AUDIO_CAPTURE:
      return video_stream_type ==
             blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE;
    case blink::mojom::MediaStreamType::DISPLAY_AUDIO_CAPTURE:
      return video_stream_type ==
                 blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE ||
             video_stream_type == blink::mojom::MediaStreamType::
                                      DISPLAY_VIDEO_CAPTURE_THIS_TAB ||
             video_stream_type == blink::mojom::MediaStreamType::NO_SERVICE;
    case blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::GUM_TAB_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_THIS_TAB:
    case blink::mojom::MediaStreamType::DISPLAY_VIDEO_CAPTURE_SET:
    case blink::mojom::MediaStreamType::NUM_MEDIA_TYPES:
      return false;
  }
  return false;
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
bool IsValidZoomLevel(int zoom_level) {
  if (blink::kPresetBrowserZoomFactors.size() == 0u) {
    return false;
  }

  if (zoom_level ==
      static_cast<int>(std::ceil(100 * blink::kPresetBrowserZoomFactors[0]))) {
    return true;
  }

  for (size_t i = 1; i < blink::kPresetBrowserZoomFactors.size(); ++i) {
    if (zoom_level == static_cast<int>(std::floor(
                          100 * blink::kPresetBrowserZoomFactors[i]))) {
      return true;
    }
  }

  return false;
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace

int MediaStreamDispatcherHost::next_requester_id_ = 0;

// Holds pending request information so that we process requests only when the
// Webcontent is in focus.
struct MediaStreamDispatcherHost::PendingAccessRequest {
  PendingAccessRequest(
      int32_t page_request_id,
      const blink::StreamControls& controls,
      bool user_gesture,
      blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
      GenerateStreamsCallback callback,
      MediaDeviceSaltAndOrigin salt_and_origin)
      : page_request_id(page_request_id),
        controls(controls),
        user_gesture(user_gesture),
        audio_stream_selection_info_ptr(
            std::move(audio_stream_selection_info_ptr)),
        callback(std::move(callback)),
        salt_and_origin(salt_and_origin) {}
  ~PendingAccessRequest() = default;

  int32_t page_request_id;
  const blink::StreamControls controls;
  bool user_gesture;
  blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr;
  GenerateStreamsCallback callback;
  MediaDeviceSaltAndOrigin salt_and_origin;
};

MediaStreamDispatcherHost::MediaStreamDispatcherHost(
    GlobalRenderFrameHostId render_frame_host_id,
    MediaStreamManager* media_stream_manager)
    : render_frame_host_id_(render_frame_host_id),
      requester_id_(next_requester_id_++),
      media_stream_manager_(media_stream_manager),
      get_salt_and_origin_cb_(
          base::BindRepeating(&GetMediaDeviceSaltAndOrigin)) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // TODO(crbug.com/40203744): Register focus_callback only when needed.
  base::RepeatingClosure focus_callback =
      base::BindRepeating(&MediaStreamDispatcherHost::OnWebContentsFocused,
                          weak_factory_.GetWeakPtr());
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StartObservingWebContents, render_frame_host_id_,
                     std::move(focus_callback)),
      base::BindOnce(&MediaStreamDispatcherHost::SetWebContentsObserver,
                     weak_factory_.GetWeakPtr()));
}

MediaStreamDispatcherHost::~MediaStreamDispatcherHost() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  web_contents_observer_.reset();
  CancelAllRequests();
}

void MediaStreamDispatcherHost::Create(
    GlobalRenderFrameHostId render_frame_host_id,
    MediaStreamManager* media_stream_manager,
    mojo::PendingReceiver<blink::mojom::MediaStreamDispatcherHost> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager->RegisterDispatcherHost(
      std::make_unique<MediaStreamDispatcherHost>(render_frame_host_id,
                                                  media_stream_manager),
      std::move(receiver));
}

void MediaStreamDispatcherHost::SetWebContentsObserver(
    std::unique_ptr<MediaStreamWebContentsObserver,
                    BrowserThread::DeleteOnUIThread> web_contents_observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  web_contents_observer_ = std::move(web_contents_observer);
}

void MediaStreamDispatcherHost::OnDeviceStopped(
    const std::string& label,
    const blink::MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDeviceObserver()->OnDeviceStopped(label, device);
}

void MediaStreamDispatcherHost::OnDeviceChanged(
    const std::string& label,
    const blink::MediaStreamDevice& old_device,
    const blink::MediaStreamDevice& new_device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDeviceObserver()->OnDeviceChanged(label, old_device,
                                                  new_device);
}

void MediaStreamDispatcherHost::OnDeviceRequestStateChange(
    const std::string& label,
    const blink::MediaStreamDevice& device,
    const blink::mojom::MediaStreamStateChange new_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDeviceObserver()->OnDeviceRequestStateChange(label, device,
                                                             new_state);
}

void MediaStreamDispatcherHost::OnDeviceCaptureConfigurationChange(
    const std::string& label,
    const blink::MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetMediaStreamDeviceObserver()->OnDeviceCaptureConfigurationChange(label,
                                                                     device);
}

void MediaStreamDispatcherHost::OnDeviceCaptureHandleChange(
    const std::string& label,
    const blink::MediaStreamDevice& device) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(device.display_media_info);

  GetMediaStreamDeviceObserver()->OnDeviceCaptureHandleChange(label, device);
}

void MediaStreamDispatcherHost::OnZoomLevelChange(
    const std::string& label,
    const blink::MediaStreamDevice& device,
    int zoom_level) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(device.display_media_info);

  if (!base::FeatureList::IsEnabled(
          features::kCapturedSurfaceControlKillswitch)) {
    return;
  }

  GetMediaStreamDeviceObserver()->OnZoomLevelChange(label, device, zoom_level);
}

void MediaStreamDispatcherHost::OnWebContentsFocused() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  while (!pending_requests_.empty()) {
    std::unique_ptr<PendingAccessRequest> request =
        std::move(pending_requests_.front());
    media_stream_manager_->GenerateStreams(
        render_frame_host_id_, requester_id_, request->page_request_id,
        request->controls, std::move(request->salt_and_origin),
        request->user_gesture,
        std::move(request->audio_stream_selection_info_ptr),
        std::move(request->callback),
        base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceStopped,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceChanged,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(
            &MediaStreamDispatcherHost::OnDeviceRequestStateChange,
            weak_factory_.GetWeakPtr()),
        base::BindRepeating(
            &MediaStreamDispatcherHost::OnDeviceCaptureConfigurationChange,
            weak_factory_.GetWeakPtr()),
        base::BindRepeating(
            &MediaStreamDispatcherHost::OnDeviceCaptureHandleChange,
            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&MediaStreamDispatcherHost::OnZoomLevelChange,
                            weak_factory_.GetWeakPtr()));
    pending_requests_.pop_front();
  }
}

void MediaStreamDispatcherHost::GenerateStreamsChecksOnUIThread(
    GlobalRenderFrameHostId render_frame_host_id,
    bool request_all_screens,
    base::OnceCallback<void(MediaDeviceSaltAndOriginCallback)>
        get_salt_and_origin_cb,
    base::OnceCallback<void(GenerateStreamsUIThreadCheckResult)>
        result_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (request_all_screens) {
    CheckRequestAllScreensAllowed(std::move(get_salt_and_origin_cb),
                                  std::move(result_callback),
                                  render_frame_host_id);
    return;
  }

  CheckStreamsPermissionResultReceived(std::move(get_salt_and_origin_cb),
                                       std::move(result_callback),
                                       /*result=*/true);
}

void MediaStreamDispatcherHost::CheckRequestAllScreensAllowed(
    base::OnceCallback<void(MediaDeviceSaltAndOriginCallback)>
        get_salt_and_origin_cb,
    base::OnceCallback<void(GenerateStreamsUIThreadCheckResult)>
        result_callback,
    GlobalRenderFrameHostId render_frame_host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(render_frame_host_id);
  if (!render_frame_host) {
    CheckStreamsPermissionResultReceived(std::move(get_salt_and_origin_cb),
                                         std::move(result_callback),
                                         /*result=*/false);
    return;
  }

  GetContentClient()->browser()->CheckGetAllScreensMediaAllowed(
      render_frame_host,
      base::BindOnce(
          &MediaStreamDispatcherHost::CheckStreamsPermissionResultReceived,
          std::move(get_salt_and_origin_cb), std::move(result_callback)));
}

void MediaStreamDispatcherHost::CheckStreamsPermissionResultReceived(
    base::OnceCallback<void(MediaDeviceSaltAndOriginCallback)>
        get_salt_and_origin_cb,
    base::OnceCallback<void(GenerateStreamsUIThreadCheckResult)>
        result_callback,
    bool result) {
  if (!result) {
    std::move(result_callback)
        .Run({.request_allowed = false,
              .salt_and_origin = MediaDeviceSaltAndOrigin::Empty()});
    return;
  }

  auto got_salt_and_origin = base::BindOnce(
      [](base::OnceCallback<void(GenerateStreamsUIThreadCheckResult)>
             result_callback,
         const MediaDeviceSaltAndOrigin& salt_and_origin) {
        std::move(result_callback)
            .Run({.request_allowed = true, .salt_and_origin = salt_and_origin});
      },
      std::move(result_callback));
  std::move(get_salt_and_origin_cb).Run(std::move(got_salt_and_origin));
}

const mojo::Remote<blink::mojom::MediaStreamDeviceObserver>&
MediaStreamDispatcherHost::GetMediaStreamDeviceObserver() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (media_stream_device_observer_) {
    return media_stream_device_observer_;
  }

  auto dispatcher_receiver =
      media_stream_device_observer_.BindNewPipeAndPassReceiver();
  media_stream_device_observer_.set_disconnect_handler(base::BindOnce(
      &MediaStreamDispatcherHost::OnMediaStreamDeviceObserverConnectionError,
      weak_factory_.GetWeakPtr()));
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&BindMediaStreamDeviceObserverReceiver,
                     render_frame_host_id_, std::move(dispatcher_receiver)));
  return media_stream_device_observer_;
}

void MediaStreamDispatcherHost::OnMediaStreamDeviceObserverConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_device_observer_.reset();
}

void MediaStreamDispatcherHost::CancelAllRequests() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (auto& pending_request : pending_requests_) {
    std::move(pending_request->callback)
        .Run(blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN,
             /*label=*/std::string(),
             /*stream_devices_set=*/nullptr,
             /*pan_tilt_zoom_allowed=*/false);
  }
  pending_requests_.clear();
  media_stream_manager_->CancelAllRequests(render_frame_host_id_,
                                           requester_id_);
}

void MediaStreamDispatcherHost::GenerateStreams(
    int32_t page_request_id,
    const blink::StreamControls& controls,
    bool user_gesture,
    blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
    GenerateStreamsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const std::optional<bad_message::BadMessageReason> bad_message =
      ValidateControlsForGenerateStreams(controls);
  if (bad_message.has_value()) {
    ReceivedBadMessage(render_frame_host_id_.child_id, bad_message.value());
    return;
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaStreamDispatcherHost::GenerateStreamsChecksOnUIThread,
          render_frame_host_id_, controls.request_all_screens,
          base::BindOnce(get_salt_and_origin_cb_, render_frame_host_id_),
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &MediaStreamDispatcherHost::DoGenerateStreams,
              weak_factory_.GetWeakPtr(), page_request_id, controls,
              user_gesture, std::move(audio_stream_selection_info_ptr),
              std::move(callback)))));
}

void MediaStreamDispatcherHost::DoGenerateStreams(
    int32_t page_request_id,
    const blink::StreamControls& controls,
    bool user_gesture,
    blink::mojom::StreamSelectionInfoPtr audio_stream_selection_info_ptr,
    GenerateStreamsCallback callback,
    GenerateStreamsUIThreadCheckResult ui_check_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!ui_check_result.request_allowed) {
    std::move(callback).Run(
        blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        /*label=*/std::string(),
        /*stream_devices_set=*/nullptr,
        /*pan_tilt_zoom_allowed=*/false);
    return;
  }

  MediaDeviceSaltAndOrigin salt_and_origin =
      std::move(ui_check_result.salt_and_origin);
  ui_check_result = {.salt_and_origin = MediaDeviceSaltAndOrigin::Empty()};
  if (!MediaStreamManager::IsOriginAllowed(render_frame_host_id_.child_id,
                                           salt_and_origin.origin())) {
    std::move(callback).Run(
        blink::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN,
        /*label=*/std::string(),
        /*stream_devices_set=*/nullptr,
        /*pan_tilt_zoom_allowed=*/false);
    return;
  }

  bool is_gum_request = (controls.audio.stream_type ==
                         blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE) ||
                        (controls.video.stream_type ==
                         blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  bool needs_focus =
      is_gum_request &&
      base::FeatureList::IsEnabled(features::kUserMediaCaptureOnFocus) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForMediaStream) &&
      !salt_and_origin.is_background();
  if (needs_focus && !salt_and_origin.has_focus()) {
    pending_requests_.push_back(std::make_unique<PendingAccessRequest>(
        page_request_id, controls, user_gesture,
        std::move(audio_stream_selection_info_ptr), std::move(callback),
        salt_and_origin));
    return;
  }

  media_stream_manager_->GenerateStreams(
      render_frame_host_id_, requester_id_, page_request_id, controls,
      std::move(salt_and_origin), user_gesture,
      std::move(audio_stream_selection_info_ptr), std::move(callback),
      base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceStopped,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceChanged,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &MediaStreamDispatcherHost::OnDeviceRequestStateChange,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &MediaStreamDispatcherHost::OnDeviceCaptureConfigurationChange,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &MediaStreamDispatcherHost::OnDeviceCaptureHandleChange,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&MediaStreamDispatcherHost::OnZoomLevelChange,
                          weak_factory_.GetWeakPtr()));
}

void MediaStreamDispatcherHost::CancelRequest(int page_request_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->CancelRequest(render_frame_host_id_, requester_id_,
                                       page_request_id);
}

void MediaStreamDispatcherHost::StopStreamDevice(
    const std::string& device_id,
    const std::optional<base::UnguessableToken>& session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->StopStreamDevice(
      render_frame_host_id_, requester_id_, device_id,
      session_id.value_or(base::UnguessableToken()));
}

void MediaStreamDispatcherHost::OpenDevice(int32_t page_request_id,
                                           const std::string& device_id,
                                           blink::mojom::MediaStreamType type,
                                           OpenDeviceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // OpenDevice is only supported for microphone or webcam capture.
  if (type != blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE &&
      type != blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE) {
    ReceivedBadMessage(render_frame_host_id_.child_id,
                       bad_message::MDDH_INVALID_DEVICE_TYPE_REQUEST);
    return;
  }

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(get_salt_and_origin_cb_, render_frame_host_id_,
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &MediaStreamDispatcherHost::DoOpenDevice,
                         weak_factory_.GetWeakPtr(), page_request_id, device_id,
                         type, std::move(callback)))));
}

void MediaStreamDispatcherHost::DoOpenDevice(
    int32_t page_request_id,
    const std::string& device_id,
    blink::mojom::MediaStreamType type,
    OpenDeviceCallback callback,
    const MediaDeviceSaltAndOrigin& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!MediaStreamManager::IsOriginAllowed(render_frame_host_id_.child_id,
                                           salt_and_origin.origin())) {
    std::move(callback).Run(false /* success */, std::string(),
                            blink::MediaStreamDevice());
    return;
  }

  media_stream_manager_->OpenDevice(
      render_frame_host_id_, requester_id_, page_request_id, device_id, type,
      std::move(salt_and_origin), std::move(callback),
      base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceStopped,
                          weak_factory_.GetWeakPtr()));
}

void MediaStreamDispatcherHost::CloseDevice(const std::string& label) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->CancelRequest(label);
}

void MediaStreamDispatcherHost::SetCapturingLinkSecured(
    const std::optional<base::UnguessableToken>& session_id,
    blink::mojom::MediaStreamType type,
    bool is_secure) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->SetCapturingLinkSecured(
      render_frame_host_id_.child_id,
      session_id.value_or(base::UnguessableToken()), type, is_secure);
}

void MediaStreamDispatcherHost::KeepDeviceAliveForTransfer(
    const base::UnguessableToken& session_id,
    const base::UnguessableToken& transfer_id,
    KeepDeviceAliveForTransferCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!base::FeatureList::IsEnabled(features::kMediaStreamTrackTransfer)) {
    ReceivedBadMessage(render_frame_host_id_.child_id,
                       bad_message::MSDH_KEEP_DEVICE_ALIVE_USE_WITHOUT_FEATURE);
    std::move(callback).Run(/*device_found=*/false);
    return;
  }
  std::move(callback).Run(media_stream_manager_->KeepDeviceAliveForTransfer(
      render_frame_host_id_, requester_id_, session_id, transfer_id));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void MediaStreamDispatcherHost::FocusCapturedSurface(const std::string& label,
                                                     bool focus) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  media_stream_manager_->SetCapturedDisplaySurfaceFocus(
      label, focus,
      /*is_from_microtask=*/false,
      /*is_from_timer=*/false);
}

void MediaStreamDispatcherHost::ApplySubCaptureTarget(
    const base::UnguessableToken& device_id,
    media::mojom::SubCaptureTargetType type,
    const base::Token& sub_capture_target,
    uint32_t sub_capture_target_version,
    ApplySubCaptureTargetCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  const GlobalRenderFrameHostId captured_id =
      media_stream_manager_->video_capture_manager()
          ->GetGlobalRenderFrameHostId(device_id);

  // Hop to the UI thread to verify that cropping or restricting to
  // |sub_capture_target| is permitted from this particular context.
  // Namely, cropping and restricting are currently only allowed
  // for self-capture, so the sub_capture_target has to be associated with the
  // top-level WebContents belonging to this very tab.
  // TODO(crbug.com/40823292): Switch away from the free function version
  // when SelfOwnedReceiver properly supports this.
  GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&MayApplySubCaptureTarget,
                     /*capturing_id=*/render_frame_host_id_, captured_id, type,
                     sub_capture_target),
      base::BindOnce(
          &MediaStreamDispatcherHost::OnSubCaptureTargetValidationComplete,
          weak_factory_.GetWeakPtr(), device_id, type, sub_capture_target,
          sub_capture_target_version,
          WrapApplySubCaptureTarget(std::move(callback),
                                    mojo::GetBadMessageCallback())));
}

void MediaStreamDispatcherHost::SendWheel(
    const base::UnguessableToken& device_id,
    blink::mojom::CapturedWheelActionPtr action,
    SendWheelCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!base::FeatureList::IsEnabled(
          features::kCapturedSurfaceControlKillswitch)) {
    std::move(callback).Run(CapturedSurfaceControlResult::kUnknownError);
    return;
  }

  if (!action || action->relative_x < 0.0 || action->relative_x >= 1.0 ||
      action->relative_y < 0.0 || action->relative_y >= 1.0) {
    ReceivedBadMessage(render_frame_host_id_.child_id,
                       bad_message::MSDH_SEND_WHEEL_INVALID_ACTION);
    std::move(callback).Run(CapturedSurfaceControlResult::kUnknownError);
    return;
  }

  media_stream_manager_->SendWheel(render_frame_host_id_, device_id,
                                   std::move(action), std::move(callback));
}

void MediaStreamDispatcherHost::SetZoomLevel(
    const base::UnguessableToken& device_id,
    int32_t zoom_level,
    SetZoomLevelCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!base::FeatureList::IsEnabled(
          features::kCapturedSurfaceControlKillswitch)) {
    std::move(callback).Run(CapturedSurfaceControlResult::kUnknownError);
    return;
  }

  if (!IsValidZoomLevel(zoom_level)) {
    ReceivedBadMessage(render_frame_host_id_.child_id,
                       bad_message::MSDH_SET_ZOOM_LEVEL_INVALID_LEVEL);
    std::move(callback).Run(CapturedSurfaceControlResult::kUnknownError);
    return;
  }

  media_stream_manager_->SetZoomLevel(render_frame_host_id_, device_id,
                                      zoom_level, std::move(callback));
}

void MediaStreamDispatcherHost::RequestCapturedSurfaceControlPermission(
    const base::UnguessableToken& session_id,
    RequestCapturedSurfaceControlPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!base::FeatureList::IsEnabled(
          features::kCapturedSurfaceControlKillswitch)) {
    std::move(callback).Run(CapturedSurfaceControlResult::kUnknownError);
    return;
  }

  media_stream_manager_->RequestCapturedSurfaceControlPermission(
      render_frame_host_id_, session_id, std::move(callback));
}

void MediaStreamDispatcherHost::OnSubCaptureTargetValidationComplete(
    const base::UnguessableToken& session_id,
    media::mojom::SubCaptureTargetType type,
    const base::Token& target,
    uint32_t sub_capture_target_version,
    ApplySubCaptureTargetCallback callback,
    bool target_passed_validation) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  CHECK(type == media::mojom::SubCaptureTargetType::kCropTarget ||
        type == media::mojom::SubCaptureTargetType::kRestrictionTarget);

  if (!target_passed_validation) {
    std::move(callback).Run(
        media::mojom::ApplySubCaptureTargetResult::kInvalidTarget);
    return;
  }

  media_stream_manager_->video_capture_manager()->ApplySubCaptureTarget(
      session_id, type, target, sub_capture_target_version,
      std::move(callback));
}
#endif

void MediaStreamDispatcherHost::GetOpenDevice(
    int32_t page_request_id,
    const base::UnguessableToken& session_id,
    const base::UnguessableToken& transfer_id,
    GetOpenDeviceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!base::FeatureList::IsEnabled(features::kMediaStreamTrackTransfer)) {
    ReceivedBadMessage(render_frame_host_id_.child_id,
                       bad_message::MSDH_GET_OPEN_DEVICE_USE_WITHOUT_FEATURE);

    std::move(callback).Run(
        blink::mojom::MediaStreamRequestResult::NOT_SUPPORTED, nullptr);
    return;
  }
  // TODO(crbug.com/40058526): Decide whether we need to have another
  // mojo method, called by the first renderer to say "I'm going to be
  // transferring this track, allow the receiving renderer to call GetOpenDevice
  // on it", and whether we can/need to specific the destination renderer/frame
  // in this case.

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(get_salt_and_origin_cb_, render_frame_host_id_,
                     base::BindPostTaskToCurrentDefault(base::BindOnce(
                         &MediaStreamDispatcherHost::DoGetOpenDevice,
                         weak_factory_.GetWeakPtr(), page_request_id,
                         session_id, transfer_id, std::move(callback)))));
}

void MediaStreamDispatcherHost::DoGetOpenDevice(
    int32_t page_request_id,
    const base::UnguessableToken& session_id,
    const base::UnguessableToken& transfer_id,
    GetOpenDeviceCallback callback,
    const MediaDeviceSaltAndOrigin& salt_and_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!MediaStreamManager::IsOriginAllowed(render_frame_host_id_.child_id,
                                           salt_and_origin.origin())) {
    std::move(callback).Run(
        blink::mojom::MediaStreamRequestResult::INVALID_SECURITY_ORIGIN,
        nullptr);
    return;
  }

  media_stream_manager_->GetOpenDevice(
      session_id, transfer_id, render_frame_host_id_, requester_id_,
      page_request_id, salt_and_origin, std::move(callback),
      base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceStopped,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&MediaStreamDispatcherHost::OnDeviceChanged,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &MediaStreamDispatcherHost::OnDeviceRequestStateChange,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &MediaStreamDispatcherHost::OnDeviceCaptureConfigurationChange,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &MediaStreamDispatcherHost::OnDeviceCaptureHandleChange,
          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&MediaStreamDispatcherHost::OnZoomLevelChange,
                          weak_factory_.GetWeakPtr()));
}

std::optional<bad_message::BadMessageReason>
MediaStreamDispatcherHost::ValidateControlsForGenerateStreams(
    const blink::StreamControls& controls) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!AllowedStreamTypeCombination(controls.audio.stream_type,
                                    controls.video.stream_type)) {
    return bad_message::MSDH_INVALID_STREAM_TYPE_COMBINATION;
  }

  if (!controls.audio.requested()) {
    if (controls.suppress_local_audio_playback) {
      return bad_message::
          MSDH_SUPPRESS_LOCAL_AUDIO_PLAYBACK_BUT_AUDIO_NOT_REQUESTED;
    }

    if (controls.hotword_enabled) {
      return bad_message::MSDH_HOTWORD_ENABLED_BUT_AUDIO_NOT_REQUESTED;
    }

    if (controls.disable_local_echo) {
      return bad_message::MSDH_DISABLE_LOCAL_ECHO_BUT_AUDIO_NOT_REQUESTED;
    }

    if (controls.exclude_monitor_type_surfaces &&
        controls.preferred_display_surface ==
            blink::mojom::PreferredDisplaySurface::MONITOR) {
      return bad_message::MSDH_EXCLUDE_MONITORS_BUT_PREFERRED_MONITOR_REQUESTED;
    }
  }

  return std::nullopt;
}

void MediaStreamDispatcherHost::ReceivedBadMessage(
    int render_process_id,
    bad_message::BadMessageReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (bad_message_callback_for_testing_) {
    bad_message_callback_for_testing_.Run(render_process_id, reason);
  }

  bad_message::ReceivedBadMessage(render_process_id, reason);
}

void MediaStreamDispatcherHost::SetBadMessageCallbackForTesting(
    base::RepeatingCallback<void(int, bad_message::BadMessageReason)>
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!bad_message_callback_for_testing_);
  bad_message_callback_for_testing_ = std::move(callback);
}

}  // namespace content
