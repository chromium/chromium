// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_LAYOUT_DUMP_H_
#define CONTENT_WEB_TEST_RENDERER_LAYOUT_DUMP_H_

#include <string>

namespace blink {
class WebLocalFrame;
}  // namespace blink

namespace content {

enum class TextResultType {
  kEmpty,
  kText,
  kMarkup,
  kLayout,
  kLayoutAsPrinting,
};

// Dumps textual representation of |frame| contents.  Exact dump mode depends
// on |flags| (i.e. dump_as_text VS dump_as_markup and/or is_printing).
std::string DumpLayoutAsString(blink::WebLocalFrame* frame,
                               TextResultType type);

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_LAYOUT_DUMP_H_
