// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_document_parser_fastpath.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/tracing/trace_event_analyzer.h"
#include "base/test/tracing/trace_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_supported_type.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/serializers/serialization.h"
#include "third_party/blink/renderer/core/html/forms/form_controller.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/parser/html_construction_site.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/core/xml/dom_parser.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {
namespace {

TEST(HTMLDocumentParserFastpathTest, SanityCheck) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);
  document->body()->AppendChild(div);
  DocumentFragment* fragment = DocumentFragment::Create(*document);
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(
      TryParsingHTMLFragment("<div>test</div>", *document, *fragment, *div,
                             ParserContentPolicy::kAllowScriptingContent, {}));
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTagType.CompositeMaskV2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.CompositeMaskV2", 0);
}

TEST(HTMLDocumentParserFastpathTest,
     SetInnerHTMLWithoutTrustedTypesUsesFastPathSuccess) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->SetInnerHTMLWithoutTrustedTypes("<div>test</div>");
  // This was html the fast path handled, so there should be one histogram with
  // success.
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLFastPathParser.ParseResult",
                                      HtmlFastPathResult::kSucceeded, 1);
}

TEST(HTMLDocumentParserFastpathTest,
     SetInnerHTMLWithoutTrustedTypesUsesFastPathFailure) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->SetInnerHTMLWithoutTrustedTypes("<div");
  // The fast path should not have handled this, so there should be one
  // histogram with a value other then success.
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  histogram_tester.ExpectBucketCount("Blink.HTMLFastPathParser.ParseResult",
                                     HtmlFastPathResult::kSucceeded, 0);
}

TEST(HTMLDocumentParserFastpathTest, LongTextIsNotSplit) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  // We previously split nodes at 64k.
  constexpr wtf_size_t kLargeTextLength = (1 << 16) + 1;
  std::vector<LChar> chars(kLargeTextLength, 'a');
  div->SetInnerHTMLWithoutTrustedTypes(String(base::span(chars)));
  Text* text_node = To<Text>(div->firstChild());
  ASSERT_TRUE(text_node);

  // Text nodes are no longer split by length, so the entire run of text is kept
  // in a single Text node.
  EXPECT_EQ(kLargeTextLength, text_node->length());
  EXPECT_FALSE(text_node->nextSibling());
}

TEST(HTMLDocumentParserFastpathTest, MaximumHTMLParserDOMTreeDepth) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);
  StringBuilder string_builder;
  const unsigned depth =
      HTMLConstructionSite::kMaximumHTMLParserDOMTreeDepth + 2;
  // Create a very nested tree, with the deepest containing the id `deepest`.
  for (unsigned i = 0; i < depth - 1; ++i) {
    string_builder.Append("<div>");
  }
  string_builder.Append("<div id='deepest'>");
  string_builder.Append("</div>");
  for (unsigned i = 0; i < depth - 1; ++i) {
    string_builder.Append("</div>");
  }
  div->SetInnerHTMLWithoutTrustedTypes(string_builder.ToString());

  // Because kMaximumHTMLParserDOMTreeDepth was encountered, the deepest
  // node should have siblings.
  Element* deepest = div->getElementById(AtomicString("deepest"));
  ASSERT_TRUE(deepest);
  EXPECT_EQ(deepest->parentNode()->CountChildren(), 3u);
}

TEST(HTMLDocumentParserFastpathTest, LogUnsupportedTags) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->SetInnerHTMLWithoutTrustedTypes("<table></table>");
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.CompositeMaskV2", 1);
  histogram_tester.ExpectBucketCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.CompositeMaskV2", 2, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask0V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask1V2", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask2V2", 0);
}

TEST(HTMLDocumentParserFastpathTest, LogUnsupportedTagsWithValidTag) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->SetInnerHTMLWithoutTrustedTypes("<div><table></table></div>");
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.CompositeMaskV2", 1);
  // Table is in the second chunk of values, so 2 should be set.
  histogram_tester.ExpectBucketCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.CompositeMaskV2", 2, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask0V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask1V2", 1);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask2V2", 0);
}

TEST(HTMLDocumentParserFastpathTest, LogUnsupportedContextTag) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* dl = MakeGarbageCollected<HTMLTextAreaElement>(*document);

  base::HistogramTester histogram_tester;
  dl->SetInnerHTMLWithoutTrustedTypes("some text");
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.CompositeMaskV2", 1);
  // Textarea is in the third chunk of values, so 3 should be set.
  histogram_tester.ExpectBucketCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.CompositeMaskV2", 4, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.Mask0V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.Mask1V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedContextTag.Mask2V2", 1);
}

TEST(HTMLDocumentParserFastpathTest, LogSvg) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->SetInnerHTMLWithoutTrustedTypes("<svg></svg>");
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.CompositeMaskV2", 1);
  // Svg is in the third chunk of values, so 4 should be set.
  histogram_tester.ExpectBucketCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.CompositeMaskV2", 4, 1);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask0V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask1V2", 0);
  histogram_tester.ExpectTotalCount(
      "Blink.HTMLFastPathParser.UnsupportedTag.Mask2V2", 1);
}

TEST(HTMLDocumentParserFastpathTest, HTMLInputElementCheckedState) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div1 = MakeGarbageCollected<HTMLDivElement>(*document);
  auto* div2 = MakeGarbageCollected<HTMLDivElement>(*document);
  document->body()->AppendChild(div1);
  document->body()->AppendChild(div2);

  // Set the state for new controls, which triggers a different code path in
  // HTMLInputElement::ParseAttribute.
  div1->SetInnerHTMLWithoutTrustedTypes("<select form='ff'></select>");
  DocumentState* document_state =
      document->EnsureFormController().ControlStates();
  Vector<String> state1 = document_state->ToStateVector();
  document->EnsureFormController().SetStateForNewControls(state1);
  EXPECT_TRUE(document->EnsureFormController().HasControlStates());

  div2->SetInnerHTMLWithoutTrustedTypes("<input checked='true'>");
  HTMLInputElement* input_element = To<HTMLInputElement>(div2->firstChild());
  ASSERT_TRUE(input_element);
  EXPECT_TRUE(input_element->Checked());
}

TEST(HTMLDocumentParserFastpathTest, CharacterReferenceCases) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  // Various subtle cases of character references that have caused problems.
  // The assertions are handled by DCHECKs in the code, specifically in
  // serialization.cc.
  div->SetInnerHTMLWithoutTrustedTypes(
      "Genius Nicer Dicer Plus | 18&nbsp&hellip;");
  div->SetInnerHTMLWithoutTrustedTypes("&nbsp&a");
  div->SetInnerHTMLWithoutTrustedTypes("&nbsp&");
  div->SetInnerHTMLWithoutTrustedTypes("&nbsp-");
}

TEST(HTMLDocumentParserFastpathTest, HandlesCompleteCharacterReference) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->SetInnerHTMLWithoutTrustedTypes("&cent;");
  Text* text_node = To<Text>(div->firstChild());
  ASSERT_TRUE(text_node);
  EXPECT_EQ(text_node->data(), String(u"\u00A2"));
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLFastPathParser.ParseResult",
                                      HtmlFastPathResult::kSucceeded, 1);
}

TEST(HTMLDocumentParserFastpathTest, FailsWithNestedLis) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->SetInnerHTMLWithoutTrustedTypes("<li><li></li></li>");
  // The html results in two children (nested <li>s implicitly close the open
  // <li>, resulting in two sibling <li>s, not one). The fast path parser does
  // not handle this case.
  EXPECT_EQ(2u, div->CountChildren());
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  histogram_tester.ExpectBucketCount("Blink.HTMLFastPathParser.ParseResult",
                                     HtmlFastPathResult::kSucceeded, 0);
}

TEST(HTMLDocumentParserFastpathTest, HandlesLi) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  div->SetInnerHTMLWithoutTrustedTypes("<div><li></li></div>");
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  histogram_tester.ExpectUniqueSample("Blink.HTMLFastPathParser.ParseResult",
                                      HtmlFastPathResult::kSucceeded, 1);
}

TEST(HTMLDocumentParserFastpathTest, NullMappedToReplacementChar) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  base::HistogramTester histogram_tester;
  // Constructor that takes a base::span is needed because of \0 in string.
  div->SetInnerHTMLWithoutTrustedTypes(
      String(base::span_from_cstring("<div id='x' name='x\0y'></div>")));
  Element* new_div = div->getElementById(AtomicString("x"));
  ASSERT_TRUE(new_div);
  // Null chars are generally mapped to \uFFFD (at least this test should
  // trigger the replacement).
  EXPECT_EQ(AtomicString(String(u"x\uFFFDy")), new_div->GetNameAttribute());
}

// Verifies DOMParser uses the fast path parser.
TEST(HTMLDocumentParserFastpathTest, DomParserUsesFastPath) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* parser = DOMParser::Create(scope.GetScriptState());
  base::HistogramTester histogram_tester;
  parser->ParseFromStringWithoutTrustedTypes(
      "<strong>0</strong> items left",
      V8SupportedType(V8SupportedType::Enum::kTextHtml));
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
}

TEST(HTMLDocumentParserFastpathTest, BodyWithLeadingWhitespace) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* parser = DOMParser::Create(scope.GetScriptState());
  base::HistogramTester histogram_tester;
  Document* document = parser->ParseFromStringWithoutTrustedTypes(
      "\n   <div></div>", V8SupportedType(V8SupportedType::Enum::kTextHtml));
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  EXPECT_EQ("<body><div></div></body>", CreateMarkup(document->body()));
  auto* first_child = document->body()->firstChild();
  ASSERT_TRUE(first_child);
}

TEST(HTMLDocumentParserFastpathTest, BodyWithLeadingAndTrailingWhitespace) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* parser = DOMParser::Create(scope.GetScriptState());
  base::HistogramTester histogram_tester;
  Document* document = parser->ParseFromStringWithoutTrustedTypes(
      "\n   x<div></div>y ", V8SupportedType(V8SupportedType::Enum::kTextHtml));
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  EXPECT_EQ("<body>x<div></div>y </body>", CreateMarkup(document->body()));
  auto* first_child = document->body()->firstChild();
  ASSERT_TRUE(first_child);
}

TEST(HTMLDocumentParserFastpathTest, BodyWithLeadingAndTrailingWhitespace2) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* parser = DOMParser::Create(scope.GetScriptState());
  base::HistogramTester histogram_tester;
  Document* document = parser->ParseFromStringWithoutTrustedTypes(
      "\n   x \n  <div></div>y \n   ",
      V8SupportedType(V8SupportedType::Enum::kTextHtml));
  histogram_tester.ExpectTotalCount("Blink.HTMLFastPathParser.ParseResult", 1);
  EXPECT_EQ("<body>x \n  <div></div>y \n   </body>",
            CreateMarkup(document->body()));
  auto* first_child = document->body()->firstChild();
  ASSERT_TRUE(first_child);
}

TEST(HTMLDocumentParserFastpathTest, MixedEncoding) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);
  div->SetInnerHTMLWithoutTrustedTypes(u"Hello");
  Text* text_node = To<Text>(div->firstChild());
  ASSERT_TRUE(text_node);
  // Even though the supplied string was utf16, it only contained 8-bit chars,
  // so should end up as 8-bit.
  EXPECT_TRUE(text_node->data().Is8Bit());
}

TEST(HTMLDocumentParserFastpathTest, Escaped8BitText) {
  test::TaskEnvironment task_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);

  div->SetInnerHTMLWithoutTrustedTypes("&amp;");
  Text* text_node = To<Text>(div->firstChild());
  ASSERT_TRUE(text_node);
  // "&amp;" should be represented as 8-bit.
  EXPECT_TRUE(text_node->data().Is8Bit());
}

TEST(HTMLDocumentParserFastpathTest, TraceExportFragmentFastAndSlowPaths) {
  test::TaskEnvironment task_environment;
  base::test::TracingEnvironment tracing_environment;
  ScopedNullExecutionContext execution_context;
  auto* document =
      HTMLDocument::CreateForTest(execution_context.GetExecutionContext());
  document->SetURL(KURL("https://example.test/search"));
  document->write("<body></body>");
  auto* div = MakeGarbageCollected<HTMLDivElement>(*document);
  div->SetIdAttribute(AtomicString("slot-1"));
  div->setAttribute(html_names::kClassAttr, AtomicString("aio-card"));
  document->body()->AppendChild(div);

  trace_analyzer::Start(
      "blink," TRACE_DISABLED_BY_DEFAULT("blink.debug.html_content"));

  const char kFastHtml[] = "<p>fast path content</p>";
  const char kSlowHtml[] = "<table><tr><td>slow path</td></tr></table>";
  div->SetInnerHTMLWithoutTrustedTypes(kFastHtml);
  div->SetInnerHTMLWithoutTrustedTypes(kSlowHtml);

  auto analyzer = trace_analyzer::Stop();
  analyzer->AssociateBeginEndEvents();
  analyzer->MergeAssociatedEventArgs();

  using trace_analyzer::Query;
  Query is_slice_start = Query::EventPhaseIs(TRACE_EVENT_PHASE_BEGIN) ||
                         Query::EventPhaseIs(TRACE_EVENT_PHASE_COMPLETE);

  trace_analyzer::TraceEventVector fragment_events;
  analyzer->FindEvents(
      Query::EventNameIs("ParseHTMLFragment") && is_slice_start,
      &fragment_events);
  ASSERT_EQ(2u, fragment_events.size());

  EXPECT_TRUE(fragment_events[0]->GetKnownArgAsBool("parsed_fast_path"));
  EXPECT_EQ("DIV id='slot-1' class='aio-card'",
            fragment_events[0]->GetKnownArgAsString("context_element"));
  EXPECT_EQ("slot-1",
            fragment_events[0]->GetKnownArgAsString("context_element_id"));
  EXPECT_GT(fragment_events[0]->GetKnownArgAsInt("context_node_id"), 0);
  EXPECT_EQ("https://example.test/search",
            fragment_events[0]->GetKnownArgAsString("document_url"));
  EXPECT_EQ(kFastHtml, fragment_events[0]->GetKnownArgAsString("html_content"));

  EXPECT_FALSE(fragment_events[1]->GetKnownArgAsBool("parsed_fast_path"));
  EXPECT_EQ("DIV id='slot-1' class='aio-card'",
            fragment_events[1]->GetKnownArgAsString("context_element"));
  EXPECT_EQ("slot-1",
            fragment_events[1]->GetKnownArgAsString("context_element_id"));
  EXPECT_EQ(fragment_events[0]->GetKnownArgAsInt("context_node_id"),
            fragment_events[1]->GetKnownArgAsInt("context_node_id"));
  EXPECT_EQ(kSlowHtml, fragment_events[1]->GetKnownArgAsString("html_content"));

  // Only the slow-path fragment should invoke HTMLDocumentParser::append, and
  // it should not duplicate html_content already exported on ParseHTMLFragment.
  trace_analyzer::TraceEventVector append_events;
  analyzer->FindEvents(
      Query::EventNameIs("HTMLDocumentParser::append") && is_slice_start,
      &append_events);
  ASSERT_EQ(1u, append_events.size());
  EXPECT_TRUE(append_events[0]->GetKnownArgAsBool("is_fragment"));
  EXPECT_FALSE(append_events[0]->HasStringArg("html_content"));
}

}  // namespace
}  // namespace blink
