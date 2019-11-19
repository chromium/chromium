// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"

#include <limits>
#include <utility>

#include "base/debug/crash_logging.h"
#import "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/numerics/ranges.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#import "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#import "content/browser/cocoa/system_hotkey_helper_mac.h"
#import "content/browser/cocoa/system_hotkey_map.h"
#include "content/browser/renderer_host/input/web_input_event_builders_mac.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#import "content/browser/renderer_host/render_widget_host_view_mac_editcommand_helper.h"
#import "content/public/browser/render_widget_host_view_mac_delegate.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#import "ui/base/cocoa/appkit_utils.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#include "ui/base/cocoa/remote_accessibility_api.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/mac/coordinate_conversion.h"

using content::EditCommand;
using content::InputEvent;
using content::NativeWebKeyboardEvent;
using content::RenderWidgetHostViewMacEditCommandHelper;
using content::WebGestureEventBuilder;
using content::WebMouseEventBuilder;
using content::WebMouseWheelEventBuilder;
using content::WebTouchEventBuilder;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebGestureEvent;
using blink::WebTouchEvent;
using remote_cocoa::mojom::RenderWidgetHostNSViewHost;
using remote_cocoa::RenderWidgetHostNSViewHostHelper;

namespace {

constexpr NSString* WebAutomaticQuoteSubstitutionEnabled =
    @"WebAutomaticQuoteSubstitutionEnabled";
constexpr NSString* const WebAutomaticDashSubstitutionEnabled =
    @"WebAutomaticDashSubstitutionEnabled";
constexpr NSString* const WebAutomaticTextReplacementEnabled =
    @"WebAutomaticTextReplacementEnabled";

// A dummy RenderWidgetHostNSViewHostHelper implementation which no-ops all
// functions.
class DummyHostHelper : public RenderWidgetHostNSViewHostHelper {
 public:
  explicit DummyHostHelper() {}

 private:
  // RenderWidgetHostNSViewHostHelper implementation.
  id GetRootBrowserAccessibilityElement() override { return nil; }
  id GetFocusedBrowserAccessibilityElement() override { return nil; }
  void SetAccessibilityWindow(NSWindow* window) override {}
  void ForwardKeyboardEvent(const NativeWebKeyboardEvent& key_event,
                            const ui::LatencyInfo& latency_info) override {}
  void ForwardKeyboardEventWithCommands(
      const NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info,
      const std::vector<EditCommand>& commands) override {}
  void RouteOrProcessMouseEvent(
      const blink::WebMouseEvent& web_event) override {}
  void RouteOrProcessTouchEvent(
      const blink::WebTouchEvent& web_event) override {}
  void RouteOrProcessWheelEvent(
      const blink::WebMouseWheelEvent& web_event) override {}
  void ForwardMouseEvent(const blink::WebMouseEvent& web_event) override {}
  void ForwardWheelEvent(const blink::WebMouseWheelEvent& web_event) override {}
  void GestureBegin(blink::WebGestureEvent begin_event,
                    bool is_synthetically_injected) override {}
  void GestureUpdate(blink::WebGestureEvent update_event) override {}
  void GestureEnd(blink::WebGestureEvent end_event) override {}
  void SmartMagnify(const blink::WebGestureEvent& web_event) override {}

  DISALLOW_COPY_AND_ASSIGN(DummyHostHelper);
};

// Touch bar identifier.
NSString* const kWebContentTouchBarId = @"web-content";

constexpr int wrap_around_distance = 10000;

// Whether a keyboard event has been reserved by OSX.
BOOL EventIsReservedBySystem(NSEvent* event) {
  return content::GetSystemHotkeyMap()->IsEventReserved(event);
}

// TODO(suzhe): Upstream this function.
SkColor SkColorFromNSColor(NSColor* color) {
  CGFloat r, g, b, a;
  [color getRed:&r green:&g blue:&b alpha:&a];

  return base::ClampToRange(static_cast<int>(lroundf(255.0f * a)), 0, 255)
             << 24 |
         base::ClampToRange(static_cast<int>(lroundf(255.0f * r)), 0, 255)
             << 16 |
         base::ClampToRange(static_cast<int>(lroundf(255.0f * g)), 0, 255)
             << 8 |
         base::ClampToRange(static_cast<int>(lroundf(255.0f * b)), 0, 255);
}

// Extract underline information from an attributed string. Mostly copied from
// third_party/WebKit/Source/WebKit/mac/WebView/WebHTMLView.mm
void ExtractUnderlines(NSAttributedString* string,
                       std::vector<ui::ImeTextSpan>* ime_text_spans) {
  int length = [[string string] length];
  int i = 0;
  while (i < length) {
    NSRange range;
    NSDictionary* attrs = [string attributesAtIndex:i
                              longestEffectiveRange:&range
                                            inRange:NSMakeRange(i, length - i)];
    if (NSNumber* style = [attrs objectForKey:NSUnderlineStyleAttributeName]) {
      SkColor color = SK_ColorBLACK;
      if (NSColor* colorAttr =
              [attrs objectForKey:NSUnderlineColorAttributeName]) {
        color = SkColorFromNSColor(
            [colorAttr colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
      }
      ui::ImeTextSpan::Thickness thickness =
          [style intValue] > 1 ? ui::ImeTextSpan::Thickness::kThick
                               : ui::ImeTextSpan::Thickness::kThin;
      ui::ImeTextSpan ui_ime_text_span =
          ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, range.location,
                          NSMaxRange(range), thickness, SK_ColorTRANSPARENT);
      ui_ime_text_span.underline_color = color;
      ime_text_spans->push_back(ui_ime_text_span);
    }
    i = range.location + range.length;
  }
}

}  // namespace

// These are not documented, so use only after checking -respondsToSelector:.
@interface NSApplication (UndocumentedSpeechMethods)
- (void)speakString:(NSString*)string;
- (void)stopSpeaking:(id)sender;
- (BOOL)isSpeaking;
@end

// RenderWidgetHostViewCocoa ---------------------------------------------------

// Private methods:
@interface RenderWidgetHostViewCocoa () {
  bool keyboardLockActive_;
  base::Optional<base::flat_set<ui::DomCode>> lockedKeys_;

  API_AVAILABLE(macos(10.12.2))
  base::scoped_nsobject<NSCandidateListTouchBarItem> candidateListTouchBarItem_;
  NSInteger textSuggestionsSequenceNumber_;
  BOOL shouldRequestTextSubstitutions_;
  BOOL substitutionWasApplied_;
}
@property(readonly) NSSpellChecker* spellChecker;
@property(getter=isAutomaticTextReplacementEnabled)
    BOOL automaticTextReplacementEnabled;
@property(getter=isAutomaticQuoteSubstitutionEnabled)
    BOOL automaticQuoteSubstitutionEnabled;
@property(getter=isAutomaticDashSubstitutionEnabled)
    BOOL automaticDashSubstitutionEnabled;

- (void)processedWheelEvent:(const blink::WebMouseWheelEvent&)event
                   consumed:(BOOL)consumed;
- (void)keyEvent:(NSEvent*)theEvent wasKeyEquivalent:(BOOL)equiv;
- (void)windowDidChangeBackingProperties:(NSNotification*)notification;
- (void)windowChangedGlobalFrame:(NSNotification*)notification;
- (void)windowDidBecomeKey:(NSNotification*)notification;
- (void)windowDidResignKey:(NSNotification*)notification;
- (void)sendViewBoundsInWindowToHost;
- (void)requestTextSubstitutions;
- (void)requestTextSuggestions API_AVAILABLE(macos(10.12.2));
- (void)sendWindowFrameInScreenToHost;
- (bool)hostIsDisconnected;
- (void)invalidateTouchBar API_AVAILABLE(macos(10.12.2));

// NSCandidateListTouchBarItemDelegate implementation
- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem*)anItem
     endSelectingCandidateAtIndex:(NSInteger)index
    API_AVAILABLE(macos(10.12.2));
- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem*)anItem
    changedCandidateListVisibility:(BOOL)isVisible
    API_AVAILABLE(macos(10.12.2));
@end

@implementation RenderWidgetHostViewCocoa
@synthesize markedRange = markedRange_;
@synthesize textInputType = textInputType_;
@synthesize textInputFlags = textInputFlags_;
@synthesize spellCheckerForTesting = spellCheckerForTesting_;

- (id)initWithHost:(RenderWidgetHostNSViewHost*)host
    withHostHelper:(RenderWidgetHostNSViewHostHelper*)hostHelper {
  self = [super initWithFrame:NSZeroRect];
  if (self) {
    self.acceptsTouchEvents = YES;
    editCommandHelper_.reset(new RenderWidgetHostViewMacEditCommandHelper);
    editCommandHelper_->AddEditingSelectorsToClass([self class]);

    host_ = host;
    hostHelper_ = hostHelper;
    canBeKeyView_ = YES;
    isStylusEnteringProximity_ = false;
    keyboardLockActive_ = false;
    textInputType_ = ui::TEXT_INPUT_TYPE_NONE;
    direct_manipulation_enabled_ =
        base::FeatureList::IsEnabled(features::kDirectManipulationStylus);
    has_pen_contact_ = false;
  }
  return self;
}

- (void)dealloc {
  DCHECK([self hostIsDisconnected]);
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  // Update and cache the new input context. Otherwise,
  // [NSTextInputContext currentInputContext] might still hold on to this
  // view's NSTextInputContext even after it's deallocated.
  // See http://crbug.com/684388.
  [[self window] makeFirstResponder:nil];
  [NSApp updateWindows];

  [super dealloc];
}

- (void)sendViewBoundsInWindowToHost {
  TRACE_EVENT0("browser",
               "RenderWidgetHostViewCocoa::sendViewBoundsInWindowToHost");
  if (inSetFrame_)
    return;

  NSRect viewBoundsInView = [self bounds];
  NSWindow* enclosingWindow = [self window];
  if (!enclosingWindow) {
    host_->OnBoundsInWindowChanged(gfx::Rect(viewBoundsInView), false);
    return;
  }

  NSRect viewBoundsInWindow = [self convertRect:viewBoundsInView toView:nil];
  gfx::Rect gfxViewBoundsInWindow(viewBoundsInWindow);
  gfxViewBoundsInWindow.set_y(NSHeight([enclosingWindow frame]) -
                              NSMaxY(viewBoundsInWindow));
  host_->OnBoundsInWindowChanged(gfxViewBoundsInWindow, true);
}

- (NSSpellChecker*)spellChecker {
  if (spellCheckerForTesting_)
    return spellCheckerForTesting_;
  return NSSpellChecker.sharedSpellChecker;
}

- (void)requestTextSubstitutions {
  NSTextCheckingType textCheckingTypes =
      self.allowedTextCheckingTypes & self.enabledTextCheckingTypes;
  if (!textCheckingTypes)
    return;

  NSString* availableText = base::SysUTF16ToNSString(textSelectionText_);

  if (!availableText)
    return;

  auto* textCheckingResults =
      [self.spellChecker checkString:availableText
                               range:NSMakeRange(0, availableText.length)
                               types:textCheckingTypes
                             options:nil
              inSpellDocumentWithTag:0
                         orthography:nullptr
                           wordCount:nullptr];

  NSUInteger cursorLocation = textSelectionRange_.start();
  base::scoped_nsobject<NSTextCheckingResult> scopedCandidateResult;
  for (NSTextCheckingResult* result in textCheckingResults) {
    NSTextCheckingResult* adjustedResult =
        [result resultByAdjustingRangesWithOffset:textSelectionOffset_];
    if (!NSLocationInRange(cursorLocation,
                           NSMakeRange(adjustedResult.range.location,
                                       adjustedResult.range.length + 1)))
      continue;
    constexpr NSTextCheckingType textCheckingTypesToReplaceImmediately =
        NSTextCheckingTypeQuote | NSTextCheckingTypeDash;
    if (adjustedResult.resultType & textCheckingTypesToReplaceImmediately) {
      [self insertText:adjustedResult.replacementString
          replacementRange:adjustedResult.range];
      continue;
    }
    scopedCandidateResult.reset([adjustedResult retain]);
  }
  NSTextCheckingResult* candidateResult = scopedCandidateResult.get();
  if (!candidateResult)
    return;

  NSRect textRectInScreenCoordinates =
      [self firstRectForCharacterRange:candidateResult.range
                           actualRange:nullptr];
  NSRect textRectInWindowCoordinates =
      [self.window convertRectFromScreen:textRectInScreenCoordinates];
  NSRect textRectInViewCoordinates =
      [self convertRect:textRectInWindowCoordinates fromView:nil];

  [self.spellChecker
      showCorrectionIndicatorOfType:NSCorrectionIndicatorTypeDefault
                      primaryString:candidateResult.replacementString
                 alternativeStrings:candidateResult.alternativeStrings
                    forStringInRect:textRectInViewCoordinates
                               view:self
                  completionHandler:^(NSString* acceptedString) {
                    [self didAcceptReplacementString:acceptedString
                               forTextCheckingResult:candidateResult];
                  }];
}

- (void)didAcceptReplacementString:(NSString*)acceptedString
             forTextCheckingResult:(NSTextCheckingResult*)correction {
  // TODO: Keep NSSpellChecker up to date on the user's response via
  // -recordResponse:toCorrection:forWord:language:inSpellDocumentWithTag:.
  // Call it to report whether they initially accepted or rejected the
  // suggestion, but also if they edit, revert, etc. later.

  if (acceptedString == nil)
    return;

  NSRange availableTextRange =
      NSMakeRange(textSelectionOffset_, textSelectionText_.length());

  if (NSMaxRange(correction.range) > NSMaxRange(availableTextRange))
    return;

  NSAttributedString* attString = [[[NSAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(textSelectionText_)] autorelease];
  NSRange trailingRange = NSMakeRange(
      NSMaxRange(correction.range),
      NSMaxRange(availableTextRange) - NSMaxRange(correction.range));

  if (trailingRange.length > 0 &&
      trailingRange.location < NSMaxRange(availableTextRange)) {
    NSRange trailingRangeInAvailableText = NSMakeRange(
        trailingRange.location - textSelectionOffset_, trailingRange.length);
    if (@available(macOS 10.12, *)) {
      NSString* trailingString =
          [attString.string substringWithRange:trailingRangeInAvailableText];
      if ([self.spellChecker preventsAutocorrectionBeforeString:trailingString
                                                       language:nil])
        return;
    }
    if ([attString doubleClickAtIndex:trailingRangeInAvailableText.location]
            .location < trailingRangeInAvailableText.location)
      return;
  }

  substitutionWasApplied_ = YES;
  [self insertText:acceptedString replacementRange:correction.range];
}

- (void)requestTextSuggestions {
  auto* touchBarItem = candidateListTouchBarItem_.get();
  if (!touchBarItem)
    return;
  [touchBarItem
      updateWithInsertionPointVisibility:textSelectionRange_.is_empty()];
  if (textInputType_ == ui::TEXT_INPUT_TYPE_PASSWORD)
    return;
  if (!touchBarItem.candidateListVisible)
    return;
  if (!textSelectionRange_.IsValid() ||
      textSelectionOffset_ > textSelectionRange_.GetMin())
    return;

  NSRange selectionRange = textSelectionRange_.ToNSRange();
  NSString* selectionText = base::SysUTF16ToNSString(textSelectionText_);
  selectionRange.location -= textSelectionOffset_;
  if (NSMaxRange(selectionRange) > selectionText.length)
    return;

  // TODO: Fetch the spell document tag from the renderer (or equivalent).
  textSuggestionsSequenceNumber_ = [self.spellChecker
      requestCandidatesForSelectedRange:selectionRange
                               inString:selectionText
                                  types:NSTextCheckingAllSystemTypes
                                options:nil
                 inSpellDocumentWithTag:0
                      completionHandler:^(
                          NSInteger sequenceNumber,
                          NSArray<NSTextCheckingResult*>* candidates) {
                        dispatch_async(dispatch_get_main_queue(), ^{
                          if (sequenceNumber != textSuggestionsSequenceNumber_)
                            return;
                          [touchBarItem setCandidates:candidates
                                     forSelectedRange:selectionRange
                                             inString:selectionText];
                        });
                      }];
}

- (NSTextCheckingType)allowedTextCheckingTypes {
  if (textInputType_ == ui::TEXT_INPUT_TYPE_NONE)
    return 0;
  if (textInputType_ == ui::TEXT_INPUT_TYPE_PASSWORD)
    return 0;
  if (textInputFlags_ & blink::kWebTextInputFlagAutocorrectOff)
    return 0;
  NSTextCheckingType checkingTypes = NSTextCheckingTypeReplacement;
  if (!(textInputFlags_ & blink::kWebTextInputFlagSpellcheckOff))
    checkingTypes |= NSTextCheckingTypeQuote | NSTextCheckingTypeDash;
  return checkingTypes;
}

- (NSTextCheckingType)enabledTextCheckingTypes {
  NSTextCheckingType checkingTypes = 0;
  if (self.automaticQuoteSubstitutionEnabled)
    checkingTypes |= NSTextCheckingTypeQuote;
  if (self.automaticDashSubstitutionEnabled)
    checkingTypes |= NSTextCheckingTypeDash;
  if (self.automaticTextReplacementEnabled)
    checkingTypes |= NSTextCheckingTypeReplacement;
  return checkingTypes;
}

- (void)orderFrontSubstitutionsPanel:(id)sender {
  [NSSpellChecker.sharedSpellChecker.substitutionsPanel orderFront:sender];
}

- (void)setTextSelectionText:(base::string16)text
                      offset:(size_t)offset
                       range:(gfx::Range)range {
  textSelectionText_ = text;
  textSelectionOffset_ = offset;
  textSelectionRange_ = range;
  substitutionWasApplied_ = NO;
  [NSSpellChecker.sharedSpellChecker dismissCorrectionIndicatorForView:self];
  if (shouldRequestTextSubstitutions_ && !substitutionWasApplied_ &&
      textSelectionRange_.is_empty()) {
    shouldRequestTextSubstitutions_ = NO;
    [self requestTextSubstitutions];
  }
  if (@available(macOS 10.12.2, *))
    [self requestTextSuggestions];
}

- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem*)anItem
     endSelectingCandidateAtIndex:(NSInteger)index {
  if (index == NSNotFound)
    return;
  NSTextCheckingResult* selectedResult = anItem.candidates[index];
  NSRange replacementRange = selectedResult.range;
  replacementRange.location += textSelectionOffset_;
  [self insertText:selectedResult.replacementString
      replacementRange:replacementRange];

  ui::LogTouchBarUMA(ui::TouchBarAction::TEXT_SUGGESTION);
}

- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem*)anItem
    changedCandidateListVisibility:(BOOL)isVisible {
  [self requestTextSuggestions];
}

- (void)setTextInputType:(ui::TextInputType)textInputType {
  if (textInputType_ == textInputType)
    return;
  textInputType_ = textInputType;

  if (@available(macOS 10.12.2, *))
    [self invalidateTouchBar];
}

- (base::string16)selectedText {
  gfx::Range textRange(textSelectionOffset_,
                       textSelectionOffset_ + textSelectionText_.size());
  gfx::Range intersectionRange = textRange.Intersect(textSelectionRange_);
  if (intersectionRange.is_empty())
    return base::string16();
  return textSelectionText_.substr(
      intersectionRange.start() - textSelectionOffset_,
      intersectionRange.length());
}

- (void)setCompositionRange:(gfx::Range)range {
  compositionRange_ = range;
}

- (void)sendWindowFrameInScreenToHost {
  TRACE_EVENT0("browser",
               "RenderWidgetHostViewCocoa::sendWindowFrameInScreenToHost");
  NSWindow* enclosingWindow = [self window];
  if (!enclosingWindow)
    return;
  host_->OnWindowFrameInScreenChanged(
      gfx::ScreenRectFromNSRect([enclosingWindow frame]));
}

- (void)setResponderDelegate:
    (NSObject<RenderWidgetHostViewMacDelegate>*)delegate {
  DCHECK(!responderDelegate_);
  responderDelegate_.reset([delegate retain]);
}

- (void)resetCursorRects {
  if (currentCursor_) {
    [self addCursorRect:[self visibleRect] cursor:currentCursor_];
    [currentCursor_ setOnMouseEntered:YES];
  }
}

- (void)processedWheelEvent:(const blink::WebMouseWheelEvent&)event
                   consumed:(BOOL)consumed {
  [responderDelegate_ rendererHandledWheelEvent:event consumed:consumed];
}

- (void)processedGestureScrollEvent:(const blink::WebGestureEvent&)event
                           consumed:(BOOL)consumed {
  [responderDelegate_ rendererHandledGestureScrollEvent:event
                                               consumed:consumed];
}

- (void)processedOverscroll:(const ui::DidOverscrollParams&)params {
  [responderDelegate_ rendererHandledOverscrollEvent:params];
}

- (BOOL)respondsToSelector:(SEL)selector {
  // Trickiness: this doesn't mean "does this object's superclass respond to
  // this selector" but rather "does the -respondsToSelector impl from the
  // superclass say that this class responds to the selector".
  if ([super respondsToSelector:selector])
    return YES;

  if (responderDelegate_)
    return [responderDelegate_ respondsToSelector:selector];

  return NO;
}

- (id)forwardingTargetForSelector:(SEL)selector {
  if ([responderDelegate_ respondsToSelector:selector])
    return responderDelegate_.get();

  return [super forwardingTargetForSelector:selector];
}

- (void)setCanBeKeyView:(BOOL)can {
  canBeKeyView_ = can;
}

- (BOOL)acceptsMouseEventsWhenInactive {
  // Some types of windows (balloons, always-on-top panels) want to accept mouse
  // clicks w/o the first click being treated as 'activation'. Same applies to
  // mouse move events.
  return [[self window] level] > NSNormalWindowLevel;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)theEvent {
  return [self acceptsMouseEventsWhenInactive];
}

- (void)setCloseOnDeactivate:(BOOL)b {
  closeOnDeactivate_ = b;
}

- (void)setHostDisconnected {
  // Set the host to be an abandoned message pipe, and set the hostHelper
  // to forward messages to that host.
  ignore_result(dummyHost_.BindNewPipeAndPassReceiver());
  dummyHostHelper_ = std::make_unique<DummyHostHelper>();
  host_ = dummyHost_.get();
  hostHelper_ = dummyHostHelper_.get();

  // |responderDelegate_| may attempt to access the RenderWidgetHostViewMac
  // through its internal pointers, so detach it here.
  // TODO(ccameron): Force |responderDelegate_| to use the |host_| as well,
  // and the viewGone method to hostGone.
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(viewGone:)])
    [responderDelegate_ viewGone:self];
  responderDelegate_.reset();
}

- (bool)hostIsDisconnected {
  return host_ == (dummyHost_.is_bound() ? dummyHost_.get() : nullptr);
}

- (void)setShowingContextMenu:(BOOL)showing {
  showingContextMenu_ = showing;

  // Create a fake mouse event to inform the render widget that the mouse
  // left or entered.
  NSWindow* window = [self window];
  int window_number = window ? [window windowNumber] : -1;

  // TODO(asvitkine): If the location outside of the event stream doesn't
  // correspond to the current event (due to delayed event processing), then
  // this may result in a cursor flicker if there are later mouse move events
  // in the pipeline. Find a way to use the mouse location from the event that
  // dismissed the context menu.
  NSPoint location = [window mouseLocationOutsideOfEventStream];
  NSTimeInterval event_time = [[NSApp currentEvent] timestamp];
  NSEvent* event = [NSEvent mouseEventWithType:NSMouseMoved
                                      location:location
                                 modifierFlags:0
                                     timestamp:event_time
                                  windowNumber:window_number
                                       context:nil
                                   eventNumber:0
                                    clickCount:0
                                      pressure:0];
  WebMouseEvent web_event = WebMouseEventBuilder::Build(event, self);
  web_event.SetModifiers(web_event.GetModifiers() |
                         WebInputEvent::kRelativeMotionEvent);
  hostHelper_->ForwardMouseEvent(web_event);
}

- (BOOL)shouldIgnoreMouseEvent:(NSEvent*)theEvent {
  NSWindow* window = [self window];
  // If this is a background window, don't handle mouse movement events. This
  // is the expected behavior on the Mac as evidenced by other applications.
  if ([theEvent type] == NSMouseMoved &&
      ![self acceptsMouseEventsWhenInactive] && ![window isKeyWindow]) {
    return YES;
  }

  // Use hitTest to check whether the mouse is over a nonWebContentView - in
  // which case the mouse event should not be handled by the render host.
  const SEL nonWebContentViewSelector = @selector(nonWebContentView);
  NSView* contentView = [window contentView];
  NSView* view = [contentView hitTest:[theEvent locationInWindow]];
  // Traverse the superview hierarchy as the hitTest will return the frontmost
  // view, such as an NSTextView, while nonWebContentView may be specified by
  // its parent view.
  BOOL hitSelf = NO;
  while (view) {
    if (view == self)
      hitSelf = YES;
    if ([view respondsToSelector:nonWebContentViewSelector] &&
        [view performSelector:nonWebContentViewSelector]) {
      // The cursor is over a nonWebContentView - ignore this mouse event.
      return YES;
    }
    if ([view isKindOfClass:[self class]] && ![view isEqual:self] &&
        !hasOpenMouseDown_) {
      // The cursor is over an overlapping render widget. This check is done by
      // both views so the one that's returned by -hitTest: will end up
      // processing the event.
      // Note that while dragging, we only get events for the render view where
      // drag started, even if mouse is  actually over another view or outside
      // the window. Cocoa does this for us. We should handle these events and
      // not ignore (since there is no other render view to handle them). Thus
      // the |!hasOpenMouseDown_| check above.
      return YES;
    }
    view = [view superview];
  }
  // Ignore events which don't hit test to this subtree (and hit, for example,
  // an overlapping view instead). As discussed above, the mouse may go outside
  // the bounds of the view and keep sending events during a drag.
  return !hitSelf && !hasOpenMouseDown_;
}

- (void)mouseEvent:(NSEvent*)theEvent {
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::mouseEvent");
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [responderDelegate_ handleEvent:theEvent];
    if (handled)
      return;
  }

  // Set the pointer type when we are receiving a NSMouseEntered event and the
  // following NSMouseExited event should have the same pointer type.
  // For NSMouseExited and NSMouseEntered events, they do not have a subtype.
  // We decide their pointer types by checking if we recevied a
  // NSTabletProximity event.
  NSEventType type = [theEvent type];
  if (type == NSMouseEntered || type == NSMouseExited) {
    pointerType_ = isStylusEnteringProximity_
                       ? pointerType_
                       : blink::WebPointerProperties::PointerType::kMouse;
  } else {
    NSEventSubtype subtype = [theEvent subtype];
    // For other mouse events and touchpad events, the pointer type is mouse.
    if (subtype != NSTabletPointEventSubtype &&
        subtype != NSTabletProximityEventSubtype) {
      pointerType_ = blink::WebPointerProperties::PointerType::kMouse;
    } else if (subtype == NSTabletProximityEventSubtype) {
      isStylusEnteringProximity_ = [theEvent isEnteringProximity];
      NSPointingDeviceType deviceType = [theEvent pointingDeviceType];
      // For all tablet events, the pointer type will be pen or eraser.
      pointerType_ = deviceType == NSEraserPointingDevice
                         ? blink::WebPointerProperties::PointerType::kEraser
                         : blink::WebPointerProperties::PointerType::kPen;
    }
  }

  // Because |updateCursor:| changes the current cursor, we have to reset it to
  // the default cursor on mouse exit.
  if (type == NSMouseExited)
    [[NSCursor arrowCursor] set];

  if ([self shouldIgnoreMouseEvent:theEvent]) {
    // If this is the first such event, send a mouse exit to the host view.
    if (!mouseEventWasIgnored_) {
      WebMouseEvent exitEvent =
          WebMouseEventBuilder::Build(theEvent, self, pointerType_);
      exitEvent.SetType(WebInputEvent::kMouseLeave);
      exitEvent.button = WebMouseEvent::Button::kNoButton;
      hostHelper_->ForwardMouseEvent(exitEvent);
    }
    mouseEventWasIgnored_ = YES;
    [self updateCursor:nil];
    return;
  }

  if (mouseEventWasIgnored_) {
    // If this is the first mouse event after a previous event that was ignored
    // due to the hitTest, send a mouse enter event to the host view.
    WebMouseEvent enterEvent =
        WebMouseEventBuilder::Build(theEvent, self, pointerType_);
    enterEvent.SetType(WebInputEvent::kMouseMove);
    enterEvent.button = WebMouseEvent::Button::kNoButton;
    hostHelper_->RouteOrProcessMouseEvent(enterEvent);
  }
  mouseEventWasIgnored_ = NO;

  // Don't cancel child popups; killing them on a mouse click would prevent the
  // user from positioning the insertion point in the text field spawning the
  // popup. A click outside the text field would cause the text field to drop
  // the focus, and then EditorHostImpl::textFieldDidEndEditing() would cancel
  // the popup anyway, so we're OK.
  if (type == NSLeftMouseDown)
    hasOpenMouseDown_ = YES;
  else if (type == NSLeftMouseUp)
    hasOpenMouseDown_ = NO;

  // TODO(suzhe): We should send mouse events to the input method first if it
  // wants to handle them. But it won't work without implementing method
  // - (NSUInteger)characterIndexForPoint:.
  // See: http://code.google.com/p/chromium/issues/detail?id=47141
  // Instead of sending mouse events to the input method first, we now just
  // simply confirm all ongoing composition here.
  if (type == NSLeftMouseDown || type == NSRightMouseDown ||
      type == NSOtherMouseDown) {
    [self finishComposingText];
  }

  if (type == NSMouseMoved)
    cursorHidden_ = NO;

  bool send_touch =
      direct_manipulation_enabled_ &&
      pointerType_ == blink::WebPointerProperties::PointerType::kPen;

  // Send touch events when the pen is in contact with the tablet.
  if (send_touch) {
    // Because the NSLeftMouseUp event's buttonMask is not
    // NSEventButtonMaskPenTip, we read |has_pen_contact_| to ensure a
    // TouchRelease is sent appropriately at the end when the stylus is
    // no longer in contact with the digitizer.
    send_touch = has_pen_contact_;
    if (type == NSLeftMouseDown || type == NSLeftMouseUp ||
        type == NSLeftMouseDragged) {
      NSEventButtonMask buttonMask = [theEvent buttonMask];
      if (buttonMask == NSEventButtonMaskPenTip) {
        DCHECK(type != NSLeftMouseUp);
        send_touch = has_pen_contact_ = true;
      } else {
        has_pen_contact_ = false;
      }
    }
  }

  if (!send_touch) {
    WebMouseEvent event =
        WebMouseEventBuilder::Build(theEvent, self, pointerType_);

    if (mouse_locked_ &&
        base::FeatureList::IsEnabled(features::kConsolidatedMovementXY)) {
      // When mouse is locked, we keep increasing |last_mouse_screen_position|
      // by movement_x/y so that we can still use PositionInScreen to calculate
      // movements in blink. We need to keep |last_mouse_screen_position_| from
      // getting too large because it will lose some precision. So whenever it
      // exceed the |wrap_around_distance|, we start again from the current
      // mouse position (locked position), and also send a synthesized event to
      // update the blink-side status.
      if (std::abs(last_mouse_screen_position_.x()) > wrap_around_distance ||
          std::abs(last_mouse_screen_position_.y()) > wrap_around_distance) {
        NSWindow* window = [self window];
        NSPoint location = [window mouseLocationOutsideOfEventStream];
        int window_number = window ? [window windowNumber] : -1;
        NSEvent* nsevent = [NSEvent mouseEventWithType:NSMouseMoved
                                              location:location
                                         modifierFlags:[theEvent modifierFlags]
                                             timestamp:[theEvent timestamp]
                                          windowNumber:window_number
                                               context:nil
                                           eventNumber:0
                                            clickCount:[theEvent clickCount]
                                              pressure:0];
        WebMouseEvent wrap_around_event =
            WebMouseEventBuilder::Build(nsevent, self, pointerType_);
        last_mouse_screen_position_ = wrap_around_event.PositionInScreen();
        wrap_around_event.SetModifiers(
            event.GetModifiers() |
            blink::WebInputEvent::Modifiers::kRelativeMotionEvent);
        hostHelper_->RouteOrProcessMouseEvent(wrap_around_event);
      }
      event.SetPositionInScreen(
          last_mouse_screen_position_ +
          gfx::Vector2dF(event.movement_x, event.movement_y));
    }

    last_mouse_screen_position_ = event.PositionInScreen();
    hostHelper_->RouteOrProcessMouseEvent(event);
  } else {
    WebTouchEvent event = WebTouchEventBuilder::Build(theEvent, self);
    hostHelper_->RouteOrProcessTouchEvent(event);
  }
}

- (void)tabletEvent:(NSEvent*)theEvent {
  if ([theEvent type] == NSTabletProximity) {
    isStylusEnteringProximity_ = [theEvent isEnteringProximity];
    NSPointingDeviceType deviceType = [theEvent pointingDeviceType];
    // For all tablet events, the pointer type will be pen or eraser.
    pointerType_ = deviceType == NSEraserPointingDevice
                       ? blink::WebPointerProperties::PointerType::kEraser
                       : blink::WebPointerProperties::PointerType::kPen;
  }
}

- (void)lockKeyboard:(base::Optional<base::flat_set<ui::DomCode>>)keysToLock {
  // TODO(joedow): Integrate System-level keyboard hook into this method.
  lockedKeys_ = std::move(keysToLock);
  keyboardLockActive_ = true;
}

- (void)unlockKeyboard {
  keyboardLockActive_ = false;
  lockedKeys_.reset();
}

- (void)setCursorLocked:(BOOL)locked {
  mouse_locked_ = locked;
  if (mouse_locked_) {
    CGAssociateMouseAndMouseCursorPosition(NO);
    [NSCursor hide];
  } else {
    // Unlock position of mouse cursor and unhide it.
    CGAssociateMouseAndMouseCursorPosition(YES);
    [NSCursor unhide];
  }
}

// CommandDispatcherTarget implementation:
- (BOOL)isKeyLocked:(NSEvent*)event {
  int keyCode = [event keyCode];
  // Note: We do not want to treat the ESC key as locked as that key is used
  // to exit fullscreen and we don't want to prevent them from exiting.
  ui::DomCode domCode = ui::KeycodeConverter::NativeKeycodeToDomCode(keyCode);
  return keyboardLockActive_ && domCode != ui::DomCode::ESCAPE &&
         (!lockedKeys_ || base::Contains(lockedKeys_.value(), domCode));
}

- (BOOL)performKeyEquivalent:(NSEvent*)theEvent {
  // |performKeyEquivalent:| is sent to all views of a window, not only down the
  // responder chain (cf. "Handling Key Equivalents" in
  // http://developer.apple.com/mac/library/documentation/Cocoa/Conceptual/EventOverview/HandlingKeyEvents/HandlingKeyEvents.html
  // ). A |performKeyEquivalent:| may also bubble up from a dialog child window
  // to perform browser commands such as switching tabs. We only want to handle
  // key equivalents if we're first responder in the keyWindow.
  if (![[self window] isKeyWindow] || [[self window] firstResponder] != self)
    return NO;

  // If the event is reserved by the system, then do not pass it to web content.
  if (EventIsReservedBySystem(theEvent))
    return NO;

  // Command key combinations are sent via performKeyEquivalent rather than
  // keyDown:. We just forward this on and if WebCore doesn't want to handle
  // it, we let the WebContentsView figure out how to reinject it.
  [self keyEvent:theEvent wasKeyEquivalent:YES];
  return YES;
}

- (BOOL)_wantsKeyDownForEvent:(NSEvent*)event {
  // This is a SPI that AppKit apparently calls after |performKeyEquivalent:|
  // returned NO. If this function returns |YES|, Cocoa sends the event to
  // |keyDown:| instead of doing other things with it. Ctrl-tab will be sent
  // to us instead of doing key view loop control, ctrl-left/right get handled
  // correctly, etc.
  // (However, there are still some keys that Cocoa swallows, e.g. the key
  // equivalent that Cocoa uses for toggling the input language. In this case,
  // that's actually a good thing, though -- see http://crbug.com/26115 .)
  return YES;
}

- (EventHandled)keyEvent:(NSEvent*)theEvent {
  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [responderDelegate_ handleEvent:theEvent];
    if (handled)
      return kEventHandled;
  }

  [self keyEvent:theEvent wasKeyEquivalent:NO];
  return kEventHandled;
}

- (void)keyEvent:(NSEvent*)theEvent wasKeyEquivalent:(BOOL)equiv {
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::keyEvent");
  NSEventType eventType = [theEvent type];
  NSEventModifierFlags modifierFlags = [theEvent modifierFlags];
  int keyCode = [theEvent keyCode];

  // If the user changes the system hotkey mapping after Chrome has been
  // launched, then it is possible that a formerly reserved system hotkey is no
  // longer reserved. The hotkey would have skipped the renderer, but would
  // also have not been handled by the system. If this is the case, immediately
  // return.
  // TODO(erikchen): SystemHotkeyHelperMac should use the File System Events
  // api to monitor changes to system hotkeys. This logic will have to be
  // updated.
  // http://crbug.com/383558.
  if (EventIsReservedBySystem(theEvent))
    return;

  if (eventType == NSFlagsChanged) {
    // Ignore NSFlagsChanged events from the NumLock and Fn keys as
    // Safari does in -[WebHTMLView flagsChanged:] (of "WebHTMLView.mm").
    // Also ignore unsupported |keyCode| (255) generated by Convert, NonConvert
    // and KanaMode from JIS PC keyboard.
    if (!keyCode || keyCode == 10 || keyCode == 63 || keyCode == 255)
      return;
  }

  // Don't cancel child popups; the key events are probably what's triggering
  // the popup in the first place.


  NativeWebKeyboardEvent event(theEvent);
  ui::LatencyInfo latency_info;
  if (event.GetType() == blink::WebInputEvent::kRawKeyDown ||
      event.GetType() == blink::WebInputEvent::kChar) {
    latency_info.set_source_event_type(ui::SourceEventType::KEY_PRESS);
  }

  latency_info.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);

  // If KeyboardLock has been requested for this keyCode, then mark the event
  // so it skips the pre-handler and is delivered straight to the website.
  if ([self isKeyLocked:theEvent])
    event.skip_in_browser = true;

  // Do not forward key up events unless preceded by a matching key down,
  // otherwise we might get an event from releasing the return key in the
  // omnibox (https://crbug.com/338736) or from closing another window
  // (https://crbug.com/155492).
  if (eventType == NSKeyUp) {
    auto numErased = keyDownCodes_.erase(keyCode);
    if (numErased < 1)
      return;
  }

  // Tell the host that we are beginning a keyboard event. This ensures that
  // all event and Ime messages target the same RenderWidgetHost throughout this
  // function call.
  host_->BeginKeyboardEvent();

  bool shouldAutohideCursor = textInputType_ != ui::TEXT_INPUT_TYPE_NONE &&
                              eventType == NSKeyDown &&
                              !(modifierFlags & NSCommandKeyMask);

  // We only handle key down events and just simply forward other events.
  if (eventType != NSKeyDown) {
    hostHelper_->ForwardKeyboardEvent(event, latency_info);

    // Possibly autohide the cursor.
    if (shouldAutohideCursor) {
      [NSCursor setHiddenUntilMouseMoves:YES];
      cursorHidden_ = YES;
    }

    host_->EndKeyboardEvent();
    return;
  }

  keyDownCodes_.insert(keyCode);

  base::scoped_nsobject<RenderWidgetHostViewCocoa> keepSelfAlive([self retain]);

  // Records the current marked text state, so that we can know if the marked
  // text was deleted or not after handling the key down event.
  BOOL oldHasMarkedText = hasMarkedText_;

  // This method should not be called recursively.
  DCHECK(!handlingKeyDown_);

  // Tells insertText: and doCommandBySelector: that we are handling a key
  // down event.
  handlingKeyDown_ = YES;

  // These variables might be set when handling the keyboard event.
  // Clear them here so that we can know whether they have changed afterwards.
  textToBeInserted_.clear();
  markedText_.clear();
  markedTextSelectedRange_ = NSMakeRange(NSNotFound, 0);
  ime_text_spans_.clear();
  setMarkedTextReplacementRange_ = gfx::Range::InvalidRange();
  unmarkTextCalled_ = NO;
  hasEditCommands_ = NO;
  editCommands_.clear();

  // Sends key down events to input method first, then we can decide what should
  // be done according to input method's feedback.
  [self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];

  handlingKeyDown_ = NO;

  // Indicates if we should send the key event and corresponding editor commands
  // after processing the input method result.
  BOOL delayEventUntilAfterImeCompostion = NO;

  // To emulate Windows, over-write |event.windowsKeyCode| to VK_PROCESSKEY
  // while an input method is composing or inserting a text.
  // Gmail checks this code in its onkeydown handler to stop auto-completing
  // e-mail addresses while composing a CJK text.
  // If the text to be inserted has only one character, then we don't need this
  // trick, because we'll send the text as a key press event instead.
  if (hasMarkedText_ || oldHasMarkedText || textToBeInserted_.length() > 1) {
    NativeWebKeyboardEvent fakeEvent = event;
    fakeEvent.windows_key_code = 0xE5;  // VKEY_PROCESSKEY
    fakeEvent.skip_in_browser = true;
    hostHelper_->ForwardKeyboardEvent(fakeEvent, latency_info);
    // If this key event was handled by the input method, but
    // -doCommandBySelector: (invoked by the call to -interpretKeyEvents: above)
    // enqueued edit commands, then in order to let webkit handle them
    // correctly, we need to send the real key event and corresponding edit
    // commands after processing the input method result.
    // We shouldn't do this if a new marked text was set by the input method,
    // otherwise the new marked text might be cancelled by webkit.
    if (hasEditCommands_ && !hasMarkedText_)
      delayEventUntilAfterImeCompostion = YES;
  } else {
    hostHelper_->ForwardKeyboardEventWithCommands(event, latency_info,
                                                  editCommands_);
  }

  // Then send keypress and/or composition related events.
  // If there was a marked text or the text to be inserted is longer than 1
  // character, then we send the text by calling FinishComposingText().
  // Otherwise, if the text to be inserted only contains 1 character, then we
  // can just send a keypress event which is fabricated by changing the type of
  // the keydown event, so that we can retain all necessary informations, such
  // as unmodifiedText, etc. And we need to set event.skip_in_browser to true to
  // prevent the browser from handling it again.
  // Note that, |textToBeInserted_| is a UTF-16 string, but it's fine to only
  // handle BMP characters here, as we can always insert non-BMP characters as
  // text.
  BOOL textInserted = NO;
  if (textToBeInserted_.length() >
      ((hasMarkedText_ || oldHasMarkedText) ? 0u : 1u)) {
    host_->ImeCommitText(textToBeInserted_, gfx::Range::InvalidRange());
    textInserted = YES;
  }

  // Updates or cancels the composition. If some text has been inserted, then
  // we don't need to cancel the composition explicitly.
  if (hasMarkedText_ && markedText_.length()) {
    // Sends the updated marked text to the renderer so it can update the
    // composition node in WebKit.
    // When marked text is available, |markedTextSelectedRange_| will be the
    // range being selected inside the marked text.
    host_->ImeSetComposition(markedText_, ime_text_spans_,
                             setMarkedTextReplacementRange_,
                             markedTextSelectedRange_.location,
                             NSMaxRange(markedTextSelectedRange_));
  } else if (oldHasMarkedText && !hasMarkedText_ && !textInserted) {
    if (unmarkTextCalled_) {
      host_->ImeFinishComposingText();
    } else {
      host_->ImeCancelCompositionFromCocoa();
    }
  }

  // Clear information from |interpretKeyEvents:|
  setMarkedTextReplacementRange_ = gfx::Range::InvalidRange();

  // If the key event was handled by the input method but it also generated some
  // edit commands, then we need to send the real key event and corresponding
  // edit commands here. This usually occurs when the input method wants to
  // finish current composition session but still wants the application to
  // handle the key event. See http://crbug.com/48161 for reference.
  if (delayEventUntilAfterImeCompostion) {
    // If |delayEventUntilAfterImeCompostion| is YES, then a fake key down event
    // with windowsKeyCode == 0xE5 has already been sent to webkit.
    // So before sending the real key down event, we need to send a fake key up
    // event to balance it.
    NativeWebKeyboardEvent fakeEvent = event;
    fakeEvent.SetType(blink::WebInputEvent::kKeyUp);
    fakeEvent.skip_in_browser = true;
    ui::LatencyInfo fake_event_latency_info = latency_info;
    fake_event_latency_info.set_source_event_type(ui::SourceEventType::OTHER);
    hostHelper_->ForwardKeyboardEvent(fakeEvent, fake_event_latency_info);
    hostHelper_->ForwardKeyboardEventWithCommands(
        event, fake_event_latency_info, editCommands_);
  }

  const NSUInteger kCtrlCmdKeyMask = NSControlKeyMask | NSCommandKeyMask;
  // Only send a corresponding key press event if there is no marked text.
  if (!hasMarkedText_) {
    if (!textInserted && textToBeInserted_.length() == 1) {
      // If a single character was inserted, then we just send it as a keypress
      // event.
      event.SetType(blink::WebInputEvent::kChar);
      event.text[0] = textToBeInserted_[0];
      event.text[1] = 0;
      event.skip_in_browser = true;
      hostHelper_->ForwardKeyboardEvent(event, latency_info);
    } else if ((!textInserted || delayEventUntilAfterImeCompostion) &&
               event.text[0] != '\0' &&
               ((modifierFlags & kCtrlCmdKeyMask) ||
                (hasEditCommands_ && editCommands_.empty()))) {
      // We don't get insertText: calls if ctrl or cmd is down, or the key event
      // generates an insert command. So synthesize a keypress event for these
      // cases, unless the key event generated any other command.
      event.SetType(blink::WebInputEvent::kChar);
      event.skip_in_browser = true;
      hostHelper_->ForwardKeyboardEvent(event, latency_info);
    }
  }

  // Possibly autohide the cursor.
  if (shouldAutohideCursor) {
    [NSCursor setHiddenUntilMouseMoves:YES];
    cursorHidden_ = YES;
  }

  host_->EndKeyboardEvent();
}

- (BOOL)suppressNextKeyUpForTesting:(int)keyCode {
  return keyDownCodes_.count(keyCode) == 0;
}

- (void)forceTouchEvent:(NSEvent*)theEvent {
  if (ui::ForceClickInvokesQuickLook())
    [self quickLookWithEvent:theEvent];
}

- (void)shortCircuitScrollWheelEvent:(NSEvent*)event {
  if ([event phase] != NSEventPhaseEnded &&
      [event phase] != NSEventPhaseCancelled) {
    return;
  }

  // History-swiping is not possible if the logic reaches this point.
  WebMouseWheelEvent webEvent = WebMouseWheelEventBuilder::Build(event, self);
  webEvent.rails_mode = mouseWheelFilter_.UpdateRailsMode(webEvent);
  hostHelper_->ForwardWheelEvent(webEvent);

  if (endWheelMonitor_) {
    [NSEvent removeMonitor:endWheelMonitor_];
    endWheelMonitor_ = nil;
  }
}

- (void)handleBeginGestureWithEvent:(NSEvent*)event
            isSyntheticallyInjected:(BOOL)isSyntheticallyInjected {
  [responderDelegate_ beginGestureWithEvent:event];

  WebGestureEvent gestureBeginEvent(WebGestureEventBuilder::Build(event, self));

  hostHelper_->GestureBegin(gestureBeginEvent, isSyntheticallyInjected);
}

- (void)handleEndGestureWithEvent:(NSEvent*)event {
  [responderDelegate_ endGestureWithEvent:event];

  // On macOS 10.11+, the end event has type = NSEventTypeMagnify and phase =
  // NSEventPhaseEnded. On macOS 10.10 and older, the event has type =
  // NSEventTypeEndGesture.
  if ([event type] == NSEventTypeMagnify ||
      [event type] == NSEventTypeEndGesture) {
    WebGestureEvent endEvent(WebGestureEventBuilder::Build(event, self));
    endEvent.SetType(WebInputEvent::kGesturePinchEnd);
    endEvent.SetSourceDevice(blink::WebGestureDevice::kTouchpad);
    endEvent.SetNeedsWheelEvent(true);
    hostHelper_->GestureEnd(endEvent);
  }
}

- (void)beginGestureWithEvent:(NSEvent*)event {
  // This method must be handled when linking with the 10.10 SDK or earlier, or
  // when the app is running on 10.10 or earlier.  In other circumstances, the
  // event will be handled by |magnifyWithEvent:|, so this method should do
  // nothing.
  bool shouldHandle = true;
#if defined(MAC_OS_X_VERSION_10_11) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_11
  shouldHandle = base::mac::IsAtMostOS10_10();
#endif

  if (shouldHandle) {
    [self handleBeginGestureWithEvent:event isSyntheticallyInjected:NO];
  }
}

- (void)endGestureWithEvent:(NSEvent*)event {
  // This method must be handled when linking with the 10.10 SDK or earlier, or
  // when the app is running on 10.10 or earlier.  In other circumstances, the
  // event will be handled by |magnifyWithEvent:|, so this method should do
  // nothing.
  bool shouldHandle = true;
#if defined(MAC_OS_X_VERSION_10_11) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_11
  shouldHandle = base::mac::IsAtMostOS10_10();
#endif

  if (shouldHandle) {
    [self handleEndGestureWithEvent:event];
  }
}

- (void)touchesMovedWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesMovedWithEvent:event];
}

- (void)touchesBeganWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesBeganWithEvent:event];
}

- (void)touchesCancelledWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesCancelledWithEvent:event];
}

- (void)touchesEndedWithEvent:(NSEvent*)event {
  [responderDelegate_ touchesEndedWithEvent:event];
}

- (void)smartMagnifyWithEvent:(NSEvent*)event {
  const WebGestureEvent& smartMagnifyEvent =
      WebGestureEventBuilder::Build(event, self);
  hostHelper_->SmartMagnify(smartMagnifyEvent);
}

- (void)showLookUpDictionaryOverlayFromRange:(NSRange)range {
  host_->LookUpDictionaryOverlayFromRange(gfx::Range(range));
}

// This is invoked only on 10.8 or newer when the user taps a word using
// three fingers.
- (void)quickLookWithEvent:(NSEvent*)event {
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  gfx::PointF rootPoint(point.x, NSHeight([self frame]) - point.y);
  host_->LookUpDictionaryOverlayAtPoint(rootPoint);
}

// This method handles 2 different types of hardware events.
// (Apple does not distinguish between them).
//  a. Scrolling the middle wheel of a mouse.
//  b. Swiping on the track pad.
//
// This method is responsible for 2 types of behavior:
//  a. Scrolling the content of window.
//  b. Navigating forwards/backwards in history.
//
// This is a brief description of the logic:
//  1. If the content can be scrolled, scroll the content.
//     (This requires a roundtrip to blink to determine whether the content
//      can be scrolled.)
//     Once this logic is triggered, the navigate logic cannot be triggered
//     until the gesture finishes.
//  2. If the user is making a horizontal swipe, start the navigate
//     forward/backwards UI.
//     Once this logic is triggered, the user can either cancel or complete
//     the gesture. If the user completes the gesture, all remaining touches
//     are swallowed, and not allowed to scroll the content. If the user
//     cancels the gesture, all remaining touches are forwarded to the content
//     scroll logic. The user cannot trigger the navigation logic again.
- (void)scrollWheel:(NSEvent*)event {
#if defined(MAC_OS_X_VERSION_10_11) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_11
  // When linking against the 10.11 (or later) SDK and running on 10.11 or
  // later, check the phase of the event and specially handle the "begin" and
  // "end" phases.
  if (base::mac::IsAtLeastOS10_11()) {
    if (event.phase == NSEventPhaseBegan) {
      [self handleBeginGestureWithEvent:event isSyntheticallyInjected:NO];
    }

    if (event.phase == NSEventPhaseEnded ||
        event.phase == NSEventPhaseCancelled) {
      [self handleEndGestureWithEvent:event];
    }
  }
#endif

  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [responderDelegate_ handleEvent:event];
    if (handled)
      return;
  }

  // Compute Event.Latency.OS.MOUSE_WHEEL histogram.
  ui::ComputeEventLatencyOS(event);

  // Use an NSEvent monitor to listen for the wheel-end end. This ensures that
  // the event is received even when the mouse cursor is no longer over the view
  // when the scrolling ends (e.g. if the tab was switched). This is necessary
  // for ending rubber-banding in such cases.
  if ([event phase] == NSEventPhaseBegan && !endWheelMonitor_) {
    endWheelMonitor_ = [NSEvent
        addLocalMonitorForEventsMatchingMask:NSScrollWheelMask
                                     handler:^(NSEvent* blockEvent) {
                                       [self shortCircuitScrollWheelEvent:
                                                 blockEvent];
                                       return blockEvent;
                                     }];
  }

  // This is responsible for content scrolling!
  WebMouseWheelEvent webEvent = WebMouseWheelEventBuilder::Build(event, self);
  webEvent.rails_mode = mouseWheelFilter_.UpdateRailsMode(webEvent);
  hostHelper_->RouteOrProcessWheelEvent(webEvent);
}

// Called repeatedly during a pinch gesture, with incremental change values.
- (void)magnifyWithEvent:(NSEvent*)event {
#if defined(MAC_OS_X_VERSION_10_11) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_11
  // When linking against the 10.11 (or later) SDK and running on 10.11 or
  // later, check the phase of the event and specially handle the "begin" and
  // "end" phases.
  if (base::mac::IsAtLeastOS10_11()) {
    if (event.phase == NSEventPhaseBegan) {
      [self handleBeginGestureWithEvent:event isSyntheticallyInjected:NO];
      return;
    }

    if (event.phase == NSEventPhaseEnded ||
        event.phase == NSEventPhaseCancelled) {
      [self handleEndGestureWithEvent:event];
      return;
    }
  }
#endif

  // If this conditional evalutes to true, and the function has not
  // short-circuited from the previous block, then this event is a duplicate of
  // a gesture event, and should be ignored.
  if (event.phase == NSEventPhaseBegan || event.phase == NSEventPhaseEnded ||
      event.phase == NSEventPhaseCancelled) {
    return;
  }

  WebGestureEvent updateEvent = WebGestureEventBuilder::Build(event, self);
  hostHelper_->GestureUpdate(updateEvent);
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  NSWindow* oldWindow = [self window];

  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];

  if (oldWindow) {
    [notificationCenter
        removeObserver:self
                  name:NSWindowDidChangeBackingPropertiesNotification
                object:oldWindow];
    [notificationCenter removeObserver:self
                                  name:NSWindowDidMoveNotification
                                object:oldWindow];
    [notificationCenter removeObserver:self
                                  name:NSWindowDidResizeNotification
                                object:oldWindow];
    [notificationCenter removeObserver:self
                                  name:NSWindowDidBecomeKeyNotification
                                object:oldWindow];
    [notificationCenter removeObserver:self
                                  name:NSWindowDidResignKeyNotification
                                object:oldWindow];
  }
  if (newWindow) {
    [notificationCenter
        addObserver:self
           selector:@selector(windowDidChangeBackingProperties:)
               name:NSWindowDidChangeBackingPropertiesNotification
             object:newWindow];
    [notificationCenter addObserver:self
                           selector:@selector(windowChangedGlobalFrame:)
                               name:NSWindowDidMoveNotification
                             object:newWindow];
    [notificationCenter addObserver:self
                           selector:@selector(windowChangedGlobalFrame:)
                               name:NSWindowDidResizeNotification
                             object:newWindow];
    [notificationCenter addObserver:self
                           selector:@selector(windowDidBecomeKey:)
                               name:NSWindowDidBecomeKeyNotification
                             object:newWindow];
    [notificationCenter addObserver:self
                           selector:@selector(windowDidResignKey:)
                               name:NSWindowDidResignKeyNotification
                             object:newWindow];
  }

  hostHelper_->SetAccessibilityWindow(newWindow);
  [self sendWindowFrameInScreenToHost];
}

- (void)updateScreenProperties {
  NSWindow* enclosingWindow = [self window];
  if (!enclosingWindow)
    return;

  // TODO(ccameron): This will call [enclosingWindow screen], which may return
  // nil. Do that call here to avoid sending bogus display info to the host.
  display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestView(self);
  host_->OnDisplayChanged(display);
}

// This will be called when the NSView's NSWindow moves from one NSScreen to
// another, and makes note of the new screen's color space, scale factor, etc.
// It is also called when the current NSScreen's properties change (which is
// redundant with display::DisplayObserver::OnDisplayMetricsChanged).
- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
  // Delay calling updateScreenProperties so that display::ScreenMac can
  // update our display::Displays first (if applicable).
  [self performSelector:@selector(updateScreenProperties)
             withObject:nil
             afterDelay:0];
}

- (void)windowChangedGlobalFrame:(NSNotification*)notification {
  [self sendWindowFrameInScreenToHost];
  // Update the view bounds relative to the window, as they may have changed
  // during layout, and we don't explicitly listen for re-layout of parent
  // views.
  [self sendViewBoundsInWindowToHost];
}

- (void)setFrame:(NSRect)r {
  // Note that -setFrame: calls through -setFrameSize: and -setFrameOrigin. To
  // avoid spamming the host with transiently invalid states, only send one
  // message at the end.
  inSetFrame_ = YES;
  [super setFrame:r];
  inSetFrame_ = NO;
  [self sendViewBoundsInWindowToHost];
}

- (void)setFrameOrigin:(NSPoint)newOrigin {
  [super setFrameOrigin:newOrigin];
  [self sendViewBoundsInWindowToHost];
}

- (void)setFrameSize:(NSSize)newSize {
  [super setFrameSize:newSize];
  [self sendViewBoundsInWindowToHost];
}

- (BOOL)canBecomeKeyView {
  if ([self hostIsDisconnected])
    return NO;

  return canBeKeyView_;
}

- (BOOL)acceptsFirstResponder {
  if ([self hostIsDisconnected])
    return NO;

  return canBeKeyView_;
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  DCHECK([self window]);
  DCHECK_EQ([self window], [notification object]);
  if ([responderDelegate_ respondsToSelector:@selector(windowDidBecomeKey)])
    [responderDelegate_ windowDidBecomeKey];
  if ([self window].isKeyWindow)
    host_->OnWindowIsKeyChanged(true);
}

- (void)windowDidResignKey:(NSNotification*)notification {
  DCHECK([self window]);
  DCHECK_EQ([self window], [notification object]);

  // If our app is still active and we're still the key window, ignore this
  // message, since it just means that a menu extra (on the "system status bar")
  // was activated; we'll get another |-windowDidResignKey| if we ever really
  // lose key window status.
  if ([NSApp isActive] && ([NSApp keyWindow] == [self window]))
    return;

  host_->OnWindowIsKeyChanged(false);
}

- (BOOL)becomeFirstResponder {
  if ([self hostIsDisconnected])
    return NO;
  if ([responderDelegate_ respondsToSelector:@selector(becomeFirstResponder)])
    [responderDelegate_ becomeFirstResponder];

  host_->OnFirstResponderChanged(true);

  // Cancel any onging composition text which was left before we lost focus.
  // TODO(suzhe): We should do it in -resignFirstResponder: method, but
  // somehow that method won't be called when switching among different tabs.
  // See http://crbug.com/47209
  [self cancelComposition];

  NSNumber* direction = [NSNumber
      numberWithUnsignedInteger:[[self window] keyViewSelectionDirection]];
  NSDictionary* userInfo =
      [NSDictionary dictionaryWithObject:direction forKey:kSelectionDirection];
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kViewDidBecomeFirstResponder
                    object:self
                  userInfo:userInfo];

  return YES;
}

- (BOOL)resignFirstResponder {
  if ([responderDelegate_ respondsToSelector:@selector(resignFirstResponder)])
    [responderDelegate_ resignFirstResponder];

  host_->OnFirstResponderChanged(false);
  if (closeOnDeactivate_) {
    [self setHidden:YES];
    host_->RequestShutdown();
  }

  // We should cancel any onging composition whenever RWH's Blur() method gets
  // called, because in this case, webkit will confirm the ongoing composition
  // internally.
  [self cancelComposition];

  return YES;
}

- (BOOL)isAutomaticQuoteSubstitutionEnabled {
  return [NSUserDefaults.standardUserDefaults
      boolForKey:WebAutomaticQuoteSubstitutionEnabled];
}

- (void)setAutomaticQuoteSubstitutionEnabled:(BOOL)enabled {
  [NSUserDefaults.standardUserDefaults
      setBool:enabled
       forKey:WebAutomaticQuoteSubstitutionEnabled];
}

- (void)toggleAutomaticQuoteSubstitution:(id)sender {
  self.automaticQuoteSubstitutionEnabled =
      !self.automaticQuoteSubstitutionEnabled;
}

- (BOOL)isAutomaticDashSubstitutionEnabled {
  return [NSUserDefaults.standardUserDefaults
      boolForKey:WebAutomaticDashSubstitutionEnabled];
}

- (void)setAutomaticDashSubstitutionEnabled:(BOOL)enabled {
  [NSUserDefaults.standardUserDefaults
      setBool:enabled
       forKey:WebAutomaticDashSubstitutionEnabled];
}

- (void)toggleAutomaticDashSubstitution:(id)sender {
  self.automaticDashSubstitutionEnabled =
      !self.automaticDashSubstitutionEnabled;
}

- (BOOL)isAutomaticTextReplacementEnabled {
  if (![NSUserDefaults.standardUserDefaults
          objectForKey:WebAutomaticTextReplacementEnabled]) {
    return NSSpellChecker.automaticTextReplacementEnabled;
  }
  return [NSUserDefaults.standardUserDefaults
      boolForKey:WebAutomaticTextReplacementEnabled];
}

- (void)setAutomaticTextReplacementEnabled:(BOOL)enabled {
  [NSUserDefaults.standardUserDefaults
      setBool:enabled
       forKey:WebAutomaticTextReplacementEnabled];
}

- (void)toggleAutomaticTextReplacement:(id)sender {
  self.automaticTextReplacementEnabled = !self.automaticTextReplacementEnabled;
}

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  if (item.action == @selector(orderFrontSubstitutionsPanel:))
    return YES;
  if (NSMenuItem* menuItem = base::mac::ObjCCast<NSMenuItem>(item)) {
    if (item.action == @selector(toggleAutomaticQuoteSubstitution:)) {
      menuItem.state = self.automaticQuoteSubstitutionEnabled;
      return !!(self.allowedTextCheckingTypes & NSTextCheckingTypeQuote);
    } else if (item.action == @selector(toggleAutomaticDashSubstitution:)) {
      menuItem.state = self.automaticDashSubstitutionEnabled;
      return !!(self.allowedTextCheckingTypes & NSTextCheckingTypeDash);
    } else if (item.action == @selector(toggleAutomaticTextReplacement:)) {
      menuItem.state = self.automaticTextReplacementEnabled;
      return !!(self.allowedTextCheckingTypes & NSTextCheckingTypeReplacement);
    }
  }

  if (responderDelegate_ &&
      [responderDelegate_ respondsToSelector:@selector
                          (validateUserInterfaceItem:isValidItem:)]) {
    BOOL valid;
    BOOL known =
        [responderDelegate_ validateUserInterfaceItem:item isValidItem:&valid];
    if (known)
      return valid;
  }

  bool is_for_main_frame = false;
  host_->SyncIsWidgetForMainFrame(&is_for_main_frame);

  bool is_speaking = false;
  host_->SyncIsSpeaking(&is_speaking);

  SEL action = [item action];

  if (action == @selector(stopSpeaking:))
    return is_for_main_frame && is_speaking;

  if (action == @selector(startSpeaking:))
    return is_for_main_frame;

  // For now, these actions are always enabled for render view,
  // this is sub-optimal.
  // TODO(suzhe): Plumb the "can*" methods up from WebCore.
  if (action == @selector(undo:) || action == @selector(redo:) ||
      action == @selector(cut:) || action == @selector(copy:) ||
      action == @selector(copyToFindPboard:) || action == @selector(paste:) ||
      action == @selector(pasteAndMatchStyle:)) {
    return is_for_main_frame;
  }

  return editCommandHelper_->IsMenuItemEnabled(action, self);
}

- (RenderWidgetHostNSViewHost*)renderWidgetHostNSViewHost {
  return host_;
}

- (void)setAccessibilityParentElement:(id)accessibilityParent {
  accessibilityParent_.reset(accessibilityParent, base::scoped_policy::RETAIN);
}

- (id)accessibilityHitTest:(NSPoint)point {
  id root_element = hostHelper_->GetRootBrowserAccessibilityElement();
  if (!root_element)
    return self;
  NSPoint pointInWindow =
      ui::ConvertPointFromScreenToWindow([self window], point);
  NSPoint localPoint = [self convertPoint:pointInWindow fromView:nil];
  localPoint.y = NSHeight([self bounds]) - localPoint.y;
  id obj = [root_element accessibilityHitTest:localPoint];
  return obj;
}

- (id)accessibilityFocusedUIElement {
  return hostHelper_->GetFocusedBrowserAccessibilityElement();
}

// NSAccessibility formal protocol:

- (NSArray*)accessibilityChildren {
  id root = hostHelper_->GetRootBrowserAccessibilityElement();
  if (root)
    return @[ root ];
  return nil;
}

- (NSArray*)accessibilityContents {
  return self.accessibilityChildren;
}

- (id)accessibilityParent {
  if (accessibilityParent_)
    return NSAccessibilityUnignoredAncestor(accessibilityParent_);
  return [super accessibilityParent];
}

- (NSAccessibilityRole)accessibilityRole {
  return NSAccessibilityScrollAreaRole;
}

// Below is our NSTextInputClient implementation.
//
// When WebHTMLView receives a NSKeyDown event, WebHTMLView calls the following
// functions to process this event.
//
// [WebHTMLView keyDown] ->
//     EventHandler::keyEvent() ->
//     ...
//     [WebEditorHost handleKeyboardEvent] ->
//     [WebHTMLView _interceptEditingKeyEvent] ->
//     [NSResponder interpretKeyEvents] ->
//     [WebHTMLView insertText] ->
//     Editor::insertText()
//
// Unfortunately, it is hard for Chromium to use this implementation because
// it causes key-typing jank.
// RenderWidgetHostViewMac is running in a browser process. On the other
// hand, Editor and EventHandler are running in a renderer process.
// So, if we used this implementation, a NSKeyDown event is dispatched to
// the following functions of Chromium.
//
// [RenderWidgetHostViewMac keyEvent] (browser) ->
//     |Sync IPC (KeyDown)| (*1) ->
//     EventHandler::keyEvent() (renderer) ->
//     ...
//     EditorHostImpl::handleKeyboardEvent() (renderer) ->
//     |Sync IPC| (*2) ->
//     [RenderWidgetHostViewMac _interceptEditingKeyEvent] (browser) ->
//     [self interpretKeyEvents] ->
//     [RenderWidgetHostViewMac insertText] (browser) ->
//     |Async IPC| ->
//     Editor::insertText() (renderer)
//
// (*1) we need to wait until this call finishes since WebHTMLView uses the
// result of EventHandler::keyEvent().
// (*2) we need to wait until this call finishes since WebEditorHost uses
// the result of [WebHTMLView _interceptEditingKeyEvent].
//
// This needs many sync IPC messages sent between a browser and a renderer for
// each key event, which would probably result in key-typing jank.
// To avoid this problem, this implementation processes key events (and input
// method events) totally in a browser process and sends asynchronous input
// events, almost same as KeyboardEvents (and TextEvents) of DOM Level 3, to a
// renderer process.
//
// [RenderWidgetHostViewMac keyEvent] (browser) ->
//     |Async IPC (RawKeyDown)| ->
//     [self interpretKeyEvents] ->
//     [RenderWidgetHostViewMac insertText] (browser) ->
//     |Async IPC (Char)| ->
//     Editor::insertText() (renderer)
//
// Since this implementation doesn't have to wait any IPC calls, this doesn't
// make any key-typing jank. --hbono 7/23/09
//
extern "C" {
extern NSString* NSTextInputReplacementRangeAttributeName;
}

- (NSArray*)validAttributesForMarkedText {
  // This code is just copied from WebKit except renaming variables.
  if (!validAttributesForMarkedText_) {
    validAttributesForMarkedText_.reset([[NSArray alloc]
        initWithObjects:NSUnderlineStyleAttributeName,
                        NSUnderlineColorAttributeName,
                        NSMarkedClauseSegmentAttributeName,
                        NSTextInputReplacementRangeAttributeName, nil]);
  }
  return validAttributesForMarkedText_.get();
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint {
  DCHECK([self window]);
  // |thePoint| is in screen coordinates, but needs to be converted to WebKit
  // coordinates (upper left origin). Scroll offsets will be taken care of in
  // the renderer.
  thePoint = ui::ConvertPointFromScreenToWindow([self window], thePoint);
  thePoint = [self convertPoint:thePoint fromView:nil];
  thePoint.y = NSHeight([self frame]) - thePoint.y;
  gfx::PointF rootPoint(thePoint.x, thePoint.y);

  uint32_t index = UINT32_MAX;
  host_->SyncGetCharacterIndexAtPoint(rootPoint, &index);
  // |index| could be WTF::notFound (-1) and its value is different from
  // NSNotFound so we need to convert it.
  if (index == UINT32_MAX)
    return NSNotFound;
  size_t char_index = index;
  return NSUInteger(char_index);
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
                         actualRange:(NSRangePointer)actualRange {
  gfx::Rect gfxRect;
  gfx::Range gfxActualRange;
  bool success = false;
  if (actualRange)
    gfxActualRange = gfx::Range(*actualRange);
  host_->SyncGetFirstRectForRange(gfx::Range(theRange), gfxRect, gfxActualRange,
                                  &gfxRect, &gfxActualRange, &success);
  if (!success) {
    // The call to cancelComposition comes from https://crrev.com/350261.
    [self cancelComposition];
    return NSZeroRect;
  }
  if (actualRange)
    *actualRange = gfxActualRange.ToNSRange();

  // The returned rectangle is in WebKit coordinates (upper left origin), so
  // flip the coordinate system.
  NSRect viewFrame = [self frame];
  NSRect rect = NSRectFromCGRect(gfxRect.ToCGRect());
  rect.origin.y = NSHeight(viewFrame) - NSMaxY(rect);

  // Convert into screen coordinates for return.
  rect = [self convertRect:rect toView:nil];
  rect = [[self window] convertRectToScreen:rect];
  return rect;
}

- (NSRange)selectedRange {
  return textSelectionRange_.ToNSRange();
}

- (NSRange)markedRange {
  // An input method calls this method to check if an application really has
  // a text being composed when hasMarkedText call returns true.
  // Returns the range saved in the setMarkedText method so the input method
  // calls the setMarkedText method and we can update the composition node
  // there. (When this method returns an empty range, the input method doesn't
  // call the setMarkedText method.)
  return hasMarkedText_ ? markedRange_ : NSMakeRange(NSNotFound, 0);
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:
                                                   (NSRangePointer)actualRange {
  // Prepare |actualRange| as if the proposed range is invalid. If it is valid,
  // then |actualRange| will be updated again.
  if (actualRange)
    *actualRange = NSMakeRange(NSNotFound, 0);

  // The caller of this method is allowed to pass nonsensical ranges. These
  // can't even be converted into gfx::Ranges.
  if (range.location == NSNotFound || range.length == 0)
    return nil;
  if (range.length >= std::numeric_limits<NSUInteger>::max() - range.location)
    return nil;

  const gfx::Range requestedRange(range);
  if (requestedRange.is_reversed())
    return nil;

  gfx::Range expectedRange;
  const base::string16* expectedText;

  if (!compositionRange_.is_empty()) {
    // This method might get called after TextInputState.type is reset to none,
    // in which case there will be no composition range information
    // https://crbug.com/698672
    expectedText = &markedText_;
    expectedRange = compositionRange_.Intersect(
        gfx::Range(compositionRange_.start(),
                   compositionRange_.start() + expectedText->length()));
  } else {
    expectedText = &textSelectionText_;
    size_t offset = textSelectionOffset_;
    expectedRange = gfx::Range(offset, offset + expectedText->size());
  }

  gfx::Range gfxActualRange = expectedRange.Intersect(requestedRange);
  if (!gfxActualRange.IsValid())
    return nil;
  if (actualRange)
    *actualRange = gfxActualRange.ToNSRange();

  base::string16 string = expectedText->substr(
      gfxActualRange.start() - expectedRange.start(), gfxActualRange.length());
  return [[[NSAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(string)] autorelease];
}

- (NSInteger)conversationIdentifier {
  return reinterpret_cast<NSInteger>(self);
}

// Each RenderWidgetHostViewCocoa has its own input context, but we return
// nil when the caret is in non-editable content to avoid making input methods
// do their work.
- (NSTextInputContext*)inputContext {
  switch (textInputType_) {
    case ui::TEXT_INPUT_TYPE_NONE:
      return nil;
    default:
      return [super inputContext];
  }
}

- (BOOL)hasMarkedText {
  // An input method calls this function to figure out whether or not an
  // application is really composing a text. If it is composing, it calls
  // the markedRange method, and maybe calls the setMarkedText method.
  // It seems an input method usually calls this function when it is about to
  // cancel an ongoing composition. If an application has a non-empty marked
  // range, it calls the setMarkedText method to delete the range.
  return hasMarkedText_;
}

- (void)unmarkText {
  // Delete the composition node of the renderer and finish an ongoing
  // composition.
  // It seems an input method calls the setMarkedText method and set an empty
  // text when it cancels an ongoing composition, i.e. I have never seen an
  // input method calls this method.
  hasMarkedText_ = NO;
  markedText_.clear();
  markedTextSelectedRange_ = NSMakeRange(NSNotFound, 0);
  ime_text_spans_.clear();

  // If we are handling a key down event, then FinishComposingText() will be
  // called in keyEvent: method.
  if (!handlingKeyDown_) {
    host_->ImeFinishComposingText();
  } else {
    unmarkTextCalled_ = YES;
  }
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)newSelRange
     replacementRange:(NSRange)replacementRange {
  // An input method updates the composition string.
  // We send the given text and range to the renderer so it can update the
  // composition node of WebKit.
  // TODO(suzhe): It's hard for us to support replacementRange without accessing
  // the full web content.
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  int length = [im_text length];

  // |markedRange_| will get set on a callback from ImeSetComposition().
  markedTextSelectedRange_ = newSelRange;
  markedText_ = base::SysNSStringToUTF16(im_text);
  hasMarkedText_ = (length > 0);

  ime_text_spans_.clear();
  if (isAttributedString) {
    ExtractUnderlines(string, &ime_text_spans_);
  } else {
    // Use a thin black underline by default.
    ime_text_spans_.push_back(ui::ImeTextSpan(
        ui::ImeTextSpan::Type::kComposition, 0, length,
        ui::ImeTextSpan::Thickness::kThin, SK_ColorTRANSPARENT));
  }

  // If we are handling a key down event, then SetComposition() will be
  // called in keyEvent: method.
  // Input methods of Mac use setMarkedText calls with an empty text to cancel
  // an ongoing composition. So, we should check whether or not the given text
  // is empty to update the input method state. (Our input method backend
  // automatically cancels an ongoing composition when we send an empty text.
  // So, it is OK to send an empty text to the renderer.)
  if (handlingKeyDown_) {
    setMarkedTextReplacementRange_ = gfx::Range(replacementRange);
  } else {
    host_->ImeSetComposition(markedText_, ime_text_spans_,
                             gfx::Range(replacementRange), newSelRange.location,
                             NSMaxRange(newSelRange));
  }
}

- (void)doCommandBySelector:(SEL)selector {
  // An input method calls this function to dispatch an editing command to be
  // handled by this view.
  if (selector == @selector(noop:))
    return;

  std::string command(base::SysNSStringToUTF8(
      RenderWidgetHostViewMacEditCommandHelper::CommandNameForSelector(
          selector)));

  // If this method is called when handling a key down event, then we need to
  // handle the command in the key event handler. Otherwise we can just handle
  // it here.
  if (handlingKeyDown_) {
    hasEditCommands_ = YES;
    // We ignore commands that insert characters, because this was causing
    // strange behavior (e.g. tab always inserted a tab rather than moving to
    // the next field on the page).
    if (!base::StartsWith(command, "insert",
                          base::CompareCase::INSENSITIVE_ASCII))
      editCommands_.push_back(EditCommand(command, ""));
  } else {
    host_->ExecuteEditCommand(command);
  }
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
  // An input method has characters to be inserted.
  // Same as Linux, Mac calls this method not only:
  // * when an input method finishes composing text, but also;
  // * when we type an ASCII character (without using input methods).
  // When we aren't using input methods, we should send the given character as
  // a Char event so it is dispatched to an onkeypress() event handler of
  // JavaScript.
  // On the other hand, when we are using input methods, we should send the
  // given characters as an input method event and prevent the characters from
  // being dispatched to onkeypress() event handlers.
  // Text inserting might be initiated by other source instead of keyboard
  // events, such as the Characters dialog. In this case the text should be
  // sent as an input method event as well.
  // TODO(suzhe): It's hard for us to support replacementRange without accessing
  // the full web content.
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  if (handlingKeyDown_) {
    textToBeInserted_.append(base::SysNSStringToUTF16(im_text));
    shouldRequestTextSubstitutions_ = YES;
  } else {
    gfx::Range replacement_range(replacementRange);
    host_->ImeCommitText(base::SysNSStringToUTF16(im_text), replacement_range);
  }

  // Inserting text will delete all marked text automatically.
  hasMarkedText_ = NO;
}

- (void)insertText:(id)string {
  [self insertText:string replacementRange:NSMakeRange(NSNotFound, 0)];
}

- (void)viewDidMoveToWindow {
  // Update the window's frame, the view's bounds, focus, and the display info,
  // as they have not been updated while unattached to a window.
  [self sendWindowFrameInScreenToHost];
  [self sendViewBoundsInWindowToHost];
  [self updateScreenProperties];
  host_->OnWindowIsKeyChanged([[self window] isKeyWindow]);
  host_->OnFirstResponderChanged([[self window] firstResponder] == self);

  // If we switch windows (or are removed from the view hierarchy), cancel any
  // open mouse-downs.
  if (hasOpenMouseDown_) {
    WebMouseEvent event(WebInputEvent::kMouseUp, WebInputEvent::kNoModifiers,
                        ui::EventTimeForNow());
    event.button = WebMouseEvent::Button::kLeft;
    hostHelper_->ForwardMouseEvent(event);
    hasOpenMouseDown_ = NO;
  }
}

- (void)undo:(id)sender {
  host_->Undo();
}

- (void)redo:(id)sender {
  host_->Redo();
}

- (void)cut:(id)sender {
  host_->Cut();
}

- (void)copy:(id)sender {
  host_->Copy();
}

- (void)copyToFindPboard:(id)sender {
  host_->CopyToFindPboard();
}

- (void)paste:(id)sender {
  host_->Paste();
}

- (void)pasteAndMatchStyle:(id)sender {
  host_->PasteAndMatchStyle();
}

- (void)selectAll:(id)sender {
  // editCommandHelper_ adds implementations for most NSResponder methods
  // dynamically. But the renderer side only sends selection results back to
  // the browser if they were triggered by a keyboard event or went through
  // one of the Select methods on RWH. Since selectAll: is called from the
  // menu handler, neither is true.
  // Explicitly call SelectAll() here to make sure the renderer returns
  // selection results.
  host_->SelectAll();
}

- (void)startSpeaking:(id)sender {
  host_->StartSpeaking();
}

- (void)stopSpeaking:(id)sender {
  host_->StopSpeaking();
}

- (void)cancelComposition {
  [NSSpellChecker.sharedSpellChecker dismissCorrectionIndicatorForView:self];

  if (!hasMarkedText_)
    return;

  NSTextInputContext* inputContext = [self inputContext];
  [inputContext discardMarkedText];

  hasMarkedText_ = NO;
  // Should not call [self unmarkText] here, because it'll send unnecessary
  // cancel composition IPC message to the renderer.
}

- (void)finishComposingText {
  if (!hasMarkedText_)
    return;

  host_->ImeFinishComposingText();
  [self cancelComposition];
}

// Overriding a NSResponder method to support application services.

- (id)validRequestorForSendType:(NSString*)sendType
                     returnType:(NSString*)returnType {
  id requestor = nil;
  BOOL sendTypeIsString = [sendType isEqual:NSStringPboardType];
  BOOL returnTypeIsString = [returnType isEqual:NSStringPboardType];
  BOOL hasText = !textSelectionRange_.is_empty();
  BOOL takesText = textInputType_ != ui::TEXT_INPUT_TYPE_NONE;

  if (sendTypeIsString && hasText && !returnType) {
    requestor = self;
  } else if (!sendType && returnTypeIsString && takesText) {
    requestor = self;
  } else if (sendTypeIsString && returnTypeIsString && hasText && takesText) {
    requestor = self;
  } else {
    requestor =
        [super validRequestorForSendType:sendType returnType:returnType];
  }
  return requestor;
}

- (BOOL)shouldChangeCurrentCursor {
  // |updateCursor:| might be called outside the view bounds. Check the mouse
  // location before setting the cursor. Also, do not set cursor if it's not a
  // key window.
  NSPoint location = ui::ConvertPointFromScreenToWindow(
      [self window], [NSEvent mouseLocation]);
  location = [self convertPoint:location fromView:nil];
  if (![self mouse:location inRect:[self bounds]] ||
      ![[self window] isKeyWindow])
    return NO;

  if (cursorHidden_ || showingContextMenu_)
    return NO;

  return YES;
}

- (void)updateCursor:(NSCursor*)cursor {
  if (currentCursor_ == cursor)
    return;

  currentCursor_.reset([cursor retain]);
  [[self window] invalidateCursorRectsForView:self];

  // NSWindow's invalidateCursorRectsForView: resets cursor rects but does not
  // update the cursor instantly. The cursor is updated when the mouse moves.
  // Update the cursor instantly by setting the current cursor.
  if ([self shouldChangeCurrentCursor])
    [currentCursor_ set];
}

- (void)popupWindowWillClose:(NSNotification*)notification {
  [self setHidden:YES];
  host_->RequestShutdown();
}

- (void)invalidateTouchBar {
  // Work around a crash (https://crbug.com/822427).
  [candidateListTouchBarItem_ setCandidates:@[]
                           forSelectedRange:NSMakeRange(NSNotFound, 0)
                                   inString:nil];
  candidateListTouchBarItem_.reset();
  self.touchBar = nil;
}

- (NSTouchBar*)makeTouchBar {
  if (textInputType_ != ui::TEXT_INPUT_TYPE_NONE &&
      !(textInputFlags_ & blink::kWebTextInputFlagAutocorrectOff) &&
      !(textInputFlags_ & blink::kWebTextInputFlagSpellcheckOff)) {
    candidateListTouchBarItem_.reset([[NSCandidateListTouchBarItem alloc]
        initWithIdentifier:NSTouchBarItemIdentifierCandidateList]);
    auto* candidateListItem = candidateListTouchBarItem_.get();

    candidateListItem.delegate = self;
    candidateListItem.client = self;
    [self requestTextSuggestions];

    base::scoped_nsobject<NSTouchBar> scopedTouchBar([[NSTouchBar alloc] init]);
    auto* touchBar = scopedTouchBar.get();
    touchBar.customizationIdentifier = ui::GetTouchBarId(kWebContentTouchBarId);
    touchBar.templateItems = [NSSet setWithObject:candidateListTouchBarItem_];
    touchBar.defaultItemIdentifiers =
        @[ NSTouchBarItemIdentifierCandidateList ];
    return scopedTouchBar.autorelease();
  }

  return [super makeTouchBar];
}

@end

//
// Supporting application services
//

@interface RenderWidgetHostViewCocoa (
    NSServicesRequests)<NSServicesMenuRequestor>
@end

@implementation RenderWidgetHostViewCocoa (NSServicesRequests)

- (BOOL)writeSelectionToPasteboard:(NSPasteboard*)pboard types:(NSArray*)types {
  // NB: The NSServicesMenuRequestor protocol has not (as of 10.14) been
  // upgraded to request UTIs rather than obsolete PboardType constants. Handle
  // either for when it is upgraded.
  DCHECK([types containsObject:NSStringPboardType] ||
         [types containsObject:base::mac::CFToNSCast(kUTTypeUTF8PlainText)]);
  if (textSelectionRange_.is_empty())
    return NO;

  NSString* text = base::SysUTF16ToNSString([self selectedText]);
  return [pboard writeObjects:@[ text ]];
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard*)pboard {
  NSArray* objects =
      [pboard readObjectsForClasses:@[ [NSString class] ] options:0];
  if (![objects count])
    return NO;

  // If the user is currently using an IME, confirm the IME input,
  // and then insert the text from the service, the same as TextEdit and Safari.
  [self finishComposingText];
  [self insertText:[objects lastObject]];
  return YES;
}

// "-webkit-app-region: drag | no-drag" is implemented on Mac by excluding
// regions that are not draggable. (See ControlRegionView in
// native_app_window_cocoa.mm). This requires the render host view to be
// draggable by default.
- (BOOL)mouseDownCanMoveWindow {
  return YES;
}

@end
