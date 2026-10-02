// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/dictation/dictation_agent_impl.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/ime/edit_context.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

namespace {

struct EditorOptions {
  std::string text = "_";
  int selection_start = 1;
  int selection_end = 1;
  bool focus = true;
  // If true, the page fills its EditContext from its keydown handler instead
  // of waiting for a textupdate.
  bool populate_on_keydown = false;
};

}  // namespace

class DictationAgentImplTest : public testing::Test {
 protected:
  void SetUp() override { helper_.Initialize(); }

  // Loads a page with an `#editor` element whose EditContext starts with
  // `options`. The page replaces the "_" placeholder with "Hello world" and
  // selects "world" when it gets a textupdate (or a keydown, if
  // `populate_on_keydown`). The keydown and textupdate events it sees are
  // logged in the body's `data-log` attribute.
  void LoadEditorPage(const EditorOptions& options) {
    frame_test_helpers::LoadHTMLString(
        helper_.LocalMainFrame(),
        base::StringPrintf(
            R"HTML(
              <body data-log=''>
                <div id='editor'></div>
                <iframe id='frame' srcdoc='<p>Other</p>'></iframe>
                <script>
                  const log = (entry) => document.body.dataset.log += entry + ';';
                  const editor = document.getElementById('editor');
                  const ec = new EditContext(
                      {text: '%s', selectionStart: %d, selectionEnd: %d});
                  editor.editContext = ec;
                  const populate = () => {
                    if (ec.text === '_') {
                      ec.updateText(0, 1, 'Hello world');
                      ec.updateSelection(6, 11);
                    }
                  };
                  editor.addEventListener('keydown', (e) => {
                    log('keydown:' + e.keyCode);
                    if (%s) {
                      populate();
                    }
                  });
                  ec.addEventListener('textupdate', (e) => {
                    log('textupdate:' + e.text + ':' + e.updateRangeStart +
                        '-' + e.updateRangeEnd);
                    populate();
                  });
                  if (%s) {
                    editor.focus();
                  }
                </script>
              </body>
            )HTML",
            options.text.c_str(), options.selection_start,
            options.selection_end,
            options.populate_on_keydown ? "true" : "false",
            options.focus ? "true" : "false"),
        url_test_helpers::ToKURL("http://example.test"));
  }

  Document& GetDocument() {
    return *helper_.LocalMainFrame()->GetFrame()->GetDocument();
  }

  int32_t GetNodeId(const char* id) {
    Element* element = GetDocument().getElementById(AtomicString(id));
    CHECK(element);
    return DOMNodeIds::IdForNode(element);
  }

  String GetLog() {
    return GetDocument().body()->getAttribute(AtomicString("data-log"));
  }

  DictationAgentImpl& GetAgent(Document& document) {
    DictationAgentImpl* agent = DictationAgentImpl::CreateIfNeeded(document);
    CHECK(agent);
    return *agent;
  }

  void PopulateEditContext(Document& document, int32_t node_id) {
    bool replied = false;
    GetAgent(document).PopulateEditContext(
        node_id, base::BindLambdaForTesting([&] { replied = true; }));
    EXPECT_TRUE(replied);
  }

  void PopulateEditContext(int32_t node_id) {
    PopulateEditContext(GetDocument(), node_id);
  }

  EditContext& GetEditorEditContext() {
    EditContext* edit_context =
        GetDocument().getElementById(AtomicString("editor"))->editContext();
    CHECK(edit_context);
    return *edit_context;
  }

  test::TaskEnvironment task_environment_;
  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(DictationAgentImplTest, PopulateEditContextFillsEditContext) {
  LoadEditorPage({});

  PopulateEditContext(GetNodeId("editor"));

  EditContext& edit_context = GetEditorEditContext();
  EXPECT_EQ(edit_context.text(), "Hello world");
  EXPECT_EQ(edit_context.selectionStart(), 6u);
  EXPECT_EQ(edit_context.selectionEnd(), 11u);
}

TEST_F(DictationAgentImplTest, PopulateEditContextSendsKeydownThenTextUpdate) {
  LoadEditorPage({});

  PopulateEditContext(GetNodeId("editor"));

  EXPECT_EQ(GetLog(), "keydown:229;textupdate::1-1;");
}

TEST_F(DictationAgentImplTest, TextUpdateUsesSelectionSetByKeydownHandler) {
  LoadEditorPage({.populate_on_keydown = true});

  PopulateEditContext(GetNodeId("editor"));

  // The textupdate is sent at the selection the keydown handler set, so it
  // doesn't change the text or move the selection.
  EXPECT_EQ(GetLog(), "keydown:229;textupdate::11-11;");
  EditContext& edit_context = GetEditorEditContext();
  EXPECT_EQ(edit_context.text(), "Hello world");
  EXPECT_EQ(edit_context.selectionStart(), 6u);
  EXPECT_EQ(edit_context.selectionEnd(), 11u);
}

TEST_F(DictationAgentImplTest, PopulateEditContextOncePerDocument) {
  LoadEditorPage({});

  PopulateEditContext(GetNodeId("editor"));
  PopulateEditContext(GetNodeId("editor"));

  EXPECT_EQ(GetLog(), "keydown:229;textupdate::1-1;");
}

TEST_F(DictationAgentImplTest, NoPopulateIfNotPlaceholder) {
  LoadEditorPage(
      {.text = "Some text", .selection_start = 0, .selection_end = 4});

  PopulateEditContext(GetNodeId("editor"));

  EXPECT_EQ(GetLog(), "");
}

TEST_F(DictationAgentImplTest, NoPopulateIfTargetNotFocused) {
  LoadEditorPage({.focus = false});

  PopulateEditContext(GetNodeId("editor"));

  EXPECT_EQ(GetLog(), "");
}

TEST_F(DictationAgentImplTest, RejectsNodeFromOtherDocument) {
  LoadEditorPage({});
  auto* iframe = To<HTMLIFrameElement>(
      GetDocument().getElementById(AtomicString("frame")));
  ASSERT_TRUE(iframe->contentDocument());

  // The iframe document's agent must not act on a node in the main document.
  PopulateEditContext(*iframe->contentDocument(), GetNodeId("editor"));

  EXPECT_EQ(GetLog(), "");
}

}  // namespace blink
