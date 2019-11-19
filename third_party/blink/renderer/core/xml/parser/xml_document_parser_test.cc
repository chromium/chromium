// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

// crbug.com/932380
TEST(XMLDocumentParserTest, NodeNamespaceWithParseError) {
  auto& doc = *MakeGarbageCollected<Document>();
  doc.SetContent(
      "<html xmlns='http://www.w3.org/1999/xhtml'>"
      "<body><d:foo/></body></html>");

  // The first child of <html> is <parseerror>, not <body>.
  auto* foo = To<Element>(doc.documentElement()->lastChild()->firstChild());
  EXPECT_TRUE(foo->namespaceURI().IsNull()) << foo->namespaceURI();
  EXPECT_TRUE(foo->prefix().IsNull()) << foo->prefix();
  EXPECT_EQ(foo->localName(), "d:foo");
}

}  // namespace blink
