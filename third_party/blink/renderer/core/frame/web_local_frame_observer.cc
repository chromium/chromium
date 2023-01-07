// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_local_frame_observer.h"

#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

WebLocalFrameObserver::WebLocalFrameObserver(WebLocalFrame* web_local_frame)
    : web_local_frame_(To<WebLocalFrameImpl>(web_local_frame)) {
  // |web_local_frame_| can be null on unit testing or if Observe() is used.
  if (web_local_frame_) {
    web_local_frame_->AddObserver(this);
  }
}

WebLocalFrameObserver::~WebLocalFrameObserver() {
  Observe(nullptr);
}

WebLocalFrame* WebLocalFrameObserver::GetWebLocalFrame() const {
  return web_local_frame_.Get();
}

void WebLocalFrameObserver::Observe(WebLocalFrameImpl* web_local_frame) {
  if (web_local_frame_) {
    web_local_frame_->RemoveObserver(this);
  }

  web_local_frame_ = web_local_frame;
  if (web_local_frame) {
    web_local_frame->AddObserver(this);
  }
}

void WebLocalFrameObserver::WebLocalFrameDetached() {
  Observe(nullptr);
  OnFrameDetached();
}

}  // namespace blink
