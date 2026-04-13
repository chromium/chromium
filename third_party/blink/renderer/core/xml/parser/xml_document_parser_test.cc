// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// crbug.com/932380
TEST(XMLDocumentParserTest, NodeNamespaceWithParseError) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  execution_context.GetExecutionContext().SetUpSecurityContextForTesting();
  auto& doc = *Document::CreateForTest(execution_context.GetExecutionContext());
  doc.SetContent(
      "<html xmlns='http://www.w3.org/1999/xhtml'>"
      "<body><d:foo/></body></html>");

  // The first child of <html> is <parseerror>, not <body>.
  auto* foo = To<Element>(doc.documentElement()->lastChild()->firstChild());
  if (RuntimeEnabledFeatures::XMLParsingRustEnabled() ||
      RuntimeEnabledFeatures::XMLRustForNonXsltEnabled()) {
    // The Rust xml parser does not generate an element for the unbound d:foo
    // prefix. The Rust xml parser also gets used when a document is created
    // without a window, as in Document::CreateForTest().
    EXPECT_FALSE(foo);
  } else {
    EXPECT_TRUE(foo->namespaceURI().IsNull()) << foo->namespaceURI();
    EXPECT_TRUE(foo->prefix().IsNull()) << foo->prefix();
    EXPECT_EQ(foo->localName(), "d:foo");
  }
}

// https://crbug.com/1239288
TEST(XMLDocumentParserTest, ParseFragmentWithUnboundNamespacePrefix) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  execution_context.GetExecutionContext().SetUpSecurityContextForTesting();
  auto& doc = *Document::CreateForTest(execution_context.GetExecutionContext());

  DummyExceptionStateForTesting exception;
  auto* svg = doc.createElementNS(svg_names::kNamespaceURI, AtomicString("svg"),
                                  exception);
  EXPECT_TRUE(svg);

  DocumentFragment* fragment = DocumentFragment::Create(doc);
  EXPECT_TRUE(fragment);

  // XMLDocumentParser::StartElementNs should notice that prefix "foo" does not
  // exist and map the element to the null namespace. It should not fall back to
  // the default namespace.
  EXPECT_TRUE(fragment->ParseXML("<foo:bar/>", svg, ASSERT_NO_EXCEPTION));
  EXPECT_TRUE(fragment->HasOneChild());
  auto* bar = To<Element>(fragment->firstChild());
  EXPECT_TRUE(bar);
  EXPECT_EQ(bar->prefix(), g_null_atom);
  EXPECT_EQ(bar->namespaceURI(), g_null_atom);
  EXPECT_EQ(bar->localName(), "foo:bar");
}

class XMLDocumentParserParameterizedTest
    : public testing::Test,
      public testing::WithParamInterface<bool> {
 public:
  XMLDocumentParserParameterizedTest() : scoped_rust_(GetParam()) {}

 protected:
  ScopedXMLParsingRustForTest scoped_rust_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         XMLDocumentParserParameterizedTest,
                         testing::Bool());

// crbug.com/501740299
TEST_P(XMLDocumentParserParameterizedTest, SingleNamespaceReset) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  execution_context.GetExecutionContext().SetUpSecurityContextForTesting();
  auto& doc = *Document::CreateForTest(execution_context.GetExecutionContext());

  // Create an XHTML context element.
  DummyExceptionStateForTesting exception;
  const AtomicString xhtml_ns("http://www.w3.org/1999/xhtml");
  auto* div = doc.createElementNS(xhtml_ns, AtomicString("div"), exception);
  ASSERT_TRUE(div);

  DocumentFragment* fragment = DocumentFragment::Create(doc);
  // Payload with a single xmlns="".
  const char* payload = "<a xmlns=''><iframe/></a>";
  EXPECT_TRUE(fragment->ParseXML(payload, div, ASSERT_NO_EXCEPTION));

  auto* a = To<Element>(fragment->firstChild());
  ASSERT_TRUE(a);
  EXPECT_EQ(a->namespaceURI(), g_null_atom);

  auto* iframe = To<Element>(a->firstChild());
  ASSERT_TRUE(iframe);
  EXPECT_EQ(iframe->localName(), "iframe");
  EXPECT_EQ(iframe->namespaceURI(), g_null_atom)
      << "iframe should be in null namespace inherited from <a>";
}

// crbug.com/501740299
TEST_P(XMLDocumentParserParameterizedTest, NestedNamespaceReset) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  execution_context.GetExecutionContext().SetUpSecurityContextForTesting();
  auto& doc = *Document::CreateForTest(execution_context.GetExecutionContext());

  // Create an XHTML context element.
  DummyExceptionStateForTesting exception;
  const AtomicString xhtml_ns("http://www.w3.org/1999/xhtml");
  auto* div = doc.createElementNS(xhtml_ns, AtomicString("div"), exception);
  ASSERT_TRUE(div);

  DocumentFragment* fragment = DocumentFragment::Create(doc);
  // Payload with nested xmlns="".
  // The outer <a> resets the namespace to null.
  // The inner <b> also resets the namespace to null.
  // Verification that closing </b> doesn't clear the reset state for outer <a>.
  const char* payload = "<a xmlns=''><b xmlns=''>x</b><iframe/></a>";
  EXPECT_TRUE(fragment->ParseXML(payload, div, ASSERT_NO_EXCEPTION));

  // Structure: fragment -> <a> -> [<b>, <iframe>]
  auto* a = To<Element>(fragment->firstChild());
  ASSERT_TRUE(a);
  EXPECT_EQ(a->localName(), "a");
  EXPECT_EQ(a->namespaceURI(), g_null_atom);

  auto* b = To<Element>(a->firstChild());
  ASSERT_TRUE(b);
  EXPECT_EQ(b->localName(), "b");
  EXPECT_EQ(b->namespaceURI(), g_null_atom);

  auto* iframe = To<Element>(b->nextSibling());
  ASSERT_TRUE(iframe);
  EXPECT_EQ(iframe->localName(), "iframe");

  // Verify that the iframe correctly inherits the null namespace from <a>,
  // ensuring that the inner <b> declaration didn't clobber the reset state.
  EXPECT_EQ(iframe->namespaceURI(), g_null_atom)
      << "iframe should be in null namespace due to outer <a> reset";
}

}  // namespace blink
