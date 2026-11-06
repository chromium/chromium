// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webrtc/media_capture_dispatcher.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/not_fatal_until.h"
#include "content/public/browser/browser_thread.h"

namespace webrtc {

namespace {
MediaCaptureDispatcher* g_instance = nullptr;
}  // namespace

// static
MediaCaptureDispatcher* MediaCaptureDispatcher::GetInstance() {
  return g_instance;
}

MediaCaptureDispatcher::MediaCaptureDispatcher() {
  CHECK(!g_instance);
  g_instance = this;
}

MediaCaptureDispatcher::~MediaCaptureDispatcher() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

void MediaCaptureDispatcher::AddObserver(Observer* observer) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  if (!observers_.HasObserver(observer)) {
    observers_.AddObserver(observer);
  }
}

void MediaCaptureDispatcher::RemoveObserver(Observer* observer) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M161);
  observers_.RemoveObserver(observer);
}

}  // namespace webrtc
