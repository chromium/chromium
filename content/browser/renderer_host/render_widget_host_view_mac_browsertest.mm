// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_mac.h"

#import <Carbon/Carbon.h>

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#import "base/mac/mac_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
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
- (void)waitForCheckString;
- (void)reset;
@end

@implementation FakeSpellChecker {
  std::unique_ptr<base::RunLoop> _run_loop;
}

@synthesize correctionCompletionHandler = _correctionCompletionHandler;
@synthesize acceptedString = _acceptedString;

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
  // set.
  EXPECT_TRUE(fakeSpellChecker.correctionCompletionHandler != nil)
      << "showCorrectionIndicatorOfType should be called";

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
  // set.
  EXPECT_TRUE(fakeSpellChecker.correctionCompletionHandler != nil)
      << "showCorrectionIndicatorOfType should be called";

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

}  // namespace content
