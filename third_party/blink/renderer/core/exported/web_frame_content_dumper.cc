// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_frame_content_dumper.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/frame/frame_content_as_text.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

WebString WebFrameContentDumper::DumpFrameTreeAsText(WebLocalFrame* frame,
                                                     size_t max_chars) {
  StringBuilder text;
  FrameContentAsText(base::checked_cast<wtf_size_t>(max_chars),
                     To<WebLocalFrameImpl>(frame)->GetFrame(), text);
  return text.ToString();
}

}  // namespace blink
