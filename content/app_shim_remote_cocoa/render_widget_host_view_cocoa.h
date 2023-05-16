// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_VIEW_COCOA_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_VIEW_COCOA_H_

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>

#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/mac/scoped_nsobject.h"
#include "content/browser/renderer_host/input/mouse_wheel_rails_filter_mac.h"
#include "content/common/render_widget_host_ns_view.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-shared.h"
#import "ui/base/cocoa/command_dispatcher.h"
#import "ui/base/cocoa/tool_tip_base_view.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/range/range.h"

namespace blink {
class WebGestureEvent;
}  // namespace blink

namespace content {
class RenderWidgetHostViewMac;
class RenderWidgetHostViewMacEditCommandHelper;
}  // namespace content

namespace remote_cocoa {
namespace mojom {
class RenderWidgetHostNSViewHost;
}  // namespace mojom
class RenderWidgetHostNSViewHostHelper;
}  // namespace remote_cocoa

namespace ui {
enum class DomCode;
struct DidOverscrollParams;
}  // namespace ui

@protocol RenderWidgetHostViewMacDelegate;

@protocol RenderWidgetHostNSViewHostOwner
- (remote_cocoa::mojom::RenderWidgetHostNSViewHost*)renderWidgetHostNSViewHost;
@end

// This is the view that lives in the Cocoa view hierarchy. In Windows-land,
// RenderWidgetHostViewWin is both the view and the delegate. We split the roles
// but that means that the view needs to own the delegate and will dispose of it
// when it's removed from the view system.
// TODO(ccameron): Hide this interface behind RenderWidgetHostNSViewBridge.
@interface RenderWidgetHostViewCocoa
    : ToolTipBaseView <CommandDispatcherTarget,
                       RenderWidgetHostNSViewHostOwner,
                       NSCandidateListTouchBarItemDelegate,
                       NSTextInputClient,
                       NSAccessibility> {
 @private
  // Dummy host and host helper that are always valid (see comments below about
  // host_).
  // These need to be declared before |host_| and |host_helper_| so that it
  // gets destroyed last.
  mojo::Remote<remote_cocoa::mojom::RenderWidgetHostNSViewHost> _dummyHost;
  std::unique_ptr<remote_cocoa::RenderWidgetHostNSViewHostHelper>
      _dummyHostHelper;

  // The communications channel to the RenderWidgetHostViewMac. This pointer is
  // always valid. When the original host disconnects, |host_| is changed to
  // point to |dummyHost_|, to avoid having to preface every dereference with
  // a nullptr check.
  raw_ptr<remote_cocoa::mojom::RenderWidgetHostNSViewHost> _host;

  // A separate host interface for the parts of the interface to
  // RenderWidgetHostViewMac that cannot or should not be forwarded over mojo.
  // This includes events (where the extra translation is unnecessary or loses
  // information) and access to accessibility structures (only present in the
  // browser process).
  raw_ptr<remote_cocoa::RenderWidgetHostNSViewHostHelper> _hostHelper;

  // This ivar is the cocoa delegate of the NSResponder.
  base::scoped_nsobject<NSObject<RenderWidgetHostViewMacDelegate>>
      _responderDelegate;
  BOOL _canBeKeyView;
  BOOL _closeOnDeactivate;
  std::unique_ptr<content::RenderWidgetHostViewMacEditCommandHelper>
      _editCommandHelper;

  // Is YES if there was a mouse-down as yet unbalanced with a mouse-up.
  BOOL _hasOpenMouseDown;

  // The cursor for the page. This is passed up from the renderer.
  base::scoped_nsobject<NSCursor> _currentCursor;

  // Is YES if the cursor is hidden by key events.
  BOOL _cursorHidden;

  // Controlled by setShowingContextMenu.
  BOOL _showingContextMenu;

  // Set during -setFrame to avoid spamming host_ with origin and size
  // changes.
  BOOL _inSetFrame;

  // Variables used by our implementaion of the NSTextInput protocol.
  // An input method of Mac calls the methods of this protocol not only to
  // notify an application of its status, but also to retrieve the status of
  // the application. That is, an application cannot control an input method
  // directly.
  // This object keeps the status of a composition of the renderer and returns
  // it when an input method asks for it.
  // We need to implement Objective-C methods for the NSTextInput protocol. On
  // the other hand, we need to implement a C++ method for an IPC-message
  // handler which receives input-method events from the renderer.

  // Represents the input-method attributes supported by this object.
  base::scoped_nsobject<NSArray> _validAttributesForMarkedText;

  // Indicates if we are currently handling a key down event.
  BOOL _handlingKeyDown;

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
  // ever updated from the renderer (unlike |markedRange_|, which sometimes but
  // not always coincides with |compositionRange_|).
  bool _hasCompositionRange;
  gfx::Range _compositionRange;

  // Text to be inserted which was generated by handling a key down event.
  std::u16string _textToBeInserted;

  // Marked text which was generated by handling a key down event.
  std::u16string _markedText;

  // Selected range of |markedText_|.
  NSRange _markedTextSelectedRange;

  // Underline information of the |markedText_|.
  std::vector<ui::ImeTextSpan> _ime_text_spans;

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
  id _endWheelMonitor;

  // This is used to indicate if a stylus is currently in the proximity of the
  // tablet.
  bool _isStylusEnteringProximity;
  blink::WebPointerProperties::PointerType _pointerType;

  // The set of key codes from key down events that we haven't seen the matching
  // key up events yet.
  // Used for filtering out non-matching NSEventTypeKeyUp events.
  std::set<unsigned short> _keyDownCodes;

  // The filter used to guide touch events towards a horizontal or vertical
  // orientation.
  content::MouseWheelRailsFilterMac _mouseWheelFilter;

  // Whether the pen's tip is in contact with the stylus digital tablet.
  bool _has_pen_contact;

  bool _mouse_locked;
  bool _mouse_lock_unaccelerated_movement;
  gfx::PointF _last_mouse_screen_position;
  gfx::PointF _mouse_locked_screen_position;

  // The parent accessibility element. This is set only in the browser process.
  base::scoped_nsobject<id> _accessibilityParent;

  uint64_t popup_parent_ns_view_id_;
}

@property(nonatomic, assign) NSRange markedRange;
@property(nonatomic, assign) ui::TextInputType textInputType;
@property(nonatomic, assign) int textInputFlags;

@property(nonatomic, assign) NSSpellChecker* spellCheckerForTesting;

// Common code path for handling begin gesture events. This helper method is
// called via different codepaths based on OS version and SDK:
// - On 10.11 and later, when linking with the 10.11 SDK, it is called from
//   |magnifyWithEvent:| when the given event's phase is NSEventPhaseBegin.
// - On 10.10 and earlier, or when linking with an earlier SDK, it is called
//   by |beginGestureWithEvent:| when a gesture begins.
- (void)handleBeginGestureWithEvent:(NSEvent*)event
            isSyntheticallyInjected:(BOOL)isSyntheticallyInjected;

// Common code path for handling end gesture events. This helper method is
// called via different codepaths based on OS version and SDK:
// - On 10.11 and later, when linking with the 10.11 SDK, it is called from
//   |magnifyWithEvent:| when the given event's phase is NSEventPhaseEnded.
// - On 10.10 and earlier, or when linking with an earlier SDK, it is called
//   by |endGestureWithEvent:| when a gesture ends.
- (void)handleEndGestureWithEvent:(NSEvent*)event;

- (void)setCanBeKeyView:(BOOL)can;
- (void)setCloseOnDeactivate:(BOOL)b;
// Inidicate that the host was destroyed and can't be called back into.
- (void)setHostDisconnected;
// True for always-on-top special windows (e.g. Balloons and Panels).
- (BOOL)acceptsMouseEventsWhenInactive;
// Cancel ongoing composition (abandon the marked text).
- (void)cancelComposition;
// Confirm ongoing composition.
- (void)finishComposingText;
- (void)updateCursor:(NSCursor*)cursor;
- (void)tabletEvent:(NSEvent*)theEvent;
- (void)quickLookWithEvent:(NSEvent*)event;
- (void)showLookUpDictionaryOverlayFromRange:(NSRange)range;
- (BOOL)suppressNextKeyUpForTesting:(int)keyCode;
// Query the display::Display from the view's NSWindow's NSScreen and forward
// it to the RenderWidgetHostNSViewHost (only if the screen is non-nil).
- (void)updateScreenProperties;
// Indicate if the embedding WebContents is showing a web content context menu.
- (void)setShowingContextMenu:(BOOL)showing;
// Set the current TextInputManager::TextSelection from the renderer.
- (void)setTextSelectionText:(std::u16string)text
                      offset:(size_t)offset
                       range:(gfx::Range)range;
- (std::u16string)selectedText;
// Set the current TextInputManager::CompositionRangeInfo from the renderer.
- (void)setCompositionRange:(gfx::Range)range;

// KeyboardLock methods.
- (void)lockKeyboard:(absl::optional<base::flat_set<ui::DomCode>>)keysToLock;
- (void)unlockKeyboard;

// Cursorlock methods.
- (void)setCursorLocked:(BOOL)locked;
- (void)setCursorLockedUnacceleratedMovement:(BOOL)unaccelerated;

// Sets |accessibilityParent| as the object returned when the
// receiver is queried for its accessibility parent.
// TODO(lgrey/ellyjones): Remove this in favor of setAccessibilityParent:
// when we switch to the new accessibility API.
- (void)setAccessibilityParentElement:(id)accessibilityParent;

// Stores a reference to the popup parent's NSView id, which we can use to
// retrieve the associated NSView.
- (void)setPopupParentNSViewId:(uint64_t)view_id;

// Methods previously marked as private.
- (instancetype)
      initWithHost:(remote_cocoa::mojom::RenderWidgetHostNSViewHost*)host
    withHostHelper:(remote_cocoa::RenderWidgetHostNSViewHostHelper*)hostHelper;
- (void)setResponderDelegate:
    (NSObject<RenderWidgetHostViewMacDelegate>*)delegate;
- (void)processedGestureScrollEvent:(const blink::WebGestureEvent&)event
                           consumed:(BOOL)consumed;
- (void)processedOverscroll:(const ui::DidOverscrollParams&)params;
@end

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_VIEW_COCOA_H_
