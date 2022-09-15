// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/layout_dump.h"

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "content/web_test/renderer/web_frame_test_proxy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/test/test_web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

using blink::TestWebFrameContentDumper;
using blink::WebFrame;
using blink::WebLocalFrame;

namespace {

std::string DumpFrameHeaderIfNeeded(WebLocalFrame* frame) {
  std::string result;

  // Add header for all but the main frame. Skip empty frames.
  if (frame->Parent() && !frame->GetDocument().DocumentElement().IsNull()) {
    result.append("\n--------\nFrame: '");
    auto* frame_proxy = static_cast<WebFrameTestProxy*>(frame->Client());
    result.append(frame_proxy->GetFrameNameForWebTests());
    result.append("'\n--------\n");
  }

  return result;
}

std::string DumpFrameScrollPosition(WebLocalFrame* frame) {
  std::string result;
  gfx::PointF offset = frame->GetScrollOffset();
  if (offset.x() > 0 || offset.y() > 0) {
    if (frame->Parent()) {
      auto* frame_proxy = static_cast<WebFrameTestProxy*>(frame->Client());
      result = std::string("frame '") + frame_proxy->GetFrameNameForWebTests() +
               "' ";
    }
    base::StringAppendF(&result, "scrolled to %d,%d\n",
                        base::ClampFloor(offset.x()),
                        base::ClampFloor(offset.y()));
  }

  return result;
}

}  // namespace

std::string DumpLayoutAsString(WebLocalFrame* frame, TextResultType type) {
  DCHECK(frame);
  std::string result;

  switch (type) {
    case TextResultType::kEmpty:
      break;
    case TextResultType::kText:
      result += DumpFrameHeaderIfNeeded(frame);
      result += frame->GetDocument().ContentAsTextForTesting().Utf8();
      result += "\n";
      break;
    case TextResultType::kMarkup:
      result += DumpFrameHeaderIfNeeded(frame);
      result += TestWebFrameContentDumper::DumpAsMarkup(frame).Utf8();
      result += "\n";
      break;
    case TextResultType::kLayout:
    case TextResultType::kLayoutAsPrinting:
      if (!frame->Parent()) {
        TestWebFrameContentDumper::LayoutAsTextControls layout_text_behavior =
            TestWebFrameContentDumper::kLayoutAsTextNormal;
        if (type == TextResultType::kLayoutAsPrinting)
          layout_text_behavior |=
              TestWebFrameContentDumper::kLayoutAsTextPrinting;

        result += TestWebFrameContentDumper::DumpLayoutTreeAsText(
                      frame, layout_text_behavior)
                      .Utf8();
      }
      result += DumpFrameScrollPosition(frame);
      break;
  }

  return result;
}

}  // namespace content
