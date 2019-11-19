// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_VIEW_COCOA_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_VIEW_COCOA_H_

#import <Cocoa/Cocoa.h>

#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/mac/scoped_nsobject.h"
#include "base/optional.h"
#include "content/browser/renderer_host/input/mouse_wheel_rails_filter_mac.h"
#include "content/common/edit_command.h"
#include "content/common/render_widget_host_ns_view.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
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
  // The communications channel to the RenderWidgetHostViewMac. This pointer is
  // always valid. When the original host disconnects, |host_| is changed to
  // point to |dummyHost_|, to avoid having to preface every dereference with
  // a nullptr check.
  remote_cocoa::mojom::RenderWidgetHostNSViewHost* host_;

  // A separate host interface for the parts of the interface to
  // RenderWidgetHostViewMac that cannot or should not be forwarded over mojo.
  // This includes events (where the extra translation is unnecessary or loses
  // information) and access to accessibility structures (only present in the
  // browser process).
  remote_cocoa::RenderWidgetHostNSViewHostHelper* hostHelper_;

  // Dummy host and host helper that are always valid (see above comments
  // about host_).
  mojo::Remote<remote_cocoa::mojom::RenderWidgetHostNSViewHost> dummyHost_;
  std::unique_ptr<remote_cocoa::RenderWidgetHostNSViewHostHelper>
      dummyHostHelper_;

  // This ivar is the cocoa delegate of the NSResponder.
  base::scoped_nsobject<NSObject<RenderWidgetHostViewMacDelegate>>
      responderDelegate_;
  BOOL canBeKeyView_;
  BOOL closeOnDeactivate_;
  std::unique_ptr<content::RenderWidgetHostViewMacEditCommandHelper>
      editCommandHelper_;

  // Is YES if there was a mouse-down as yet unbalanced with a mouse-up.
  BOOL hasOpenMouseDown_;

  // The cursor for the page. This is passed up from the renderer.
  base::scoped_nsobject<NSCursor> currentCursor_;

  // Is YES if the cursor is hidden by key events.
  BOOL cursorHidden_;

  // Controlled by setShowingContextMenu.
  BOOL showingContextMenu_;

  // Set during -setFrame to avoid spamming host_ with origin and size
  // changes.
  BOOL inSetFrame_;

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
  base::scoped_nsobject<NSArray> validAttributesForMarkedText_;

  // Indicates if we are currently handling a key down event.
  BOOL handlingKeyDown_;

  // Indicates if there is any marked text.
  BOOL hasMarkedText_;

  // Indicates if unmarkText is called or not when handling a keyboard
  // event.
  BOOL unmarkTextCalled_;

  // The range of current marked text inside the whole content of the DOM node
  // being edited.
  // TODO(suzhe): This is currently a fake value, as we do not support accessing
  // the whole content yet.
  NSRange markedRange_;

  // The text selection, cached from the RenderWidgetHostView. This is only ever
  // updated from the renderer.
  base::string16 textSelectionText_;
  size_t textSelectionOffset_;
  gfx::Range textSelectionRange_;

  // The composition range, cached from the RenderWidgetHostView. This is only
  // ever updated from the renderer (unlike |markedRange_|, which sometimes but
  // not always coincides with |compositionRange_|).
  bool hasCompositionRange_;
  gfx::Range compositionRange_;

  // Text to be inserted which was generated by handling a key down event.
  base::string16 textToBeInserted_;

  // Marked text which was generated by handling a key down event.
  base::string16 markedText_;

  // Selected range of |markedText_|.
  NSRange markedTextSelectedRange_;

  // Underline information of the |markedText_|.
  std::vector<ui::ImeTextSpan> ime_text_spans_;

  // Replacement range information received from |setMarkedText:|.
  gfx::Range setMarkedTextReplacementRange_;

  // Indicates if doCommandBySelector method receives any edit command when
  // handling a key down event.
  BOOL hasEditCommands_;

  // Contains edit commands received by the -doCommandBySelector: method when
  // handling a key down event, not including inserting commands, eg. insertTab,
  // etc.
  content::EditCommands editCommands_;

  // Whether the previous mouse event was ignored due to hitTest check.
  BOOL mouseEventWasIgnored_;

  // Event monitor for scroll wheel end event.
  id endWheelMonitor_;

  // This is used to indicate if a stylus is currently in the proximity of the
  // tablet.
  bool isStylusEnteringProximity_;
  blink::WebPointerProperties::PointerType pointerType_;

  // The set of key codes from key down events that we haven't seen the matching
  // key up events yet.
  // Used for filtering out non-matching NSKeyUp events.
  std::set<unsigned short> keyDownCodes_;

  // The filter used to guide touch events towards a horizontal or vertical
  // orientation.
  content::MouseWheelRailsFilterMac mouseWheelFilter_;

  // Whether the direct manipulation feature is enabled.
  bool direct_manipulation_enabled_;

  // Whether the pen's tip is in contact with the stylus digital tablet.
  bool has_pen_contact_;

  bool mouse_locked_;
  gfx::PointF last_mouse_screen_position_;
  gfx::PointF mouse_locked_screen_position_;

  // The parent accessibility element. This is set only in the browser process.
  base::scoped_nsobject<id> accessibilityParent_;
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
- (void)setTextSelectionText:(base::string16)text
                      offset:(size_t)offset
                       range:(gfx::Range)range;
- (base::string16)selectedText;
// Set the current TextInputManager::CompositionRangeInfo from the renderer.
- (void)setCompositionRange:(gfx::Range)range;

// KeyboardLock methods.
- (void)lockKeyboard:(base::Optional<base::flat_set<ui::DomCode>>)keysToLock;
- (void)unlockKeyboard;

// Cursorlock methods.
- (void)setCursorLocked:(BOOL)locked;

// Sets |accessibilityParent| as the object returned when the
// receiver is queried for its accessibility parent.
// TODO(lgrey/ellyjones): Remove this in favor of setAccessibilityParent:
// when we switch to the new accessibility API.
- (void)setAccessibilityParentElement:(id)accessibilityParent;

// Methods previously marked as private.
- (id)initWithHost:(remote_cocoa::mojom::RenderWidgetHostNSViewHost*)host
    withHostHelper:(remote_cocoa::RenderWidgetHostNSViewHostHelper*)hostHelper;
- (void)setResponderDelegate:
    (NSObject<RenderWidgetHostViewMacDelegate>*)delegate;
- (void)processedGestureScrollEvent:(const blink::WebGestureEvent&)event
                           consumed:(BOOL)consumed;
- (void)processedOverscroll:(const ui::DidOverscrollParams&)params;
@end

#endif  // CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_VIEW_COCOA_H_
