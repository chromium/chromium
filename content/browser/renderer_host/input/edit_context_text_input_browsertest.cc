// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/scoped_page_focus_override.h"
#include "content/public/test/text_input_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/mojom/text_input_state.mojom.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

namespace content {

namespace {

// End-to-end Browser <-> Renderer Mojo IME (`WidgetInputHandler` and
// `TextInputManager`) tests for `EditContextOnDemand`.
class EditContextOnDemandTextInputBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  WebContents* web_contents() { return shell()->web_contents(); }

  RenderWidgetHost* main_widget_host() {
    return web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost();
  }

  // Enqueues `SetFocus(kFocused)` immediately ahead of the IME message on the
  // same `WidgetInputHandler` Mojo pipe so main-frame
  // `WebFrameWidgetImpl::ShouldHandleImeEvents()` returns true even if a
  // parallel `ContentShell` process stole native OS window focus.
  void SendImeSetComposition(const std::u16string& text,
                             const gfx::Range& replacement_range,
                             int selection_start,
                             int selection_end) {
    RenderWidgetHost* rwh = main_widget_host();
    ASSERT_NE(rwh, nullptr);
    rwh->Focus();
    SendImeSetCompositionTextToWidget(rwh, text, {}, replacement_range,
                                      selection_start, selection_end);
  }

  void SendImeCommitText(const std::u16string& text,
                         const gfx::Range& replacement_range,
                         int relative_cursor_pos) {
    RenderWidgetHost* rwh = main_widget_host();
    ASSERT_NE(rwh, nullptr);
    rwh->Focus();
    SendImeCommitTextToWidget(rwh, text, {}, replacement_range,
                              relative_cursor_pos);
  }

  void FocusWidgetAndWaitForTextInputValue(const std::u16string& expected) {
    RenderWidgetHost* rwh = main_widget_host();
    ASSERT_NE(rwh, nullptr);
    rwh->Focus();
    WaitForBrowserTextInputValue(expected);
  }

  void WaitForBrowserTextInputValue(const std::u16string& expected) {
    while (true) {
      const ui::mojom::TextInputState* state =
          GetTextInputStateFromWebContents(web_contents());
      if (state && state->value.has_value() && *state->value == expected) {
        return;
      }
      base::RunLoop loop;
      TextInputManagerTester tester(web_contents());
      tester.SetUpdateTextInputStateCalledCallback(loop.QuitClosure());
      // Pump an animation frame in case the update is still queued in Blink.
      std::ignore =
          EvalJs(web_contents(), "new Promise(r => requestAnimationFrame(r))");
      const ui::mojom::TextInputState* after_raf =
          GetTextInputStateFromWebContents(web_contents());
      if (after_raf && after_raf->value.has_value() &&
          *after_raf->value == expected) {
        return;
      }
      loop.Run();
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kEditContextOnDemand};
};

// Browser -> Renderer Mojo `WidgetInputHandler` IME composition and commit on
// a multi-paragraph document model that serializes only the active paragraph
// window on `contextrequest` and re-invalidates after commit.
IN_PROC_BROWSER_TEST_F(EditContextOnDemandTextInputBrowserTest,
                       WindowedDocumentModelImeCompositionAndCommitOverMojo) {
  const GURL url = embedded_test_server()->GetURL("/simple_page.html");
  ASSERT_TRUE(NavigateToURL(shell(), url));
  ScopedPageFocusOverride focus_override(web_contents());

  ASSERT_TRUE(ExecJs(web_contents(), R"JS(
    const editor = document.createElement('div');
    editor.id = 'editor';
    // Suppress the Android emulator's live system soft keyboard (Gboard) from
    // asynchronously sending unrequested `setComposingRegion()` calls during
    // synthetic `WidgetInputHandler` Mojo tests.
    editor.inputMode = 'none';
    document.body.appendChild(editor);
    window.paragraphs = [
      'Header paragraph',
      'Active: hello ',
      'Footer paragraph',
    ];
    window.activeIdx = 1;
    window.cursorInPara = window.paragraphs[1].length;
    window.events = [];
    window.inComposition = false;
    window.finalCommitDone = new Promise((resolve) => {
      window.resolveFinalCommit = resolve;
    });

    window.ec = new EditContext({
      text: '_',
      selectionStart: 1,
      selectionEnd: 1,
    });
    editor.editContext = window.ec;

    window.ec.addEventListener('contextrequest', () => {
      window.events.push('contextrequest');
      const para = window.paragraphs[window.activeIdx];
      window.ec.updateText(0, window.ec.text.length, para);
      window.ec.updateSelection(window.cursorInPara, window.cursorInPara);
    });
    window.ec.addEventListener('compositionstart', () => {
      window.inComposition = true;
      window.events.push('compositionstart');
    });
    window.ec.addEventListener('textupdate', (e) => {
      window.events.push('textupdate:' + e.text);
      const para = window.paragraphs[window.activeIdx];
      window.paragraphs[window.activeIdx] =
          para.slice(0, e.updateRangeStart) + e.text +
          para.slice(e.updateRangeEnd);
      window.cursorInPara = e.selectionEnd;
      if (!window.inComposition) {
        window.ec.invalidate();
        if (e.text === ' Nice.') {
          window.resolveFinalCommit(true);
        }
      }
    });
    window.ec.addEventListener('compositionend', (e) => {
      window.inComposition = false;
      window.events.push('compositionend:' + e.data);
      window.ec.invalidate();
    });

    editor.focus();
  )JS"));
  ASSERT_EQ(
      true,
      EvalJs(web_contents(),
             "document.hasFocus() && "
             "document.activeElement === document.getElementById('editor')"));

  FocusWidgetAndWaitForTextInputValue(u"_");
  ASSERT_TRUE(ExecJs(web_contents(), "window.ec.invalidate();"));

  SendImeSetComposition(u"wor", gfx::Range::InvalidRange(), 3, 3);
  SendImeSetComposition(u"world", gfx::Range::InvalidRange(), 5, 5);
  SendImeCommitText(u"world!", gfx::Range::InvalidRange(), 0);

  // Follow up with a non-composition commit after `compositionend`
  // re-invalidated `ec`.
  SendImeCommitText(u" Nice.", gfx::Range::InvalidRange(), 0);

  // Synchronize with the compositor-thread -> Blink main-thread
  // `ImeCommitText()` task hop before inspecting JS state on `LocalFrame`.
  ASSERT_EQ(true, EvalJs(web_contents(), "window.finalCommitDone"));

  EXPECT_EQ(EvalJs(web_contents(), "window.paragraphs[1]"),
            "Active: hello world! Nice.");
  EXPECT_EQ(EvalJs(web_contents(), "window.events.join('|')"),
            "contextrequest|compositionstart|textupdate:wor|textupdate:world|"
            "textupdate:world!|compositionend:world!|contextrequest|textupdate:"
            " Nice.");
}

// Verifies crbug.com/587428137 safeguards across `TextInputManager` and
// `WidgetInputHandler`:
// - `invalidate()` does not eagerly pull during passive `BeginMainFrame()`,
//   leaving browser `TextInputManager` with the pre-invalidation value.
// - An `ImeCommitText()` carrying an explicit `replacement_range` computed
//   against that stale `TextInputState` pulls fresh context, bails out instead
//   of overwriting stale offsets, and syncs the fresh buffer back to browser
//   `TextInputManager`.
// - Once synced, a follow-up explicit `replacement_range` commit succeeds.
IN_PROC_BROWSER_TEST_F(
    EditContextOnDemandTextInputBrowserTest,
    StaleExplicitReplacementRangeBailsOutAndSyncsFreshTextInputState) {
  const GURL url = embedded_test_server()->GetURL("/simple_page.html");
  ASSERT_TRUE(NavigateToURL(shell(), url));
  ScopedPageFocusOverride focus_override(web_contents());

  ASSERT_TRUE(ExecJs(web_contents(), R"JS(
    const editor = document.createElement('div');
    editor.id = 'editor';
    editor.inputMode = 'none';
    document.body.appendChild(editor);
    window.ec = new EditContext({
      text: 'cat dog ',
      selectionStart: 8,
      selectionEnd: 8,
    });
    editor.editContext = window.ec;
    window.contextRequestCount = 0;
    window.ec.addEventListener('contextrequest', () => {
      window.contextRequestCount++;
      window.ec.updateText(0, window.ec.text.length, 'dog cat ');
      window.ec.updateSelection(8, 8);
    });
    editor.focus();
  )JS"));
  ASSERT_EQ(
      true,
      EvalJs(web_contents(),
             "document.hasFocus() && "
             "document.activeElement === document.getElementById('editor')"));

  FocusWidgetAndWaitForTextInputValue(u"cat dog ");

  ASSERT_TRUE(ExecJs(web_contents(), "window.ec.invalidate();"));
  ASSERT_TRUE(ExecJs(web_contents(),
                     "new Promise(r => requestAnimationFrame(() => r()))"));
  EXPECT_EQ(EvalJs(web_contents(), "window.contextRequestCount"), 0);

  SendImeCommitText(u"fox", gfx::Range(4, 7), 0);
  WaitForBrowserTextInputValue(u"dog cat ");

  EXPECT_EQ(EvalJs(web_contents(), "window.contextRequestCount"), 1);
  EXPECT_EQ(EvalJs(web_contents(), "window.ec.text"), "dog cat ");

  SendImeCommitText(u"bird", gfx::Range(4, 7), 0);
  WaitForBrowserTextInputValue(u"dog bird ");
  EXPECT_EQ(EvalJs(web_contents(), "window.contextRequestCount"), 1);
  EXPECT_EQ(EvalJs(web_contents(), "window.ec.text"), "dog bird ");
}

// Independently exercises re-entrant `invalidate()` inside `contextrequest`
// (without calling `updateText()`) and inside `SetComposition()`'s `textupdate`
// listener across separate Browser -> Renderer Mojo IPC steps so neither path
// masks a regression in the other.
IN_PROC_BROWSER_TEST_F(
    EditContextOnDemandTextInputBrowserTest,
    ReentrantInvalidateInContextRequestAndCompositionOverMojo) {
  const GURL url = embedded_test_server()->GetURL("/simple_page.html");
  ASSERT_TRUE(NavigateToURL(shell(), url));
  ScopedPageFocusOverride focus_override(web_contents());

  ASSERT_TRUE(ExecJs(web_contents(), R"JS(
    const editor = document.createElement('div');
    editor.id = 'editor';
    editor.inputMode = 'none';
    document.body.appendChild(editor);
    window.ec = new EditContext({
      text: 'base ',
      selectionStart: 5,
      selectionEnd: 5,
    });
    editor.editContext = window.ec;
    window.contextRequestCount = 0;
    window.reenterInContextRequest = true;
    window.invalidateInCompositionTextUpdate = true;
    window.compositionInvalidatedDone = new Promise((resolve) => {
      window.resolveCompositionInvalidated = resolve;
    });
    window.finalCommitDone = new Promise((resolve) => {
      window.resolveFinalCommit = resolve;
    });

    window.ec.addEventListener('contextrequest', () => {
      window.contextRequestCount++;
      if (window.reenterInContextRequest) {
        window.reenterInContextRequest = false;
        window.ec.invalidate();
      }
    });
    window.ec.addEventListener('textupdate', (e) => {
      if (e.text === 'comp' && window.invalidateInCompositionTextUpdate) {
        window.invalidateInCompositionTextUpdate = false;
        window.ec.invalidate();
        window.resolveCompositionInvalidated(true);
      } else if (e.text === 'done') {
        window.resolveFinalCommit(true);
      }
    });
    editor.focus();
  )JS"));
  ASSERT_EQ(
      true,
      EvalJs(web_contents(),
             "document.hasFocus() && "
             "document.activeElement === document.getElementById('editor')"));

  FocusWidgetAndWaitForTextInputValue(u"base ");

  // Step 1: Re-entrant `invalidate()` inside `contextrequest` only
  // (`textupdate` for `'one '` does not invalidate). The dirty flag must remain
  // set after `CleanContextIfNeeded()` finishes so Step 2 pulls context again.
  ASSERT_TRUE(ExecJs(web_contents(), "window.ec.invalidate();"));
  SendImeCommitText(u"one ", gfx::Range::InvalidRange(), 0);
  WaitForBrowserTextInputValue(u"base one ");
  EXPECT_EQ(EvalJs(web_contents(), "window.contextRequestCount"), 1);

  // Step 2: Confirm Step 1 kept the context dirty (bumping count to 2), and
  // independently exercise `invalidate()` inside `SetComposition()`'s
  // `textupdate` listener (which cancels `'comp'` and re-dirties the context).
  SendImeSetComposition(u"comp", gfx::Range::InvalidRange(), 4, 4);
  ASSERT_EQ(true, EvalJs(web_contents(), "window.compositionInvalidatedDone"));
  EXPECT_EQ(EvalJs(web_contents(), "window.contextRequestCount"), 2);
  EXPECT_EQ(EvalJs(web_contents(), "window.ec.text"), "base one ");

  // Step 3: Confirm Step 2's `invalidate()` in `textupdate` left the context
  // dirty so committing `'done'` triggers a 3rd `contextrequest`.
  SendImeCommitText(u"done", gfx::Range::InvalidRange(), 0);
  ASSERT_EQ(true, EvalJs(web_contents(), "window.finalCommitDone"));
  WaitForBrowserTextInputValue(u"base one done");

  EXPECT_EQ(EvalJs(web_contents(), "window.contextRequestCount"), 3);
  EXPECT_EQ(EvalJs(web_contents(), "window.ec.text"), "base one done");
}

}  // namespace

}  // namespace content
