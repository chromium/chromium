// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_view_observer.h"

#include "content/renderer/render_view_impl.h"

namespace content {

RenderViewObserver::RenderViewObserver(RenderView* render_view)
    : render_view_(static_cast<RenderViewImpl*>(render_view)) {
  // |render_view_| can be null on unit testing or if Observe() is used.
  if (render_view_) {
    render_view_->AddObserver(this);
  }
}

RenderViewObserver::~RenderViewObserver() {
  if (render_view_)
    render_view_->RemoveObserver(this);
}

RenderView* RenderViewObserver::render_view() {
  return render_view_;
}

void RenderViewObserver::RenderViewGone() {
  render_view_ = nullptr;
}

void RenderViewObserver::Observe(RenderView* render_view) {
  if (render_view_) {
    render_view_->RemoveObserver(this);
  }

  render_view_ = static_cast<RenderViewImpl*>(render_view);
  if (render_view_) {
    render_view_->AddObserver(this);
  }
}

}  // namespace content
