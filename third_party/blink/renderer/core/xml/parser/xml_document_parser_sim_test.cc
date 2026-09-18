// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// Writes the given data (if any) to the response and then finishes it when
// an event is dispatched. Dispatching the event from an inline script lets a
// test deliver the end of the navigation body while XMLDocumentParser is
// still parsing the chunk that contains the script.
class FinishResponseListener final : public NativeEventListener {
 public:
  FinishResponseListener(SimRequest* response, const String& final_data)
      : response_(response), final_data_(final_data) {}

  void Invoke(ExecutionContext*, Event*) override {
    EXPECT_FALSE(was_invoked_);
    was_invoked_ = true;
    if (!final_data_.empty()) {
      response_->Write(final_data_);
    }
    response_->Finish();
  }

  bool WasInvoked() const { return was_invoked_; }

 private:
  SimRequest* const response_;
  const String final_data_;
  bool was_invoked_ = false;
};

constexpr int kNestingDepth = 24;

// An inline script that reports the end of the response and records the
// document's readyState right afterwards, followed by a deeply nested run of
// elements with namespace declarations so that plenty of markup remains to
// be parsed after the script.
String BuildChunkAfterExternalScript(bool close_document) {
  StringBuilder markup;
  markup.Append(
      "<script>"
      "document.dispatchEvent(new Event('finish-response'));"
      "document.documentElement.setAttribute('data-ready-state',"
      " document.readyState);"
      "</script>");
  for (int i = 0; i < kNestingDepth; ++i) {
    markup.Append("<e");
    markup.AppendNumber(i);
    markup.Append(" xmlns:n");
    markup.AppendNumber(i);
    markup.Append("=\"http://example.com/ns/");
    markup.AppendNumber(i);
    markup.Append("\">");
  }
  markup.Append("<p id=\"last\">x</p>");
  for (int i = kNestingDepth - 1; i >= 0; --i) {
    markup.Append("</e");
    markup.AppendNumber(i);
    markup.Append(">");
  }
  if (close_document) {
    markup.Append("</body></html>");
  }
  return markup.ToString();
}

class XMLDocumentParserSimTest : public SimTest {};

// The response of an XHTML navigation may finish while the parser is in the
// middle of a chunk, e.g. when an inline script run by the parser causes the
// remainder of the body to be consumed. The parser must defer the completion
// until the chunk has been fully parsed instead of ending the document from
// inside the script.
TEST_F(XMLDocumentParserSimTest, LoadCompletionDuringInlineScript) {
  SimRequest main_resource("https://example.com/test.xhtml",
                           "application/xhtml+xml");
  SimSubresourceRequest external_script("https://example.com/ext.js",
                                        "text/javascript");

  LoadURL("https://example.com/test.xhtml");

  // The external script pauses parsing until it has loaded.
  main_resource.Write(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<html xmlns=\"http://www.w3.org/1999/xhtml\"><body>"
      "<script src=\"ext.js\"></script>");

  auto* listener = MakeGarbageCollected<FinishResponseListener>(
      &main_resource, /*final_data=*/String());
  GetDocument().addEventListener(AtomicString("finish-response"), listener);

  // Buffered while the parser waits for the external script.
  main_resource.Write(BuildChunkAfterExternalScript(/*close_document=*/true));

  // Resumes parsing; the buffered chunk above is parsed in one go and the
  // inline script finishes the response from within it.
  external_script.Complete("");
  test::RunPendingTasks();

  EXPECT_TRUE(listener->WasInvoked());

  // The document must not have been finished while the inline script was
  // still running.
  EXPECT_EQ(GetDocument().documentElement()->getAttribute(
                AtomicString("data-ready-state")),
            "loading");

  // The markup following the inline script must have been parsed normally.
  Element* last = GetDocument().getElementById(AtomicString("last"));
  ASSERT_TRUE(last);
  ASSERT_TRUE(last->parentElement());
  EXPECT_EQ(last->parentElement()->localName(),
            StrCat({"e", String::Number(kNestingDepth - 1)}));

  // The deferred completion must still finish the document.
  EXPECT_TRUE(GetDocument().LoadEventFinished());
}

// Same as above, but the final piece of the body arrives together with the
// completion while the parser is inside the chunk. The data must be parsed
// before the document is finished.
TEST_F(XMLDocumentParserSimTest, FinalDataAndLoadCompletionDuringInlineScript) {
  SimRequest main_resource("https://example.com/test.xhtml",
                           "application/xhtml+xml");
  SimSubresourceRequest external_script("https://example.com/ext.js",
                                        "text/javascript");

  LoadURL("https://example.com/test.xhtml");

  main_resource.Write(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<html xmlns=\"http://www.w3.org/1999/xhtml\"><body>"
      "<script src=\"ext.js\"></script>");

  auto* listener = MakeGarbageCollected<FinishResponseListener>(
      &main_resource, /*final_data=*/"<p id=\"tail\">y</p></body></html>");
  GetDocument().addEventListener(AtomicString("finish-response"), listener);

  main_resource.Write(BuildChunkAfterExternalScript(/*close_document=*/false));

  external_script.Complete("");
  test::RunPendingTasks();

  EXPECT_TRUE(listener->WasInvoked());

  EXPECT_EQ(GetDocument().documentElement()->getAttribute(
                AtomicString("data-ready-state")),
            "loading");

  Element* last = GetDocument().getElementById(AtomicString("last"));
  ASSERT_TRUE(last);
  ASSERT_TRUE(last->parentElement());
  EXPECT_EQ(last->parentElement()->localName(),
            StrCat({"e", String::Number(kNestingDepth - 1)}));

  // The data received together with the completion must have been parsed.
  Element* tail = GetDocument().getElementById(AtomicString("tail"));
  ASSERT_TRUE(tail);
  EXPECT_EQ(tail->parentElement(), GetDocument().body());

  EXPECT_TRUE(GetDocument().LoadEventFinished());
}

// Like LoadCompletionDuringInlineScript, but the markup after the inline
// script is not well-formed: a stray end tag followed by a deep run of
// unclosed elements with namespace declarations. The response finishing
// during the inline script must not end the document from inside the chunk;
// the parser has to fail the document in an orderly way afterwards.
TEST_F(XMLDocumentParserSimTest, LoadCompletionDuringInlineScriptBadMarkup) {
  SimRequest main_resource("https://example.com/test.xhtml",
                           "application/xhtml+xml");
  SimSubresourceRequest external_script("https://example.com/ext.js",
                                        "text/javascript");

  LoadURL("https://example.com/test.xhtml");

  main_resource.Write(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<html xmlns=\"http://www.w3.org/1999/xhtml\"><body>"
      "<script src=\"ext.js\"></script>");

  auto* listener = MakeGarbageCollected<FinishResponseListener>(
      &main_resource, /*final_data=*/String());
  GetDocument().addEventListener(AtomicString("finish-response"), listener);

  StringBuilder markup;
  markup.Append(
      "<script>"
      "document.dispatchEvent(new Event('finish-response'));"
      "document.documentElement.setAttribute('data-ready-state',"
      " document.readyState);"
      "</script>"
      "</script>");
  for (int i = 0; i < kNestingDepth; ++i) {
    markup.Append("<e");
    markup.AppendNumber(i);
    markup.Append(" xmlns:n");
    markup.AppendNumber(i);
    markup.Append("=\"http://example.com/ns/");
    markup.AppendNumber(i);
    markup.Append("\">");
  }
  main_resource.Write(markup.ToString());

  external_script.Complete("");
  test::RunPendingTasks();

  EXPECT_TRUE(listener->WasInvoked());

  // The document must not have been finished while the inline script was
  // still running, even though the markup afterwards fails to parse.
  EXPECT_EQ(GetDocument().documentElement()->getAttribute(
                AtomicString("data-ready-state")),
            "loading");

  // The malformed document must still reach completion.
  EXPECT_TRUE(GetDocument().LoadEventFinished());
}

}  // namespace

}  // namespace blink
