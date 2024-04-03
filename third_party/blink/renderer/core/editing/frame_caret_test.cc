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
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
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
  GetDocument().body()->setInnerHTML("<textarea>");
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
  GetDocument().body()->setInnerHTML(
      "<div id='outer' tabindex='-1'>"
      "<div id='input' contenteditable>foo</div>"
      "</div>");
  Element* input = GetDocument().QuerySelector(AtomicString("#input"));
  input->Focus();
  Element* outer = GetDocument().QuerySelector(AtomicString("#outer"));
  outer->Focus();
  UpdateAllLifecyclePhasesForTest();
  const SelectionInDOMTree& selection = Selection().GetSelectionInDOMTree();
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

}  // namespace blink
