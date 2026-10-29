// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mac.h"

#import <Carbon/Carbon.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#import "base/mac/mac_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"
#include "content/browser/renderer_host/text_input_client_mac.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/features.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "ui/events/test/cocoa_test_event_utils.h"
#include "ui/gfx/geometry/rect.h"

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

@interface FakeTextCheckingResult : NSTextCheckingResult <NSCopying>
@property(readonly) NSRange range;
@property(readonly) NSString* replacementString;
@property(readonly) NSTextCheckingType resultType;
@end

@implementation FakeTextCheckingResult

@synthesize range = _range;
@synthesize replacementString = _replacementString;
@synthesize resultType = _resultType;

+ (FakeTextCheckingResult*)resultWithRange:(NSRange)range
                         replacementString:(NSString*)replacementString {
  FakeTextCheckingResult* result = [[FakeTextCheckingResult alloc] init];
  result->_range = range;
  result->_replacementString = replacementString;
  result->_resultType = NSTextCheckingTypeReplacement;
  return result;
}

- (NSString*)replacementString {
  return _replacementString;
}

- (NSTextCheckingResult*)resultByAdjustingRangesWithOffset:(NSInteger)offset {
  NSRange adjustedRange = NSMakeRange(_range.location + offset, _range.length);
  return [FakeTextCheckingResult resultWithRange:adjustedRange
                               replacementString:_replacementString];
}

@end

@interface FakeSpellChecker : NSObject
@property(strong) void (^correctionCompletionHandler)(NSString*);
@property(strong) NSString* acceptedString;
// The number of correction indicators shown, and the rect of the last one.
@property NSUInteger correctionIndicatorCount;
@property NSRect correctionIndicatorRect;
- (void)waitForCheckString;
- (void)reset;
@end

@implementation FakeSpellChecker {
  std::unique_ptr<base::RunLoop> _run_loop;
}

@synthesize correctionCompletionHandler = _correctionCompletionHandler;
@synthesize acceptedString = _acceptedString;
@synthesize correctionIndicatorCount = _correctionIndicatorCount;
@synthesize correctionIndicatorRect = _correctionIndicatorRect;

- (instancetype)init {
  if (self = [super init]) {
    [self reset];
  }
  return self;
}

- (void)reset {
  _run_loop = std::make_unique<base::RunLoop>();
}

- (void)waitForCheckString {
  _run_loop->Run();
  [self reset];
}

// Mock spell checker that provides text replacement "omw" -> "On my way".
- (NSArray<NSTextCheckingResult*>*)
               checkString:(NSString*)stringToCheck
                     range:(NSRange)range
                     types:(NSTextCheckingTypes)checkingTypes
                   options:(nullable NSDictionary<NSTextCheckingOptionKey, id>*)
                               options
    inSpellDocumentWithTag:(NSInteger)tag
               orthography:(NSOrthography**)orthography
                 wordCount:(NSInteger*)wordCount {
  if (_run_loop) {
    _run_loop->Quit();
  }

  NSRange omwRange = [stringToCheck rangeOfString:@"omw"];
  if (omwRange.location != NSNotFound) {
    FakeTextCheckingResult* fakeResult =
        [FakeTextCheckingResult resultWithRange:omwRange
                              replacementString:@"On my way"];
    return @[ static_cast<NSTextCheckingResult*>(fakeResult) ];
  }
  return @[];
}

- (void)showCorrectionIndicatorOfType:(NSCorrectionIndicatorType)type
                        primaryString:(NSString*)primaryString
                   alternativeStrings:(NSArray<NSString*>*)alternativeStrings
                      forStringInRect:(NSRect)rect
                                 view:(NSView*)view
                    completionHandler:(void (^)(NSString*))completionHandler {
  self.correctionCompletionHandler = completionHandler;
  self.acceptedString = primaryString;
  _correctionIndicatorCount++;
  _correctionIndicatorRect = rect;
}

- (void)dismissCorrectionIndicatorForView:(NSView*)view {
  // Call the completion handler with the accepted string when dismissed
  if (self.correctionCompletionHandler) {
    self.correctionCompletionHandler(self.acceptedString);
    self.correctionCompletionHandler = nil;
    self.acceptedString = nil;
  }
}

- (BOOL)preventsAutocorrectionBeforeString:(NSString*)string
                                  language:(NSString*)language {
  return NO;
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

  void GetText(std::u16string_view text) {
    text_ = std::u16string(text);
    run_loop_.Quit();
  }

 private:
  std::u16string text_;
  base::RunLoop run_loop_;
};

class TextSelectionWaiter : public TextInputManager::Observer {
 public:
  explicit TextSelectionWaiter(RenderWidgetHostViewMac* rwhv,
                               int expected_changes = 1)
      : expected_changes_(expected_changes), observed_changes_(0) {
    rwhv->GetTextInputManager()->AddObserver(this);
  }

  void OnTextSelectionChanged(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override {
    observed_changes_++;
    if (observed_changes_ >= expected_changes_) {
      text_input_manager->RemoveObserver(this);
      run_loop_.Quit();
    }
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  int expected_changes_;
  int observed_changes_;
};

// Stores GetFirstRectForRange() requests instead of sending them to the
// renderer, so that tests control when the responses arrive.
class HoldingTextInputClientMacDelegate
    : public TextInputClientMac::AsyncRequestDelegate {
 public:
  HoldingTextInputClientMacDelegate() = default;
  ~HoldingTextInputClientMacDelegate() override = default;

  void GetCharacterIndexAtPoint(
      RenderFrameHost* rfh,
      const TextInputClientMac::RequestToken& request_token,
      const gfx::Point& point) override {}

  void GetFirstRectForRange(
      RenderFrameHost* rfh,
      const TextInputClientMac::RequestToken& request_token,
      const gfx::Range& range) override {
    first_rect_requests_.push_back(request_token);
  }

  const std::vector<TextInputClientMac::RequestToken>& first_rect_requests()
      const {
    return first_rect_requests_;
  }

 private:
  std::vector<TextInputClientMac::RequestToken> first_rect_requests_;
};

// Holds GetFirstRectForRange() requests in a HoldingTextInputClientMacDelegate
// while in scope, with a timeout long enough that they don't expire during a
// test.
class ScopedHeldFirstRectRequests {
 public:
  ScopedHeldFirstRectRequests()
      : original_timeout_(
            TextInputClientMac::GetInstance()->GetTimeoutForTesting()) {
    auto delegate = std::make_unique<HoldingTextInputClientMacDelegate>();
    delegate_ = delegate.get();
    TextInputClientMac::GetInstance()->SetTimeoutForTesting(
        TestTimeouts::action_timeout());
    TextInputClientMac::GetInstance()->SetAsyncRequestDelegateForTesting(
        std::move(delegate));
  }

  ScopedHeldFirstRectRequests(const ScopedHeldFirstRectRequests&) = delete;
  ScopedHeldFirstRectRequests& operator=(const ScopedHeldFirstRectRequests&) =
      delete;

  ~ScopedHeldFirstRectRequests() {
    delegate_ = nullptr;
    TextInputClientMac::GetInstance()->SetAsyncRequestDelegateForTesting(
        nullptr);
    TextInputClientMac::GetInstance()->SetTimeoutForTesting(original_timeout_);
  }

  const std::vector<TextInputClientMac::RequestToken>& requests() const {
    return delegate_->first_rect_requests();
  }

  // Responds to all held requests with `rect`, and waits for the responses to
  // be handled.
  void RespondToAll(const gfx::Rect& rect) {
    for (const TextInputClientMac::RequestToken& request_token : requests()) {
      TextInputClientMac::GetInstance()->SetFirstRectAndSignal(request_token,
                                                               rect);
    }
    base::RunLoop run_loop;
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  const base::TimeDelta original_timeout_;
  raw_ptr<HoldingTextInputClientMacDelegate> delegate_ = nullptr;
};

// Makes the primary main frame the focused frame, so that TextInputClientMac
// sends its requests there.
void FocusPrimaryMainFrame(Shell* shell) {
  auto* web_contents_impl =
      static_cast<WebContentsImpl*>(shell->web_contents());
  auto* root = web_contents_impl->GetPrimaryFrameTree().root();
  web_contents_impl->GetPrimaryFrameTree().SetFocusedFrame(
      root, root->current_frame_host()->GetSiteInstance()->group());
}

// Types `text` into `rwhv_cocoa`, waiting for each character to be checked for
// text replacements.
void TypeTextAndWaitForChecks(RenderWidgetHostViewCocoa* rwhv_cocoa,
                              FakeSpellChecker* fakeSpellChecker,
                              NSString* text) {
  for (NSUInteger i = 0; i < text.length; i++) {
    unichar c = [text characterAtIndex:i];
    [rwhv_cocoa keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                             c, c, NSEventTypeKeyDown, 0)];
    [rwhv_cocoa keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                             c, c, NSEventTypeKeyUp, 0)];
    [fakeSpellChecker waitForCheckString];
  }
}

// Presses and releases the key with `key_code`, which types `character`.
void PressKey(RenderWidgetHostViewCocoa* rwhv_cocoa,
              unsigned short key_code,
              unichar character,
              NSUInteger modifiers) {
  [rwhv_cocoa keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                           key_code, character, NSEventTypeKeyDown, modifiers)];
  [rwhv_cocoa keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                           key_code, character, NSEventTypeKeyUp, modifiers)];
}

}  // namespace

class RenderWidgetHostViewMacTest : public ContentBrowserTest {
 public:
  RenderWidgetHostViewMacTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSonomaAccessibilityActivationRefinements);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    // UpdateInputFlags asserts the spec-default autocorrect IME mapping, which
    // is gated behind the WebAutocorrectByDefault runtime feature.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "WebAutocorrectByDefault");
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
  // If AllowRangeOutsideSelection is enabled, the result is allowed to be empty
  // to skip the sync IPC.
  if (base::FeatureList::IsEnabled(
          features::kCachedFirstRectAllowRangeOutsideSelection)) {
    GTEST_SKIP();
  }

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

  GURL url("data:text/html,<!doctype html><textarea id=ta></textarea>"
           "<input type=email id=em>");
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

  // An <input type=email> (and likewise type=url/password) has a used
  // autocorrection state of Off per spec even without an autocorrect
  // attribute, so the Off flag is set. Focusing it also changes the
  // autocapitalize bits, so textInputFlags is observed to change.
  EXPECT_TRUE(ExecJs(shell(), "em.focus();"));
  [flag_change_waiter wait];
  EXPECT_TRUE(rwhv_cocoa.textInputFlags &
              blink::kWebTextInputFlagAutocorrectOff);
}

// TODO(crbug.com/421820726): Enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_InputTextPreventedSyncsCursorLocation \
  DISABLED_InputTextPreventedSyncsCursorLocation
#else
#define MAYBE_InputTextPreventedSyncsCursorLocation \
  InputTextPreventedSyncsCursorLocation
#endif
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       MAYBE_InputTextPreventedSyncsCursorLocation) {
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

  // Enable platform activation since that is what is begin tested here.
  BrowserAccessibilityState::GetInstance()->SetActivationFromPlatformEnabled(
      /*enabled=*/true);

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

IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       WebAccessibilityRootUsesScrollArea) {
  GURL url("data:text/html,<!doctype html><p>Web content</p>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  std::unique_ptr<ScopedAccessibilityMode> accessibility_mode =
      BrowserAccessibilityState::GetInstance()->CreateScopedModeForProcess(
          ui::kAXModeComplete);

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return rwhv_mac->GetRootBrowserAccessibilityElement() != nil; }));

  id web_area = rwhv_mac->GetRootBrowserAccessibilityElement();
  EXPECT_TRUE(rwhv_cocoa.isAccessibilityElement);
  EXPECT_TRUE(
      [rwhv_cocoa.accessibilityRole isEqual:NSAccessibilityScrollAreaRole]);
  ASSERT_EQ(1u, rwhv_cocoa.accessibilityChildren.count);
  EXPECT_EQ(web_area, rwhv_cocoa.accessibilityChildren[0]);
  EXPECT_EQ(rwhv_cocoa, [web_area accessibilityParent]);

  NSArray* children = NSAccessibilityUnignoredChildren(@[ rwhv_cocoa ]);
  ASSERT_EQ(1u, children.count);
  EXPECT_EQ(rwhv_cocoa, children[0]);
}

// Tests that text replacement is not accepted during intermediate selection
// adjustment on backward deletion in EditContext.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       EditContextTextReplacementOnBackSpace) {
  // Create a simple HTML page with EditContext.
  GURL url("data:text/html,<div id=editor></div>"
           "<script>"
           "const editor = document.getElementById('editor');"
           "editor.editContext = new EditContext();"
           "</script>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  // Set up fake spell checker to provide text replacement.
  FakeSpellChecker* fakeSpellChecker = [[FakeSpellChecker alloc] init];
  rwhv_cocoa.spellCheckerForTesting =
      static_cast<NSSpellChecker*>(fakeSpellChecker);

  EXPECT_TRUE(ExecJs(shell(), "editor.focus()"));

  // Ensure the input router is active to handle key events.
  static_cast<RenderWidgetHostImpl*>(rwhv->GetRenderWidgetHost())
      ->input_router()
      ->MakeActive();

  // Type "omw" character by character.
  NSString* testWord = @"omw";
  for (NSUInteger i = 0; i < [testWord length]; i++) {
    unichar c = [testWord characterAtIndex:i];
    NSEvent* keyDownEvent = cocoa_test_event_utils::KeyEventWithKeyCode(
        c, c, NSEventTypeKeyDown, 0);
    [rwhv_cocoa keyEvent:keyDownEvent];

    NSEvent* keyUpEvent =
        cocoa_test_event_utils::KeyEventWithKeyCode(c, c, NSEventTypeKeyUp, 0);
    [rwhv_cocoa keyEvent:keyUpEvent];

    [fakeSpellChecker waitForCheckString];
  }

  std::string current_value =
      EvalJs(shell(), "editor.editContext.text").ExtractString();
  EXPECT_EQ("omw", current_value);

  // For active text replacement suggestion, the completion handler should be
  // set once the bounds of the text to be replaced arrive.
  ASSERT_TRUE(base::test::RunUntil([&] {
    return fakeSpellChecker.correctionCompletionHandler != nil;
  })) << "showCorrectionIndicatorOfType should be called";

  TextSelectionWaiter selection_waiter(rwhv_mac);
  NSEvent* backspaceDownEvent = cocoa_test_event_utils::KeyEventWithKeyCode(
      kVK_Delete, '\b', NSEventTypeKeyDown, 0);
  [rwhv_cocoa keyEvent:backspaceDownEvent];

  NSEvent* backspaceUpEvent = cocoa_test_event_utils::KeyEventWithKeyCode(
      kVK_Delete, '\b', NSEventTypeKeyUp, 0);
  [rwhv_cocoa keyEvent:backspaceUpEvent];
  selection_waiter.Wait();

  std::string final_value =
      EvalJs(shell(), "editor.editContext.text").ExtractString();

  EXPECT_EQ("om", final_value)
      << "Text replacement should not be accepted on backward deletion.";
}

// Tests that text replacement is not accepted during DOM update when there is
// an active EditContext.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       EditContextTextReplacementOnDOMSelectionUpdate) {
  // Create a simple HTML page with EditContext.
  GURL url("data:text/html,<div id=editor></div>"
           "<script>"
           "const editor = document.getElementById('editor');"
           "editor.editContext = new EditContext();"
           "editor.editContext.addEventListener('textupdate', (event) => {"
           "if(editor.editContext.text == 'omw') {"
           "editor.innerText = 'omw';"
           "document.getSelection().collapse(editor.firstChild, "
           "editor.innerText.length);"
           "}"
           "});"
           "</script>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  // Set up fake spell checker to provide text replacement.
  FakeSpellChecker* fakeSpellChecker = [[FakeSpellChecker alloc] init];
  rwhv_cocoa.spellCheckerForTesting =
      static_cast<NSSpellChecker*>(fakeSpellChecker);

  EXPECT_TRUE(ExecJs(shell(), "editor.focus()"));

  // Ensure the input router is active to handle key events.
  static_cast<RenderWidgetHostImpl*>(rwhv->GetRenderWidgetHost())
      ->input_router()
      ->MakeActive();

  // Type "omw" character by character.
  NSString* testWord = @"omw";
  for (NSUInteger i = 0; i < [testWord length]; i++) {
    unichar c = [testWord characterAtIndex:i];
    NSEvent* keyDownEvent = cocoa_test_event_utils::KeyEventWithKeyCode(
        c, c, NSEventTypeKeyDown, 0);
    [rwhv_cocoa keyEvent:keyDownEvent];

    NSEvent* keyUpEvent =
        cocoa_test_event_utils::KeyEventWithKeyCode(c, c, NSEventTypeKeyUp, 0);
    [rwhv_cocoa keyEvent:keyUpEvent];

    [fakeSpellChecker waitForCheckString];
  }

  std::string text = EvalJs(shell(), "editor.editContext.text").ExtractString();
  EXPECT_EQ("omw", text);

  // For active text replacement suggestion, the completion handler should be
  // set once the bounds of the text to be replaced arrive.
  ASSERT_TRUE(base::test::RunUntil([&] {
    return fakeSpellChecker.correctionCompletionHandler != nil;
  })) << "showCorrectionIndicatorOfType should be called";

  // Wait for any pending DOM update or selection sync to complete between
  // browser and renderer process.
  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop->QuitClosure(), base::Milliseconds(1000));
  run_loop->Run();

  // Verify that the text is set in editor and DOM selection is updated.
  text = EvalJs(shell(), "editor.editContext.text").ExtractString();
  EXPECT_EQ("omw", text)
      << "Text replacement should not be accepted on DOM selection update.";
  EXPECT_EQ(
      3, EvalJs(shell(), "document.getSelection().anchorOffset").ExtractInt());
  EXPECT_EQ(
      3, EvalJs(shell(), "document.getSelection().focusOffset").ExtractInt());

  // Wait for selection sync between browser and renderer process.
  // Selection should be synced twice: [1] for inserting ' ' and [2] when text
  // replacement is accepted.
  TextSelectionWaiter selection_waiter(rwhv_mac, 2);
  // Type space to trigger text replacement.
  NSEvent* spaceDownEvent = cocoa_test_event_utils::KeyEventWithKeyCode(
      ' ', ' ', NSEventTypeKeyDown, 0);
  [rwhv_cocoa keyEvent:spaceDownEvent];

  NSEvent* spaceUpEvent = cocoa_test_event_utils::KeyEventWithKeyCode(
      ' ', ' ', NSEventTypeKeyUp, 0);
  [rwhv_cocoa keyEvent:spaceUpEvent];
  selection_waiter.Wait();

  text = EvalJs(shell(), "editor.editContext.text").ExtractString();
  EXPECT_EQ("On my way ", text)
      << "Text replacement should be accepted on space.";
}

// Tests that text replacement suggestion is not shown when typing space before
// the replacement string (i.e. when cursor is at the beginning of the
// replacement string, rather than at the end).
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       TextReplacementNotShownWhenSpaceInsertedAtStart) {
  GURL url("data:text/html,<textarea id=editor autofocus>omw</textarea>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  // Set up fake spell checker to provide text replacement ("omw" -> "On my
  // way").
  FakeSpellChecker* fakeSpellChecker = [[FakeSpellChecker alloc] init];
  rwhv_cocoa.spellCheckerForTesting =
      static_cast<NSSpellChecker*>(fakeSpellChecker);

  EXPECT_TRUE(ExecJs(shell(), "editor.focus()"));

  // Ensure the input router is active to handle key events.
  static_cast<RenderWidgetHostImpl*>(rwhv->GetRenderWidgetHost())
      ->input_router()
      ->MakeActive();

  // Place the cursor at the beginning of the text (index 0, before "omw").
  EXPECT_TRUE(ExecJs(shell(), "editor.setSelectionRange(0, 0);"));

  EXPECT_TRUE(fakeSpellChecker.correctionCompletionHandler == nil);

  // Press space at the beginning of the text.
  NSEvent* spaceDownEvent = cocoa_test_event_utils::KeyEventWithKeyCode(
      ' ', ' ', NSEventTypeKeyDown, 0);
  [rwhv_cocoa keyEvent:spaceDownEvent];

  NSEvent* spaceUpEvent = cocoa_test_event_utils::KeyEventWithKeyCode(
      ' ', ' ', NSEventTypeKeyUp, 0);
  [rwhv_cocoa keyEvent:spaceUpEvent];

  [fakeSpellChecker waitForCheckString];

  std::string text = EvalJs(shell(), "editor.value").ExtractString();
  EXPECT_EQ(" omw", text);

  // Correction indicator should not be shown because the cursor is at index 1
  // (the start of "omw"), not at the end of "omw" (index 4).
  EXPECT_TRUE(fakeSpellChecker.correctionCompletionHandler == nil)
      << "showCorrectionIndicatorOfType should not be called when cursor is at "
         "start";
}

// Tests that text replacement suggestion is not shown when completing a
// replacement string in the middle (e.g. typing 'm' to make "ow" into "omw").
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       TextReplacementNotShownWhenCharInsertedInMiddle) {
  GURL url("data:text/html,<textarea id=editor autofocus>ow</textarea>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  // Set up fake spell checker to provide text replacement ("omw" -> "On my
  // way").
  FakeSpellChecker* fakeSpellChecker = [[FakeSpellChecker alloc] init];
  rwhv_cocoa.spellCheckerForTesting =
      static_cast<NSSpellChecker*>(fakeSpellChecker);

  EXPECT_TRUE(ExecJs(shell(), "editor.focus()"));

  // Ensure the input router is active to handle key events.
  static_cast<RenderWidgetHostImpl*>(rwhv->GetRenderWidgetHost())
      ->input_router()
      ->MakeActive();

  // Place the cursor in the middle (index 1, between 'o' and 'w').
  EXPECT_TRUE(ExecJs(shell(), "editor.setSelectionRange(1, 1);"));

  EXPECT_TRUE(fakeSpellChecker.correctionCompletionHandler == nil);

  // Type 'm' in the middle of the text.
  NSEvent* mDownEvent = cocoa_test_event_utils::KeyEventWithKeyCode(
      'm', 'm', NSEventTypeKeyDown, 0);
  [rwhv_cocoa keyEvent:mDownEvent];

  NSEvent* mUpEvent = cocoa_test_event_utils::KeyEventWithKeyCode(
      'm', 'm', NSEventTypeKeyUp, 0);
  [rwhv_cocoa keyEvent:mUpEvent];

  [fakeSpellChecker waitForCheckString];

  std::string text = EvalJs(shell(), "editor.value").ExtractString();
  EXPECT_EQ("omw", text);

  // Correction indicator should not be shown because the cursor is at index 2
  // (after 'm', in the middle of "omw"), not at the end of "omw" (index 3).
  EXPECT_TRUE(fakeSpellChecker.correctionCompletionHandler == nil)
      << "showCorrectionIndicatorOfType should not be called when cursor is in "
         "middle";
}

// Tests that the text replacement indicator is shown for the bounds of the
// text to be replaced.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       TextReplacementIndicatorCoversReplacedText) {
  GURL url("data:text/html,<div id=editor contenteditable "
           "style='font:20px monospace;padding:10px'></div>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  // Set up fake spell checker to provide text replacement ("omw" -> "On my
  // way").
  FakeSpellChecker* fakeSpellChecker = [[FakeSpellChecker alloc] init];
  rwhv_cocoa.spellCheckerForTesting =
      static_cast<NSSpellChecker*>(fakeSpellChecker);

  EXPECT_TRUE(ExecJs(shell(), "editor.focus()"));
  FocusPrimaryMainFrame(shell());

  // Ensure the input router is active to handle key events.
  static_cast<RenderWidgetHostImpl*>(rwhv->GetRenderWidgetHost())
      ->input_router()
      ->MakeActive();

  TypeTextAndWaitForChecks(rwhv_cocoa, fakeSpellChecker, @"omw");
  ASSERT_EQ("omw", EvalJs(shell(), "editor.textContent"));

  // The indicator is shown once the bounds of "omw" arrive from the renderer.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return fakeSpellChecker.correctionIndicatorCount > 0; }));

  // Flip the indicator's rect, which is in the view's coordinates, so that its
  // origin is at the upper-left corner like the page's.
  const NSRect indicator_rect = fakeSpellChecker.correctionIndicatorRect;
  const double indicator_top =
      NSHeight(rwhv_cocoa.frame) - NSMaxY(indicator_rect);

  const EvalJsResult text_bounds_result = EvalJs(shell(), R"(
      (() => {
        const range = document.createRange();
        range.setStart(editor.firstChild, 0);
        range.setEnd(editor.firstChild, 3);
        const bounds = range.getBoundingClientRect();
        return [bounds.left, bounds.top, bounds.width, bounds.height];
      })())");
  const base::ListValue& text_bounds = text_bounds_result.ExtractList();
  ASSERT_EQ(4u, text_bounds.size());
  const double text_left = text_bounds[0].GetDouble();
  const double text_top = text_bounds[1].GetDouble();
  const double text_width = text_bounds[2].GetDouble();
  const double text_height = text_bounds[3].GetDouble();

  // The indicator's bounds are rounded out to whole DIPs.
  constexpr double kTolerance = 2;
  EXPECT_NEAR(NSMinX(indicator_rect), text_left, kTolerance);
  EXPECT_NEAR(NSWidth(indicator_rect), text_width, kTolerance);
  // The indicator and the text overlap vertically.
  EXPECT_LT(indicator_top, text_top + text_height);
  EXPECT_GT(indicator_top + NSHeight(indicator_rect), text_top);
}

// Tests that a text replacement is accepted if the text changes before the
// bounds of the text to be replaced arrive from the renderer, when the
// correction indicator hasn't been shown yet.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       TextReplacementAcceptedWhenTextChangesBeforeRect) {
  GURL url("data:text/html,<textarea id=editor autofocus></textarea>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  // Set up fake spell checker to provide text replacement ("omw" -> "On my
  // way").
  FakeSpellChecker* fakeSpellChecker = [[FakeSpellChecker alloc] init];
  rwhv_cocoa.spellCheckerForTesting =
      static_cast<NSSpellChecker*>(fakeSpellChecker);

  EXPECT_TRUE(ExecJs(shell(), "editor.focus()"));
  FocusPrimaryMainFrame(shell());

  // Ensure the input router is active to handle key events.
  static_cast<RenderWidgetHostImpl*>(rwhv->GetRenderWidgetHost())
      ->input_router()
      ->MakeActive();

  // Hold the requests for the bounds of the text to be replaced.
  ScopedHeldFirstRectRequests held_requests;

  TypeTextAndWaitForChecks(rwhv_cocoa, fakeSpellChecker, @"omw");
  ASSERT_FALSE(held_requests.requests().empty());
  EXPECT_EQ(0u, fakeSpellChecker.correctionIndicatorCount);

  // Type a space before the bounds of "omw" arrive.
  [rwhv_cocoa keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                           ' ', ' ', NSEventTypeKeyDown, 0)];
  [rwhv_cocoa keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                           ' ', ' ', NSEventTypeKeyUp, 0)];

  // The replacement is applied even though the indicator was never shown.
  EXPECT_EQ("On my way ", EvalJs(shell(), R"(
      new Promise(resolve => {
        const check = () => {
          if (editor.value === 'On my way ') {
            resolve(editor.value);
          } else {
            setTimeout(check, 10);
          }
        };
        check();
      }))"));
  EXPECT_EQ(0u, fakeSpellChecker.correctionIndicatorCount);

  // Bounds that arrive after the text changed don't show the indicator.
  held_requests.RespondToAll(gfx::Rect(10, 10, 30, 20));
  EXPECT_EQ(0u, fakeSpellChecker.correctionIndicatorCount);
}

// Verifies that a text replacement is accepted if a newline is typed before the
// bounds of the text to be replaced arrive from the renderer.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       TextReplacementAcceptedWhenNewlineTypedBeforeRect) {
  GURL url("data:text/html,<textarea id=editor autofocus></textarea>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  // Set up fake spell checker to provide text replacement ("omw" -> "On my
  // way").
  FakeSpellChecker* fakeSpellChecker = [[FakeSpellChecker alloc] init];
  rwhv_cocoa.spellCheckerForTesting =
      static_cast<NSSpellChecker*>(fakeSpellChecker);

  EXPECT_TRUE(ExecJs(shell(), "editor.focus()"));
  FocusPrimaryMainFrame(shell());

  // Ensure the input router is active to handle key events.
  static_cast<RenderWidgetHostImpl*>(rwhv->GetRenderWidgetHost())
      ->input_router()
      ->MakeActive();

  // Hold the requests for the bounds of the text to be replaced.
  ScopedHeldFirstRectRequests held_requests;

  TypeTextAndWaitForChecks(rwhv_cocoa, fakeSpellChecker, @"omw");
  ASSERT_FALSE(held_requests.requests().empty());

  // Press Return before the bounds of "omw" arrive.
  [rwhv_cocoa keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                           kVK_Return, '\r', NSEventTypeKeyDown, 0)];
  [rwhv_cocoa keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                           kVK_Return, '\r', NSEventTypeKeyUp, 0)];

  EXPECT_EQ("On my way\n", EvalJs(shell(), R"(
      new Promise(resolve => {
        const check = () => {
          if (editor.value === 'On my way\n') {
            resolve(editor.value);
          } else {
            setTimeout(check, 10);
          }
        };
        check();
      }))"));

  held_requests.RespondToAll(gfx::Rect(10, 10, 30, 20));
  EXPECT_EQ(0u, fakeSpellChecker.correctionIndicatorCount);
}

// Verifies that a text replacement isn't accepted if only the selection
// changes before the bounds of the text to be replaced arrive from the
// renderer.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       TextReplacementNotAcceptedWhenCaretMovesBeforeRect) {
  GURL url("data:text/html,<textarea id=editor autofocus></textarea>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  // Set up fake spell checker to provide text replacement ("omw" -> "On my
  // way").
  FakeSpellChecker* fakeSpellChecker = [[FakeSpellChecker alloc] init];
  rwhv_cocoa.spellCheckerForTesting =
      static_cast<NSSpellChecker*>(fakeSpellChecker);

  EXPECT_TRUE(ExecJs(shell(), "editor.focus()"));
  FocusPrimaryMainFrame(shell());

  // Ensure the input router is active to handle key events.
  static_cast<RenderWidgetHostImpl*>(rwhv->GetRenderWidgetHost())
      ->input_router()
      ->MakeActive();

  // Hold the requests for the bounds of the text to be replaced.
  ScopedHeldFirstRectRequests held_requests;

  TypeTextAndWaitForChecks(rwhv_cocoa, fakeSpellChecker, @"omw");
  ASSERT_FALSE(held_requests.requests().empty());

  // Move the caret left before the bounds of "omw" arrive.
  TextSelectionWaiter selection_waiter(rwhv_mac);
  constexpr NSUInteger kArrowKeyModifiers =
      NSEventModifierFlagFunction | NSEventModifierFlagNumericPad;
  [rwhv_cocoa keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                           kVK_LeftArrow, NSLeftArrowFunctionKey,
                           NSEventTypeKeyDown, kArrowKeyModifiers)];
  [rwhv_cocoa keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                           kVK_LeftArrow, NSLeftArrowFunctionKey,
                           NSEventTypeKeyUp, kArrowKeyModifiers)];
  selection_waiter.Wait();

  // Type at the caret, so that the text reflects any replacement made before.
  TypeTextAndWaitForChecks(rwhv_cocoa, fakeSpellChecker, @"x");
  EXPECT_EQ("omxw", EvalJs(shell(), "editor.value"));

  held_requests.RespondToAll(gfx::Rect(10, 10, 30, 20));
  EXPECT_EQ(0u, fakeSpellChecker.correctionIndicatorCount);
}

// Verifies that a text replacement isn't accepted if its text is edited before
// its bounds arrive from the renderer, when other text follows it.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       TextReplacementNotAcceptedWhenTextDeletedBeforeRect) {
  GURL url(
      "data:text/html,<textarea id=editor autofocus>hello  world</textarea>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  // Set up fake spell checker to provide text replacement ("omw" -> "On my
  // way").
  FakeSpellChecker* fakeSpellChecker = [[FakeSpellChecker alloc] init];
  rwhv_cocoa.spellCheckerForTesting =
      static_cast<NSSpellChecker*>(fakeSpellChecker);

  EXPECT_TRUE(ExecJs(shell(), "editor.focus()"));
  FocusPrimaryMainFrame(shell());

  // Ensure the input router is active to handle key events.
  static_cast<RenderWidgetHostImpl*>(rwhv->GetRenderWidgetHost())
      ->input_router()
      ->MakeActive();

  // Place the caret between the two spaces.
  EXPECT_TRUE(ExecJs(shell(), "editor.setSelectionRange(6, 6);"));

  // Hold the requests for the bounds of the text to be replaced.
  ScopedHeldFirstRectRequests held_requests;

  TypeTextAndWaitForChecks(rwhv_cocoa, fakeSpellChecker, @"omw");
  ASSERT_EQ("hello omw world", EvalJs(shell(), "editor.value"));
  ASSERT_FALSE(held_requests.requests().empty());

  // Delete the "w" before the bounds of "omw" arrive.
  TextSelectionWaiter selection_waiter(rwhv_mac);
  [rwhv_cocoa keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                           kVK_Delete, '\b', NSEventTypeKeyDown, 0)];
  [rwhv_cocoa keyEvent:cocoa_test_event_utils::KeyEventWithKeyCode(
                           kVK_Delete, '\b', NSEventTypeKeyUp, 0)];
  selection_waiter.Wait();

  // Type at the caret, so that the text reflects any replacement made before.
  TypeTextAndWaitForChecks(rwhv_cocoa, fakeSpellChecker, @"x");
  EXPECT_EQ("hello omx world", EvalJs(shell(), "editor.value"));

  held_requests.RespondToAll(gfx::Rect(10, 10, 30, 20));
  EXPECT_EQ(0u, fakeSpellChecker.correctionIndicatorCount);
}

// Verifies that a text replacement isn't accepted when Backspace deletes part
// of the text to be replaced while its indicator is shown.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       TextReplacementNotAcceptedOnBackspace) {
  GURL url("data:text/html,<textarea id=editor autofocus></textarea>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  // Set up fake spell checker to provide text replacement ("omw" -> "On my
  // way").
  FakeSpellChecker* fakeSpellChecker = [[FakeSpellChecker alloc] init];
  rwhv_cocoa.spellCheckerForTesting =
      static_cast<NSSpellChecker*>(fakeSpellChecker);

  EXPECT_TRUE(ExecJs(shell(), "editor.focus()"));
  FocusPrimaryMainFrame(shell());

  // Ensure the input router is active to handle key events.
  static_cast<RenderWidgetHostImpl*>(rwhv->GetRenderWidgetHost())
      ->input_router()
      ->MakeActive();

  // Place the caret before existing text, so that "omw" is followed by text.
  EXPECT_TRUE(ExecJs(
      shell(), "editor.value = ' dark'; editor.setSelectionRange(0, 0);"));
  TypeTextAndWaitForChecks(rwhv_cocoa, fakeSpellChecker, @"omw");
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return fakeSpellChecker.correctionIndicatorCount > 0; }));

  TextSelectionWaiter selection_waiter(rwhv_mac);
  PressKey(rwhv_cocoa, kVK_Delete, '\b', 0);
  selection_waiter.Wait();

  // Wait for a replacement, if any, to reach the page.
  RunUntilInputProcessed(rwhv->GetRenderWidgetHost());
  EXPECT_EQ("om dark", EvalJs(shell(), "editor.value").ExtractString());
}

// Verifies that a text replacement isn't accepted when Forward Delete deletes
// the text after it while its indicator is shown.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       TextReplacementNotAcceptedOnForwardDelete) {
  GURL url("data:text/html,<textarea id=editor autofocus></textarea>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  // Set up fake spell checker to provide text replacement ("omw" -> "On my
  // way").
  FakeSpellChecker* fakeSpellChecker = [[FakeSpellChecker alloc] init];
  rwhv_cocoa.spellCheckerForTesting =
      static_cast<NSSpellChecker*>(fakeSpellChecker);

  EXPECT_TRUE(ExecJs(shell(), "editor.focus()"));
  FocusPrimaryMainFrame(shell());

  // Ensure the input router is active to handle key events.
  static_cast<RenderWidgetHostImpl*>(rwhv->GetRenderWidgetHost())
      ->input_router()
      ->MakeActive();

  // Place the caret before two spaces, so that "omw" is still followed by a
  // space after one of them is deleted.
  EXPECT_TRUE(ExecJs(
      shell(), "editor.value = '  dark'; editor.setSelectionRange(0, 0);"));
  TypeTextAndWaitForChecks(rwhv_cocoa, fakeSpellChecker, @"omw");
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return fakeSpellChecker.correctionIndicatorCount > 0; }));

  TextSelectionWaiter selection_waiter(rwhv_mac);
  PressKey(rwhv_cocoa, kVK_ForwardDelete, NSDeleteFunctionKey,
           NSEventModifierFlagFunction);
  selection_waiter.Wait();

  // Wait for a replacement, if any, to reach the page.
  RunUntilInputProcessed(rwhv->GetRenderWidgetHost());
  EXPECT_EQ("omw dark", EvalJs(shell(), "editor.value").ExtractString());
}

// Verifies that a text replacement is accepted when a space is typed after it
// while its indicator is shown.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       TextReplacementAcceptedOnSpace) {
  GURL url("data:text/html,<textarea id=editor autofocus></textarea>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  // Set up fake spell checker to provide text replacement ("omw" -> "On my
  // way").
  FakeSpellChecker* fakeSpellChecker = [[FakeSpellChecker alloc] init];
  rwhv_cocoa.spellCheckerForTesting =
      static_cast<NSSpellChecker*>(fakeSpellChecker);

  EXPECT_TRUE(ExecJs(shell(), "editor.focus()"));
  FocusPrimaryMainFrame(shell());

  // Ensure the input router is active to handle key events.
  static_cast<RenderWidgetHostImpl*>(rwhv->GetRenderWidgetHost())
      ->input_router()
      ->MakeActive();

  TypeTextAndWaitForChecks(rwhv_cocoa, fakeSpellChecker, @"omw");
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return fakeSpellChecker.correctionIndicatorCount > 0; }));

  TextSelectionWaiter selection_waiter(rwhv_mac);
  PressKey(rwhv_cocoa, kVK_Space, ' ', 0);
  selection_waiter.Wait();

  // Wait for the replacement to reach the page.
  RunUntilInputProcessed(rwhv->GetRenderWidgetHost());
  EXPECT_EQ("On my way ", EvalJs(shell(), "editor.value").ExtractString());
}

// Tests that selection is synced between browser and renderer when
// `EditContext::updateSelection` is called from a selectionchange handler. The
// browser's cached selectedRange must reflect the current selection for IME
// queries.
IN_PROC_BROWSER_TEST_F(RenderWidgetHostViewMacTest,
                       EditContextSelectionSyncOnUpdateSelection) {
  if (!base::FeatureList::IsEnabled(features::kEditContextSelectionSync)) {
    GTEST_SKIP();
  }
  GURL url(
      "data:text/html,"
      "<div id=editor contenteditable style='font-size:20px'>hello world</div>"
      "<script>"
      "const editor = document.getElementById('editor');"
      "const ec = new EditContext({text: 'hello world'});"
      "editor.editContext = ec;"
      "document.addEventListener('selectionchange', () => {"
      "  const sel = document.getSelection();"
      "  if (sel.rangeCount > 0 && editor.contains(sel.anchorNode)) {"
      "    ec.updateSelection(sel.anchorOffset, sel.focusOffset);"
      "  }"
      "});"
      "</script>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  RenderWidgetHostView* rwhv =
      shell()->web_contents()->GetPrimaryMainFrame()->GetView();
  RenderWidgetHostViewMac* rwhv_mac =
      static_cast<RenderWidgetHostViewMac*>(rwhv);
  RenderWidgetHostViewCocoa* rwhv_cocoa = rwhv_mac->GetInProcessNSView();

  EXPECT_TRUE(ExecJs(shell(), "editor.focus()"));

  TextSelectionWaiter selection_waiter(rwhv_mac);

  // Select "hello" in the renderer to trigger selection sync to browser.
  EXPECT_TRUE(ExecJs(shell(), "const range = document.createRange();"
                              "range.setStart(editor.firstChild, 0);"
                              "range.setEnd(editor.firstChild, 5);"
                              "const sel = document.getSelection();"
                              "sel.removeAllRanges();"
                              "sel.addRange(range);"));

  // Wait for selection sync between browser and renderer process.
  selection_waiter.Wait();

  // Verify the browser-side cached selection matches the renderer's selection.
  NSRange selected_range = [rwhv_cocoa selectedRange];
  EXPECT_EQ(0lu, selected_range.location);
  EXPECT_EQ(5lu, selected_range.length);
}

}  // namespace content
