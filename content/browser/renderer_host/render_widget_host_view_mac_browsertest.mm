// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mac.h"

#include <string>

#include "base/functional/bind.h"
#import "base/mac/mac_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "ui/events/test/cocoa_test_event_utils.h"

@interface TextInputFlagChangeWaiter : NSObject
@end

@implementation TextInputFlagChangeWaiter {
  RenderWidgetHostViewCocoa* _rwhv_cocoa;
  std::unique_ptr<base::RunLoop> _run_loop;
}

- (instancetype)initWithRenderWidgetHostViewCocoa:
    (RenderWidgetHostViewCocoa*)rwhv_cocoa {
  if ((self = [super init])) {
    _rwhv_cocoa = rwhv_cocoa;
    [_rwhv_cocoa addObserver:self
                  forKeyPath:@"textInputFlags"
                     options:NSKeyValueObservingOptionNew
                     context:nullptr];
    [self reset];
  }
  return self;
}

- (void)dealloc {
  [_rwhv_cocoa removeObserver:self forKeyPath:@"textInputFlags"];
}

- (void)reset {
  _run_loop = std::make_unique<base::RunLoop>();
}

- (void)wait {
  _run_loop->Run();

  [self reset];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  _run_loop->Quit();
}
@end

namespace content {

namespace {

class TextCallbackWaiter {
 public:
  TextCallbackWaiter() = default;

  TextCallbackWaiter(const TextCallbackWaiter&) = delete;
  TextCallbackWaiter& operator=(const TextCallbackWaiter&) = delete;

  void Wait() { run_loop_.Run(); }

  const std::u16string& text() const { return text_; }

  void GetText(const std::u16string& text) {
    text_ = text;
    run_loop_.Quit();
  }

 private:
  std::u16string text_;
  base::RunLoop run_loop_;
};

class TextSelectionWaiter : public TextInputManager::Observer {
 public:
  explicit TextSelectionWaiter(RenderWidgetHostViewMac* rwhv) {
    rwhv->GetTextInputManager()->AddObserver(this);
  }

  void OnTextSelectionChanged(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override {
    text_input_manager->RemoveObserver(this);
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

class RenderWidgetHostViewMacTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewMacTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSonomaAccessibilityActivationRefinements);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest, GetPageTextForSpeech) {
  GURL url(
      "data:text/html,<span>Hello</span>"
      "<span style='display:none'>Goodbye</span>"
      "<span>World</span>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);

  TextCallbackWaiter waiter;
  rwhv_mac->GetPageTextForSpeech(
      base::BindOnce(&TextCallbackWaiter::GetText, base::Unretained(&waiter)));
  waiter.Wait();

  EXPECT_EQ(u"Hello\nWorld", waiter.text());
}

// Test that -firstRectForCharacterRange:actualRange: works when the range
// isn't in the active selection, which requires a sync IPC to the renderer.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       GetFirstRectForCharacterRangeUncached) {
  GURL url("data:text/html,Hello");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto* web_contents_impl =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  auto* root = web_contents_impl->GetPrimaryFrameTree().root();
  web_contents_impl->GetPrimaryFrameTree().SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance()->group());

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);

  NSRect rect = [rwhv_mac->GetInProcessNSView()
      firstRectForCharacterRange:NSMakeRange(2, 1)
                     actualRange:nullptr];
  EXPECT_GT(NSMinX(rect), 0);
  EXPECT_GT(NSWidth(rect), 0);
  EXPECT_GT(NSHeight(rect), 0);
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest, UpdateInputFlags) {
  class InputMethodObserver {};

  GURL url("data:text/html,<!doctype html><textarea id=ta></textarea>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();
  TextInputFlagChangeWaiter* flag_change_waiter =
      [[TextInputFlagChangeWaiter alloc]
          initWithRenderWidgetHostViewCocoa:rwhv_cocoa];

  EXPECT_TRUE(ExecJs(shell(), "ta.focus();"));
  [flag_change_waiter wait];
  EXPECT_FALSE(rwhv_cocoa.textInputFlags &
               blink::kWebTextInputFlagAutocorrectOff);

  EXPECT_TRUE(ExecJs(
      shell(),
      "ta.setAttribute('autocorrect', 'off'); console.log(ta.outerHTML);"));
  [flag_change_waiter wait];
  EXPECT_TRUE(rwhv_cocoa.textInputFlags &
              blink::kWebTextInputFlagAutocorrectOff);
}

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       InputTextPreventedSyncsCursorLocation) {
  class InputMethodObserver {};

  GURL url("data:text/html,<!doctype html><textarea id=ta></textarea>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  EXPECT_TRUE(ExecJs(
      shell(),
      "ta.addEventListener('keypress', (evt) => {evt.preventDefault();}); "
      "ta.focus();"));

  // Before typing anything, we're at position 0.
  EXPECT_EQ(0lu, [rwhv_cocoa selectedRange].location);

  TextSelectionWaiter waiter(rwhv_mac);
  NSEvent* key_a = cocoa_test_event_utils::KeyEventWithKeyCode(
      'a', 'a', NSEventTypeKeyDown, 0);
  [rwhv_cocoa keyEvent:key_a];

  // After typing 'a', the browser process assumes that the text was entered and
  // we are at position 1.
  EXPECT_EQ(1lu, [rwhv_cocoa selectedRange].location);

  // Wait for the renderer to sync the selection to the browser.
  waiter.Wait();

  // The synced selection updates the selected position back to 0.
  EXPECT_EQ(0lu, [rwhv_cocoa selectedRange].location);
}

// Tests that accessibility role requests sent to the web contents enable
// basic (native + web contents) accessibility support.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       RespondToAccessibilityRoleRequestsOnWebContent) {
  if (base::mac::MacOSVersion() < 14'00'00) {
    GTEST_SKIP();
  }

  // Load some content.
  GURL url("data:text/html,<!doctype html><textarea id=ta></textarea>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  content::BrowserAccessibilityState* accessibility_state =
      content::BrowserAccessibilityState::GetInstance();

  // No accessibility support enabled at this time.
  EXPECT_EQ(accessibility_state->GetAccessibilityMode(), ui::AXMode());

  // An AT descending the AX tree calls -accessibilityRole on the nodes as it
  // goes. Simulate an AT calling -accessibilityRole on the web contents.
  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  [rwhv_cocoa accessibilityRole];

  // Calling -accessibilityRole on the RenderWidgetHostViewCocoa should have
  // activated basic accessibility support.
  EXPECT_EQ(accessibility_state->GetAccessibilityMode(), ui::kAXModeBasic);
}

}  // namespace content
