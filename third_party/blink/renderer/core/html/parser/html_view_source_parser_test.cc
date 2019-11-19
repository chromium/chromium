// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_view_source_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/document_parser.h"
#include "third_party/blink/renderer/core/html/html_view_source_document.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// This is a regression test for https://crbug.com/664915
TEST(HTMLViewSourceParserTest, DetachThenFinish_ShouldNotCrash) {
  String mime_type("text/html");
  auto* document = MakeGarbageCollected<HTMLViewSourceDocument>(
      DocumentInit::Create(), mime_type);
  auto* parser =
      MakeGarbageCollected<HTMLViewSourceParser>(*document, mime_type);
  // A client may detach the parser from the document.
  parser->Detach();
  // A DocumentWriter may call finish() after detach().
  static_cast<DocumentParser*>(parser)->Finish();
  // The test passed if finish did not crash.
}

}  // namespace blink
