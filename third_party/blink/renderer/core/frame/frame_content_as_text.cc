// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/frame_content_as_text.h"

#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html_element_type_helpers.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

void FrameContentAsText(wtf_size_t max_chars,
                        LocalFrame* frame,
                        StringBuilder& output) {
  Document* document = frame->GetDocument();
  if (!document)
    return;

  if (!frame->View() || frame->View()->CanThrottleRendering())
    return;

  DCHECK(!frame->View()->NeedsLayout());
  DCHECK(!document->NeedsLayoutTreeUpdate());

  if (document->documentElement() &&
      document->documentElement()->GetLayoutObject()) {
    output.Append(document->documentElement()->innerText());
    if (output.length() >= max_chars)
      output.Resize(max_chars);
  }

  // The separator between frames when the frames are converted to plain text.
  const LChar kFrameSeparator[] = {'\n', '\n'};
  const size_t frame_separator_length = std::size(kFrameSeparator);

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
    if (!layout_view || !layout_view->Size().width ||
        !layout_view->Size().height ||
        (layout_view->PhysicalLocation().left + layout_view->Size().width <=
         0) ||
        (layout_view->PhysicalLocation().top + layout_view->Size().height <=
         0) ||
        (owner_layout_object && owner_layout_object->Style() &&
         owner_layout_object->Style()->UsedVisibility() !=
             EVisibility::kVisible)) {
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
    FrameContentAsText(max_chars, cur_local_child, output);
    if (output.length() >= max_chars)
      return;  // Filled up the buffer.
  }
}

}  // namespace blink
