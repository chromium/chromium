// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_VIEW_OBSERVER_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_VIEW_OBSERVER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/page_visibility_state.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_sender.h"

namespace blink {
class WebLocalFrame;
}

namespace content {

class RenderView;
class RenderViewImpl;

// Base class for objects that want to filter incoming IPCs, and also get
// notified of changes to the frame.
class CONTENT_EXPORT RenderViewObserver : public IPC::Listener,
                                          public IPC::Sender {
 public:
  // A subclass can use this to delete itself. If it does not, the subclass must
  // always null-check each call to render_view() becase the RenderView can
  // go away at any time.
  virtual void OnDestruct() = 0;

  // These match the WebKit API notifications
  virtual void DidClearWindowObject(blink::WebLocalFrame* frame) {}
  virtual void DidCommitCompositorFrame() {}
  virtual void DidUpdateMainFrameLayout() {}

  virtual void OnZoomLevelChanged() {}

  virtual void OnPageVisibilityChanged(PageVisibilityState visibility_state) {}

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // IPC::Sender implementation.
  bool Send(IPC::Message* message) override;

  RenderView* render_view() const;
  int routing_id() const { return routing_id_; }

 protected:
  explicit RenderViewObserver(RenderView* render_view);
  ~RenderViewObserver() override;

  // Sets |render_view_| to track.
  // Removes itself of previous (if any) |render_view_| observer list and adds
  // to the new |render_view|. Since it assumes that observer outlives
  // render_view, OnDestruct should be overridden.
  void Observe(RenderView* render_view);

 private:
  friend class RenderViewImpl;

  // This is called by the RenderView when it's going away so that this object
  // can null out its pointer.
  void RenderViewGone();

  RenderViewImpl* render_view_;
  // The routing ID of the associated RenderView.
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_VIEW_OBSERVER_H_
