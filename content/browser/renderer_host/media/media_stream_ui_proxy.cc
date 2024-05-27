// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/media_stream_ui_proxy.h"

#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/common/content_switches.h"
#include "media/capture/video/fake_video_capture_device.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

bool IsFeatureEnabled(RenderFrameHost* rfh,
                      bool tests_use_fake_render_frame_hosts,
                      blink::mojom::PermissionsPolicyFeature feature) {
  // Some tests don't (or can't) set up the RenderFrameHost. In these cases we
  // just ignore permissions policy checks (there is no permissions policy to
  // test).
  if (!rfh && tests_use_fake_render_frame_hosts)
    return true;

  if (!rfh)
    return false;

  return rfh->IsFeatureEnabled(feature);
}

class MediaStreamUIProxy::Core {
 public:
  explicit Core(const base::WeakPtr<MediaStreamUIProxy>& proxy,
                RenderFrameHostDelegate* test_render_delegate);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core();

  void RequestAccess(std::unique_ptr<MediaStreamRequest> request);
  void OnStarted(gfx::NativeViewId* window_id,
                 bool has_source_callback,
                 const std::string& label,
                 std::vector<DesktopMediaID> screen_capture_ids);
  void OnDeviceStopped(const std::string& label,
                       const DesktopMediaID& media_id);
  void OnDeviceStoppedForSourceChange(const std::string& label,
                                      const DesktopMediaID& old_media_id,
                                      const DesktopMediaID& new_media_id,
                                      bool captured_surface_control_active);

  void OnRegionCaptureRectChanged(
      const std::optional<gfx::Rect>& region_capture_rect);

#if !BUILDFLAG(IS_ANDROID)
  void SetFocus(const DesktopMediaID& media_id,
                bool focus,
                bool is_from_microtask,
                bool is_from_timer);
#endif

  // The type blink::mojom::StreamDevices is not movable, therefore stream
  // devices cannot be captured for usage with PostTask.
  void ProcessAccessRequestResponseForPostTask(
      int render_process_id,
      int render_frame_id,
      blink::mojom::StreamDevicesSetPtr stream_devices_set_ptr,
      blink::mojom::MediaStreamRequestResult result,
      std::unique_ptr<MediaStreamUI> stream_ui);

  void ProcessAccessRequestResponse(
      int render_process_id,
      int render_frame_id,
      const blink::mojom::StreamDevicesSet& stream_devices_set,
      blink::mojom::MediaStreamRequestResult result,
      std::unique_ptr<MediaStreamUI> stream_ui);

  base::WeakPtr<Core> GetWeakPtr() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    // This weak pointer is created in the ctor, which runs on the IO thread.
    // This pointer is always posted from the IO thread to the UI thread,
    // meaning reading |weak_this_| happens on the IO thead, but dereferencing
    // the actual pointer happens in the UI thread. Invalidation happens in the
    // destructor, which runs on the UI thread, so this is safe.
    return weak_this_;
  }

 private:
  friend class FakeMediaStreamUIProxy;
  void ProcessStopRequestFromUI();
  void ProcessChangeSourceRequestFromUI(const DesktopMediaID& media_id,
                                        bool captured_surface_control_active);
  void ProcessStateChangeFromUI(const DesktopMediaID& media,
                                blink::mojom::MediaStreamStateChange);
  RenderFrameHostDelegate* GetRenderFrameHostDelegate(int render_process_id,
                                                      int render_frame_id);

  base::WeakPtr<MediaStreamUIProxy> proxy_;
  std::unique_ptr<MediaStreamUI> ui_;

  bool tests_use_fake_render_frame_hosts_;
  const raw_ptr<RenderFrameHostDelegate> test_render_delegate_;

  base::WeakPtr<Core> weak_this_;

  base::WeakPtrFactory<Core> weak_factory_{this};

  // Used for calls supplied to `ui_`. Invalidated every time a new UI is
  // created.
  base::WeakPtrFactory<Core> weak_factory_for_ui_{this};
};

MediaStreamUIProxy::Core::Core(const base::WeakPtr<MediaStreamUIProxy>& proxy,
                               RenderFrameHostDelegate* test_render_delegate)
    : proxy_(proxy),
      tests_use_fake_render_frame_hosts_(test_render_delegate != nullptr),
      test_render_delegate_(test_render_delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  weak_this_ = weak_factory_.GetWeakPtr();
}

MediaStreamUIProxy::Core::~Core() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void MediaStreamUIProxy::Core::RequestAccess(
    std::unique_ptr<MediaStreamRequest> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostDelegate* render_delegate = GetRenderFrameHostDelegate(
      request->render_process_id, request->render_frame_id);

  // Tab may have gone away, or has no delegate from which to request access.
  if (!render_delegate) {
    ProcessAccessRequestResponse(
        request->render_process_id, request->render_frame_id,
        blink::mojom::StreamDevicesSet(),
        blink::mojom::MediaStreamRequestResult::FAILED_DUE_TO_SHUTDOWN,
        std::unique_ptr<MediaStreamUI>());
    return;
  }

  render_delegate->RequestMediaAccessPermission(
      *request,
      base::BindOnce(&Core::ProcessAccessRequestResponse, weak_this_,
                     request->render_process_id, request->render_frame_id));
}

void MediaStreamUIProxy::Core::OnStarted(
    gfx::NativeViewId* window_id,
    bool has_source_callback,
    const std::string& label,
    std::vector<DesktopMediaID> screen_share_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ui_)
    return;

  MediaStreamUI::SourceCallback device_change_cb;
  if (has_source_callback) {
    device_change_cb =
        base::BindRepeating(&Core::ProcessChangeSourceRequestFromUI,
                            weak_factory_for_ui_.GetWeakPtr());
  }

  *window_id =
      ui_->OnStarted(base::BindRepeating(&Core::ProcessStopRequestFromUI,
                                         weak_factory_for_ui_.GetWeakPtr()),
                     device_change_cb, label, screen_share_ids,
                     base::BindRepeating(&Core::ProcessStateChangeFromUI,
                                         weak_factory_for_ui_.GetWeakPtr()));
}

void MediaStreamUIProxy::Core::OnDeviceStopped(const std::string& label,
                                               const DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ui_) {
    ui_->OnDeviceStopped(label, media_id);
  }
}

void MediaStreamUIProxy::Core::OnDeviceStoppedForSourceChange(
    const std::string& label,
    const DesktopMediaID& old_media_id,
    const DesktopMediaID& new_media_id,
    bool captured_surface_control_active) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ui_) {
    ui_->OnDeviceStoppedForSourceChange(label, old_media_id, new_media_id,
                                        captured_surface_control_active);
    ui_->OnDeviceStopped(label, old_media_id);
  }
}

void MediaStreamUIProxy::Core::OnRegionCaptureRectChanged(
    const std::optional<gfx::Rect>& region_capture_rec) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ui_) {
    ui_->OnRegionCaptureRectChanged(region_capture_rec);
  }
}

#if !BUILDFLAG(IS_ANDROID)
void MediaStreamUIProxy::Core::SetFocus(const DesktopMediaID& media_id,
                                        bool focus,
                                        bool is_from_microtask,
                                        bool is_from_timer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ui_) {
    ui_->SetFocus(media_id, focus, is_from_microtask, is_from_timer);
  }
}
#endif

void MediaStreamUIProxy::Core::ProcessAccessRequestResponseForPostTask(
    int render_process_id,
    int render_frame_id,
    blink::mojom::StreamDevicesSetPtr stream_devices_set_ptr,
    blink::mojom::MediaStreamRequestResult result,
    std::unique_ptr<MediaStreamUI> stream_ui) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(stream_devices_set_ptr);
  ProcessAccessRequestResponse(render_process_id, render_frame_id,
                               *stream_devices_set_ptr, result,
                               std::move(stream_ui));
}

void MediaStreamUIProxy::Core::ProcessAccessRequestResponse(
    int render_process_id,
    int render_frame_id,
    const blink::mojom::StreamDevicesSet& stream_devices_set,
    blink::mojom::MediaStreamRequestResult result,
    std::unique_ptr<MediaStreamUI> stream_ui) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK((result != blink::mojom::MediaStreamRequestResult::OK &&
          stream_devices_set.stream_devices.empty()) ||
         (result == blink::mojom::MediaStreamRequestResult::OK &&
          stream_devices_set.stream_devices.size() == 1u));

  blink::mojom::StreamDevicesSetPtr filtered_devices_set =
      blink::mojom::StreamDevicesSet::New();
  blink::mojom::StreamDevices devices;
  if (!stream_devices_set.stream_devices.empty()) {
    devices = *stream_devices_set.stream_devices[0];
    filtered_devices_set->stream_devices.emplace_back(
        blink::mojom::StreamDevices::New());
  }

  auto* host = RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  if (devices.audio_device.has_value()) {
    const blink::MediaStreamDevice& audio_device = devices.audio_device.value();
    if (audio_device.type !=
            blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE ||
        IsFeatureEnabled(host, tests_use_fake_render_frame_hosts_,
                         blink::mojom::PermissionsPolicyFeature::kMicrophone)) {
      filtered_devices_set->stream_devices[0]->audio_device = audio_device;
    }
  }

  if (devices.video_device.has_value()) {
    const blink::MediaStreamDevice& video_device = devices.video_device.value();
    if (video_device.type !=
            blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE ||
        IsFeatureEnabled(host, tests_use_fake_render_frame_hosts_,
                         blink::mojom::PermissionsPolicyFeature::kCamera)) {
      filtered_devices_set->stream_devices[0]->video_device = video_device;
    }
  }

  if ((filtered_devices_set->stream_devices.empty() ||
       (!filtered_devices_set->stream_devices[0]->audio_device.has_value() &&
        !filtered_devices_set->stream_devices[0]->video_device.has_value())) &&
      result == blink::mojom::MediaStreamRequestResult::OK) {
    result = blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED;
    filtered_devices_set->stream_devices.clear();
  }

  if (stream_ui) {
    // Callbacks that were supplied to the existing `ui_` are no longer
    // applicable. This is important as some implementions (TabSharingUIViews)
    // always run the callback when destroyed. However at the point the UI is
    // replaced while the screencast is ongoing. Invalidating ensures the
    // screencast is not terminated. See crbug.com/1155426 for details.
    weak_factory_for_ui_.InvalidateWeakPtrs();
    ui_ = std::move(stream_ui);
  }

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaStreamUIProxy::ProcessAccessRequestResponse, proxy_,
                     std::move(filtered_devices_set), result));
}

void MediaStreamUIProxy::Core::ProcessStopRequestFromUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaStreamUIProxy::ProcessStopRequestFromUI, proxy_));
}

void MediaStreamUIProxy::Core::ProcessChangeSourceRequestFromUI(
    const DesktopMediaID& media_id,
    bool captured_surface_control_active) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaStreamUIProxy::ProcessChangeSourceRequestFromUI,
                     proxy_, media_id, captured_surface_control_active));
}

void MediaStreamUIProxy::Core::ProcessStateChangeFromUI(
    const DesktopMediaID& media_id,
    blink::mojom::MediaStreamStateChange new_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&MediaStreamUIProxy::ProcessStateChangeFromUI,
                                proxy_, media_id, new_state));
}

RenderFrameHostDelegate* MediaStreamUIProxy::Core::GetRenderFrameHostDelegate(
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (test_render_delegate_)
    return test_render_delegate_;
  RenderFrameHostImpl* host =
      RenderFrameHostImpl::FromID(render_process_id, render_frame_id);
  return host ? host->delegate() : nullptr;
}

// static
std::unique_ptr<MediaStreamUIProxy> MediaStreamUIProxy::Create() {
  return std::unique_ptr<MediaStreamUIProxy>(new MediaStreamUIProxy(nullptr));
}

// static
std::unique_ptr<MediaStreamUIProxy> MediaStreamUIProxy::CreateForTests(
    RenderFrameHostDelegate* render_delegate) {
  return std::unique_ptr<MediaStreamUIProxy>(
      new MediaStreamUIProxy(render_delegate));
}

MediaStreamUIProxy::MediaStreamUIProxy(
    RenderFrameHostDelegate* test_render_delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  core_.reset(new Core(weak_factory_.GetWeakPtr(), test_render_delegate));
}

MediaStreamUIProxy::~MediaStreamUIProxy() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void MediaStreamUIProxy::RequestAccess(
    std::unique_ptr<MediaStreamRequest> request,
    ResponseCallback response_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  response_callback_ = std::move(response_callback);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Core::RequestAccess, core_->GetWeakPtr(),
                                std::move(request)));
}

void MediaStreamUIProxy::OnStarted(
    base::OnceClosure stop_callback,
    MediaStreamUI::SourceCallback source_callback,
    WindowIdCallback window_id_callback,
    const std::string& label,
    std::vector<DesktopMediaID> screen_share_ids,
    MediaStreamUI::StateChangeCallback state_change_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  stop_callback_ = std::move(stop_callback);
  source_callback_ = std::move(source_callback);
  state_change_callback_ = std::move(state_change_callback);

  // Owned by the PostTaskAndReply callback.
  gfx::NativeViewId* window_id = new gfx::NativeViewId(0);

  GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&Core::OnStarted, core_->GetWeakPtr(), window_id,
                     !!source_callback_, label, screen_share_ids),
      base::BindOnce(&MediaStreamUIProxy::OnWindowId,
                     weak_factory_.GetWeakPtr(), std::move(window_id_callback),
                     base::Owned(window_id)));
}

void MediaStreamUIProxy::OnDeviceStopped(const std::string& label,
                                         const DesktopMediaID& media_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Core::OnDeviceStopped, core_->GetWeakPtr(),
                                label, media_id));
}

void MediaStreamUIProxy::OnDeviceStoppedForSourceChange(
    const std::string& label,
    const DesktopMediaID& old_media_id,
    const DesktopMediaID& new_media_id,
    bool captured_surface_control_active) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Core::OnDeviceStoppedForSourceChange,
                                core_->GetWeakPtr(), label, old_media_id,
                                new_media_id, captured_surface_control_active));
}

void MediaStreamUIProxy::OnRegionCaptureRectChanged(
    const std::optional<gfx::Rect>& region_capture_rec) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Core::OnRegionCaptureRectChanged,
                                core_->GetWeakPtr(), region_capture_rec));
}

#if !BUILDFLAG(IS_ANDROID)
void MediaStreamUIProxy::SetFocus(const DesktopMediaID& media_id,
                                  bool focus,
                                  bool is_from_microtask,
                                  bool is_from_timer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Core::SetFocus, core_->GetWeakPtr(), media_id,
                                focus, is_from_microtask, is_from_timer));
}
#endif

void MediaStreamUIProxy::ProcessAccessRequestResponse(
    blink::mojom::StreamDevicesSetPtr stream_devices_set,
    blink::mojom::MediaStreamRequestResult result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!response_callback_.is_null());
  std::move(response_callback_).Run(*stream_devices_set, result);
}

void MediaStreamUIProxy::ProcessStopRequestFromUI() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Careful when changing the following lines: upstream, this function is
  // wrapped into a RepeatingClosure, which allows duplicating it and enabling
  // multiple potentital sources to stop the stream; however only the first
  // invocation should actually stop the stream.
  if (stop_callback_)
    std::move(stop_callback_).Run();
}

void MediaStreamUIProxy::ProcessChangeSourceRequestFromUI(
    const DesktopMediaID& media_id,
    bool captured_surface_control_active) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (source_callback_)
    source_callback_.Run(media_id, captured_surface_control_active);
}

void MediaStreamUIProxy::ProcessStateChangeFromUI(
    const DesktopMediaID& media_id,
    blink::mojom::MediaStreamStateChange new_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (state_change_callback_)
    state_change_callback_.Run(media_id, new_state);
}

void MediaStreamUIProxy::OnWindowId(WindowIdCallback window_id_callback,
                                    gfx::NativeViewId* window_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!window_id_callback.is_null())
    std::move(window_id_callback).Run(*window_id);
}

FakeMediaStreamUIProxy::FakeMediaStreamUIProxy(
    bool tests_use_fake_render_frame_hosts)
    : MediaStreamUIProxy(nullptr) {
  core_->tests_use_fake_render_frame_hosts_ = tests_use_fake_render_frame_hosts;
}

FakeMediaStreamUIProxy::~FakeMediaStreamUIProxy() = default;

void FakeMediaStreamUIProxy::SetAvailableDevices(
    const blink::MediaStreamDevices& devices) {
  devices_ = devices;
}

void FakeMediaStreamUIProxy::SetMicAccess(bool access) {
  mic_access_ = access;
}

void FakeMediaStreamUIProxy::SetCameraAccess(bool access) {
  camera_access_ = access;
}

void FakeMediaStreamUIProxy::SetAudioShare(bool audio_share) {
  audio_share_ = audio_share;
}

void FakeMediaStreamUIProxy::RequestAccess(
    std::unique_ptr<MediaStreamRequest> request,
    ResponseCallback response_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  response_callback_ = std::move(response_callback);

  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseFakeUIForMediaStream) == "deny") {
    // Immediately deny the request.
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &MediaStreamUIProxy::Core::ProcessAccessRequestResponseForPostTask,
            core_->GetWeakPtr(), request->render_process_id,
            request->render_frame_id, blink::mojom::StreamDevicesSet::New(),
            blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
            std::unique_ptr<MediaStreamUI>()));
    return;
  }

  // Use the first capture device of the same media type in the list for the
  // fake UI.
  blink::mojom::StreamDevicesSetPtr devices_set_to_use =
      blink::mojom::StreamDevicesSet::New();
  devices_set_to_use->stream_devices.emplace_back(
      blink::mojom::StreamDevices::New());
  blink::mojom::StreamDevices& devices_to_use =
      *devices_set_to_use->stream_devices[0];
  for (const blink::MediaStreamDevice& device : devices_) {
    if (!devices_to_use.audio_device.has_value() &&
        blink::IsAudioInputMediaType(request->audio_type) &&
        blink::IsAudioInputMediaType(device.type) &&
        (request->requested_audio_device_ids.empty() ||
         request->requested_audio_device_ids.front().empty() ||
         request->requested_audio_device_ids.front() == device.id)) {
      devices_to_use.audio_device = device;
    } else if (!devices_to_use.video_device.has_value() &&
               blink::IsVideoInputMediaType(request->video_type) &&
               blink::IsVideoInputMediaType(device.type) &&
               (request->requested_video_device_ids.empty() ||
                request->requested_video_device_ids.front().empty() ||
                request->requested_video_device_ids.front() == device.id)) {
      devices_to_use.video_device = device;
    }
  }

  // Fail the request if a device doesn't exist for the requested type.
  if ((request->audio_type != blink::mojom::MediaStreamType::NO_SERVICE &&
       !devices_to_use.audio_device.has_value()) ||
      (request->video_type != blink::mojom::MediaStreamType::NO_SERVICE &&
       !devices_to_use.video_device.has_value())) {
    devices_to_use = blink::mojom::StreamDevices();
  }

  if (!audio_share_) {
    devices_to_use.audio_device = std::nullopt;
  }
  const bool is_devices_empty = !devices_to_use.audio_device.has_value() &&
                                !devices_to_use.video_device.has_value();
  if (is_devices_empty) {
    devices_set_to_use->stream_devices.clear();
  }
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &MediaStreamUIProxy::Core::ProcessAccessRequestResponseForPostTask,
          core_->GetWeakPtr(), request->render_process_id,
          request->render_frame_id, std::move(devices_set_to_use),
          is_devices_empty ? blink::mojom::MediaStreamRequestResult::NO_HARDWARE
                           : blink::mojom::MediaStreamRequestResult::OK,
          std::unique_ptr<MediaStreamUI>()));
}

void FakeMediaStreamUIProxy::OnStarted(
    base::OnceClosure stop_callback,
    MediaStreamUI::SourceCallback source_callback,
    WindowIdCallback window_id_callback,
    const std::string& label,
    std::vector<DesktopMediaID> screen_share_ids,
    MediaStreamUI::StateChangeCallback state_change_callback) {}

void FakeMediaStreamUIProxy::OnDeviceStopped(const std::string& label,
                                             const DesktopMediaID& media_id) {}

void FakeMediaStreamUIProxy::OnDeviceStoppedForSourceChange(
    const std::string& label,
    const DesktopMediaID& old_media_id,
    const DesktopMediaID& new_media_id,
    bool captured_surface_control_active) {}

}  // namespace content
