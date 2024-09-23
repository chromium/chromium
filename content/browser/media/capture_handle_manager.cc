// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture_handle_manager.h"

#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/origin.h"

namespace content {

namespace {
// TODO(crbug.com/40181897): Eliminate code duplication with
// desktop_capture_devices_util.cc.
media::mojom::CaptureHandlePtr CreateCaptureHandle(
    RenderFrameHostImpl* capturer,
    WebContents* captured,
    const blink::mojom::CaptureHandleConfig& capture_handle_config) {
  if (!captured) {
    return nullptr;
  }

  if (!capture_handle_config.expose_origin &&
      capture_handle_config.capture_handle.empty()) {
    return nullptr;
  }

  const url::Origin& capturer_origin = capturer->GetLastCommittedOrigin();
  if (!capture_handle_config.all_origins_permitted &&
      base::ranges::none_of(
          capture_handle_config.permitted_origins,
          [capturer_origin](const url::Origin& permitted_origin) {
            return capturer_origin.IsSameOriginWith(permitted_origin);
          })) {
    return nullptr;
  }

  // Observing CaptureHandle wheneither the capturing or the captured party
  // is incognito is disallowed, except for self-capture.
  if (capturer->GetMainFrame() != captured->GetPrimaryMainFrame()) {
    if (capturer->GetBrowserContext()->IsOffTheRecord() ||
        captured->GetBrowserContext()->IsOffTheRecord()) {
      return nullptr;
    }
  }

  auto result = media::mojom::CaptureHandle::New();
  if (capture_handle_config.expose_origin) {
    result->origin = captured->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  }
  result->capture_handle = capture_handle_config.capture_handle;

  return result;
}

bool IsEqual(const media::mojom::CaptureHandlePtr& lhs,
             const media::mojom::CaptureHandlePtr& rhs) {
  if (!lhs || !rhs) {  // If either is null, equal only if both are null.
    return !lhs && !rhs;
  }

  if (lhs->origin.opaque() != rhs->origin.opaque()) {
    return false;  // One is empty, the other is non-empty.
  }

  // Either both are opaque or neither is. We only compare non-opaque origins.
  if (!lhs->origin.opaque()) {
    if (lhs->origin != rhs->origin) {
      return false;
    }
  }

  return lhs->capture_handle == rhs->capture_handle;
}
}  // namespace

class CaptureHandleManager::Observer final : public WebContentsObserver {
 public:
  static std::unique_ptr<Observer> Create(
      const CaptureKey& capture_key,
      GlobalRenderFrameHostId captured,
      GlobalRenderFrameHostId capturer,
      DeviceCaptureHandleChangeCallback handle_change_callback);

  ~Observer() override;

  // Implements WebContentsObserver.
  void OnCaptureHandleConfigUpdate(
      const blink::mojom::CaptureHandleConfig& config) override;

  // Forces an immediate polling of the captured tab for the current config.
  // Reports it back via |handle_change_callback_|.
  void UpdateCaptureHandleConfig();

 private:
  Observer(WebContents* web_contents,
           const CaptureKey& capture_key,
           GlobalRenderFrameHostId capturer,
           DeviceCaptureHandleChangeCallback handle_change_callback);

  const CaptureKey capture_key_;
  const GlobalRenderFrameHostId capturer_;
  const DeviceCaptureHandleChangeCallback handle_change_callback_;
};

std::unique_ptr<CaptureHandleManager::Observer>
CaptureHandleManager::Observer::Create(
    const CaptureKey& capture_key,
    GlobalRenderFrameHostId captured,
    GlobalRenderFrameHostId capturer,
    DeviceCaptureHandleChangeCallback handle_change_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* const capturer_rfhi = RenderFrameHostImpl::FromID(capturer);
  if (!capturer_rfhi || !capturer_rfhi->IsActive()) {
    return nullptr;
  }

  auto* const captured_rfhi = RenderFrameHostImpl::FromID(captured);
  if (!captured_rfhi || !captured_rfhi->IsActive()) {
    return nullptr;
  }

  auto* const captured_web_contents =
      WebContents::FromRenderFrameHost(captured_rfhi);
  if (!captured_web_contents) {
    return nullptr;
  }

  return base::WrapUnique(new Observer(captured_web_contents, capture_key,
                                       capturer,
                                       std::move(handle_change_callback)));
}

CaptureHandleManager::Observer::Observer(
    WebContents* web_contents,
    const CaptureKey& capture_key,
    GlobalRenderFrameHostId capturer,
    DeviceCaptureHandleChangeCallback handle_change_callback)
    : WebContentsObserver(web_contents),
      capture_key_(capture_key),
      capturer_(capturer),
      handle_change_callback_(std::move(handle_change_callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle_change_callback_);
}

CaptureHandleManager::Observer::~Observer() = default;

void CaptureHandleManager::Observer::OnCaptureHandleConfigUpdate(
    const blink::mojom::CaptureHandleConfig& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* const capturer_rfhi = RenderFrameHostImpl::FromID(capturer_);
  if (!capturer_rfhi || !capturer_rfhi->IsActive()) {
    DVLOG(1) << "Invalid capturer: " << capturer_ << ".";
    return;
  }

  handle_change_callback_.Run(
      capture_key_.label, capture_key_.type,
      CreateCaptureHandle(capturer_rfhi, web_contents(), config));
}

void CaptureHandleManager::Observer::UpdateCaptureHandleConfig() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* wc = web_contents();
  if (wc) {
    OnCaptureHandleConfigUpdate(wc->GetCaptureHandleConfig());
  }
}

CaptureHandleManager::CaptureInfo::CaptureInfo(
    std::unique_ptr<Observer> observer,
    media::mojom::CaptureHandlePtr last_capture_handle,
    DeviceCaptureHandleChangeCallback callback)
    : observer(std::move(observer)),
      last_capture_handle(std::move(last_capture_handle)),
      callback(std::move(callback)) {}

CaptureHandleManager::CaptureInfo::~CaptureInfo() = default;

CaptureHandleManager::CaptureHandleManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

CaptureHandleManager::~CaptureHandleManager() {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::IO));
}

void CaptureHandleManager::OnTabCaptureStarted(
    const std::string& label,
    const blink::MediaStreamDevice& captured_device,
    GlobalRenderFrameHostId capturer,
    DeviceCaptureHandleChangeCallback handle_change_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  WebContentsMediaCaptureId captured_tab_id;
  if (!WebContentsMediaCaptureId::Parse(captured_device.id, &captured_tab_id)) {
    DVLOG(1) << "Not a tab-capture ID:" << captured_device.id << ".";
    return;
  }
  const GlobalRenderFrameHostId captured(captured_tab_id.render_process_id,
                                         captured_tab_id.main_render_frame_id);

  const CaptureKey capture_key{label, captured_device.type};

  // base::Unretained(this) is safe because the observer is owned by |this|
  // and both live on the UI thread together.
  std::unique_ptr<Observer> observer = Observer::Create(
      capture_key, captured, capturer,
      base::BindRepeating(&CaptureHandleManager::OnCaptureHandleConfigUpdate,
                          base::Unretained(this)));
  if (!observer) {
    DVLOG(1) << "Observer creation failed.";
    return;
  }

  auto iter = captures_.find(capture_key);
  if (iter == captures_.end()) {
    // Creating a new tracking session.
    const media::mojom::DisplayMediaInformationPtr& info =
        captured_device.display_media_info;
    media::mojom::CaptureHandlePtr capture_handle =
        info ? info->capture_handle.Clone() : nullptr;
    captures_[capture_key] = std::make_unique<CaptureInfo>(
        std::move(observer), std::move(capture_handle),
        std::move(handle_change_callback));
  } else {
    // Updating an existing tracking session in response to a device change.
    iter->second->observer = std::move(observer);
  }

  // The currently executing task comes in response to a chain of tasks juggled
  // between the IO and UI thread. During this time, the CaptureHandleConfig
  // might have changed. Fetch the latest handle.
  captures_[capture_key]->observer->UpdateCaptureHandleConfig();
}

void CaptureHandleManager::OnTabCaptureStopped(
    const std::string& label,
    const blink::MediaStreamDevice& captured_device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  captures_.erase({label, captured_device.type});
}

void CaptureHandleManager::OnTabCaptureDevicesUpdated(
    const std::string& label,
    blink::mojom::StreamDevicesSetPtr new_stream_devices_set,
    GlobalRenderFrameHostId capturer,
    DeviceCaptureHandleChangeCallback handle_change_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(new_stream_devices_set);
  DCHECK_EQ(1u, new_stream_devices_set->stream_devices.size());

  // Pause tracking of all old devices.
  for (auto& capture : captures_) {
    if (capture.first.label == label) {
      capture.second->observer = nullptr;
    }
  }

  // Start tracking any new devices; resume tracking of changed devices.
  const blink::mojom::StreamDevices& new_devices =
      *new_stream_devices_set->stream_devices[0];
  if (new_devices.audio_device.has_value()) {
    OnTabCaptureStarted(label, new_devices.audio_device.value(), capturer,
                        handle_change_callback);
  }
  if (new_devices.video_device.has_value()) {
    OnTabCaptureStarted(label, new_devices.video_device.value(), capturer,
                        handle_change_callback);
  }

  // Forget any old device which was not in |new_devices|.
  for (auto iter = captures_.begin(); iter != captures_.end();) {
    if (!iter->second->observer) {
      iter = captures_.erase(iter);
    } else {
      ++iter;
    }
  }
}

void CaptureHandleManager::OnCaptureHandleConfigUpdate(
    const std::string& label,
    blink::mojom::MediaStreamType type,
    media::mojom::CaptureHandlePtr capture_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iter = captures_.find({label, type});
  if (iter == captures_.end()) {
    DVLOG(1) << "Unknown session.";
    return;
  }

  const CaptureKey& key = iter->first;
  CaptureInfo& info = *iter->second;

  if (IsEqual(capture_handle, info.last_capture_handle)) {
    // Nothing has changed -> do not report. This avoids exposing navigation
    // between non-exposing sites to a potentially malicious render process.
    return;
  }

  iter->second->last_capture_handle = capture_handle.Clone();

  info.callback.Run(key.label, key.type, std::move(capture_handle));
}

}  // namespace content
