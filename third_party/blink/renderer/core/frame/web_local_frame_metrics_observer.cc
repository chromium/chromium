// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_local_frame_metrics_observer.h"

#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

WebLocalFrameMetricsObserver::WebLocalFrameMetricsObserver(
    WebLocalFrame* web_local_frame)
    : web_local_frame_(To<WebLocalFrameImpl>(web_local_frame)) {
  // |web_local_frame_| can be null on unit testing or if Observe() is used.
  if (web_local_frame_) {
    web_local_frame_->AddMetricsObserver(this);
  }
}

WebLocalFrameMetricsObserver::~WebLocalFrameMetricsObserver() {
  Observe(nullptr);
}

WebLocalFrame* WebLocalFrameMetricsObserver::GetWebLocalFrame() const {
  return web_local_frame_.Get();
}

void WebLocalFrameMetricsObserver::Observe(WebLocalFrameImpl* web_local_frame) {
  if (web_local_frame_) {
    web_local_frame_->RemoveMetricsObserver(this);
  }

  web_local_frame_ = web_local_frame;
  if (web_local_frame) {
    web_local_frame->AddMetricsObserver(this);
  }
}

void WebLocalFrameMetricsObserver::WebLocalFrameDetached() {
  Observe(nullptr);
  OnFrameDetached();
}

}  // namespace blink
