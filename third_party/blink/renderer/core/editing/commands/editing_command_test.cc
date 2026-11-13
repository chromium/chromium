// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/platform/web_policy_container.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/renderer/core/dom/events/native_event_listener.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/static_range.h"
#include "third_party/blink/renderer/core/editing/commands/editing_command_type.h"
#include "third_party/blink/renderer/core/editing/commands/editor_command.h"
#include "third_party/blink/renderer/core/editing/commands/editor_command_names.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/core/testing/mock_policy_container_host.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/static_data_navigation_body_loader.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

struct CommandNameEntry {
  const char* name;
  EditingCommandType type;
};

constexpr auto kCommandNameEntries = std::to_array<CommandNameEntry>({
#define V(name) {#name, EditingCommandType::k##name},
    FOR_EACH_BLINK_EDITING_COMMAND_NAME(V)
#undef V
});
// Test all commands except EditingCommandType::Invalid.
static_assert(
    std::size(kCommandNameEntries) + 1 ==
        static_cast<size_t>(EditingCommandType::kNumberOfCommandTypes),
    "must test all valid EditingCommandType");

}  // anonymous namespace

class EditingCommandTest : public EditingTestBase {};

TEST_F(EditingCommandTest, EditorCommandOrder) {
  for (size_t i = 1; i < std::size(kCommandNameEntries); ++i) {
    EXPECT_GT(0,
              CodeUnitCompareIgnoringAsciiCase(kCommandNameEntries[i - 1].name,
                                               kCommandNameEntries[i].name))
        << "EDITOR_COMMAND_MAP must be case-folding ordered. Incorrect index:"
        << i;
  }
}

TEST_F(EditingCommandTest, CreateCommandFromString) {
  Editor& dummy_editor = GetDocument().GetFrame()->GetEditor();
  for (const auto& entry : kCommandNameEntries) {
    const EditorCommand command = dummy_editor.CreateCommand(entry.name);
    EXPECT_EQ(static_cast<int>(entry.type), command.IdForHistogram())
        << entry.name;
  }
}

TEST_F(EditingCommandTest, CreateCommandFromStringCaseFolding) {
  Editor& dummy_editor = GetDocument().GetFrame()->GetEditor();
  for (const auto& entry : kCommandNameEntries) {
    const EditorCommand lower_name_command =
        dummy_editor.CreateCommand(String(entry.name).ToAsciiLower());
    EXPECT_EQ(static_cast<int>(entry.type), lower_name_command.IdForHistogram())
        << entry.name;
    const EditorCommand upper_name_command =
        dummy_editor.CreateCommand(String(entry.name).ToAsciiUpper());
    EXPECT_EQ(static_cast<int>(entry.type), upper_name_command.IdForHistogram())
        << entry.name;
  }
}

TEST_F(EditingCommandTest, CreateCommandFromInvalidString) {
  const String kInvalidCommandName[] = {
      "", "iNvAlId", "12345",
  };
  Editor& dummy_editor = GetDocument().GetFrame()->GetEditor();
  for (const auto& command_name : kInvalidCommandName) {
    const EditorCommand command = dummy_editor.CreateCommand(command_name);
    EXPECT_EQ(0, command.IdForHistogram());
  }
}

TEST_F(EditingCommandTest, EnabledVisibleSelection) {
  Editor& editor = GetDocument().GetFrame()->GetEditor();
  const EditorCommand command =
      editor.CreateCommand("MoveRightAndModifySelection");
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>a|b<div>"),
      SetSelectionOptions());
  Element* div = QuerySelector("div");
  GetDocument().SetFocusedElement(
      div, FocusParams(SelectionBehaviorOnFocus::kNone,
                       mojom::blink::FocusType::kNone, nullptr));
  EXPECT_TRUE(command.IsEnabled());
  div->removeAttribute(html_names::kContenteditableAttr);
  EXPECT_FALSE(command.IsEnabled());
  GetDocument().GetFrame()->GetSettings()->SetCaretBrowsingEnabled(true);
  EXPECT_TRUE(command.IsEnabled());
}

TEST_F(EditingCommandTest, LineEndModifySelectionEnabledInReadOnlyInput) {
  SetBodyContent("<input id='inp' readonly value='hello world'>");
  Element* inp = QuerySelector("#inp");
  inp->Focus();
  UpdateAllLifecyclePhasesForTest();

  Editor& editor = GetDocument().GetFrame()->GetEditor();
  // Sanity check: the non-line-end variant is already enabled here.
  EXPECT_TRUE(editor.CreateCommand("MoveLeftAndModifySelection").IsEnabled());
  EXPECT_TRUE(editor.CreateCommand("MoveToLeftEndOfLineAndModifySelection")
                  .IsEnabled());
  EXPECT_TRUE(editor.CreateCommand("MoveToRightEndOfLineAndModifySelection")
                  .IsEnabled());
}

TEST_F(EditingCommandTest, EnabledVisibleSelectionAndMark) {
  Editor& editor = GetDocument().GetFrame()->GetEditor();
  const EditorCommand command = editor.CreateCommand("SelectToMark");
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>a|b<div>"),
      SetSelectionOptions());
  Element* div = QuerySelector("div");
  GetDocument().SetFocusedElement(
      div, FocusParams(SelectionBehaviorOnFocus::kNone,
                       mojom::blink::FocusType::kNone, nullptr));
  EXPECT_FALSE(command.IsEnabled());
  editor.SetMark();
  EXPECT_TRUE(command.IsEnabled());
  div->removeAttribute(html_names::kContenteditableAttr);
  EXPECT_FALSE(command.IsEnabled());
  GetDocument().GetFrame()->GetSettings()->SetCaretBrowsingEnabled(true);
  EXPECT_TRUE(command.IsEnabled());
}

TEST_F(EditingCommandTest, EnabledInEditableTextOrCaretBrowsing) {
  Editor& editor = GetDocument().GetFrame()->GetEditor();
  const EditorCommand command = editor.CreateCommand("MoveRight");

  SetBodyContent("<div>abc</div>");
  GetDocument().GetFrame()->GetSettings()->SetCaretBrowsingEnabled(false);
  EXPECT_FALSE(command.IsEnabled());
  GetDocument().GetFrame()->GetSettings()->SetCaretBrowsingEnabled(true);
  EXPECT_TRUE(command.IsEnabled());

  GetDocument().GetFrame()->GetSettings()->SetCaretBrowsingEnabled(false);
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>a|b<div>"),
      SetSelectionOptions());
  Element* div = QuerySelector("div");
  GetDocument().SetFocusedElement(
      div, FocusParams(SelectionBehaviorOnFocus::kNone,
                       mojom::blink::FocusType::kNone, nullptr));
  EXPECT_TRUE(command.IsEnabled());
  div->removeAttribute(html_names::kContenteditableAttr);
  EXPECT_FALSE(command.IsEnabled());
}

TEST_F(EditingCommandTest, DeleteSoftLineBackwardTargetRanges) {
  Editor& editor = GetDocument().GetFrame()->GetEditor();
  const EditorCommand command = editor.CreateCommand("DeleteToBeginningOfLine");

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>abcdef<br>123|<div>"),
      SetSelectionOptions());
  Element* div = QuerySelector("div");
  GetDocument().SetFocusedElement(
      div, FocusParams(SelectionBehaviorOnFocus::kNone,
                       mojom::blink::FocusType::kNone, nullptr));
  EXPECT_TRUE(command.IsEnabled());
  const GCedStaticRangeVector* ranges = command.GetTargetRanges();
  EXPECT_EQ(1u, ranges->size());
  const StaticRange& range = *ranges->at(0);
  EXPECT_EQ("123", range.startContainer()->textContent());
  EXPECT_EQ(0u, range.startOffset());
  EXPECT_EQ(range.startContainer(), range.endContainer());
  EXPECT_EQ(3u, range.endOffset());
}

// http://crbug.com/539754135
TEST_F(EditingCommandTest, DeleteBackwardTargetRangesInGraphemeCluster) {
  Editor& editor = GetFrame().GetEditor();
  const EditorCommand command = editor.CreateCommand("DeleteBackward");

  // The Thai word "ที่" is a single grapheme cluster made of
  // three code points, but Backspace deletes only its last code point.
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<div contenteditable>&#x0E17;&#x0E35;&#x0E48;|</div>"),
      SetSelectionOptions());
  Element* div = QuerySelector("div");
  GetDocument().SetFocusedElement(
      div, FocusParams(SelectionBehaviorOnFocus::kNone,
                       mojom::blink::FocusType::kNone, nullptr));
  const GCedStaticRangeVector* ranges = command.GetTargetRanges();
  ASSERT_EQ(1u, ranges->size());
  const StaticRange& range = *ranges->at(0);
  EXPECT_EQ(div->firstChild(), range.startContainer());
  EXPECT_EQ(2u, range.startOffset());
  EXPECT_EQ(div->firstChild(), range.endContainer());
  EXPECT_EQ(3u, range.endOffset());
}

// http://crbug.com/539754135
TEST_F(EditingCommandTest, DeleteBackwardTargetRangesInEmojiSequence) {
  Editor& editor = GetFrame().GetEditor();
  const EditorCommand command = editor.CreateCommand("DeleteBackward");

  // Unlike a grapheme cluster of combining characters, Backspace deletes the
  // whole regional indicator pair "\U0001F1FA\U0001F1F8", so the target ranges
  // should cover all of its four code units.
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>&#x1F1FA;&#x1F1F8;|</div>"),
      SetSelectionOptions());
  Element* div = QuerySelector("div");
  GetDocument().SetFocusedElement(
      div, FocusParams(SelectionBehaviorOnFocus::kNone,
                       mojom::blink::FocusType::kNone, nullptr));
  const GCedStaticRangeVector* ranges = command.GetTargetRanges();
  ASSERT_EQ(1u, ranges->size());
  const StaticRange& range = *ranges->at(0);
  EXPECT_EQ(div->firstChild(), range.startContainer());
  EXPECT_EQ(0u, range.startOffset());
  EXPECT_EQ(div->firstChild(), range.endContainer());
  EXPECT_EQ(4u, range.endOffset());
}

// http://crbug.com/539754135
TEST_F(EditingCommandTest, DeleteBackwardTargetRangesWithRangeSelection) {
  Editor& editor = GetFrame().GetEditor();
  const EditorCommand command = editor.CreateCommand("DeleteBackward");

  // A non-collapsed selection is deleted as is, even when it ends in the middle
  // of a grapheme cluster.
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<div contenteditable>^&#x0E17;&#x0E35;&#x0E48;|</div>"),
      SetSelectionOptions());
  Element* div = QuerySelector("div");
  GetDocument().SetFocusedElement(
      div, FocusParams(SelectionBehaviorOnFocus::kNone,
                       mojom::blink::FocusType::kNone, nullptr));
  const GCedStaticRangeVector* ranges = command.GetTargetRanges();
  ASSERT_EQ(1u, ranges->size());
  const StaticRange& range = *ranges->at(0);
  EXPECT_EQ(div->firstChild(), range.startContainer());
  EXPECT_EQ(0u, range.startOffset());
  EXPECT_EQ(div->firstChild(), range.endContainer());
  EXPECT_EQ(3u, range.endOffset());
}

// http://crbug.com/539754135
TEST_F(EditingCommandTest, DeleteForwardTargetRangesInGraphemeCluster) {
  Editor& editor = GetFrame().GetEditor();
  const EditorCommand command = editor.CreateCommand("DeleteForward");

  // Forward deletion removes the whole grapheme cluster, hence the target
  // ranges should cover all of its code points.
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<div contenteditable>|&#x0E17;&#x0E35;&#x0E48;</div>"),
      SetSelectionOptions());
  Element* div = QuerySelector("div");
  GetDocument().SetFocusedElement(
      div, FocusParams(SelectionBehaviorOnFocus::kNone,
                       mojom::blink::FocusType::kNone, nullptr));
  const GCedStaticRangeVector* ranges = command.GetTargetRanges();
  ASSERT_EQ(1u, ranges->size());
  const StaticRange& range = *ranges->at(0);
  EXPECT_EQ(div->firstChild(), range.startContainer());
  EXPECT_EQ(0u, range.startOffset());
  EXPECT_EQ(div->firstChild(), range.endContainer());
  EXPECT_EQ(3u, range.endOffset());
}

TEST_F(EditingCommandTest, RemoveFormatInPlaintextOnly) {
  Editor& editor = GetDocument().GetFrame()->GetEditor();
  const EditorCommand command = editor.CreateCommand("RemoveFormat");

  Selection().SetSelection(
      SetSelectionTextToBody(
          "<div contenteditable='plaintext-only'>ab^cd|ef</div>"),
      SetSelectionOptions());
  Element* plaintext_div = QuerySelector("div");
  GetDocument().SetFocusedElement(
      plaintext_div, FocusParams(SelectionBehaviorOnFocus::kNone,
                                 mojom::blink::FocusType::kNone, nullptr));
  EXPECT_FALSE(command.IsEnabled())
      << "removeFormat should be disabled in plaintext-only";

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable='true'>ab^cd|ef</div>"),
      SetSelectionOptions());
  Element* richly_editable_div = QuerySelector("div");
  GetDocument().SetFocusedElement(
      richly_editable_div,
      FocusParams(SelectionBehaviorOnFocus::kNone,
                  mojom::blink::FocusType::kNone, nullptr));
  EXPECT_TRUE(command.IsEnabled())
      << "removeFormat should be enabled in richly editable";
}

namespace {

void CommitHtmlNavigation(LocalFrame& frame,
                          const KURL& url,
                          const char* html) {
  auto params = WebNavigationParams::CreateWithEmptyHTMLForTesting(url);
  params->body_loader = std::make_unique<StaticDataNavigationBodyLoader>();
  MockPolicyContainerHost mock_policy_container_host;
  params->policy_container = std::make_unique<WebPolicyContainer>(
      WebPolicyContainerPolicies(),
      mock_policy_container_host.BindNewEndpointAndPassDedicatedRemote());
  frame.Loader().CommitNavigation(std::move(params), /*extra_data=*/nullptr);
  Document* doc = frame.GetDocument();
  if (!doc->documentElement()) {
    doc->AppendChild(MakeGarbageCollected<HTMLHtmlElement>(*doc));
  }
  doc->documentElement()->SetInnerHTMLWithoutTrustedTypes(
      String::FromUtf8(html));
}

class CallbackEventListener final : public NativeEventListener {
 public:
  explicit CallbackEventListener(base::RepeatingClosure callback)
      : callback_(std::move(callback)) {}

  void Invoke(ExecutionContext*, Event*) override { callback_.Run(); }

 private:
  base::RepeatingClosure callback_;
};

class ContextDestroyObserver final
    : public GarbageCollected<ContextDestroyObserver>,
      public ExecutionContextLifecycleObserver {
 public:
  ContextDestroyObserver(ExecutionContext* context, base::OnceClosure callback)
      : ExecutionContextLifecycleObserver(context),
        callback_(std::move(callback)) {}

  void ContextDestroyed() override {
    if (callback_) {
      std::move(callback_).Run();
    }
  }

  void Trace(Visitor* visitor) const override {
    ExecutionContextLifecycleObserver::Trace(visitor);
  }

 private:
  base::OnceClosure callback_;
};

class EditingCommandSubframeTest : public SimTest {
 public:
  void TearDown() override {
    if (subframe_ && subframe_->IsLoading()) {
      subframe_->Loader().StopAllLoaders(/*abort_client=*/true);
    }
    observer_ = nullptr;
    subframe_ = nullptr;
    SimTest::TearDown();
  }

  void SetUpSubframeWithUseRebuildCallback(base::OnceClosure on_use_rebuilt) {
    SimRequest main_resource("https://example.com/", "text/html");
    LoadURL("https://example.com/");

    main_resource.Complete(
        "<!doctype html><body>"
        "<svg width='9' height='9'><defs><rect id='r1' width='2' height='2'/>"
        "<rect id='r2' width='2' height='2'/></defs>"
        "<use id='target_use' href='#r1'/></svg>"
        "<iframe id='subframe'></iframe></body>");
    Compositor().BeginFrame();

    subframe_ = To<LocalFrame>(
        To<HTMLIFrameElement>(GetElementById("subframe"))->ContentFrame());
    ASSERT_TRUE(subframe_);

    Element* use = GetElementById("target_use");
    ShadowRoot* root = use->UserAgentShadowRoot();
    ASSERT_TRUE(root);
    auto* nested_frame = MakeGarbageCollected<HTMLIFrameElement>(GetDocument());
    root->AppendChild(nested_frame);
    auto* nested_window = To<LocalDOMWindow>(nested_frame->contentWindow());
    ASSERT_TRUE(nested_window);
    observer_ = MakeGarbageCollected<ContextDestroyObserver>(
        nested_window, std::move(on_use_rebuilt));
    Compositor().BeginFrame();
  }

  void ScheduleUseRebuild() {
    GetElementById("target_use")
        ->setAttribute(svg_names::kHrefAttr, AtomicString("#r2"));
  }

  LocalFrame& Subframe() const { return *subframe_; }

 private:
  Persistent<LocalFrame> subframe_;
  Persistent<ContextDestroyObserver> observer_;
};

}  // namespace

TEST_F(EditingCommandSubframeTest,
       InsertHtmlNoOpWhenFrameNavigatesDuringLayout) {
  bool did_navigate = false;
  SetUpSubframeWithUseRebuildCallback(base::BindLambdaForTesting([&]() {
    did_navigate = true;
    CommitHtmlNavigation(Subframe(), KURL("https://example.com/next.html"),
                         "<body contenteditable>next</body>");
    Document* next_doc = Subframe().GetDocument();
    Subframe().Selection().SetSelection(
        SelectionInDomTree::Builder()
            .SelectAllChildren(*next_doc->body())
            .Build(),
        SetSelectionOptions::Builder().SetDoNotSetFocus(true).Build());
  }));

  Document& initial_doc = *Subframe().GetDocument();
  initial_doc.setDesignMode("on");
  initial_doc.body()->SetInnerHTMLWithoutTrustedTypes("initial");
  Subframe().Selection().SetSelection(
      SelectionInDomTree::Builder()
          .SelectAllChildren(*initial_doc.body())
          .Build(),
      SetSelectionOptions());
  Compositor().BeginFrame();

  const EditorCommand command = Subframe().GetEditor().CreateCommand(
      "InsertHTML", EditorCommandSource::kDom);
  ScheduleUseRebuild();
  EXPECT_FALSE(command.Execute("<b id='inserted'>inserted</b>"));
  EXPECT_TRUE(did_navigate);
  Document* next_doc = Subframe().GetDocument();
  ASSERT_NE(&initial_doc, next_doc);
  EXPECT_EQ(KURL("https://example.com/next.html"), next_doc->Url());
  EXPECT_EQ(nullptr, next_doc->getElementById(AtomicString("inserted")));
  EXPECT_EQ("next", next_doc->body()->GetInnerHTMLString());
}

TEST_F(EditingCommandSubframeTest,
       QueryCommandValueEmptyWhenFrameNavigatesDuringLayout) {
  SetUpSubframeWithUseRebuildCallback(base::BindLambdaForTesting([&]() {
    CommitHtmlNavigation(
        Subframe(), KURL("https://example.com/next.html"),
        "<body contenteditable style='color: rgb(1, 2, 3)'>styled</body>");
    Document* next_doc = Subframe().GetDocument();
    Subframe().Selection().SetSelection(
        SelectionInDomTree::Builder()
            .SelectAllChildren(*next_doc->body())
            .Build(),
        SetSelectionOptions::Builder().SetDoNotSetFocus(true).Build());
  }));

  Document& initial_doc = *Subframe().GetDocument();
  initial_doc.setDesignMode("on");
  initial_doc.body()->SetInnerHTMLWithoutTrustedTypes("initial");
  Subframe().Selection().SetSelection(
      SelectionInDomTree::Builder()
          .SelectAllChildren(*initial_doc.body())
          .Build(),
      SetSelectionOptions());
  Compositor().BeginFrame();

  const EditorCommand command = Subframe().GetEditor().CreateCommand(
      "ForeColor", EditorCommandSource::kDom);
  ScheduleUseRebuild();
  EXPECT_EQ(String(), command.Value());
  EXPECT_EQ(KURL("https://example.com/next.html"),
            Subframe().GetDocument()->Url());
}

TEST_F(EditingCommandSubframeTest,
       FindStringNoOpWhenFrameNavigatesDuringLayout) {
  SetUpSubframeWithUseRebuildCallback(base::BindLambdaForTesting([&]() {
    CommitHtmlNavigation(Subframe(), KURL("https://example.com/next.html"),
                         "<body>sample-text</body>");
  }));

  const EditorCommand command = Subframe().GetEditor().CreateCommand(
      "FindString", EditorCommandSource::kDom);
  ScheduleUseRebuild();
  EXPECT_FALSE(command.Execute("sample-text"));
  EXPECT_EQ(KURL("https://example.com/next.html"),
            Subframe().GetDocument()->Url());
}

TEST_F(EditingCommandTest, ExecuteNoOpWhenBeforeInputNavigatesToNewDocument) {
  NavigateTo(KURL("https://example.com/"));
  Selection().SetSelection(
      SetSelectionTextToBody("<div id='ed' contenteditable>^initial|</div>"),
      SetSelectionOptions());
  Element* div = GetElementById("ed");
  div->Focus();

  Document& initial_doc = GetDocument();
  div->addEventListener(event_type_names::kBeforeinput,
                        MakeGarbageCollected<CallbackEventListener>(
                            base::BindLambdaForTesting([&]() {
                              CommitHtmlNavigation(
                                  GetFrame(),
                                  KURL("https://example.com/next.html"),
                                  "<body contenteditable>next</body>");
                              Document* next_doc = GetFrame().GetDocument();
                              next_doc->body()->Focus();
                              GetFrame().Selection().SetSelection(
                                  SelectionInDomTree::Builder()
                                      .SelectAllChildren(*next_doc->body())
                                      .Build(),
                                  SetSelectionOptions());
                            })));

  EXPECT_FALSE(GetFrame().GetEditor().ExecuteCommand("InsertText", "inserted"));
  ASSERT_NE(&initial_doc, &GetDocument());
  EXPECT_EQ(KURL("https://example.com/next.html"), GetDocument().Url());
  EXPECT_EQ("next", GetDocument().body()->GetInnerHTMLString());
}

}  // namespace blink
