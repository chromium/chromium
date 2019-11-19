// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_frame_content_dumper.h"

#include "base/stl_util.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_tree_as_text.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

void FrameContentAsPlainText(size_t max_chars,
                             LocalFrame* frame,
                             StringBuilder& output) {
  Document* document = frame->GetDocument();
  if (!document)
    return;

  if (!frame->View() || frame->View()->ShouldThrottleRendering())
    return;

  DCHECK(!frame->View()->NeedsLayout());
  DCHECK(!document->NeedsLayoutTreeUpdate());

  if (document->documentElement()) {
    output.Append(document->documentElement()->innerText());
    if (output.length() >= max_chars)
      output.Resize(max_chars);
  }

  // The separator between frames when the frames are converted to plain text.
  const LChar kFrameSeparator[] = {'\n', '\n'};
  const size_t frame_separator_length = base::size(kFrameSeparator);

  // Recursively walk the children.
  const FrameTree& frame_tree = frame->Tree();
  for (Frame* cur_child = frame_tree.FirstChild(); cur_child;
       cur_child = cur_child->Tree().NextSibling()) {
    auto* cur_local_child = DynamicTo<LocalFrame>(cur_child);
    if (!cur_local_child)
      continue;
    // Ignore the text of non-visible frames.
    LayoutView* layout_view = cur_local_child->ContentLayoutObject();
    LayoutObject* owner_layout_object = cur_local_child->OwnerLayoutObject();
    if (!layout_view || !layout_view->Size().Width() ||
        !layout_view->Size().Height() ||
        (layout_view->Location().X() + layout_view->Size().Width() <= 0) ||
        (layout_view->Location().Y() + layout_view->Size().Height() <= 0) ||
        (owner_layout_object && owner_layout_object->Style() &&
         owner_layout_object->Style()->Visibility() != EVisibility::kVisible)) {
      continue;
    }

    // Make sure the frame separator won't fill up the buffer, and give up if
    // it will. The danger is if the separator will make the buffer longer than
    // maxChars. This will cause the computation above:
    //   maxChars - output->size()
    // to be a negative number which will crash when the subframe is added.
    if (output.length() >= max_chars - frame_separator_length)
      return;

    output.Append(kFrameSeparator, frame_separator_length);
    FrameContentAsPlainText(max_chars, cur_local_child, output);
    if (output.length() >= max_chars)
      return;  // Filled up the buffer.
  }
}

}  // namespace

WebString WebFrameContentDumper::DeprecatedDumpFrameTreeAsText(
    WebLocalFrame* frame,
    size_t max_chars) {
  if (!frame)
    return WebString();
  StringBuilder text;
  FrameContentAsPlainText(max_chars, To<WebLocalFrameImpl>(frame)->GetFrame(),
                          text);
  return text.ToString();
}

WebString WebFrameContentDumper::DumpWebViewAsText(WebView* web_view,
                                                   size_t max_chars) {
  DCHECK(web_view);
  WebLocalFrame* frame = web_view->MainFrame()->ToWebLocalFrame();
  if (!frame)
    return WebString();

  DCHECK(web_view->MainFrameWidget());
  web_view->MainFrameWidget()->UpdateAllLifecyclePhases(
      WebWidget::LifecycleUpdateReason::kTest);

  StringBuilder text;
  FrameContentAsPlainText(max_chars, To<WebLocalFrameImpl>(frame)->GetFrame(),
                          text);
  return text.ToString();
}

WebString WebFrameContentDumper::DumpAsMarkup(WebLocalFrame* frame) {
  if (!frame)
    return WebString();
  return CreateMarkup(To<WebLocalFrameImpl>(frame)->GetFrame()->GetDocument());
}

WebString WebFrameContentDumper::DumpLayoutTreeAsText(
    WebLocalFrame* frame,
    LayoutAsTextControls to_show) {
  if (!frame)
    return WebString();
  LayoutAsTextBehavior behavior = kLayoutAsTextShowAllLayers;

  if (to_show & kLayoutAsTextWithLineTrees)
    behavior |= kLayoutAsTextShowLineTrees;

  if (to_show & kLayoutAsTextDebug) {
    behavior |= kLayoutAsTextShowCompositedLayers | kLayoutAsTextShowAddresses |
                kLayoutAsTextShowIDAndClass | kLayoutAsTextShowLayerNesting;
  }

  if (to_show & kLayoutAsTextPrinting)
    behavior |= kLayoutAsTextPrintingMode;

  return ExternalRepresentation(To<WebLocalFrameImpl>(frame)->GetFrame(),
                                behavior);
}
}
