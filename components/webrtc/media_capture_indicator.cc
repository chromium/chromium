// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/media_capture_indicator.h"

#include "base/check.h"
#include "base/not_fatal_until.h"

namespace webrtc {

MediaCaptureIndicator::Observer::~Observer() {
  CHECK(!IsInObserverList(), base::NotFatalUntil::M161);
}

MediaCaptureIndicator::MediaCaptureIndicator() = default;

MediaCaptureIndicator::~MediaCaptureIndicator() = default;

void MediaCaptureIndicator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MediaCaptureIndicator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace webrtc
