// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/test/test_web_frame_content_dumper.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/frame_content_as_text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

WebString TestWebFrameContentDumper::DumpWebViewAsText(WebView* web_view,
                                                       size_t max_chars) {
  DCHECK(web_view);
  WebLocalFrame* frame = web_view->MainFrame()->ToWebLocalFrame();

  WebViewImpl* web_view_impl = To<WebViewImpl>(web_view);
  DCHECK(web_view_impl->MainFrameViewWidget());
  // Updating the document lifecycle isn't enough, the BeginFrame() step
  // should come first which runs events such as notifying of media query
  // changes or raf-based events.
  web_view_impl->MainFrameViewWidget()->BeginMainFrame(base::TimeTicks::Now());
  web_view_impl->MainFrameViewWidget()->UpdateAllLifecyclePhases(
      DocumentUpdateReason::kTest);

  StringBuilder text;
  FrameContentAsText(static_cast<wtf_size_t>(max_chars),
                     To<WebLocalFrameImpl>(frame)->GetFrame(), text);
  return text.ToString();
}

WebString TestWebFrameContentDumper::DumpAsMarkup(WebLocalFrame* frame) {
  return CreateMarkup(To<WebLocalFrameImpl>(frame)->GetFrame()->GetDocument());
}

WebString TestWebFrameContentDumper::DumpLayoutTreeAsText(
    WebLocalFrame* frame,
    LayoutAsTextControls to_show) {
  LayoutAsTextBehavior behavior = 0;

  if (to_show & kLayoutAsTextWithLineTrees)
    behavior |= kLayoutAsTextShowLineTrees;

  if (to_show & kLayoutAsTextDebug) {
    behavior |= kLayoutAsTextShowAddresses | kLayoutAsTextShowIDAndClass |
                kLayoutAsTextShowLayerNesting;
  }

  if (to_show & kLayoutAsTextPrinting)
    behavior |= kLayoutAsTextPrintingMode;

  return ExternalRepresentation(To<WebLocalFrameImpl>(frame)->GetFrame(),
                                behavior);
}

}  // namespace blink
