// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/pip_screen_capture_coordinator_impl.h"

#include "build/build_config.h"
#include "content/public/browser/web_contents.h"

namespace content {

PipScreenCaptureCoordinatorImpl::PipScreenCaptureCoordinatorImpl() {}

PipScreenCaptureCoordinatorImpl::~PipScreenCaptureCoordinatorImpl() = default;

void PipScreenCaptureCoordinatorImpl::OnPipShown(
    WebContents& pip_web_contents) {
  std::optional<NativeWindowId> new_pip_window_id;
  new_pip_window_id = GetNativeWindowIdMac(pip_web_contents);
  if (new_pip_window_id) {
    OnPipShown(*new_pip_window_id);
  }
}

void PipScreenCaptureCoordinatorImpl::OnPipShown(
    NativeWindowId new_pip_window_id) {
  if (pip_window_id_ == new_pip_window_id) {
    return;
  }

  pip_window_id_ = new_pip_window_id;
  for (Observer& obs : observers_) {
    obs.OnPipWindowIdChanged(pip_window_id_);
  }
}

void PipScreenCaptureCoordinatorImpl::OnPipClosed() {
  if (!pip_window_id_) {
    return;
  }
  pip_window_id_ = std::nullopt;
  for (Observer& obs : observers_) {
    obs.OnPipWindowIdChanged(pip_window_id_);
  }
}

std::optional<PipScreenCaptureCoordinatorImpl::NativeWindowId>
PipScreenCaptureCoordinatorImpl::PipWindowId() const {
  return pip_window_id_;
}

void PipScreenCaptureCoordinatorImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PipScreenCaptureCoordinatorImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace content
