// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"

#include <libxml/encoding.h>
#include <libxml/parser.h>

#include <fstream>
#include <iterator>
#include <string_view>

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/xml/parser/xml_document_parser_rs.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

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

static xmlCharEncError flushCrashConvert(void* vctxt,
                                         unsigned char* out,
                                         int* outlen,
                                         const unsigned char* in,
                                         int* inlen,
                                         int flush) {
  int toCopy = *inlen;
  if (!flush && toCopy > 0) {
    toCopy--;
  }
  if (toCopy > *outlen) {
    toCopy = *outlen;
  }
  // SAFETY: This implements a libxml2 callback interface using raw pointer
  // arguments, where safe spans are not possible. toCopy is bounded by the
  // buffer sizes of both in and out.
  UNSAFE_BUFFERS(memcpy(out, in, toCopy));
  *inlen = toCopy;
  *outlen = toCopy;
  return XML_ENC_ERR_SUCCESS;
}

static xmlParserErrors flushCrashConvImpl(void* vctxt,
                                          const char* name,
                                          xmlCharEncFlags flags,
                                          xmlCharEncodingHandler** out) {
  if (std::string_view(name) != "flush-crash") {
    return XML_ERR_UNSUPPORTED_ENCODING;
  }

  return xmlCharEncNewCustomHandler(name, flushCrashConvert, nullptr, nullptr,
                                    nullptr, nullptr, out);
}

TEST(XMLDocumentParserTest, ReproICUFlushCrash) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  execution_context.GetExecutionContext().SetUpSecurityContextForTesting();

  xmlParserCtxtPtr ctxt = xmlNewParserCtxt();
  xmlCtxtSetCharEncConvImpl(ctxt, flushCrashConvImpl, nullptr);

  // XML document with a trailing space that decodes only on flush.
  std::string xml = "<?xml version=\"1.0\" encoding=\"flush-crash\"?><root/> ";

  xmlDocPtr xml_doc = xmlCtxtReadMemory(
      ctxt, xml.data(), static_cast<int>(xml.size()), "http://example.com",
      nullptr,
      XML_PARSE_NOENT | XML_PARSE_DTDLOAD | XML_PARSE_DTDATTR |
          XML_PARSE_NOCDATA | XML_PARSE_HUGE);

  if (xml_doc) {
    xmlFreeDoc(xml_doc);
  }
  EXPECT_EQ(ctxt->errNo, XML_ERR_OK);
  xmlFreeParserCtxt(ctxt);
}

TEST(XMLDocumentParserTest, ChunkedParsingWithPauseRust) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  execution_context.GetExecutionContext().SetUpSecurityContextForTesting();
  auto& doc = *Document::CreateForTest(execution_context.GetExecutionContext());

  ScopedXMLParsingRustForTest scoped_rust(true);

  ScriptableDocumentParser* parser =
      MakeGarbageCollected<XMLDocumentParserRs>(doc, nullptr);

  // 1. Append first incomplete chunk. This sets carry_unbalanced_root_error_.
  parser->Append("<root><child>");

  // 2. Simulate a pending stylesheet that blocks the parser.
  parser->DidAddPendingParserBlockingStylesheet();

  // 3. Append second chunk that closes elements.
  // This will trigger EndElementNs, which calls
  // CheckIfBlockingStyleSheetAdded() and pauses the parser.
  parser->Append("</child></root>");

  parser->Finish();

  EXPECT_TRUE(parser->WellFormed());
}

}  // namespace blink
