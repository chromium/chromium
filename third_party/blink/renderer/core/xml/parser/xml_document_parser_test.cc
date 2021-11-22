// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// crbug.com/932380
TEST(XMLDocumentParserTest, NodeNamespaceWithParseError) {
  auto& doc = *Document::CreateForTest();
  doc.SetContent(
      "<html xmlns='http://www.w3.org/1999/xhtml'>"
      "<body><d:foo/></body></html>");

  // The first child of <html> is <parseerror>, not <body>.
  auto* foo = To<Element>(doc.documentElement()->lastChild()->firstChild());
  EXPECT_TRUE(foo->namespaceURI().IsNull()) << foo->namespaceURI();
  EXPECT_TRUE(foo->prefix().IsNull()) << foo->prefix();
  EXPECT_EQ(foo->localName(), "d:foo");
}

// https://crbug.com/1239288
TEST(XMLDocumentParserTest, ParseFragmentWithUnboundNamespacePrefix) {
  auto& doc = *Document::CreateForTest();

  DummyExceptionStateForTesting exception;
  auto* svg =
      doc.createElementNS("http://www.w3.org/2000/svg", "svg", exception);
  EXPECT_TRUE(svg);

  DocumentFragment* fragment = DocumentFragment::Create(doc);
  EXPECT_TRUE(fragment);

  // XMLDocumentParser::StartElementNs should notice that prefix "foo" does not
  // exist and map the element to the null namespace. It should not fall back to
  // the default namespace.
  EXPECT_TRUE(fragment->ParseXML("<foo:bar/>", svg));
  EXPECT_TRUE(fragment->HasOneChild());
  auto* bar = To<Element>(fragment->firstChild());
  EXPECT_TRUE(bar);
  EXPECT_EQ(bar->prefix(), WTF::g_null_atom);
  EXPECT_EQ(bar->namespaceURI(), WTF::g_null_atom);
  EXPECT_EQ(bar->localName(), "foo:bar");
}

}  // namespace blink
