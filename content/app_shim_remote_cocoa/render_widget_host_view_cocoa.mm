// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#import "content/app_shim_remote_cocoa/render_widget_host_view_cocoa.h"

#include <AppKit/AppKit.h>
#include <Carbon/Carbon.h>  // for <HIToolbox/Events.h>

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>

#import "base/apple/foundation_util.h"
#include "base/apple/owned_objc.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#import "base/mac/mac_util.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/input/web_input_event_builders_mac.h"
#include "components/remote_cocoa/app_shim/ns_view_ids.h"
#import "content/browser/cocoa/system_hotkey_helper_mac.h"
#import "content/browser/cocoa/system_hotkey_map.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#import "content/browser/renderer_host/render_widget_host_view_mac_editcommand_helper.h"
#include "content/common/features.h"
#include "content/public/browser/browser_accessibility_state.h"
#import "content/public/browser/render_widget_host_view_mac_delegate.h"
#include "content/public/common/content_features.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "ui/accessibility/accessibility_features.h"
#import "ui/accessibility/platform/browser_accessibility_cocoa.h"
#import "ui/accessibility/platform/browser_accessibility_mac.h"
#include "ui/accessibility/platform/browser_accessibility_manager_mac.h"
#import "ui/base/clipboard/clipboard_util_mac.h"
#import "ui/base/cocoa/appkit_utils.h"
#import "ui/base/cocoa/nsmenu_additions.h"
#import "ui/base/cocoa/nsmenuitem_additions.h"
#include "ui/base/cocoa/remote_accessibility_api.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/mac/coordinate_conversion.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using content::RenderWidgetHostViewMacEditCommandHelper;
using input::NativeWebKeyboardEvent;
using input::WebGestureEventBuilder;
using input::WebMouseEventBuilder;
using input::WebMouseWheelEventBuilder;
using input::WebTouchEventBuilder;
using remote_cocoa::RenderWidgetHostNSViewHostHelper;
using remote_cocoa::mojom::RenderWidgetHostNSViewHost;

namespace {

constexpr NSString* WebAutomaticQuoteSubstitutionEnabled =
    @"WebAutomaticQuoteSubstitutionEnabled";
constexpr NSString* const WebAutomaticDashSubstitutionEnabled =
    @"WebAutomaticDashSubstitutionEnabled";
constexpr NSString* const WebAutomaticTextReplacementEnabled =
    @"WebAutomaticTextReplacementEnabled";

constexpr NSString* const kGoogleJapaneseInputPrefix =
    @"com.google.inputmethod.Japanese.";

// A dummy RenderWidgetHostNSViewHostHelper implementation which no-ops all
// functions.
class DummyHostHelper : public RenderWidgetHostNSViewHostHelper {
 public:
  explicit DummyHostHelper() = default;

  DummyHostHelper(const DummyHostHelper&) = delete;
  DummyHostHelper& operator=(const DummyHostHelper&) = delete;

 private:
  // RenderWidgetHostNSViewHostHelper implementation.
  id GetAccessibilityElement() override { return nil; }
  id GetRootBrowserAccessibilityElement() override { return nil; }
  id GetFocusedBrowserAccessibilityElement() override { return nil; }
  void SetAccessibilityWindow(NSWindow* window) override {}
  void ForwardKeyboardEvent(const input::NativeWebKeyboardEvent& key_event,
                            const ui::LatencyInfo& latency_info) override {}
  void ForwardKeyboardEventWithCommands(
      const input::NativeWebKeyboardEvent& key_event,
      const ui::LatencyInfo& latency_info,
      const std::vector<blink::mojom::EditCommandPtr> commands) override {}
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
};

// Touch bar identifier.
NSString* const kWebContentTouchBarId = @"web-content";

constexpr int kWrapAroundDistance = 10000;

// Whether a keyboard event has been reserved by macOS.
BOOL EventIsReservedBySystem(NSEvent* event) {
  return content::GetSystemHotkeyMap()->IsEventReserved(event);
}

// Extract underline information from an attributed string. Inspired by
// `extractUnderlines` in
// https://github.com/WebKit/WebKit/blob/main/Source/WebKitLegacy/mac/WebView/WebHTMLView.mm
void ExtractUnderlines(NSAttributedString* string,
                       std::vector<ui::ImeTextSpan>* ime_text_spans) {
  NSUInteger length = string.length;
  NSUInteger i = 0;
  while (i < length) {
    NSRange range;
    NSDictionary* attrs = [string attributesAtIndex:i
                              longestEffectiveRange:&range
                                            inRange:NSMakeRange(i, length - i)];
    if (NSNumber* style_attr = attrs[NSUnderlineStyleAttributeName]) {
      SkColor color = SK_ColorBLACK;
      if (NSColor* color_attr = attrs[NSUnderlineColorAttributeName]) {
        color = skia::NSDeviceColorToSkColor(
            [color_attr colorUsingColorSpace:NSColorSpace.deviceRGBColorSpace]);
      }

      // `NSUnderlineStyle` is the combination of a type enum with a pattern
      // style in the higher bits. Fold anything more complicated than a single
      // unstyled underline down to "thick" rather than "thin".
      ui::ImeTextSpan::Thickness thickness =
          style_attr.intValue > NSUnderlineStyleSingle
              ? ui::ImeTextSpan::Thickness::kThick
              : ui::ImeTextSpan::Thickness::kThin;
      ui::ImeTextSpan ui_ime_text_span = ui::ImeTextSpan(
          ui::ImeTextSpan::Type::kComposition, range.location,
          NSMaxRange(range), thickness, ui::ImeTextSpan::UnderlineStyle::kSolid,
          SK_ColorTRANSPARENT);
      ui_ime_text_span.underline_color = color;
      ime_text_spans->push_back(ui_ime_text_span);
    }
    i = NSMaxRange(range);
  }
}

}  // namespace

// RenderWidgetHostViewCocoa ---------------------------------------------------

// Private methods:
@interface RenderWidgetHostViewCocoa ()

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
- (void)windowDidChangeScreenOrBackingProperties:(NSNotification*)notification;
- (void)windowChangedGlobalFrame:(NSNotification*)notification;
- (void)windowDidBecomeKey:(NSNotification*)notification;
- (void)windowDidResignKey:(NSNotification*)notification;
- (void)sendViewBoundsInWindowToHost;
- (void)requestTextSubstitutions;
- (void)requestTextSuggestions;
- (void)sendWindowFrameInScreenToHost;
- (bool)hostIsDisconnected;
- (void)invalidateTouchBar;

// NSCandidateListTouchBarItemDelegate implementation
- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem*)anItem
     endSelectingCandidateAtIndex:(NSInteger)index;
- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem*)anItem
    changedCandidateListVisibility:(BOOL)isVisible;
@end

@implementation RenderWidgetHostViewCocoa {
  // Dummy host and host helper that are always valid (see comments below about
  // _host).
  // These need to be declared before |_host| and |_hostHelper| so that it
  // gets destroyed last.
  mojo::Remote<remote_cocoa::mojom::RenderWidgetHostNSViewHost> _dummyHost;
  std::unique_ptr<remote_cocoa::RenderWidgetHostNSViewHostHelper>
      _dummyHostHelper;

  // The communications channel to the RenderWidgetHostViewMac. This pointer is
  // always valid. When the original host disconnects, |_host| is changed to
  // point to |_dummyHost|, to avoid having to preface every dereference with
  // a nullptr check.
  raw_ptr<remote_cocoa::mojom::RenderWidgetHostNSViewHost> _host;

  // A separate host interface for the parts of the interface to
  // RenderWidgetHostViewMac that cannot or should not be forwarded over mojo.
  // This includes events (where the extra translation is unnecessary or loses
  // information) and access to accessibility structures (only present in the
  // browser process).
  raw_ptr<remote_cocoa::RenderWidgetHostNSViewHostHelper> _hostHelper;

  // This ivar is the cocoa delegate of the NSResponder.
  NSObject<RenderWidgetHostViewMacDelegate>* __strong _responderDelegate;
  BOOL _canBeKeyView;
  BOOL _closeOnDeactivate;
  std::unique_ptr<content::RenderWidgetHostViewMacEditCommandHelper>
      _editCommandHelper;

  // Is YES if there was a mouse-down as yet unbalanced with a mouse-up.
  BOOL _hasOpenMouseDown;

  // The cursor for the page. This is passed up from the renderer.
  NSCursor* __strong _currentCursor;

  // Is YES if the cursor is hidden by key events.
  BOOL _cursorHidden;

  // Controlled by setShowingContextMenu.
  BOOL _showingContextMenu;

  // Set during -setFrame to avoid spamming host_ with origin and size
  // changes.
  BOOL _inSetFrame;

  // Variables used by our implementation of the NSTextInput protocol.
  // An input method of Mac calls the methods of this protocol not only to
  // notify an application of its status, but also to retrieve the status of
  // the application. That is, an application cannot control an input method
  // directly.
  // This object keeps the status of a composition of the renderer and returns
  // it when an input method asks for it.
  // We need to implement Objective-C methods for the NSTextInput protocol. On
  // the other hand, we need to implement a C++ method for an IPC-message
  // handler which receives input-method events from the renderer.

  // keyCode value of an NSEvent. This field has a value while we're handling
  // a key down event.
  std::optional<unsigned short> _currentKeyDownCode;

  // Indicates if a reconversion (which means a piece of committed text becomes
  // part of the composition again) is triggered in Japanese IME when Live
  // Conversion is on.
  BOOL _isReconversionTriggered;

  // Indicates if there is any marked text.
  BOOL _hasMarkedText;

  // Indicates if unmarkText is called or not when handling a keyboard
  // event.
  BOOL _unmarkTextCalled;

  // The range of current marked text inside the whole content of the DOM node
  // being edited.
  // TODO(suzhe): This is currently a fake value, as we do not support accessing
  // the whole content yet.
  NSRange _markedRange;

  // The text selection, cached from the RenderWidgetHostView.
  // |_availableText| contains the selected text and is a substring of the
  // full string in the renderer.
  std::u16string _availableText;
  size_t _availableTextOffset;
  gfx::Range _textSelectionRange;

  // The composition range, cached from the RenderWidgetHostView. This is only
  // ever updated from the renderer (unlike |_markedRange|, which sometimes but
  // not always coincides with |_compositionRange|).
  gfx::Range _compositionRange;

  // Text to be inserted which was generated by handling a key down event.
  std::u16string _textToBeInserted;

  // Marked text which was generated by handling a key down event.
  std::u16string _markedText;

  // Selected range of |markedText_|.
  NSRange _markedTextSelectedRange;

  // Underline information of the |markedText_|.
  std::vector<ui::ImeTextSpan> _imeTextSpans;

  // Replacement range information received from |setMarkedText:|.
  gfx::Range _setMarkedTextReplacementRange;

  // Indicates if doCommandBySelector method receives any edit command when
  // handling a key down event.
  BOOL _hasEditCommands;

  // Contains edit commands received by the -doCommandBySelector: method when
  // handling a key down event, not including inserting commands, eg. insertTab,
  // etc.
  std::vector<blink::mojom::EditCommandPtr> _editCommands;

  // Whether the previous mouse event was ignored due to hitTest check.
  BOOL _mouseEventWasIgnored;

  // Event monitor for scroll wheel end event.
  id __strong _endWheelMonitor;

  // This is used to indicate if a stylus is currently in the proximity of the
  // tablet.
  bool _isStylusEnteringProximity;
  blink::WebPointerProperties::PointerType _pointerType;

  // The set of key codes from key down events that we haven't seen the matching
  // key up events yet.
  // Used for filtering out non-matching NSEventTypeKeyUp events.
  std::set<unsigned short> _unmatchedKeyDownCodes;

  // The filter used to guide touch events towards a horizontal or vertical
  // orientation.
  content::MouseWheelRailsFilterMac _mouseWheelFilter;

  bool _mouseLocked;
  bool _mouseLockUnacceleratedMovement;
  gfx::PointF _lastMouseScreenPosition;

  // The parent accessibility element. This is set only in the browser process.
  id __strong _accessibilityParent;

  uint64_t _popupParentNSViewId;

  bool _keyboardLockActive;
  std::optional<base::flat_set<ui::DomCode>> _lockedKeys;

  NSCandidateListTouchBarItem* __strong _candidateListTouchBarItem;
  NSInteger _textSuggestionsSequenceNumber;
  BOOL _shouldRequestTextSubstitutions;
  BOOL _substitutionWasApplied;
  bool _sonomaAccessibilityRefinementsAreActive;
}

@synthesize markedRange = _markedRange;
@synthesize textInputType = _textInputType;
@synthesize textInputFlags = _textInputFlags;
@synthesize spellCheckerForTesting = _spellCheckerForTesting;

+ (void)initialize {
  RenderWidgetHostViewMacEditCommandHelper::AddEditingSelectorsToClass(self);
}

- (instancetype)initWithHost:(RenderWidgetHostNSViewHost*)host
              withHostHelper:(RenderWidgetHostNSViewHostHelper*)hostHelper {
  self = [super initWithFrame:NSZeroRect tracking:YES];
  if (self) {
    // Enable trackpad touches ("direct" touches are touchbar touches).
    self.allowedTouchTypes |= NSTouchTypeMaskIndirect;
    _editCommandHelper =
        std::make_unique<RenderWidgetHostViewMacEditCommandHelper>();

    _host = host;
    _hostHelper = hostHelper;
    _canBeKeyView = YES;
    _isStylusEnteringProximity = false;
    _keyboardLockActive = false;
    _textInputType = ui::TEXT_INPUT_TYPE_NONE;
    _sonomaAccessibilityRefinementsAreActive =
        base::mac::MacOSVersion() >= 14'00'00 &&
        base::FeatureList::IsEnabled(
            features::kSonomaAccessibilityActivationRefinements);
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
}

- (void)sendViewBoundsInWindowToHost {
  TRACE_EVENT0("browser",
               "RenderWidgetHostViewCocoa::sendViewBoundsInWindowToHost");
  if (_inSetFrame)
    return;

  NSRect viewBoundsInView = [self bounds];
  NSWindow* enclosingWindow = [self window];
  if (!enclosingWindow) {
    _host->OnBoundsInWindowChanged(gfx::Rect(viewBoundsInView), false);
    return;
  }

  NSRect viewBoundsInWindow = [self convertRect:viewBoundsInView toView:nil];
  gfx::Rect gfxViewBoundsInWindow(viewBoundsInWindow);
  gfxViewBoundsInWindow.set_y(NSHeight([enclosingWindow frame]) -
                              NSMaxY(viewBoundsInWindow));
  _host->OnBoundsInWindowChanged(gfxViewBoundsInWindow, true);
}

- (NSSpellChecker*)spellChecker {
  if (_spellCheckerForTesting)
    return _spellCheckerForTesting;
  return NSSpellChecker.sharedSpellChecker;
}

- (void)requestTextSubstitutions {
  NSTextCheckingType textCheckingTypes =
      self.allowedTextCheckingTypes & self.enabledTextCheckingTypes;
  if (!textCheckingTypes)
    return;

  NSString* availableText = base::SysUTF16ToNSString(_availableText);

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

  NSUInteger cursorLocation = _textSelectionRange.start();
  NSTextCheckingResult* candidateResult;
  for (NSTextCheckingResult* result in textCheckingResults) {
    NSTextCheckingResult* adjustedResult =
        [result resultByAdjustingRangesWithOffset:_availableTextOffset];
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
    candidateResult = adjustedResult;
  }
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
      NSMakeRange(_availableTextOffset, _availableText.length());

  if (NSMaxRange(correction.range) > NSMaxRange(availableTextRange))
    return;

  NSAttributedString* attString = [[NSAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(_availableText)];
  NSRange trailingRange = NSMakeRange(
      NSMaxRange(correction.range),
      NSMaxRange(availableTextRange) - NSMaxRange(correction.range));

  if (trailingRange.length > 0 &&
      trailingRange.location < NSMaxRange(availableTextRange)) {
    NSRange trailingRangeInAvailableText = NSMakeRange(
        trailingRange.location - _availableTextOffset, trailingRange.length);
    NSString* trailingString =
        [attString.string substringWithRange:trailingRangeInAvailableText];
    if ([self.spellChecker preventsAutocorrectionBeforeString:trailingString
                                                     language:nil])
      return;
    if ([attString doubleClickAtIndex:trailingRangeInAvailableText.location]
            .location < trailingRangeInAvailableText.location)
      return;
  }

  _substitutionWasApplied = YES;
  [self insertText:acceptedString replacementRange:correction.range];
}

- (void)requestTextSuggestions {
  auto* touchBarItem = _candidateListTouchBarItem;
  if (!touchBarItem)
    return;
  [touchBarItem
      updateWithInsertionPointVisibility:_textSelectionRange.is_empty()];
  if (_textInputType == ui::TEXT_INPUT_TYPE_PASSWORD)
    return;
  if (!touchBarItem.candidateListVisible)
    return;
  if (!_textSelectionRange.IsValid() ||
      _availableTextOffset > _textSelectionRange.GetMin())
    return;

  NSRange selectionRange = _textSelectionRange.ToNSRange();
  NSString* selectionText = base::SysUTF16ToNSString(_availableText);
  selectionRange.location -= _availableTextOffset;
  if (NSMaxRange(selectionRange) > selectionText.length)
    return;

  // TODO: Fetch the spell document tag from the renderer (or equivalent).
  _textSuggestionsSequenceNumber = [self.spellChecker
      requestCandidatesForSelectedRange:selectionRange
                               inString:selectionText
                                  types:NSTextCheckingAllSystemTypes
                                options:nil
                 inSpellDocumentWithTag:0
                      completionHandler:^(
                          NSInteger sequenceNumber,
                          NSArray<NSTextCheckingResult*>* candidates) {
                        dispatch_async(dispatch_get_main_queue(), ^{
                          if (sequenceNumber !=
                              self->_textSuggestionsSequenceNumber) {
                            return;
                          }
                          [touchBarItem setCandidates:candidates
                                     forSelectedRange:selectionRange
                                             inString:selectionText];
                        });
                      }];
}

- (NSTextCheckingType)allowedTextCheckingTypes {
  if (_textInputType == ui::TEXT_INPUT_TYPE_NONE)
    return 0;
  if (_textInputType == ui::TEXT_INPUT_TYPE_PASSWORD)
    return 0;
  if (_textInputFlags & blink::kWebTextInputFlagAutocorrectOff)
    return 0;
  NSTextCheckingType checkingTypes = NSTextCheckingTypeReplacement;
  if (!(_textInputFlags & blink::kWebTextInputFlagSpellcheckOff))
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

- (bool)canTransformText {
  if (_textInputType == ui::TEXT_INPUT_TYPE_NONE)
    return NO;
  if (_textInputType == ui::TEXT_INPUT_TYPE_PASSWORD)
    return NO;

  return YES;
}

- (void)uppercaseWord:(id)sender {
  NSString *text = base::SysUTF16ToNSString([self selectedText]);
  if (!text)
    return;

  [self insertText:text.localizedUppercaseString
      replacementRange:_textSelectionRange.ToNSRange()];
}

- (void)lowercaseWord:(id)sender {
  NSString *text = base::SysUTF16ToNSString([self selectedText]);
  if (!text)
    return;

  [self insertText:text.localizedLowercaseString
      replacementRange:_textSelectionRange.ToNSRange()];
}

- (void)capitalizeWord:(id)sender {
  NSString *text = base::SysUTF16ToNSString([self selectedText]);
  if (!text)
    return;

  [self insertText:text.localizedCapitalizedString
      replacementRange:_textSelectionRange.ToNSRange()];
}

- (void)setTextSelectionText:(std::u16string)text
                      offset:(size_t)offset
                       range:(gfx::Range)range {
  _availableText = text;
  _availableTextOffset = offset;
  _textSelectionRange = range;
  _substitutionWasApplied = NO;
  [NSSpellChecker.sharedSpellChecker dismissCorrectionIndicatorForView:self];
  if (_shouldRequestTextSubstitutions && !_substitutionWasApplied &&
      _textSelectionRange.is_empty()) {
    _shouldRequestTextSubstitutions = NO;
    [self requestTextSubstitutions];
  }
  [self requestTextSuggestions];
}

- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem*)anItem
     endSelectingCandidateAtIndex:(NSInteger)index {
  if (index == NSNotFound)
    return;
  NSTextCheckingResult* selectedResult = anItem.candidates[index];
  NSRange replacementRange = selectedResult.range;
  replacementRange.location += _availableTextOffset;
  [self insertText:selectedResult.replacementString
      replacementRange:replacementRange];
}

- (void)candidateListTouchBarItem:(NSCandidateListTouchBarItem*)anItem
    changedCandidateListVisibility:(BOOL)isVisible {
  [self requestTextSuggestions];
}

- (void)setTextInputType:(ui::TextInputType)textInputType {
  if (_textInputType == textInputType)
    return;
  _textInputType = textInputType;

  [self invalidateTouchBar];
}

- (std::u16string)selectedText {
  gfx::Range textRange(_availableTextOffset,
                       _availableTextOffset + _availableText.size());
  gfx::Range intersectionRange = textRange.Intersect(_textSelectionRange);
  if (intersectionRange.is_empty())
    return std::u16string();
  return _availableText.substr(intersectionRange.start() - _availableTextOffset,
                               intersectionRange.length());
}

- (void)setCompositionRange:(gfx::Range)range {
  _compositionRange = range;
}

- (void)sendWindowFrameInScreenToHost {
  TRACE_EVENT0("browser",
               "RenderWidgetHostViewCocoa::sendWindowFrameInScreenToHost");
  NSWindow* enclosingWindow = [self window];
  if (!enclosingWindow)
    return;
  _host->OnWindowFrameInScreenChanged(
      gfx::ScreenRectFromNSRect([enclosingWindow frame]));
}

- (void)setResponderDelegate:
    (NSObject<RenderWidgetHostViewMacDelegate>*)delegate {
  DCHECK(!_responderDelegate);
  _responderDelegate = delegate;
}

- (void)resetCursorRects {
  if (_currentCursor)
    [self addCursorRect:[self visibleRect] cursor:_currentCursor];
}

- (void)processedWheelEvent:(const blink::WebMouseWheelEvent&)event
                   consumed:(BOOL)consumed {
  [_responderDelegate rendererHandledWheelEvent:event consumed:consumed];
}

- (void)processedGestureScrollEvent:(const blink::WebGestureEvent&)event
                           consumed:(BOOL)consumed {
  [_responderDelegate rendererHandledGestureScrollEvent:event
                                               consumed:consumed];
}

- (void)processedOverscroll:(const ui::DidOverscrollParams&)params {
  [_responderDelegate rendererHandledOverscrollEvent:params];
}

- (BOOL)respondsToSelector:(SEL)selector {
  // Trickiness: this doesn't mean "does this object's superclass respond to
  // this selector" but rather "does the -respondsToSelector impl from the
  // superclass say that this class responds to the selector".
  if ([super respondsToSelector:selector])
    return YES;

  if (_responderDelegate)
    return [_responderDelegate respondsToSelector:selector];

  return NO;
}

- (id)forwardingTargetForSelector:(SEL)selector {
  if ([_responderDelegate respondsToSelector:selector])
    return _responderDelegate;

  return [super forwardingTargetForSelector:selector];
}

- (void)setCanBeKeyView:(BOOL)can {
  _canBeKeyView = can;
}

- (AcceptMouseEventsOption)acceptsMouseEventsOption {
  // Always-on-top windows, e.g picture-in-picture window, accepts all mouse
  // events even if the window or the application is inactive.
  if ([[self window] level] > NSNormalWindowLevel) {
    return kAcceptMouseEventsAlways;
  }

  // By default, only active window accepts mouse events. The embedder may
  // override this to mimic the hover and click behavior of native UIs.
  if (_responderDelegate && [_responderDelegate respondsToSelector:@selector
                                                (acceptsMouseEventsOption)]) {
    return [_responderDelegate acceptsMouseEventsOption];
  }

  // By default, only active window accepts mouse events.
  return kAcceptMouseEventsInActiveWindow;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)theEvent {
  // Enable "click-through" if mouse clicks are accepted in inactive windows
  return [self acceptsMouseEventsOption] > kAcceptMouseEventsInActiveWindow;
}

- (void)setCloseOnDeactivate:(BOOL)b {
  _closeOnDeactivate = b;
}

- (void)setHostDisconnected {
  // Set the host to be an abandoned message pipe, and set the hostHelper
  // to forward messages to that host.
  std::ignore = _dummyHost.BindNewPipeAndPassReceiver();
  _dummyHostHelper = std::make_unique<DummyHostHelper>();
  _host = _dummyHost.get();
  _hostHelper = _dummyHostHelper.get();

  // |responderDelegate_| may attempt to access the RenderWidgetHostViewMac
  // through its internal pointers, so detach it here.
  // TODO(ccameron): Force |responderDelegate_| to use the |host_| as well,
  // and the viewGone method to hostGone.
  if (_responderDelegate &&
      [_responderDelegate respondsToSelector:@selector(viewGone:)])
    [_responderDelegate viewGone:self];
  _responderDelegate = nil;
}

- (bool)hostIsDisconnected {
  return _host == (_dummyHost.is_bound() ? _dummyHost.get() : nullptr);
}

- (void)setShowingContextMenu:(BOOL)showing {
  _showingContextMenu = showing;

  // Create a fake mouse event to inform the render widget that the mouse
  // left or entered.
  NSWindow* window = [self window];
  int windowNumber = window ? [window windowNumber] : -1;

  // TODO(asvitkine): If the location outside of the event stream doesn't
  // correspond to the current event (due to delayed event processing), then
  // this may result in a cursor flicker if there are later mouse move events
  // in the pipeline. Find a way to use the mouse location from the event that
  // dismissed the context menu.
  NSPoint location = [window mouseLocationOutsideOfEventStream];
  NSTimeInterval eventTime = [[NSApp currentEvent] timestamp];
  NSEvent* event = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
                                      location:location
                                 modifierFlags:0
                                     timestamp:eventTime
                                  windowNumber:windowNumber
                                       context:nil
                                   eventNumber:0
                                    clickCount:0
                                      pressure:0];
  WebMouseEvent webEvent = WebMouseEventBuilder::Build(event, self);
  webEvent.SetModifiers(webEvent.GetModifiers() |
                        WebInputEvent::kRelativeMotionEvent);
  // TODO(crbug.com/40276040): We shouldn't be posting events with null
  // timestamps. However `MessagePumpNSApplication::DoQuit` usage of
  // `otherEventWithType` seems to truncate `NSTimeInterval` to seconds. Which
  // could lead to those generated events be in the past, compared to ones
  // created directly in later parts of the pipeline or in tests.
  if (webEvent.TimeStamp().is_null()) {
    webEvent.SetTimeStamp(ui::EventTimeForNow());
  }
  _hostHelper->ForwardMouseEvent(webEvent);
}

- (BOOL)shouldIgnoreMouseEvent:(NSEvent*)theEvent {
  NSWindow* window = [self window];
  if ([theEvent type] == NSEventTypeMouseMoved) {
    bool inActiveWindow = [window isMainWindow] || [window isKeyWindow];
    bool inActiveApp = [[NSApplication sharedApplication] isActive];
    AcceptMouseEventsOption option = [self acceptsMouseEventsOption];
    // If events are accepted only in active window but this window is inactive,
    // ignore this event. This is the default behavior.
    if (option == kAcceptMouseEventsInActiveWindow && !inActiveWindow) {
      return YES;
    }
    // If events are accepted in active app but the app in active, ignore this
    // event. This only happens if the content embedder overrides the default
    // behavior.
    if (option == kAcceptMouseEventsInActiveApp && !inActiveApp) {
      return YES;
    }
  }

  NSView* contentView = [window contentView];
  NSView* view = [contentView hitTest:[theEvent locationInWindow]];
  // Traverse the superview hierarchy as the hitTest will return the frontmost
  // view, such as an NSTextView, while nonWebContentView may be specified by
  // its parent view.
  BOOL hitSelf = NO;
  while (view) {
    if (view == self)
      hitSelf = YES;
    if ([view isKindOfClass:[self class]] && ![view isEqual:self] &&
        !_hasOpenMouseDown) {
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
  return !hitSelf && !_hasOpenMouseDown;
}

- (void)mouseEvent:(NSEvent*)theEvent {
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::mouseEvent");
  if (_responderDelegate &&
      [_responderDelegate respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [_responderDelegate handleEvent:theEvent];
    if (handled)
      return;
  }
  if (ui::PlatformEventSource::ShouldIgnoreNativePlatformEvents())
    return;

  // Set the pointer type when we are receiving a NSEventTypeMouseEntered event
  // and the following NSEventTypeMouseExited event should have the same pointer
  // type. For NSEventTypeMouseExited and NSEventTypeMouseEntered events, they
  // do not have a subtype. We decide their pointer types by checking if we
  // received a NSEventTypeTabletProximity event.
  NSEventType type = [theEvent type];
  if (type == NSEventTypeMouseEntered || type == NSEventTypeMouseExited) {
    _pointerType = _isStylusEnteringProximity
                       ? _pointerType
                       : blink::WebPointerProperties::PointerType::kMouse;
  } else {
    NSEventSubtype subtype = [theEvent subtype];
    // For other mouse events and touchpad events, the pointer type is mouse.
    if (subtype != NSEventSubtypeTabletPoint &&
        subtype != NSEventSubtypeTabletProximity) {
      _pointerType = blink::WebPointerProperties::PointerType::kMouse;
    } else if (subtype == NSEventSubtypeTabletProximity) {
      _isStylusEnteringProximity = [theEvent isEnteringProximity];
      NSPointingDeviceType deviceType = [theEvent pointingDeviceType];
      // For all tablet events, the pointer type will be pen or eraser.
      _pointerType = deviceType == NSPointingDeviceTypeEraser
                         ? blink::WebPointerProperties::PointerType::kEraser
                         : blink::WebPointerProperties::PointerType::kPen;
    }
  }

  // Because |updateCursor:| changes the current cursor, we have to reset it to
  // the default cursor on mouse exit.
  if (type == NSEventTypeMouseExited)
    [[NSCursor arrowCursor] set];

  if ([self shouldIgnoreMouseEvent:theEvent]) {
    // If this is the first such event, send a mouse exit to the host view.
    if (!_mouseEventWasIgnored && !self.hidden) {
      WebMouseEvent exitEvent =
          WebMouseEventBuilder::Build(theEvent, self, _pointerType);
      exitEvent.SetType(WebInputEvent::Type::kMouseLeave);
      exitEvent.button = WebMouseEvent::Button::kNoButton;
      _hostHelper->ForwardMouseEvent(exitEvent);
    }
    _mouseEventWasIgnored = YES;
    [self updateCursor:nil];
    return;
  }

  if (_mouseEventWasIgnored) {
    // If this is the first mouse event after a previous event that was ignored
    // due to the hitTest, send a mouse enter event to the host view.
    WebMouseEvent enterEvent =
        WebMouseEventBuilder::Build(theEvent, self, _pointerType);
    enterEvent.SetType(WebInputEvent::Type::kMouseMove);
    enterEvent.button = WebMouseEvent::Button::kNoButton;
    _hostHelper->RouteOrProcessMouseEvent(enterEvent);
  }
  _mouseEventWasIgnored = NO;

  // Don't cancel child popups; killing them on a mouse click would prevent the
  // user from positioning the insertion point in the text field spawning the
  // popup. A click outside the text field would cause the text field to drop
  // the focus, and then EditorHostImpl::textFieldDidEndEditing() would cancel
  // the popup anyway, so we're OK.
  if (type == NSEventTypeLeftMouseDown)
    _hasOpenMouseDown = YES;
  else if (type == NSEventTypeLeftMouseUp)
    _hasOpenMouseDown = NO;

  // TODO(suzhe): We should send mouse events to the input method first if it
  // wants to handle them. But it won't work without implementing method
  // - (NSUInteger)characterIndexForPoint:.
  // See: http://code.google.com/p/chromium/issues/detail?id=47141
  // Instead of sending mouse events to the input method first, we now just
  // simply confirm all ongoing composition here.
  if (type == NSEventTypeLeftMouseDown || type == NSEventTypeRightMouseDown ||
      type == NSEventTypeOtherMouseDown) {
    [self finishComposingText];
  }

  if (type == NSEventTypeMouseMoved)
    _cursorHidden = NO;

  bool unacceleratedMovement = _mouseLocked && _mouseLockUnacceleratedMovement;
  WebMouseEvent event = WebMouseEventBuilder::Build(
      theEvent, self, _pointerType, unacceleratedMovement);

  if (_mouseLocked) {
    // When mouse is locked, we keep increasing |_lastMouseScreenPosition|
    // by movement_x/y so that we can still use PositionInScreen to calculate
    // movements in blink. We need to keep |_lastMouseScreenPosition| from
    // getting too large because it will lose some precision. So whenever it
    // exceed the |kWrapAroundDistance|, we start again from the current
    // mouse position (locked position), and also send a synthesized event to
    // update the blink-side status.
    if (std::abs(_lastMouseScreenPosition.x()) > kWrapAroundDistance ||
        std::abs(_lastMouseScreenPosition.y()) > kWrapAroundDistance) {
      NSWindow* window = [self window];
      NSPoint location = [window mouseLocationOutsideOfEventStream];
      int windowNumber = window ? [window windowNumber] : -1;
      NSEvent* nsevent = [NSEvent mouseEventWithType:NSEventTypeMouseMoved
                                            location:location
                                       modifierFlags:[theEvent modifierFlags]
                                           timestamp:[theEvent timestamp]
                                        windowNumber:windowNumber
                                             context:nil
                                         eventNumber:0
                                          clickCount:[theEvent clickCount]
                                            pressure:0];
      WebMouseEvent wrapAroundEvent =
          WebMouseEventBuilder::Build(nsevent, self, _pointerType);
      _lastMouseScreenPosition = wrapAroundEvent.PositionInScreen();
      wrapAroundEvent.SetModifiers(
          event.GetModifiers() |
          blink::WebInputEvent::Modifiers::kRelativeMotionEvent);
      _hostHelper->RouteOrProcessMouseEvent(wrapAroundEvent);
    }
    event.SetPositionInScreen(
        _lastMouseScreenPosition +
        gfx::Vector2dF(event.movement_x, event.movement_y));
  }

  _lastMouseScreenPosition = event.PositionInScreen();
  _hostHelper->RouteOrProcessMouseEvent(event);
}

- (void)tabletEvent:(NSEvent*)theEvent {
  if ([theEvent type] == NSEventTypeTabletProximity) {
    _isStylusEnteringProximity = [theEvent isEnteringProximity];
    NSPointingDeviceType deviceType = [theEvent pointingDeviceType];
    // For all tablet events, the pointer type will be pen or eraser.
    _pointerType = deviceType == NSPointingDeviceTypeEraser
                       ? blink::WebPointerProperties::PointerType::kEraser
                       : blink::WebPointerProperties::PointerType::kPen;
  }
}

- (void)lockKeyboard:(std::optional<base::flat_set<ui::DomCode>>)keysToLock {
  // TODO(joedow): Integrate System-level keyboard hook into this method.
  _lockedKeys = std::move(keysToLock);
  _keyboardLockActive = true;
}

- (void)unlockKeyboard {
  _keyboardLockActive = false;
  _lockedKeys.reset();
}

- (void)setCursorLocked:(BOOL)locked {
  _mouseLocked = locked;
  if (_mouseLocked) {
    CGAssociateMouseAndMouseCursorPosition(NO);
    [NSCursor hide];
  } else {
    // Unlock position of mouse cursor and unhide it.
    CGAssociateMouseAndMouseCursorPosition(YES);
    [NSCursor unhide];
  }
}

- (void)setCursorLockedUnacceleratedMovement:(BOOL)unaccelerated {
  _mouseLockUnacceleratedMovement = unaccelerated;
}

// CommandDispatcherTarget implementation:
- (BOOL)isKeyLocked:(NSEvent*)event {
  int keyCode = [event keyCode];
  // Note: We do not want to treat the ESC key as locked as that key is used
  // to exit fullscreen and we don't want to prevent them from exiting.
  ui::DomCode domCode = ui::KeycodeConverter::NativeKeycodeToDomCode(keyCode);
  return _keyboardLockActive && domCode != ui::DomCode::ESCAPE &&
         (!_lockedKeys || base::Contains(_lockedKeys.value(), domCode));
}

- (BOOL)performKeyEquivalent:(NSEvent*)theEvent {
  // TODO(bokan): Tracing added temporarily to diagnose crbug.com/1039833.
  TRACE_EVENT0("browser", "RenderWidgetHostViewCocoa::performKeyEquivalent");
  // |performKeyEquivalent:| is sent to all views of a window, not only down the
  // responder chain (cf. "Handling Key Equivalents" in
  // http://developer.apple.com/mac/library/documentation/Cocoa/Conceptual/EventOverview/HandlingKeyEvents/HandlingKeyEvents.html
  // ). A |performKeyEquivalent:| may also bubble up from a dialog child window
  // to perform browser commands such as switching tabs. We only want to handle
  // key equivalents if we're first responder in the keyWindow.
  if (![[self window] isKeyWindow] || [[self window] firstResponder] != self) {
    TRACE_EVENT_INSTANT0("browser", "NotKeyWindow", TRACE_EVENT_SCOPE_THREAD);
    return NO;
  }

  // If the event is reserved by the system, do not pass it to web content.
  // If the user changes the system hotkey mapping after Chrome has been
  // launched, it is possible that a formerly reserved system hotkey is no
  // longer reserved. The hotkey would have skipped the renderer, but would
  // also have not been handled by the system. If this is the case, immediately
  // return.
  // TODO(erikchen): SystemHotkeyHelperMac should use the File System Events
  // api to monitor changes to system hotkeys. This logic will have to be
  // updated.
  // http://crbug.com/383558.
  if (EventIsReservedBySystem(theEvent)) {
    return NO;
  }

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
  if (_responderDelegate &&
      [_responderDelegate respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [_responderDelegate handleEvent:theEvent];
    if (handled)
      return kEventHandled;
  }

  [self keyEvent:theEvent wasKeyEquivalent:NO];
  return kEventHandled;
}

- (void)keyEvent:(NSEvent*)theEvent wasKeyEquivalent:(BOOL)equiv {
  TRACE_EVENT1("browser", "RenderWidgetHostViewCocoa::keyEvent", "WindowNum",
               [[self window] windowNumber]);
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

  if (eventType == NSEventTypeFlagsChanged) {
    // Ignore NSEventTypeFlagsChanged events from the NumLock and Fn keys as
    // Safari does in -[WebHTMLView flagsChanged:] (of "WebHTMLView.mm").
    // Also ignore unsupported |keyCode| (255) generated by Convert, NonConvert
    // and KanaMode from JIS PC keyboard.
    if (!keyCode || keyCode == 10 || keyCode == 63 || keyCode == 255)
      return;
  }

  // Don't cancel child popups; the key events are probably what's triggering
  // the popup in the first place.

  NativeWebKeyboardEvent event((base::apple::OwnedNSEvent(theEvent)));
  ui::LatencyInfo latencyInfo;
  latencyInfo.AddLatencyNumber(ui::INPUT_EVENT_LATENCY_UI_COMPONENT);

  // If KeyboardLock has been requested for this keyCode, then mark the event
  // so it skips the pre-handler and is delivered straight to the website.
  if ([self isKeyLocked:theEvent])
    event.skip_if_unhandled = true;

  // Do not forward key up events unless preceded by a matching key down,
  // otherwise we might get an event from releasing the return key in the
  // omnibox (https://crbug.com/338736) or from closing another window
  // (https://crbug.com/155492).
  if (eventType == NSEventTypeKeyUp) {
    auto numErased = _unmatchedKeyDownCodes.erase(keyCode);
    if (numErased < 1)
      return;
  }

  // Tell the host that we are beginning a keyboard event. This ensures that
  // all event and Ime messages target the same RenderWidgetHost throughout this
  // function call.
  _host->BeginKeyboardEvent();

  bool shouldAutohideCursor = _textInputType != ui::TEXT_INPUT_TYPE_NONE &&
                              eventType == NSEventTypeKeyDown &&
                              !(modifierFlags & NSEventModifierFlagCommand);

  // We only handle key down events and just simply forward other events.
  if (eventType != NSEventTypeKeyDown) {
    _hostHelper->ForwardKeyboardEvent(event, latencyInfo);

    // Possibly autohide the cursor.
    if (shouldAutohideCursor) {
      [NSCursor setHiddenUntilMouseMoves:YES];
      _cursorHidden = YES;
    }

    _host->EndKeyboardEvent();
    return;
  }

  _unmatchedKeyDownCodes.insert(keyCode);

  NS_VALID_UNTIL_END_OF_SCOPE RenderWidgetHostViewCocoa* keepSelfAlive = self;

  // Records the current marked text state, so that we can know if the marked
  // text was deleted or not after handling the key down event.
  BOOL oldHasMarkedText = _hasMarkedText;

  // This method should not be called recursively.
  DCHECK(![self isHandlingKeyDown]);

  // Tells insertText: and doCommandBySelector: that we are handling a key
  // down event.
  _currentKeyDownCode = keyCode;

  // This is to handle an edge case for the "Live Conversion" feature in default
  // Japanese IME. When the feature is on, pressing the left key at the
  // composition boundary will reconvert previously committed text. The text
  // input system will call setMarkedText multiple times to end the current
  // composition and start a new one. In this case we'll need to call
  // ImeSetComposition in setMarkedText instead of here in keyEvent:, otherwise,
  // only the last setMarkedText will be processed.
  ui::DomCode domCode = ui::KeycodeConverter::NativeKeycodeToDomCode(keyCode);
  _isReconversionTriggered =
      base::FeatureList::IsEnabled(features::kMacImeLiveConversionFix) &&
      _hasMarkedText && domCode == ui::DomCode::ARROW_LEFT &&
      _markedTextSelectedRange.location == 0 && _markedRange.location != 0 &&
      _markedRange.location != NSNotFound;

  // These variables might be set when handling the keyboard event.
  // Clear them here so that we can know whether they have changed afterwards.
  _textToBeInserted.clear();
  _markedText.clear();
  _markedTextSelectedRange = NSMakeRange(NSNotFound, 0);
  _imeTextSpans.clear();
  _setMarkedTextReplacementRange = gfx::Range::InvalidRange();
  _unmarkTextCalled = NO;
  _hasEditCommands = NO;
  _editCommands.clear();

  // Since Mac Eisu Kana keys cannot be handled by interpretKeyEvents to enable/
  // disable an IME, we need to pass the event to processInputKeyBindings.
  // processInputKeyBindings is available at least on 10.11-11.0.
  if (keyCode == kVK_JIS_Eisu || keyCode == kVK_JIS_Kana) {
    if ([NSTextInputContext
            respondsToSelector:@selector(processInputKeyBindings:)]) {
      [NSTextInputContext performSelector:@selector(processInputKeyBindings:)
                               withObject:theEvent];
    }
  } else {
    // Previously, we would just send the event, shortcut or no, to
    // -interpretKeyEvents: below. The problem is that certain keyboard
    // shortcuts now use the Function/World key and in those cases the
    // corresponding shortcut fires but the shortcut event also gets processed
    // as if it were a key press. As a result, with the insertion point
    // in a web text box, after typing something like "Function e" to invoke
    // the Emoji palette, we would wind up in -insertText:replacementRange:.
    // The logic there ([self isHandlingKeyDown] && replacementRange.location ==
    // NSNotFound) would create an invisible placeholder for the character. This
    // invisible placeholder would cause macOS to position the palette at the
    // upper-left corner of the webcontents instead of at the insertion point.
    //
    // For these Function/World events, we want the AppKit to process them
    // as it usually would (i.e. via performKeyEquivalent:). It would be simpler
    // if we could pass all of these keyboard shortcut events along to
    // performKeyEquivalent:, however web pages are allowed to hijack keyboard
    // shortcuts and apparently that's done through interpretKeyEvents:.
    // Applications are not allowed to create these system events (you get a
    // warning if you try and the Function event modifier flag doesn't stick)
    // so it's OK not to allow web pages to do so either.
    //
    // If the event's not a shortcut, send it along to the input method first.
    // We can then decide what should be done according to the input method's
    // feedback.
    bool isASystemShortcutEvent = false;
    if (equiv) {
      bool isAKeyboardShortcutEvent =
          [[NSApp mainMenu] cr_menuItemForKeyEquivalentEvent:theEvent] != nil;

      const NSEventModifierFlags kSystemShortcutModifierFlag =
          NSEventModifierFlagFunction;
      isASystemShortcutEvent = isAKeyboardShortcutEvent &&
                               (ui::cocoa::ModifierMaskForKeyEvent(theEvent) &
                                kSystemShortcutModifierFlag) != 0;
    }

    if (isASystemShortcutEvent) {
      [[NSApp mainMenu] performKeyEquivalent:theEvent];

      // Behavior changed in macOS Sonoma - now it's important we early-out
      // rather than allow the code to reach
      // _hostHelper->ForwardKeyboardEventWithCommands(). Go with the existing
      // behavior for prior versions because we know it works for them.
      if (base::mac::MacOSVersion() >= 14'00'00) {
        _currentKeyDownCode.reset();
        _host->EndKeyboardEvent();
        return;
      }
    } else {
      [self interpretKeyEvents:@[ theEvent ]];
    }
  }

  _currentKeyDownCode.reset();

  // Indicates if we should send the key event and corresponding editor commands
  // after processing the input method result.
  BOOL delayEventUntilAfterImeComposition = NO;

  // To emulate Windows, over-write |event.windowsKeyCode| to VK_PROCESSKEY
  // while an input method is composing or inserting a text.
  // Gmail checks this code in its onkeydown handler to stop auto-completing
  // e-mail addresses while composing a CJK text.
  // If the text to be inserted has only one character, then we don't need this
  // trick, because we'll send the text as a key press event instead.
  if (_hasMarkedText || oldHasMarkedText || _textToBeInserted.length() > 1) {
    NativeWebKeyboardEvent fakeEvent = event;
    fakeEvent.windows_key_code = 0xE5;  // VKEY_PROCESSKEY
    fakeEvent.skip_if_unhandled = true;
    _hostHelper->ForwardKeyboardEvent(fakeEvent, latencyInfo);
    // If this key event was handled by the input method, but
    // -doCommandBySelector: (invoked by the call to -interpretKeyEvents: above)
    // enqueued edit commands, then in order to let webkit handle them
    // correctly, we need to send the real key event and corresponding edit
    // commands after processing the input method result.
    // We shouldn't do this if a new marked text was set by the input method,
    // otherwise the new marked text might be cancelled by webkit.
    if (_hasEditCommands && !_hasMarkedText)
      delayEventUntilAfterImeComposition = YES;
  } else {
    _hostHelper->ForwardKeyboardEventWithCommands(event, latencyInfo,
                                                  std::move(_editCommands));
  }

  // Then send keypress and/or composition related events.
  // If there was a marked text or the text to be inserted is longer than 1
  // character, then we send the text by calling FinishComposingText().
  // Otherwise, if the text to be inserted only contains 1 character, then we
  // can just send a keypress event which is fabricated by changing the type of
  // the keydown event, so that we can retain all necessary information, such
  // as unmodifiedText, etc. And we need to set event.skip_if_unhandled to true
  // to prevent the browser from handling it again. Note that,
  // |textToBeInserted_| is a UTF-16 string, but it's fine to only handle BMP
  // characters here, as we can always insert non-BMP characters as text.
  BOOL textInserted = NO;
  if (_textToBeInserted.length() >
      ((_hasMarkedText || oldHasMarkedText) ? 0u : 1u)) {
    _host->ImeCommitText(_textToBeInserted, gfx::Range::InvalidRange());
    textInserted = YES;
  }

  // Updates or cancels the composition. If some text has been inserted, then
  // we don't need to cancel the composition explicitly.
  if (_hasMarkedText && _markedText.length()) {
    // Sends the updated marked text to the renderer so it can update the
    // composition node in WebKit.
    // When marked text is available, |markedTextSelectedRange_| will be the
    // range being selected inside the marked text.
    if (!_isReconversionTriggered) {
      _host->ImeSetComposition(_markedText, _imeTextSpans,
                               _setMarkedTextReplacementRange,
                               _markedTextSelectedRange.location,
                               NSMaxRange(_markedTextSelectedRange));
    }
  } else if (oldHasMarkedText && !_hasMarkedText && !textInserted) {
    if (_unmarkTextCalled) {
      _host->ImeFinishComposingText();
    } else {
      _host->ImeCancelCompositionFromCocoa();
    }
  }

  _isReconversionTriggered = NO;

  // Clear information from |interpretKeyEvents:|
  _setMarkedTextReplacementRange = gfx::Range::InvalidRange();

  // If the key event was handled by the input method but it also generated some
  // edit commands, then we need to send the real key event and corresponding
  // edit commands here. This usually occurs when the input method wants to
  // finish current composition session but still wants the application to
  // handle the key event. See http://crbug.com/48161 for reference.
  if (delayEventUntilAfterImeComposition) {
    // If |delayEventUntilAfterImeComposition| is YES, then a fake key down
    // event with windowsKeyCode == 0xE5 has already been sent to webkit. So
    // before sending the real key down event, we need to send a fake key up
    // event to balance it.
    NativeWebKeyboardEvent fakeEvent = event;
    fakeEvent.SetType(blink::WebInputEvent::Type::kKeyUp);
    fakeEvent.skip_if_unhandled = true;
    ui::LatencyInfo fakeEventLatencyInfo = latencyInfo;
    _hostHelper->ForwardKeyboardEvent(fakeEvent, fakeEventLatencyInfo);
    _hostHelper->ForwardKeyboardEventWithCommands(event, fakeEventLatencyInfo,
                                                  std::move(_editCommands));
  }

  const NSUInteger kCtrlCmdKeyMask =
      NSEventModifierFlagControl | NSEventModifierFlagCommand;
  // Only send a corresponding key press event if there is no marked text.
  if (!_hasMarkedText) {
    if (!textInserted && _textToBeInserted.length() == 1) {
      // If a single character was inserted, then we just send it as a keypress
      // event.
      event.SetType(blink::WebInputEvent::Type::kChar);
      event.text[0] = _textToBeInserted[0];
      event.text[1] = 0;
      event.skip_if_unhandled = true;
      _hostHelper->ForwardKeyboardEvent(event, latencyInfo);
    } else if ((!textInserted || delayEventUntilAfterImeComposition) &&
               event.text[0] != '\0' &&
               ((modifierFlags & kCtrlCmdKeyMask) ||
                (_hasEditCommands && _editCommands.empty()))) {
      // We don't get insertText: calls if ctrl or cmd is down, or the key event
      // generates an insert command. So synthesize a keypress event for these
      // cases, unless the key event generated any other command.
      event.SetType(blink::WebInputEvent::Type::kChar);
      event.skip_if_unhandled = true;
      _hostHelper->ForwardKeyboardEvent(event, latencyInfo);
    }
  }

  // Possibly autohide the cursor.
  if (shouldAutohideCursor) {
    [NSCursor setHiddenUntilMouseMoves:YES];
    _cursorHidden = YES;
  }

  _host->EndKeyboardEvent();
}

- (BOOL)suppressNextKeyUpForTesting:(int)keyCode {
  return _unmatchedKeyDownCodes.count(keyCode) == 0;
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
  webEvent.rails_mode = _mouseWheelFilter.UpdateRailsMode(webEvent);
  _hostHelper->ForwardWheelEvent(webEvent);

  if (_endWheelMonitor) {
    [NSEvent removeMonitor:_endWheelMonitor];
    _endWheelMonitor = nil;
  }
}

- (void)handleBeginGestureWithEvent:(NSEvent*)event
            isSyntheticallyInjected:(BOOL)isSyntheticallyInjected {
  [_responderDelegate beginGestureWithEvent:event];

  WebGestureEvent gestureBeginEvent(WebGestureEventBuilder::Build(event, self));

  _hostHelper->GestureBegin(gestureBeginEvent, isSyntheticallyInjected);
}

- (void)handleEndGestureWithEvent:(NSEvent*)event {
  [_responderDelegate endGestureWithEvent:event];

  // On macOS 10.11+, the end event has type = NSEventTypeMagnify and phase =
  // NSEventPhaseEnded. On macOS 10.10 and older, the event has type =
  // NSEventTypeEndGesture.
  if ([event type] == NSEventTypeMagnify ||
      [event type] == NSEventTypeEndGesture) {
    WebGestureEvent endEvent(WebGestureEventBuilder::Build(event, self));
    endEvent.SetType(WebInputEvent::Type::kGesturePinchEnd);
    endEvent.SetSourceDevice(blink::WebGestureDevice::kTouchpad);
    endEvent.SetNeedsWheelEvent(true);
    _hostHelper->GestureEnd(endEvent);
  }
}

- (void)touchesMovedWithEvent:(NSEvent*)event {
  [_responderDelegate touchesMovedWithEvent:event];
}

- (void)touchesBeganWithEvent:(NSEvent*)event {
  [_responderDelegate touchesBeganWithEvent:event];
}

- (void)touchesCancelledWithEvent:(NSEvent*)event {
  [_responderDelegate touchesCancelledWithEvent:event];
}

- (void)touchesEndedWithEvent:(NSEvent*)event {
  [_responderDelegate touchesEndedWithEvent:event];
}

- (void)smartMagnifyWithEvent:(NSEvent*)event {
  const WebGestureEvent& smartMagnifyEvent =
      WebGestureEventBuilder::Build(event, self);
  _hostHelper->SmartMagnify(smartMagnifyEvent);
}

- (void)showLookUpDictionaryOverlayFromRange:(NSRange)range {
  _host->LookUpDictionaryOverlayFromRange(gfx::Range(range));
}

// This is invoked only on 10.8 or newer when the user taps a word using
// three fingers.
- (void)quickLookWithEvent:(NSEvent*)event {
  NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
  gfx::PointF rootPoint(point.x, NSHeight([self frame]) - point.y);
  _host->LookUpDictionaryOverlayAtPoint(rootPoint);
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
  if (event.phase == NSEventPhaseBegan) {
    [self handleBeginGestureWithEvent:event isSyntheticallyInjected:NO];
  }

  if (event.phase == NSEventPhaseEnded ||
      event.phase == NSEventPhaseCancelled) {
    [self handleEndGestureWithEvent:event];
  }

  if (_responderDelegate &&
      [_responderDelegate respondsToSelector:@selector(handleEvent:)]) {
    BOOL handled = [_responderDelegate handleEvent:event];
    if (handled)
      return;
  }

  // Use an NSEvent monitor to listen for the wheel-end end. This ensures that
  // the event is received even when the mouse cursor is no longer over the view
  // when the scrolling ends (e.g. if the tab was switched). This is necessary
  // for ending rubber-banding in such cases.
  if ([event phase] == NSEventPhaseBegan && !_endWheelMonitor) {
    _endWheelMonitor = [NSEvent
        addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
                                     handler:^(NSEvent* blockEvent) {
                                       [self shortCircuitScrollWheelEvent:
                                                 blockEvent];
                                       return blockEvent;
                                     }];
  }

  // This is responsible for content scrolling!
  WebMouseWheelEvent webEvent = WebMouseWheelEventBuilder::Build(event, self);
  webEvent.rails_mode = _mouseWheelFilter.UpdateRailsMode(webEvent);
  _hostHelper->RouteOrProcessWheelEvent(webEvent);
}

// Called repeatedly during a pinch gesture, with incremental change values.
- (void)magnifyWithEvent:(NSEvent*)event {
  if (event.phase == NSEventPhaseBegan) {
    [self handleBeginGestureWithEvent:event isSyntheticallyInjected:NO];
    return;
  }

  if (event.phase == NSEventPhaseEnded ||
      event.phase == NSEventPhaseCancelled) {
    [self handleEndGestureWithEvent:event];
    return;
  }

  // If this conditional evaluates to true, and the function has not
  // short-circuited from the previous block, then this event is a duplicate of
  // a gesture event, and should be ignored.
  if (event.phase == NSEventPhaseBegan || event.phase == NSEventPhaseEnded ||
      event.phase == NSEventPhaseCancelled) {
    return;
  }

  WebGestureEvent updateEvent = WebGestureEventBuilder::Build(event, self);
  _hostHelper->GestureUpdate(updateEvent);
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  NSWindow* oldWindow = [self window];

  NSNotificationCenter* notificationCenter =
      [NSNotificationCenter defaultCenter];

  if (oldWindow) {
    [notificationCenter removeObserver:self
                                  name:NSWindowDidChangeScreenNotification
                                object:oldWindow];
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
           selector:@selector(windowDidChangeScreenOrBackingProperties:)
               name:NSWindowDidChangeScreenNotification
             object:newWindow];
    [notificationCenter
        addObserver:self
           selector:@selector(windowDidChangeScreenOrBackingProperties:)
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

  _hostHelper->SetAccessibilityWindow(newWindow);
  [self sendWindowFrameInScreenToHost];
}

- (void)updateScreenProperties {
  NSWindow* enclosingWindow = [self window];
  if (!enclosingWindow)
    return;

  // TODO(ccameron): This will call [enclosingWindow screen], which may return
  // nil. Do that call here to avoid sending bogus display info to the host.
  auto* screen = display::Screen::GetScreen();
  const display::ScreenInfos newScreenInfos =
      screen->GetScreenInfosNearestDisplay(
          screen->GetDisplayNearestView(self).id());
  _host->OnScreenInfosChanged(newScreenInfos);
}

// This will be called when the NSView's NSWindow moves from one NSScreen to
// another, and makes note of the new screen's color space, scale factor, etc.
// It is also called when the current NSScreen's properties change (which is
// redundant with display::DisplayObserver::OnDisplayMetricsChanged).
- (void)windowDidChangeScreenOrBackingProperties:(NSNotification*)notification {
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
  _inSetFrame = YES;
  [super setFrame:r];
  _inSetFrame = NO;
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

  return _canBeKeyView;
}

- (BOOL)acceptsFirstResponder {
  if ([self hostIsDisconnected])
    return NO;

  return _canBeKeyView;
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  DCHECK([self window]);
  DCHECK_EQ([self window], [notification object]);
  if ([_responderDelegate respondsToSelector:@selector(windowDidBecomeKey)])
    [_responderDelegate windowDidBecomeKey];
  if ([self window].isKeyWindow)
    _host->OnWindowIsKeyChanged(true);
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

  _host->OnWindowIsKeyChanged(false);
}

- (BOOL)becomeFirstResponder {
  if ([self hostIsDisconnected])
    return NO;
  if ([_responderDelegate respondsToSelector:@selector(becomeFirstResponder)])
    [_responderDelegate becomeFirstResponder];

  _host->OnFirstResponderChanged(true);

  // Cancel any ongoing composition text which was left before we lost focus.
  // TODO(suzhe): We should do it in -resignFirstResponder: method, but
  // somehow that method won't be called when switching among different tabs.
  // See http://crbug.com/47209
  [self cancelComposition];

  NSNumber* direction = @([[self window] keyViewSelectionDirection]);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kViewDidBecomeFirstResponder
                    object:self
                  userInfo:@{kSelectionDirection : direction}];

  return YES;
}

- (BOOL)resignFirstResponder {
  if ([_responderDelegate respondsToSelector:@selector(resignFirstResponder)])
    [_responderDelegate resignFirstResponder];

  _host->OnFirstResponderChanged(false);
  if (_closeOnDeactivate) {
    [self setHidden:YES];
    _host->RequestShutdown();
  }

  // We should cancel any ongoing composition whenever RWH's Blur() method gets
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
  if (NSMenuItem* menuItem = base::apple::ObjCCast<NSMenuItem>(item)) {
    if (item.action == @selector(toggleAutomaticQuoteSubstitution:)) {
      menuItem.state = self.automaticQuoteSubstitutionEnabled;
      return !!(self.allowedTextCheckingTypes & NSTextCheckingTypeQuote);
    } else if (item.action == @selector(toggleAutomaticDashSubstitution:)) {
      menuItem.state = self.automaticDashSubstitutionEnabled;
      return !!(self.allowedTextCheckingTypes & NSTextCheckingTypeDash);
    } else if (item.action == @selector(toggleAutomaticTextReplacement:)) {
      menuItem.state = self.automaticTextReplacementEnabled;
      return !!(self.allowedTextCheckingTypes & NSTextCheckingTypeReplacement);
    } else if (item.action == @selector(uppercaseWord:)) {
      return self.canTransformText;
    } else if (item.action == @selector(lowercaseWord:)) {
      return self.canTransformText;
    } else if (item.action == @selector(capitalizeWord:)) {
      return self.canTransformText;
    }
  }

  if (_responderDelegate &&
      [_responderDelegate respondsToSelector:@selector
                          (validateUserInterfaceItem:isValidItem:)]) {
    BOOL valid;
    BOOL known =
        [_responderDelegate validateUserInterfaceItem:item isValidItem:&valid];
    if (known)
      return valid;
  }

  bool isForMainFrame = false;
  _host->SyncIsWidgetForMainFrame(&isForMainFrame);

  SEL action = [item action];

  if (action == @selector(stopSpeaking:)) {
    if (!isForMainFrame) {
      return NO;
    }
    bool isSpeaking = false;
    _host->SyncIsSpeaking(&isSpeaking);
    return isSpeaking;
  }

  if (action == @selector(startSpeaking:))
    return isForMainFrame;

  // For now, these actions are always enabled for render view,
  // this is sub-optimal.
  // TODO(suzhe): Plumb the "can*" methods up from WebCore.
  if (action == @selector(undo:) || action == @selector(redo:) ||
      action == @selector(cut:) || action == @selector(copy:) ||
      action == @selector(centerSelectionInVisibleArea:) ||
      action == @selector(copyToFindPboard:) || action == @selector(paste:) ||
      action == @selector(pasteAndMatchStyle:)) {
    return isForMainFrame;
  }

  return _editCommandHelper->IsMenuItemEnabled(action, self);
}

- (RenderWidgetHostNSViewHost*)renderWidgetHostNSViewHost {
  return _host;
}

- (void)setAccessibilityParentElement:(id)accessibilityParent {
  _accessibilityParent = accessibilityParent;
}

- (void)setPopupParentNSViewId:(uint64_t)viewId {
  _popupParentNSViewId = viewId;
}

- (id)accessibilityHitTest:(NSPoint)point {
  id rootElement = _hostHelper->GetRootBrowserAccessibilityElement();
  if (!rootElement) {
    if (features::IsAccessibilityRemoteUIAppEnabled()) {
      id rwhvElement = _hostHelper->GetAccessibilityElement();
      if (rwhvElement && rwhvElement != self) {
        return [rwhvElement accessibilityHitTest:point];
      }
    }
    return self;
  }

  // Calling accessibilityHitTest on the BrowserAccessibility element will
  // redirect the hit test request to the render side, in
  // RenderAccessibilityImpl::HitTest. This function expects the point passed by
  // parameter to be relative to the main document, not relative to the popup
  // window. In order to satisfy this requirement, we need to keep a reference
  // to the parent NSView of the popup NSView and transform the |point| using
  // that view.
  NSView* popupParentNSView =
      remote_cocoa::GetNSViewFromId(_popupParentNSViewId);

  NSView* view = popupParentNSView ? popupParentNSView : self;

  NSPoint pointInWindow = [view.window convertPointFromScreen:point];
  NSPoint localPoint = [view convertPoint:pointInWindow fromView:nil];
  localPoint.y = NSHeight([view bounds]) - localPoint.y;

  return [rootElement accessibilityHitTest:localPoint];
}

- (id)accessibilityFocusedUIElement {
  return _hostHelper->GetFocusedBrowserAccessibilityElement();
}

// NSAccessibility formal protocol:

- (NSArray*)accessibilityChildren {
  id root = _hostHelper->GetRootBrowserAccessibilityElement();
  if (root)
    return @[ root ];
  return nil;
}

- (NSArray*)accessibilityContents {
  return self.accessibilityChildren;
}

- (id)accessibilityParent {
  if (_accessibilityParent)
    return NSAccessibilityUnignoredAncestor(_accessibilityParent);
  return [super accessibilityParent];
}

- (NSAccessibilityRole)accessibilityRole {
  if (_sonomaAccessibilityRefinementsAreActive) {
    content::BrowserAccessibilityState* accessibility_state =
        content::BrowserAccessibilityState::GetInstance();

    // When an AT asks the application object for its role, we activate
    // nativeAPI accessibility support. If the AT descends into the AX tree
    // and arrives here (the web contents container), activate basic support
    // so that the AT can descend further into the web content.
    if (!accessibility_state->GetAccessibilityMode().has_mode(
            ui::kAXModeBasic.flags())) {
      accessibility_state->AddAccessibilityModeFlags(ui::kAXModeBasic);
    }
  }

  return NSAccessibilityScrollAreaRole;
}

// Below is our NSTextInputClient implementation.
//
// When WebHTMLView receives a NSEventTypeKeyDown event, WebHTMLView calls the
// following functions to process this event.
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
// So, if we used this implementation, a NSEventTypeKeyDown event is dispatched
// to the following functions of Chromium.
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
  static NSArray* const kAttributes = @[
    NSUnderlineStyleAttributeName, NSUnderlineColorAttributeName,
    NSMarkedClauseSegmentAttributeName, NSTextInputReplacementRangeAttributeName
  ];
  return kAttributes;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint {
  DCHECK([self window]);
  // |thePoint| is in screen coordinates, but needs to be converted to WebKit
  // coordinates (upper left origin). Scroll offsets will be taken care of in
  // the renderer.
  thePoint = [self.window convertPointFromScreen:thePoint];
  thePoint = [self convertPoint:thePoint fromView:nil];
  thePoint.y = NSHeight([self frame]) - thePoint.y;
  gfx::PointF rootPoint(thePoint.x, thePoint.y);

  uint32_t index = UINT32_MAX;
  _host->SyncGetCharacterIndexAtPoint(rootPoint, &index);
  // |index| could be WTF::notFound (-1) and its value is different from
  // NSNotFound so we need to convert it.
  if (index == UINT32_MAX)
    return NSNotFound;
  size_t charIndex = index;
  return NSUInteger(charIndex);
}

- (BOOL)drawsVerticallyForCharacterAtIndex:(NSUInteger)charIndex {
  return !!(_textInputFlags & blink::kWebTextInputFlagVertical);
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
                         actualRange:(NSRangePointer)actualRange {
  gfx::Rect gfxRect;
  gfx::Range gfxActualRange;
  bool success = false;
  if (actualRange)
    gfxActualRange = gfx::Range::FromPossiblyInvalidNSRange(*actualRange);
  _host->SyncGetFirstRectForRange(
      gfx::Range::FromPossiblyInvalidNSRange(theRange), &gfxRect,
      &gfxActualRange, &success);
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

  if (_textInputFlags & blink::kWebTextInputFlagVertical) {
    // Google Japanese Input doesn't use the result of
    // drawsVerticallyForCharacterAtIndex. So we'd like to ask it to show its
    // horizontal candidate window at the right side of the caret if the text
    // is vertical.
    NSString* inputSourceName =
        [[self inputContext] selectedKeyboardInputSource];
    if ([inputSourceName hasPrefix:kGoogleJapaneseInputPrefix])
      rect.origin.x += rect.size.width;
  }

  return rect;
}

- (NSRange)selectedRange {
  return _textSelectionRange.ToNSRange();
}

- (NSRange)markedRange {
  // An input method calls this method to check if an application really has
  // a text being composed when hasMarkedText call returns true.
  // Returns the range saved in the setMarkedText method so the input method
  // calls the setMarkedText method and we can update the composition node
  // there. (When this method returns an empty range, the input method doesn't
  // call the setMarkedText method.)
  return _hasMarkedText ? _markedRange : NSMakeRange(NSNotFound, 0);
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

  const gfx::Range requestedRange =
      gfx::Range::FromPossiblyInvalidNSRange(range);
  if (requestedRange.is_reversed())
    return nil;

  gfx::Range expectedRange;
  const std::u16string* expectedText;

  expectedText = &_availableText;
  size_t offset = _availableTextOffset;
  expectedRange = gfx::Range(offset, offset + expectedText->size());

  gfx::Range gfxActualRange = expectedRange.Intersect(requestedRange);
  if (!gfxActualRange.IsValid())
    return nil;
  if (actualRange)
    *actualRange = gfxActualRange.ToNSRange();

  std::u16string string = expectedText->substr(
      gfxActualRange.start() - expectedRange.start(), gfxActualRange.length());
  return [[NSAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(string)];
}

- (NSInteger)conversationIdentifier {
  return reinterpret_cast<NSInteger>(self);
}

// Each RenderWidgetHostViewCocoa has its own input context, but we return
// nil when the caret is in non-editable content or password box to avoid
// making input methods do their work.
// We disable input method inside password field as it is normal for Mac OS X
// password input fields to not allow dead keys or non ASCII input methods.
// There is also a privacy risk if the composition candidate window shows your
// password when the user is "composing" inside a password field. See
// crbug.com/1196101 for more info.
- (NSTextInputContext*)inputContext {
  switch (_textInputType) {
    case ui::TEXT_INPUT_TYPE_NONE:
    case ui::TEXT_INPUT_TYPE_PASSWORD:
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
  return _hasMarkedText;
}

- (void)unmarkText {
  // Delete the composition node of the renderer and finish an ongoing
  // composition.
  // It seems an input method calls the setMarkedText method and set an empty
  // text when it cancels an ongoing composition, i.e. I have never seen an
  // input method calls this method.
  _hasMarkedText = NO;
  _markedText.clear();
  _markedTextSelectedRange = NSMakeRange(NSNotFound, 0);
  _imeTextSpans.clear();

  // If we are handling a key down event, then FinishComposingText() will be
  // called in keyEvent: method.
  if (![self isHandlingKeyDown]) {
    _host->ImeFinishComposingText();
  } else {
    _unmarkTextCalled = YES;
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
  NSString* imText = isAttributedString ? [string string] : string;
  int length = [imText length];
  const BOOL fixLiveConversion =
      base::FeatureList::IsEnabled(features::kMacImeLiveConversionFix);

  // |markedRange_| will get set on a callback from ImeSetComposition().
  _markedTextSelectedRange = newSelRange;
  _markedText = base::SysNSStringToUTF16(imText);

  // Update markedRange/textSelectionRange assuming blink sets composition text
  // as is. We need this because the IME checks markedRange/textSelectionRange
  // before IPC to blink. If markedRange/textSelectionRange is not updated, IME
  // will behave incorrectly, e.g., wrong popup window position or duplicate
  // characters.
  if (length > 0) {
    _hasMarkedText = YES;
    if (!fixLiveConversion) {
      length = [string length];
    }
    if (replacementRange.location != NSNotFound) {
      // If the replacement range is valid, the range should be replaced with
      // the new text.
      _markedRange = NSMakeRange(replacementRange.location, length);
    } else if (fixLiveConversion && _markedRange.location == NSNotFound) {
      // If no replacement range and no marked range, the current selection
      // should be replaced.
      _markedRange = NSMakeRange(_textSelectionRange.start(), length);
    } else {
      // If no replacement range and the marked range is valid, the current
      // marked text should be replaced.
      _markedRange.length = length;
    }

    if (fixLiveConversion && newSelRange.location != NSNotFound) {
      CHECK_NE(_markedRange.location, static_cast<NSUInteger>(NSNotFound));
      CHECK_LE(_markedRange.location, std::numeric_limits<uint32_t>::max());
      CHECK_LE(newSelRange.location, std::numeric_limits<uint32_t>::max());
      // `_markedRange.location + NSMaxRange(newSelRange)` can be larger than
      // the maximum uint32_t. See crbug.com/40060200.
      uint32_t new_end = base::saturated_cast<uint32_t>(
          _markedRange.location + NSMaxRange(newSelRange));
      _textSelectionRange =
          gfx::Range(_markedRange.location + newSelRange.location, new_end);
    }
  } else {
    // An empty text means the composition is about to be cancelled,
    // collapse the selection to the beginning of the current marked range.
    if (fixLiveConversion && _hasMarkedText) {
      CHECK_LE(_markedRange.location, std::numeric_limits<uint32_t>::max())
          << "_markedRange.location is too large.";
      _textSelectionRange =
          gfx::Range(_markedRange.location, _markedRange.location);
    }
    _markedRange = NSMakeRange(NSNotFound, 0);
    _hasMarkedText = NO;
  }

  _imeTextSpans.clear();
  if (isAttributedString) {
    ExtractUnderlines(string, &_imeTextSpans);
  } else {
    // Use a thin black underline by default.
    _imeTextSpans.emplace_back(ui::ImeTextSpan::Type::kComposition, 0, length,
                               ui::ImeTextSpan::Thickness::kThin,
                               ui::ImeTextSpan::UnderlineStyle::kSolid,
                               SK_ColorTRANSPARENT);
  }

  // If we are handling a key down event and the reconversion is not triggered,
  // SetComposition() will be called in keyEvent: method.
  // Input methods of Mac use setMarkedText calls with an empty text to cancel
  // an ongoing composition. So, we should check whether or not the given text
  // is empty to update the input method state. (Our input method backend
  // automatically cancels an ongoing composition when we send an empty text.
  // So, it is OK to send an empty text to the renderer.)
  if ([self isHandlingKeyDown] && !_isReconversionTriggered) {
    _setMarkedTextReplacementRange = gfx::Range(replacementRange);
  } else {
    _host->ImeSetComposition(_markedText, _imeTextSpans,
                             gfx::Range(replacementRange), newSelRange.location,
                             NSMaxRange(newSelRange));
  }

  [[self inputContext] invalidateCharacterCoordinates];
  [self setNeedsDisplay:YES];
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
  if ([self isHandlingKeyDown]) {
    if ((_textInputFlags & blink::kWebTextInputFlagVertical)) {
      // Commands assigned to arrow keys are ignored and Blink handles key down
      // events because macOS doesn't work well with some vertical writing
      // modes. See editing_behavior.cc.
      //
      // The following bindings are affected:
      //                 Left: moveLeft
      //           Shift-Left: moveLeftAndModifySelection
      //          Option-Left: moveWordLeft
      //    Option-Shift-Left: moveWordLeftAndModifySelection
      //                Right: moveRight
      //          Shift-Right: moveRightAndModifySelection
      //         Option-Right: moveWordRight
      //   Option-Shift-Right: moveWordRightAndModifySelection
      //                   Up: moveUp
      //             Shift-Up: moveUpAndModifySelection
      //            Option-Up: moveBackward + moveToBeginningOfParagraph
      //                 Down: moveDown
      //           Shift-Down: moveDownAndModifySelection
      //          Option-Down: moveForward + moveToEndOfParagraph:
      //
      // This doesn't affect Fn + an arrow key, which produces a keyCode for
      // PageUp, PageDown, Home, or End.
      unsigned short keyCode = *_currentKeyDownCode;
      if (keyCode == kVK_LeftArrow || keyCode == kVK_RightArrow ||
          keyCode == kVK_DownArrow || keyCode == kVK_UpArrow) {
        return;
      }
    }
    _hasEditCommands = YES;
    // We ignore commands that insert characters, because this was causing
    // strange behavior (e.g. tab always inserted a tab rather than moving to
    // the next field on the page).
    if (!base::StartsWith(command, "insert",
                          base::CompareCase::INSENSITIVE_ASCII))
      _editCommands.push_back(blink::mojom::EditCommand::New(command, ""));
  } else {
    _host->ExecuteEditCommand(command);
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
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* imText = isAttributedString ? [string string] : string;
  if ([self isHandlingKeyDown] && replacementRange.location == NSNotFound) {
    // The user uses keyboard to type in a char without an IME or select a word
    // on the IME. Don't commit the change to the render, because the event is
    // being processed in |keyEvent:|. The commit will happen later after
    // |interpretKeyEvents:| returns.
    _textToBeInserted.append(base::SysNSStringToUTF16(imText));
    _shouldRequestTextSubstitutions = YES;
  } else {
    // The user uses mouse or touch bar to select a word on the IME.
    gfx::Range replacementGfxRange =
        gfx::Range::FromPossiblyInvalidNSRange(replacementRange);
    _host->ImeCommitText(base::SysNSStringToUTF16(imText), replacementGfxRange);
  }

  if (replacementRange.location == NSNotFound) {
    // Cancel selection after a IME commit by setting a zero-length selection
    // at the end of insertion point.
    // This is required for macOS 10.12+, otherwise the predictive completions
    // of IMEs won't work. See crbug.com/710101.
    int insertEndpoint = _markedRange.location + [imText length];
    _textSelectionRange = gfx::Range(insertEndpoint, insertEndpoint);
    // IMEs read |_availableText| preceding the insertion point as the context
    // for predictive completion. Unfortunately by the moment IME reads the
    // text, Blink likely hasn't finished the commit so the IME will read a
    // wrong context. We hack it by temporarily inserting the committed text
    // into
    // |_availableText|. This variable will ultimately be asynchronously updated
    // by Blink.
    // TODO(crbug.com/40165347): Mac's IME API is synchronous and it plays badly
    // with async APIs between the browser and the renderer. Probably replace
    // the sync |interpretKeyEvents:| with the async
    // |handleEventByInputMethod:|, which is an undocumented API used in
    // Webkit2.
    if (_markedRange.location >= _availableTextOffset &&
        _markedRange.location <= _availableTextOffset + _availableText.length())
      _availableText.insert(_markedRange.location - _availableTextOffset,
                            base::SysNSStringToUTF16(imText));
  }

  // Inserting text will delete all marked text automatically.
  _hasMarkedText = NO;
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
  _host->OnWindowIsKeyChanged([[self window] isKeyWindow]);
  _host->OnFirstResponderChanged([[self window] firstResponder] == self);

  // If we switch windows (or are removed from the view hierarchy), cancel any
  // open mouse-downs.
  if (_hasOpenMouseDown) {
    WebMouseEvent event(WebInputEvent::Type::kMouseUp,
                        WebInputEvent::kNoModifiers, ui::EventTimeForNow());
    event.button = WebMouseEvent::Button::kLeft;
    _hostHelper->ForwardMouseEvent(event);
    _hasOpenMouseDown = NO;
  }
}

- (void)undo:(id)sender {
  _host->Undo();
}

- (void)redo:(id)sender {
  _host->Redo();
}

- (void)cut:(id)sender {
  _host->Cut();
}

- (void)copy:(id)sender {
  _host->Copy();
}

- (void)copyToFindPboard:(id)sender {
  _host->CopyToFindPboard();
}

- (void)centerSelectionInVisibleArea:(id)sender {
  _host->CenterSelection();
}

- (void)paste:(id)sender {
  _host->Paste();
}

- (void)pasteAndMatchStyle:(id)sender {
  _host->PasteAndMatchStyle();
}

- (void)selectAll:(id)sender {
  // editCommandHelper_ adds implementations for most NSResponder methods
  // dynamically. But the renderer side only sends selection results back to
  // the browser if they were triggered by a keyboard event or went through
  // one of the Select methods on RWH. Since selectAll: is called from the
  // menu handler, neither is true.
  // Explicitly call SelectAll() here to make sure the renderer returns
  // selection results.
  _host->SelectAll();
}

- (void)startSpeaking:(id)sender {
  _host->StartSpeaking();
}

- (void)stopSpeaking:(id)sender {
  _host->StopSpeaking();
}

- (void)cancelComposition {
  [NSSpellChecker.sharedSpellChecker dismissCorrectionIndicatorForView:self];

  if (!_hasMarkedText)
    return;

  NSTextInputContext* inputContext = [self inputContext];
  [inputContext discardMarkedText];

  _hasMarkedText = NO;
  // Should not call [self unmarkText] here, because it'll send unnecessary
  // cancel composition IPC message to the renderer.
}

- (void)finishComposingText {
  if (!_hasMarkedText)
    return;

  _host->ImeFinishComposingText();
  [self cancelComposition];
}

// Overriding a NSResponder method to support application services.

- (id)validRequestorForSendType:(NSString*)sendType
                     returnType:(NSString*)returnType {
  id requestor = nil;
  BOOL sendTypeIsString = [sendType isEqualToString:NSPasteboardTypeString];
  BOOL returnTypeIsString = [returnType isEqualToString:NSPasteboardTypeString];
  BOOL hasText = !_textSelectionRange.is_empty();
  BOOL takesText = _textInputType != ui::TEXT_INPUT_TYPE_NONE;

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
  NSPoint location = [self.window convertPointFromScreen:NSEvent.mouseLocation];
  location = [self convertPoint:location fromView:nil];
  if (![self mouse:location inRect:[self bounds]] ||
      ![[self window] isKeyWindow])
    return NO;

  if (_cursorHidden || _showingContextMenu)
    return NO;

  return YES;
}

- (void)updateCursor:(NSCursor*)cursor {
  if (_currentCursor == cursor)
    return;

  _currentCursor = cursor;
  [[self window] invalidateCursorRectsForView:self];

  // NSWindow's invalidateCursorRectsForView: resets cursor rects but does not
  // update the cursor instantly. The cursor is updated when the mouse moves.
  // Update the cursor instantly by setting the current cursor.
  if ([self shouldChangeCurrentCursor])
    [_currentCursor set];
}

- (void)popupWindowWillClose:(NSNotification*)notification {
  [self setHidden:YES];
  _host->RequestShutdown();
}

- (void)invalidateTouchBar {
  // Work around a crash (https://crbug.com/822427).
  [_candidateListTouchBarItem setCandidates:@[]
                           forSelectedRange:NSMakeRange(NSNotFound, 0)
                                   inString:nil];
  _candidateListTouchBarItem = nil;
  self.touchBar = nil;
}

- (NSTouchBar*)makeTouchBar {
  if (_textInputType != ui::TEXT_INPUT_TYPE_NONE) {
    _candidateListTouchBarItem = [[NSCandidateListTouchBarItem alloc]
        initWithIdentifier:NSTouchBarItemIdentifierCandidateList];

    _candidateListTouchBarItem.delegate = self;
    _candidateListTouchBarItem.client = self;
    [self requestTextSuggestions];

    auto* touchBar = [[NSTouchBar alloc] init];
    touchBar.customizationIdentifier = ui::GetTouchBarId(kWebContentTouchBarId);
    touchBar.templateItems = [NSSet setWithObject:_candidateListTouchBarItem];
    bool includeEmojiPicker =
        _textInputType == ui::TEXT_INPUT_TYPE_TEXT ||
        _textInputType == ui::TEXT_INPUT_TYPE_SEARCH ||
        _textInputType == ui::TEXT_INPUT_TYPE_TEXT_AREA ||
        _textInputType == ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE;
    if (includeEmojiPicker) {
      touchBar.defaultItemIdentifiers = @[
        NSTouchBarItemIdentifierCharacterPicker,
        NSTouchBarItemIdentifierCandidateList
      ];
    } else {
      touchBar.defaultItemIdentifiers =
          @[ NSTouchBarItemIdentifierCandidateList ];
    }
    return touchBar;
  }

  return [super makeTouchBar];
}

- (BOOL)isHandlingKeyDown {
  return _currentKeyDownCode.has_value();
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
  // /!\ Compatibility hack!
  //
  // The NSServicesMenuRequestor protocol does not pass in the correct
  // NSPasteboardType constants in the `types` array, verified through macOS 13
  // (FB11838671). To keep the code below clean, if an obsolete type is passed
  // in, rewrite the array.
  //
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  if ([types containsObject:NSStringPboardType] &&
      ![types containsObject:NSPasteboardTypeString]) {
    types = [types arrayByAddingObject:NSPasteboardTypeString];
  }
#pragma clang diagnostic pop
  // /!\ End compatibility hack.

  bool wasAbleToWriteAtLeastOneType = false;

  if ([types containsObject:NSPasteboardTypeString] &&
      !_textSelectionRange.is_empty()) {
    NSString* text = base::SysUTF16ToNSString([self selectedText]);
    wasAbleToWriteAtLeastOneType |= [pboard writeObjects:@[ text ]];
  }

  return wasAbleToWriteAtLeastOneType;
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard*)pboard {
  NSArray* objects = [pboard readObjectsForClasses:@[ [NSString class] ]
                                           options:nil];
  if (!objects.count) {
    return NO;
  }

  // If the user is currently using an IME, confirm the IME input,
  // and then insert the text from the service, the same as TextEdit and Safari.
  [self finishComposingText];

  // It's expected that there only will be one string object on the pasteboard,
  // but if there is more than one, catenate them. This is the same compat
  // technique used by the compatibility call, -[NSPasteboard stringForType:].
  NSString* allTheText = [objects componentsJoinedByString:@"\n"];
  [self insertText:allTheText];
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
