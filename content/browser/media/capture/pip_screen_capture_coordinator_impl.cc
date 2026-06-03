// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/pip_screen_capture_coordinator_impl.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "content/browser/media/capture/pip_screen_capture_coordinator_proxy_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "media/capture/capture_switches.h"
#include "ui/gfx/native_ui_types.h"

#if BUILDFLAG(IS_MAC)
#include "content/browser/media/capture/desktop_capture_util_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

namespace content {
namespace {
std::optional<DesktopMediaID::Id> GetPipWindowId(
    WebContents& pip_web_contents) {
#if BUILDFLAG(IS_MAC)
  return GetNativeWindowIdMac(pip_web_contents);
#elif BUILDFLAG(IS_WIN)
  gfx::NativeWindow native_window = pip_web_contents.GetTopLevelNativeWindow();
  if (native_window != nullptr && native_window->GetHost()) {
    HWND hwnd = native_window->GetHost()->GetAcceleratedWidget();
    return reinterpret_cast<DesktopMediaID::Id>(hwnd);
  }
  return std::nullopt;
#elif
#error "Unsupported platform"
#endif
}
}  // namespace

// static
PipScreenCaptureCoordinatorImpl*
PipScreenCaptureCoordinatorImpl::GetInstance() {
  if (!base::FeatureList::IsEnabled(features::kExcludePipFromScreenCapture)) {
    return nullptr;
  }
  static base::NoDestructor<PipScreenCaptureCoordinatorImpl> instance;
  return instance.get();
}

// static
void PipScreenCaptureCoordinatorImpl::AddCapture(
    PipScreenCaptureCoordinatorProxy::CaptureInfo capture_info) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&PipScreenCaptureCoordinatorImpl::AddCapture,
                                  std::move(capture_info)));
    return;
  }

  if (auto* instance = GetInstance()) {
    instance->AddCaptureOnUIThread(std::move(capture_info));
  }
}

// static
void PipScreenCaptureCoordinatorImpl::RemoveCapture(
    const base::UnguessableToken& session_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&PipScreenCaptureCoordinatorImpl::RemoveCapture,
                       session_id));
    return;
  }

  if (auto* instance = GetInstance()) {
    instance->RemoveCaptureOnUIThread(session_id);
  }
}

PipScreenCaptureCoordinatorImpl::PipScreenCaptureCoordinatorImpl() = default;

PipScreenCaptureCoordinatorImpl::~PipScreenCaptureCoordinatorImpl() = default;

void PipScreenCaptureCoordinatorImpl::OnPipShown(
    WebContents& pip_web_contents,
    const GlobalRenderFrameHostId& pip_owner_render_frame_host_id) {
  std::optional<DesktopMediaID::Id> new_pip_window_id =
      GetPipWindowId(pip_web_contents);
  if (new_pip_window_id) {
    OnPipShown(*new_pip_window_id, pip_owner_render_frame_host_id);
  }
}

void PipScreenCaptureCoordinatorImpl::OnPipShown(
    DesktopMediaID::Id new_pip_window_id,
    const GlobalRenderFrameHostId& new_pip_owner_render_frame_host_id) {
  if (pip_window_id_ == new_pip_window_id &&
      pip_owner_render_frame_host_id_ == new_pip_owner_render_frame_host_id) {
    return;
  }

  bool was_excluded = IsExcludedFromScreenCapture();
  pip_window_id_ = new_pip_window_id;
  pip_owner_render_frame_host_id_ = new_pip_owner_render_frame_host_id;
  NotifyStateChanged();
  NotifyExclusionChanged(was_excluded);
}

void PipScreenCaptureCoordinatorImpl::OnPipClosed() {
  if (!pip_window_id_) {
    return;
  }
  pip_window_id_ = std::nullopt;
  pip_owner_render_frame_host_id_ = {};
  NotifyStateChanged();
  // OnPipClosed does not trigger OnExcludeFromScreenCaptureChanged. Since the
  // window is being destroyed, a status update offers no utility and skipping
  // it prevents potential flickering where the window might be momentarily
  // captured.
}

std::optional<DesktopMediaID::Id> PipScreenCaptureCoordinatorImpl::PipWindowId()
    const {
  return pip_window_id_;
}

GlobalRenderFrameHostId
PipScreenCaptureCoordinatorImpl::GetPipOwnerRenderFrameHostId() const {
  return pip_owner_render_frame_host_id_;
}

std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo>
PipScreenCaptureCoordinatorImpl::Captures() const {
  return captures_;
}

std::optional<DesktopMediaID::Id>
PipScreenCaptureCoordinatorImpl::GetPipWindowToExcludeFromScreenCapture(
    DesktopMediaID::Id desktop_id) {
  if (!pip_window_id_) {
    return std::nullopt;
  }

  // The document PiP window should not be excluded if there are captures from
  // other render frame hosts besides the PiP opener.
  for (const auto& capture : captures_) {
    if (capture.desktop_media_id.id == desktop_id &&
        capture.render_frame_host_id != pip_owner_render_frame_host_id_) {
      return std::nullopt;
    }
  }

  return *pip_window_id_;
}

void PipScreenCaptureCoordinatorImpl::AddCaptureOnUIThread(
    PipScreenCaptureCoordinatorProxy::CaptureInfo capture_info) {
  bool was_excluded = IsExcludedFromScreenCapture();
  captures_.push_back(std::move(capture_info));
  NotifyStateChanged();
  NotifyExclusionChanged(was_excluded);
}

void PipScreenCaptureCoordinatorImpl::RemoveCaptureOnUIThread(
    const base::UnguessableToken& session_id) {
  bool was_excluded = IsExcludedFromScreenCapture();
  captures_.erase(std::remove_if(captures_.begin(), captures_.end(),
                                 [&session_id](const auto& c) {
                                   return c.session_id == session_id;
                                 }),
                  captures_.end());
  NotifyStateChanged();
  NotifyExclusionChanged(was_excluded);
}

void PipScreenCaptureCoordinatorImpl::NotifyStateChanged() {
  for (Observer& obs : observers_) {
    obs.OnStateChanged(pip_window_id_, pip_owner_render_frame_host_id_,
                       captures_);
  }
}

void PipScreenCaptureCoordinatorImpl::NotifyExclusionChanged(
    bool was_excluded) {
  bool is_excluded = IsExcludedFromScreenCapture();
  if (was_excluded != is_excluded) {
    for (desktop_capture::PipScreenCaptureExclusionObserver& obs :
         exclusion_observers_) {
      obs.OnExcludeFromScreenCaptureChanged(is_excluded);
    }
  }
}

std::unique_ptr<PipScreenCaptureCoordinatorProxy>
PipScreenCaptureCoordinatorImpl::CreateProxy() {
  return std::make_unique<PipScreenCaptureCoordinatorProxyImpl>(
      weak_factory_.GetWeakPtr(), PipWindowId(), GetPipOwnerRenderFrameHostId(),
      captures_);
}

void PipScreenCaptureCoordinatorImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PipScreenCaptureCoordinatorImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PipScreenCaptureCoordinatorImpl::AddExclusionObserver(
    desktop_capture::PipScreenCaptureExclusionObserver* observer) {
  exclusion_observers_.AddObserver(observer);
}

void PipScreenCaptureCoordinatorImpl::RemoveExclusionObserver(
    desktop_capture::PipScreenCaptureExclusionObserver* observer) {
  exclusion_observers_.RemoveObserver(observer);
}

bool PipScreenCaptureCoordinatorImpl::IsExcludedFromScreenCapture() const {
  if (!base::FeatureList::IsEnabled(features::kExcludePipFromScreenCapture)) {
    return false;
  }
  if (!pip_window_id_ || captures_.empty()) {
    return false;
  }
  for (const auto& capture : captures_) {
    if (capture.render_frame_host_id != pip_owner_render_frame_host_id_) {
      return false;
    }
  }
  return true;
}

void PipScreenCaptureCoordinatorImpl::ResetForTesting() {
  pip_window_id_ = std::nullopt;
  pip_owner_render_frame_host_id_ = {};
  observers_.Clear();
  exclusion_observers_.Clear();
  captures_.clear();
  weak_factory_.InvalidateWeakPtrs();
}

}  // namespace content
