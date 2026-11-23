// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/xml/parser/xml_document_parser.h"

#include <libxml/dict.h>
#include <libxml/encoding.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <libxslt/transform.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/xsltutils.h>

#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include "base/check_op.h"
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

TEST(XMLDocumentParserTest, FlushTrailingNonWhitespaceIsDocumentEndError) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  execution_context.GetExecutionContext().SetUpSecurityContextForTesting();

  xmlParserCtxtPtr ctxt = xmlNewParserCtxt();
  xmlCtxtSetCharEncConvImpl(ctxt, flushCrashConvImpl, nullptr);

  // XML document with trailing non-whitespace that decodes only on flush.
  std::string xml = "<?xml version=\"1.0\" encoding=\"flush-crash\"?><root/>x";

  xmlDocPtr xml_doc = xmlCtxtReadMemory(
      ctxt, xml.data(), static_cast<int>(xml.size()), "http://example.com",
      nullptr,
      XML_PARSE_NOENT | XML_PARSE_DTDLOAD | XML_PARSE_DTDATTR |
          XML_PARSE_NOCDATA | XML_PARSE_HUGE);

  if (xml_doc) {
    xmlFreeDoc(xml_doc);
  }
  EXPECT_EQ(ctxt->errNo, XML_ERR_DOCUMENT_END);
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

TEST(XMLDocumentParserTest, RustParserInstantiation) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  execution_context.GetExecutionContext().SetUpSecurityContextForTesting();

  {
    auto& doc =
        *Document::CreateForTest(execution_context.GetExecutionContext());
    ScopedXMLRustForNonXsltForTest rust_non_xslt(false);
    ScopedXMLParsingRustForTest rust_all(false);
    DocumentParser* parser = doc.CreateParser();
    EXPECT_FALSE(doc.UsingRustXmlParserForTesting());
    EXPECT_TRUE(parser);
  }

  {
    auto& doc =
        *Document::CreateForTest(execution_context.GetExecutionContext());
    ScopedXMLRustForNonXsltForTest rust_non_xslt(false);
    ScopedXMLParsingRustForTest rust_all(true);
    DocumentParser* parser = doc.CreateParser();
    EXPECT_TRUE(doc.UsingRustXmlParserForTesting());
    EXPECT_TRUE(parser);
  }

  {
    auto& doc =
        *Document::CreateForTest(execution_context.GetExecutionContext());
    ScopedXMLRustForNonXsltForTest rust_non_xslt(true);
    ScopedXMLParsingRustForTest rust_all(false);
    DocumentParser* parser = doc.CreateParser();
    EXPECT_TRUE(doc.UsingRustXmlParserForTesting());
    EXPECT_TRUE(parser);
  }
}

namespace {

void IgnoreXsltError(void*, const char*, ...) {}

class ScopedXmlAllocHooks {
 public:
  ScopedXmlAllocHooks(xmlDictPtr dict, size_t fail_size) {
    xmlInitParser();
    CHECK_EQ(0, xmlMemGet(&orig_free_, &orig_malloc_, &orig_realloc_,
                          &orig_strdup_));
    active_dict_ = dict;
    fail_size_ = fail_size;
    failed_alloc_count_ = 0;
    unexpected_dict_op_count_ = 0;
    CHECK_EQ(0, xmlMemSetup(&FreeHook, &MallocHook, &ReallocHook, &StrdupHook));
  }

  ~ScopedXmlAllocHooks() {
    CHECK_EQ(
        0, xmlMemSetup(orig_free_, orig_malloc_, orig_realloc_, orig_strdup_));
    active_dict_ = nullptr;
    fail_size_ = 0;
  }

  ScopedXmlAllocHooks(const ScopedXmlAllocHooks&) = delete;
  ScopedXmlAllocHooks& operator=(const ScopedXmlAllocHooks&) = delete;

  void SetFailSize(size_t fail_size) { fail_size_ = fail_size; }
  size_t failed_alloc_count() const { return failed_alloc_count_; }
  size_t unexpected_dict_op_count() const { return unexpected_dict_op_count_; }

 private:
  static void FreeHook(void* ptr) {
    if (ptr && active_dict_ &&
        xmlDictOwns(active_dict_, static_cast<const xmlChar*>(ptr))) {
      ++unexpected_dict_op_count_;
      ADD_FAILURE() << "xmlFree invoked on dictionary-owned string";
      return;
    }
    orig_free_(ptr);
  }

  static void* MallocHook(size_t size) {
    if (fail_size_ != 0 && size == fail_size_) {
      ++failed_alloc_count_;
      return nullptr;
    }
    return orig_malloc_(size);
  }

  static void* ReallocHook(void* ptr, size_t size) {
    if (ptr && active_dict_ &&
        xmlDictOwns(active_dict_, static_cast<const xmlChar*>(ptr))) {
      ++unexpected_dict_op_count_;
      ADD_FAILURE() << "xmlRealloc invoked on dictionary-owned string";
      return nullptr;
    }
    return orig_realloc_(ptr, size);
  }

  static char* StrdupHook(const char* str) { return orig_strdup_(str); }

  inline static thread_local xmlFreeFunc orig_free_ = nullptr;
  inline static thread_local xmlMallocFunc orig_malloc_ = nullptr;
  inline static thread_local xmlReallocFunc orig_realloc_ = nullptr;
  inline static thread_local xmlStrdupFunc orig_strdup_ = nullptr;
  inline static thread_local xmlDictPtr active_dict_ = nullptr;
  inline static thread_local size_t fail_size_ = 0;
  inline static thread_local size_t failed_alloc_count_ = 0;
  inline static thread_local size_t unexpected_dict_op_count_ = 0;
};

std::string BuildCoalescingStylesheet(const std::string& literal_text,
                                      const std::string& appended_text,
                                      bool use_copy_of) {
  const std::string variable_decl =
      use_copy_of
          ? "<xsl:variable name=\"chunk\">" + appended_text + "</xsl:variable>"
          : "<xsl:variable name=\"chunk\" select=\"'" + appended_text + "'\"/>";
  const std::string append_instruction =
      use_copy_of ? "<xsl:copy-of select=\"$chunk\"/>"
                  : "<xsl:value-of select=\"$chunk\"/>";
  return "<?xml version=\"1.0\"?>"
         "<xsl:stylesheet version=\"1.0\" "
         "xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\">" +
         variable_decl +
         "<xsl:variable name=\"reference\">placeholder_text</xsl:variable>"
         "<xsl:template match=\"/\">"
         "<out>" +
         literal_text + append_instruction +
         "<xsl:value-of select=\"'_suffix'\"/>"
         "<item><xsl:value-of select=\"$reference\"/></item>"
         "</out>"
         "</xsl:template>"
         "</xsl:stylesheet>";
}

}  // namespace

TEST(XMLDocumentParserTest, XsltTextCoalescingAllocationFailure) {
  test::TaskEnvironment task_environment;

  constexpr size_t kLiteralLen = 128;
  constexpr size_t kAppendedLen = 4096;
  // xmlStrncatNew allocates literal length + appended length + NUL terminator.
  constexpr size_t kMergedAllocSize = kLiteralLen + kAppendedLen + 1;

  const std::string literal_text(kLiteralLen, 'D');
  const std::string appended_text(kAppendedLen, 'A');
  const std::string source_xml = "<?xml version=\"1.0\"?><root/>";

  xmlDocPtr source_doc =
      xmlReadMemory(source_xml.data(), static_cast<int>(source_xml.size()),
                    "source.xml", nullptr, XML_PARSE_NOENT);
  ASSERT_NE(nullptr, source_doc);

  for (bool use_copy_of : {false, true}) {
    const std::string stylesheet_xml =
        BuildCoalescingStylesheet(literal_text, appended_text, use_copy_of);
    xmlDocPtr style_doc = xmlReadMemory(
        stylesheet_xml.data(), static_cast<int>(stylesheet_xml.size()),
        "style.xsl", nullptr, XML_PARSE_NOENT | XML_PARSE_NOCDATA);
    ASSERT_NE(nullptr, style_doc);
    xsltStylesheetPtr sheet = xsltParseStylesheetDoc(style_doc);
    ASSERT_NE(nullptr, sheet);
    ASSERT_NE(nullptr, sheet->dict);

    const xmlChar* dict_reference = xmlDictExists(
        sheet->dict, reinterpret_cast<const xmlChar*>("placeholder_text"), -1);
    ASSERT_NE(nullptr, dict_reference);

    // 1. Failing the initial coalescing allocation onto a dictionary-backed
    // text node must abort cleanly without passing the dictionary pointer to
    // xmlRealloc on subsequent text appends.
    {
      xsltTransformContextPtr ctxt = xsltNewTransformContext(sheet, source_doc);
      ASSERT_NE(nullptr, ctxt);
      ASSERT_NE(nullptr, ctxt->dict);
      xsltSetTransformErrorFunc(ctxt, nullptr, &IgnoreXsltError);

      xmlDocPtr result_doc = nullptr;
      {
        ScopedXmlAllocHooks hooks(ctxt->dict, kMergedAllocSize);
        result_doc = xsltApplyStylesheetUser(sheet, source_doc, nullptr,
                                             nullptr, nullptr, ctxt);
        EXPECT_GE(hooks.failed_alloc_count(), 1u);
        EXPECT_EQ(0u, hooks.unexpected_dict_op_count());
      }
      EXPECT_EQ(nullptr, result_doc);
      EXPECT_EQ(XSLT_STATE_ERROR, ctxt->state);
      EXPECT_STREQ("placeholder_text",
                   reinterpret_cast<const char*>(dict_reference));
      if (result_doc) {
        xmlFreeDoc(result_doc);
      }
      xsltFreeTransformContext(ctxt);
    }

    // 2. Directly verify that a failed append onto a dictionary-backed node
    // clears a non-null ctxt->lasttext from an earlier node and allows a
    // subsequent append to allocate a fresh heap buffer cleanly.
    {
      xsltTransformContextPtr ctxt = xsltNewTransformContext(sheet, source_doc);
      ASSERT_NE(nullptr, ctxt);
      xsltSetTransformErrorFunc(ctxt, nullptr, &IgnoreXsltError);

      xmlDocPtr out_doc = xmlNewDoc(reinterpret_cast<const xmlChar*>("1.0"));
      ASSERT_NE(nullptr, out_doc);
      out_doc->dict = ctxt->dict;
      xmlDictReference(ctxt->dict);
      ctxt->output = out_doc;

      xmlNodePtr root = xmlNewDocNode(
          out_doc, nullptr, reinterpret_cast<const xmlChar*>("r"), nullptr);
      ASSERT_NE(nullptr, root);
      xmlDocSetRootElement(out_doc, root);

      xmlNodePtr other_elem = xmlNewDocNode(
          out_doc, nullptr, reinterpret_cast<const xmlChar*>("other"), nullptr);
      ASSERT_NE(nullptr, other_elem);
      ASSERT_NE(nullptr,
                xsltCopyTextString(ctxt, other_elem,
                                   reinterpret_cast<const xmlChar*>("seed"),
                                   /*noescape=*/0));
      ASSERT_NE(nullptr, ctxt->lasttext);

      xmlNodePtr dict_text = xmlNewTextLen(nullptr, 0);
      ASSERT_NE(nullptr, dict_text);
      xmlFree(dict_text->content);
      dict_text->content = const_cast<xmlChar*>(dict_reference);
      ASSERT_NE(nullptr, xmlAddChild(root, dict_text));

      {
        // "placeholder_text" (16) + "_extra" (6) + NUL (1) = 23 bytes.
        ScopedXmlAllocHooks hooks(ctxt->dict, 23u);
        EXPECT_EQ(nullptr,
                  xsltCopyTextString(ctxt, root,
                                     reinterpret_cast<const xmlChar*>("_extra"),
                                     /*noescape=*/0));
        EXPECT_EQ(nullptr, ctxt->lasttext);
        EXPECT_EQ(XSLT_STATE_ERROR, ctxt->state);
        EXPECT_EQ(dict_reference, dict_text->content);

        hooks.SetFailSize(0u);
        xmlNodePtr updated = xsltCopyTextString(
            ctxt, root, reinterpret_cast<const xmlChar*>("_next"),
            /*noescape=*/0);
        ASSERT_NE(nullptr, updated);
        EXPECT_EQ(0u, hooks.unexpected_dict_op_count());
        EXPECT_STREQ("placeholder_text_next",
                     reinterpret_cast<const char*>(updated->content));
        EXPECT_STREQ("placeholder_text",
                     reinterpret_cast<const char*>(dict_reference));
      }

      xmlFreeNode(other_elem);
      xmlFreeDoc(out_doc);
      xsltFreeTransformContext(ctxt);
    }

    // 3. Normal coalescing with the same stylesheet must succeed and preserve
    // both concatenated output and sibling dictionary entries.
    {
      xsltTransformContextPtr ctxt = xsltNewTransformContext(sheet, source_doc);
      ASSERT_NE(nullptr, ctxt);
      xmlDocPtr result_doc = nullptr;
      {
        ScopedXmlAllocHooks hooks(ctxt->dict, /*fail_size=*/0);
        result_doc = xsltApplyStylesheetUser(sheet, source_doc, nullptr,
                                             nullptr, nullptr, ctxt);
        EXPECT_EQ(0u, hooks.unexpected_dict_op_count());
      }
      ASSERT_NE(nullptr, result_doc);
      EXPECT_EQ(XSLT_STATE_OK, ctxt->state);

      xmlNodePtr root = xmlDocGetRootElement(result_doc);
      ASSERT_NE(nullptr, root);
      ASSERT_NE(nullptr, root->children);
      EXPECT_EQ(XML_TEXT_NODE, root->children->type);
      const std::string expected_text =
          literal_text + appended_text + "_suffix";
      ASSERT_NE(nullptr, root->children->content);
      EXPECT_EQ(expected_text,
                reinterpret_cast<const char*>(root->children->content));

      ASSERT_NE(nullptr, root->last);
      xmlChar* item_text = xmlNodeGetContent(root->last);
      ASSERT_NE(nullptr, item_text);
      EXPECT_STREQ("placeholder_text",
                   reinterpret_cast<const char*>(item_text));
      xmlFree(item_text);

      xmlFreeDoc(result_doc);
      xsltFreeTransformContext(ctxt);
    }

    xsltFreeStylesheet(sheet);
  }

  xmlFreeDoc(source_doc);
}

TEST(XMLDocumentParserTest, XsltTextCoalescingEmptyNode) {
  test::TaskEnvironment task_environment;

  const std::string stylesheet_xml =
      "<?xml version=\"1.0\"?>"
      "<xsl:stylesheet version=\"1.0\" "
      "xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\">"
      "<xsl:template match=\"/\"><out/></xsl:template>"
      "</xsl:stylesheet>";
  const std::string source_xml = "<?xml version=\"1.0\"?><root/>";

  xmlDocPtr style_doc = xmlReadMemory(stylesheet_xml.data(),
                                      static_cast<int>(stylesheet_xml.size()),
                                      "style.xsl", nullptr, XML_PARSE_NOENT);
  ASSERT_NE(nullptr, style_doc);
  xsltStylesheetPtr sheet = xsltParseStylesheetDoc(style_doc);
  ASSERT_NE(nullptr, sheet);

  xmlDocPtr source_doc =
      xmlReadMemory(source_xml.data(), static_cast<int>(source_xml.size()),
                    "source.xml", nullptr, XML_PARSE_NOENT);
  ASSERT_NE(nullptr, source_doc);

  xsltTransformContextPtr ctxt = xsltNewTransformContext(sheet, source_doc);
  ASSERT_NE(nullptr, ctxt);

  xmlDocPtr out_doc = xmlNewDoc(reinterpret_cast<const xmlChar*>("1.0"));
  ASSERT_NE(nullptr, out_doc);
  ctxt->output = out_doc;

  xmlNodePtr first_elem = xmlNewDocNode(
      out_doc, nullptr, reinterpret_cast<const xmlChar*>("first"), nullptr);
  ASSERT_NE(nullptr, first_elem);
  xmlDocSetRootElement(out_doc, first_elem);

  // Populate lasttsize and lasttuse via normal text coalescing on first_elem.
  ASSERT_NE(nullptr,
            xsltCopyTextString(ctxt, first_elem,
                               reinterpret_cast<const xmlChar*>("initial_text"),
                               /*noescape=*/0));
  ASSERT_NE(nullptr,
            xsltCopyTextString(ctxt, first_elem,
                               reinterpret_cast<const xmlChar*>("_extended"),
                               /*noescape=*/0));
  ASSERT_GT(ctxt->lasttsize, ctxt->lasttuse);
  ASSERT_GT(ctxt->lasttuse, 0);

  // Attach a second element with an empty text node (content == nullptr) while
  // ctxt->lasttext is reset to nullptr, leaving stale lasttsize/lasttuse.
  xmlNodePtr second_elem = xmlNewDocNode(
      out_doc, nullptr, reinterpret_cast<const xmlChar*>("second"), nullptr);
  ASSERT_NE(nullptr, second_elem);
  ASSERT_NE(nullptr, xmlAddChild(first_elem, second_elem));

  xmlNodePtr empty_text = xmlNewDocText(out_doc, nullptr);
  ASSERT_NE(nullptr, empty_text);
  ASSERT_EQ(nullptr, empty_text->content);
  ASSERT_NE(nullptr, xmlAddChild(second_elem, empty_text));
  ctxt->lasttext = nullptr;

  xmlNodePtr updated = xsltCopyTextString(
      ctxt, second_elem, reinterpret_cast<const xmlChar*>("sample_data"),
      /*noescape=*/0);
  ASSERT_EQ(empty_text, updated);
  ASSERT_NE(nullptr, empty_text->content);
  EXPECT_STREQ("sample_data",
               reinterpret_cast<const char*>(empty_text->content));

  xmlFreeDoc(out_doc);
  xsltFreeTransformContext(ctxt);
  xmlFreeDoc(source_doc);
  xsltFreeStylesheet(sheet);
}

}  // namespace blink
