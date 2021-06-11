// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDER_VIEW_H_
#define CONTENT_PUBLIC_RENDERER_RENDER_VIEW_H_

#include <stddef.h>

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

namespace blink {
class WebView;
}  // namespace blink

namespace content {

class RenderFrame;
class RenderViewVisitor;

// RenderView corresponds to the content container of a renderer's subset
// of the frame tree. A frame tree that spans multiple renderers will have a
// RenderView in each renderer, containing the local frames that belong to
// that renderer. The RenderView holds non-frame-related state that is
// replicated across all renderers, and is a fairly shallow object.
// Generally, most APIs care about state related to the document content which
// should be accessed through RenderFrame instead.
//
// WARNING: Historically RenderView was the path to get to the main frame,
// and the entire frame tree, but that is no longer the case. Usually
// RenderFrame is a more appropriate surface for new code, unless the code is
// agnostic of frames and document content or structure. For more context,
// please see https://crbug.com/467770 and
// https://www.chromium.org/developers/design-documents/site-isolation.
class CONTENT_EXPORT RenderView {
 public:
  // Returns the RenderView containing the given WebView.
  static RenderView* FromWebView(blink::WebView* webview);

  // Visit all RenderViews with a live WebView (i.e., RenderViews that have
  // been closed but not yet destroyed are excluded).
  static void ForEach(RenderViewVisitor* visitor);

  // Get the routing ID of the view.
  virtual int GetRoutingID() = 0;

  // Returns the associated WebView. May return NULL when the view is closing.
  virtual blink::WebView* GetWebView() = 0;

 protected:
  virtual ~RenderView() {}

 private:
  // This interface should only be implemented inside content.
  friend class RenderViewImpl;
  RenderView() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDER_VIEW_H_
