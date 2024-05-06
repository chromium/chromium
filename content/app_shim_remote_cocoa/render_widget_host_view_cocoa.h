// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_VIEW_COCOA_H_
#define CONTENT_APP_SHIM_REMOTE_COCOA_RENDER_WIDGET_HOST_VIEW_COCOA_H_

#import <Cocoa/Cocoa.h>

#include <optional>
#include <set>
#include <vector>

#include "base/containers/flat_set.h"
#include "content/browser/renderer_host/input/mouse_wheel_rails_filter_mac.h"
#include "content/common/render_widget_host_ns_view.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-shared.h"
#import "ui/base/cocoa/command_dispatcher.h"
#import "ui/base/cocoa/tool_tip_base_view.h"
#include "ui/base/ime/ime_text_span.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/range/range.h"

namespace blink {
class WebGestureEvent;
}  // namespace blink

namespace remote_cocoa {
namespace mojom {
class RenderWidgetHostNSViewHost;
}  // namespace mojom
class RenderWidgetHostNSViewHostHelper;
}  // namespace remote_cocoa

namespace ui {
enum class DomCode : uint32_t;
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
                       NSAccessibility>

@property(nonatomic, assign) NSRange markedRange;
@property(nonatomic, assign) ui::TextInputType textInputType;
@property(nonatomic, assign) int textInputFlags;

@property(nonatomic, strong) NSSpellChecker* spellCheckerForTesting;

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
// Indicate that the host was destroyed and can't be called back into.
- (void)setHostDisconnected;
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
- (void)lockKeyboard:(std::optional<base::flat_set<ui::DomCode>>)keysToLock;
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
