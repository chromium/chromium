// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/frame_caret.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/core/editing/commands/typing_command.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

class FrameCaretTest : public EditingTestBase {
 public:
  static bool ShouldShowCaret(const FrameCaret& caret) {
    return caret.ShouldShowCaret();
  }

  static bool IsVisibleIfActive(const FrameCaret& caret) {
    return caret.IsVisibleIfActive();
  }

 private:
  // The caret blink timer doesn't work if IsRunningWebTest() because
  // LayoutTheme::CaretBlinkInterval() returns 0.
  ScopedWebTestMode web_test_mode_{false};
};

#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
// https://crbug.com/1222649
#define MAYBE_BlinkAfterTyping DISABLED_BlinkAfterTyping
#else
#define MAYBE_BlinkAfterTyping BlinkAfterTyping
#endif
TEST_F(FrameCaretTest, MAYBE_BlinkAfterTyping) {
  FrameCaret& caret = Selection().FrameCaretForTesting();
  scoped_refptr<scheduler::FakeTaskRunner> task_runner =
      base::MakeRefCounted<scheduler::FakeTaskRunner>();
  task_runner->SetTime(0);
  caret.RecreateCaretBlinkTimerForTesting(task_runner.get(),
                                          task_runner->GetMockTickClock());
  const double kInterval = 10;
  LayoutTheme::GetTheme().SetCaretBlinkInterval(base::Seconds(kInterval));
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes("<textarea>");
  auto* editor = To<Element>(GetDocument().body()->firstChild());
  editor->Focus();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(caret.IsActive());
  EXPECT_TRUE(IsVisibleIfActive(caret))
      << "Initially a caret should be in visible cycle.";

  task_runner->AdvanceTimeAndRun(kInterval);
  EXPECT_FALSE(IsVisibleIfActive(caret)) << "The caret blinks normally.";

  TypingCommand::InsertLineBreak(GetDocument());
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsVisibleIfActive(caret))
      << "The caret should be in visible cycle just after a typing command.";

  task_runner->AdvanceTimeAndRun(kInterval - 1);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsVisibleIfActive(caret))
      << "The typing command reset the timer. The caret is still visible.";

  task_runner->AdvanceTimeAndRun(1);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_FALSE(IsVisibleIfActive(caret))
      << "The caret should blink after the typing command.";
}

TEST_F(FrameCaretTest, ShouldNotBlinkWhenSelectionLooseFocus) {
  FrameCaret& caret = Selection().FrameCaretForTesting();
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);
  GetDocument().body()->SetInnerHTMLWithoutTrustedTypes(
      "<div id='outer' tabindex='-1'>"
      "<div id='input' contenteditable>foo</div>"
      "</div>");
  Element* input = QuerySelector("#input");
  input->Focus();
  QuerySelector("#outer")->Focus();
  UpdateAllLifecyclePhasesForTest();
  const SelectionInDomTree& selection = Selection().GetSelectionInDomTree();
  EXPECT_EQ(selection.Anchor(),
            Position::FirstPositionInNode(*(input->firstChild())));
  EXPECT_FALSE(ShouldShowCaret(caret));
}

TEST_F(FrameCaretTest, ShouldBlinkCaretWhileCaretBrowsing) {
  FrameCaret& caret = Selection().FrameCaretForTesting();
  Selection().SetSelection(SetSelectionTextToBody("<div>a|b</div>"),
                           SetSelectionOptions());
  Selection().SetCaretEnabled(true);
  EXPECT_FALSE(ShouldShowCaret(caret));
  GetDocument().GetFrame()->GetSettings()->SetCaretBrowsingEnabled(true);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(ShouldShowCaret(caret));
}

TEST_F(FrameCaretTest, ShouldShowCaretInReadOnlyInputAndTextarea) {
  FrameCaret& caret = Selection().FrameCaretForTesting();
  scoped_refptr<scheduler::FakeTaskRunner> task_runner =
      base::MakeRefCounted<scheduler::FakeTaskRunner>();
  task_runner->SetTime(0);
  caret.RecreateCaretBlinkTimerForTesting(task_runner.get(),
                                          task_runner->GetMockTickClock());
  const double kInterval = 1;
  LayoutTheme::GetTheme().SetCaretBlinkInterval(base::Seconds(kInterval));
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  SetBodyContent(
      "<input id='inp' readonly value='test'><textarea id='ta' "
      "readonly>hello</textarea>");
  Element* inp = QuerySelector("#inp");
  Element* ta = QuerySelector("#ta");

  inp->Focus();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(ShouldShowCaret(caret));
  EXPECT_TRUE(IsVisibleIfActive(caret));

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_TRUE(IsVisibleIfActive(caret))
      << "Caret should not blink off in readonly input.";

  ta->Focus();
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(ShouldShowCaret(caret));
  EXPECT_TRUE(IsVisibleIfActive(caret));

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_TRUE(IsVisibleIfActive(caret))
      << "Caret should not blink off in readonly textarea.";

  // Removing readonly should re-enable blinking.
  inp->Focus();
  inp->removeAttribute(html_names::kReadonlyAttr);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsVisibleIfActive(caret));

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_FALSE(IsVisibleIfActive(caret))
      << "Caret should blink off when readonly attribute is removed.";

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_TRUE(IsVisibleIfActive(caret))
      << "Caret should blink on again when readonly attribute is removed.";

  // Re-adding readonly should disable blinking again.
  inp->setAttribute(html_names::kReadonlyAttr, AtomicString(""));
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsVisibleIfActive(caret));

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_TRUE(IsVisibleIfActive(caret))
      << "Caret should not blink off when readonly is re-added.";
}

TEST_F(FrameCaretTest, ShouldNotShowCaretInDisabledInputInFocusedAncestor) {
  FrameCaret& caret = Selection().FrameCaretForTesting();
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  for (const char* control :
       {"<input id='control' disabled value='disabled'>",
        "<input id='control' readonly disabled value='both'>",
        "<textarea id='control' disabled>textarea</textarea>"}) {
    SCOPED_TRACE(control);
    SetBodyContent(std::string("<section id='section' tabindex='-1'>") +
                   control + "</section>");
    Element* section = QuerySelector("#section");
    section->Focus();
    UpdateAllLifecyclePhasesForTest();
    ASSERT_EQ(section, GetDocument().FocusedElement());

    // Simulate clicking at the start of the disabled text control. Disabled
    // controls can't be focused, so focus stays on the section.
    auto* text_control = To<TextControlElement>(QuerySelector("#control"));
    Selection().SetSelection(
        SelectionInDomTree::Builder()
            .Collapse(Position(text_control->InnerEditorElement(), 0))
            .Build(),
        SetSelectionOptions());
    UpdateAllLifecyclePhasesForTest();
    EXPECT_EQ(section, GetDocument().FocusedElement());
    EXPECT_TRUE(Selection().SelectionHasFocus());

    EXPECT_FALSE(ShouldShowCaret(caret))
        << "Caret should not be shown in a disabled text control.";
  }

  // A readonly input, unlike a disabled one, can be focused and should then
  // show the caret.
  SetBodyContent(
      "<section tabindex='-1'><input id='control' readonly value='readonly'>"
      "</section>");
  Element* readonly = QuerySelector("#control");
  readonly->Focus();
  UpdateAllLifecyclePhasesForTest();
  ASSERT_EQ(readonly, GetDocument().FocusedElement());
  EXPECT_TRUE(ShouldShowCaret(caret))
      << "Caret should be shown in a focused readonly input.";
}

TEST_F(FrameCaretTest, CaretAnimationNone) {
  ScopedCSSCaretAnimationForTest css_caret_animation_enabled(true);
  FrameCaret& caret = Selection().FrameCaretForTesting();
  scoped_refptr<scheduler::FakeTaskRunner> task_runner =
      base::MakeRefCounted<scheduler::FakeTaskRunner>();
  task_runner->SetTime(0);
  caret.RecreateCaretBlinkTimerForTesting(task_runner.get(),
                                          task_runner->GetMockTickClock());
  const double kInterval = 1;
  LayoutTheme::GetTheme().SetCaretBlinkInterval(base::Seconds(kInterval));
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  InsertStyleElement("body { caret-animation: auto; }");
  SetBodyContent("<textarea>");
  auto* editor = To<Element>(GetDocument().body()->firstChild());
  editor->Focus();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(caret.IsActive());
  EXPECT_TRUE(IsVisibleIfActive(caret))
      << "Initially a caret should be in visible cycle.";

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_FALSE(IsVisibleIfActive(caret)) << "The caret blinks off normally.";

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_TRUE(IsVisibleIfActive(caret)) << "The caret blinks on normally.";
}

TEST_F(FrameCaretTest, BlinkingDisabledByFeature) {
  ScopedCaretBlinkingForTest caret_blinking(false);
  FrameCaret& caret = Selection().FrameCaretForTesting();
  scoped_refptr<scheduler::FakeTaskRunner> task_runner =
      base::MakeRefCounted<scheduler::FakeTaskRunner>();
  task_runner->SetTime(0);
  caret.RecreateCaretBlinkTimerForTesting(task_runner.get(),
                                          task_runner->GetMockTickClock());
  const double kInterval = 1;
  LayoutTheme::GetTheme().SetCaretBlinkInterval(base::Seconds(kInterval));
  GetFocusController().SetActive(true);
  GetFocusController().SetFocused(true);

  SetBodyContent("<textarea>");
  auto* editor = To<Element>(GetDocument().body()->firstChild());
  editor->Focus();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(caret.IsActive());
  EXPECT_TRUE(IsVisibleIfActive(caret))
      << "Initially a caret should be in visible cycle.";

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_TRUE(IsVisibleIfActive(caret)) << "The caret stays on.";

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_TRUE(IsVisibleIfActive(caret)) << "The caret is still on.";
}

TEST_F(FrameCaretTest, CaretAnimationManual) {
  ScopedCSSCaretAnimationForTest css_caret_animation_enabled(true);
  FrameCaret& caret = Selection().FrameCaretForTesting();
  scoped_refptr<scheduler::FakeTaskRunner> task_runner =
      base::MakeRefCounted<scheduler::FakeTaskRunner>();
  task_runner->SetTime(0);
  caret.RecreateCaretBlinkTimerForTesting(task_runner.get(),
                                          task_runner->GetMockTickClock());
  const double kInterval = 1;
  LayoutTheme::GetTheme().SetCaretBlinkInterval(base::Seconds(kInterval));
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  InsertStyleElement("body { caret-animation: manual; }");
  SetBodyContent("<textarea>");
  auto* editor = To<Element>(GetDocument().body()->firstChild());
  editor->Focus();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(caret.IsActive());
  EXPECT_TRUE(IsVisibleIfActive(caret))
      << "Initially a caret should be in visible cycle.";

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_TRUE(IsVisibleIfActive(caret)) << "The caret stays on.";

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_TRUE(IsVisibleIfActive(caret)) << "The caret is still on.";

  editor->SetInlineStyleProperty(CSSPropertyID::kCaretAnimation,
                                 CSSValueID::kAuto);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsVisibleIfActive(caret)) << "The caret starts on again.";

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_FALSE(IsVisibleIfActive(caret)) << "The caret blinks off normally.";
}

TEST_F(FrameCaretTest, CaretAnimationInvalidation) {
  ScopedCSSCaretAnimationForTest css_caret_animation_enabled(true);
  FrameCaret& caret = Selection().FrameCaretForTesting();
  scoped_refptr<scheduler::FakeTaskRunner> task_runner =
      base::MakeRefCounted<scheduler::FakeTaskRunner>();
  task_runner->SetTime(0);
  caret.RecreateCaretBlinkTimerForTesting(task_runner.get(),
                                          task_runner->GetMockTickClock());
  const double kInterval = 1;
  LayoutTheme::GetTheme().SetCaretBlinkInterval(base::Seconds(kInterval));
  GetDocument().GetPage()->GetFocusController().SetActive(true);
  GetDocument().GetPage()->GetFocusController().SetFocused(true);

  InsertStyleElement("body { caret-animation: auto; }");
  SetBodyContent("<textarea>");
  auto* editor = To<Element>(GetDocument().body()->firstChild());
  editor->Focus();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_TRUE(caret.IsActive());
  EXPECT_TRUE(IsVisibleIfActive(caret))
      << "Initially a caret should be in visible cycle.";

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_FALSE(IsVisibleIfActive(caret)) << "The caret blinks off normally.";

  editor->SetInlineStyleProperty(CSSPropertyID::kCaretAnimation,
                                 CSSValueID::kManual);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsVisibleIfActive(caret))
      << "The caret turns on when manual again.";

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_TRUE(IsVisibleIfActive(caret)) << "The caret should stay on.";

  editor->RemoveInlineStyleProperty(CSSPropertyID::kCaretAnimation);
  UpdateAllLifecyclePhasesForTest();
  EXPECT_TRUE(IsVisibleIfActive(caret))
      << "The caret initially on when auto again.";

  task_runner->AdvanceTimeAndRun(kInterval + 0.1);
  EXPECT_FALSE(IsVisibleIfActive(caret)) << "The caret blinks off normally.";
}

}  // namespace blink
