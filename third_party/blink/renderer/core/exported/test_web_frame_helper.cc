// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/test/test_web_frame_helper.h"

#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

HTMLFrameOwnerElement* GetOwnerWebElementForWebLocalFrame(
    WebLocalFrame* frame) {
  DCHECK(frame);
  WebLocalFrameImpl* frame_impl = DynamicTo<WebLocalFrameImpl>(frame);
  if (!frame_impl->GetFrame() || !frame_impl->GetFrame()->Owner())
    return nullptr;
  return To<HTMLFrameOwnerElement>(frame_impl->GetFrame()->Owner());
}

// static
void TestWebFrameHelper::FillStaticResponseForSrcdocNavigation(
    WebLocalFrame* frame,
    WebNavigationParams* params) {
  HTMLFrameOwnerElement* owner_element =
      GetOwnerWebElementForWebLocalFrame(frame);
  String srcdoc_value;
  String mime_type = "text/html";
  String charset = "UTF-8";
  if (owner_element->hasAttribute(html_names::kSrcdocAttr)) {
    srcdoc_value = owner_element->getAttribute(html_names::kSrcdocAttr);
  }
  blink::WebNavigationParams::FillStaticResponse(params, mime_type, charset,
                                                 srcdoc_value.Utf8());
}

}  // namespace blink
