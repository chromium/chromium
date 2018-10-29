// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_WIDGET_MESSAGES_H_
#define CONTENT_COMMON_WIDGET_MESSAGES_H_

// IPC messages for controlling painting and input events.

#include "base/time/time.h"
#include "cc/input/touch_action.h"
#include "content/common/content_param_traits.h"
#include "content/common/cursors/webcursor.h"
#include "content/common/text_input_state.h"
#include "content/common/visual_properties.h"
#include "content/public/common/common_param_traits.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_intrinsic_sizing_info.h"
#include "third_party/blink/public/web/web_device_emulation_params.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START WidgetMsgStart

// Traits for VisualProperties.
IPC_ENUM_TRAITS_MAX_VALUE(blink::WebDeviceEmulationParams::ScreenPosition,
                          blink::WebDeviceEmulationParams::kScreenPositionLast)

IPC_ENUM_TRAITS_MAX_VALUE(content::ScreenOrientationValues,
                          content::SCREEN_ORIENTATION_VALUES_LAST)

IPC_ENUM_TRAITS_MIN_MAX_VALUE(blink::WebScreenOrientationType,
                              blink::kWebScreenOrientationUndefined,
                              blink::WebScreenOrientationTypeLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebDisplayMode,
                          blink::WebDisplayMode::kWebDisplayModeLast)

IPC_STRUCT_TRAITS_BEGIN(content::VisualProperties)
  IPC_STRUCT_TRAITS_MEMBER(screen_info)
  IPC_STRUCT_TRAITS_MEMBER(auto_resize_enabled)
  IPC_STRUCT_TRAITS_MEMBER(min_size_for_auto_resize)
  IPC_STRUCT_TRAITS_MEMBER(max_size_for_auto_resize)
  IPC_STRUCT_TRAITS_MEMBER(new_size)
  IPC_STRUCT_TRAITS_MEMBER(compositor_viewport_pixel_size)
  IPC_STRUCT_TRAITS_MEMBER(browser_controls_shrink_blink_size)
  IPC_STRUCT_TRAITS_MEMBER(scroll_focused_node_into_view)
  IPC_STRUCT_TRAITS_MEMBER(top_controls_height)
  IPC_STRUCT_TRAITS_MEMBER(bottom_controls_height)
  IPC_STRUCT_TRAITS_MEMBER(local_surface_id)
  IPC_STRUCT_TRAITS_MEMBER(local_surface_id_allocation_time)
  IPC_STRUCT_TRAITS_MEMBER(visible_viewport_size)
  IPC_STRUCT_TRAITS_MEMBER(is_fullscreen_granted)
  IPC_STRUCT_TRAITS_MEMBER(display_mode)
  IPC_STRUCT_TRAITS_MEMBER(capture_sequence_number)
  IPC_STRUCT_TRAITS_MEMBER(zoom_level)
IPC_STRUCT_TRAITS_END()

// Traits for WebDeviceEmulationParams.
IPC_STRUCT_TRAITS_BEGIN(blink::WebFloatPoint)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebFloatRect)
  IPC_STRUCT_TRAITS_MEMBER(x)
  IPC_STRUCT_TRAITS_MEMBER(y)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebSize)
  IPC_STRUCT_TRAITS_MEMBER(width)
  IPC_STRUCT_TRAITS_MEMBER(height)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(blink::WebDeviceEmulationParams)
  IPC_STRUCT_TRAITS_MEMBER(screen_position)
  IPC_STRUCT_TRAITS_MEMBER(screen_size)
  IPC_STRUCT_TRAITS_MEMBER(view_position)
  IPC_STRUCT_TRAITS_MEMBER(device_scale_factor)
  IPC_STRUCT_TRAITS_MEMBER(view_size)
  IPC_STRUCT_TRAITS_MEMBER(scale)
  IPC_STRUCT_TRAITS_MEMBER(viewport_offset)
  IPC_STRUCT_TRAITS_MEMBER(viewport_scale)
  IPC_STRUCT_TRAITS_MEMBER(screen_orientation_angle)
  IPC_STRUCT_TRAITS_MEMBER(screen_orientation_type)
IPC_STRUCT_TRAITS_END()

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebTextDirection,
                          blink::WebTextDirection::kWebTextDirectionLast)

IPC_STRUCT_BEGIN(WidgetHostMsg_SelectionBounds_Params)
  IPC_STRUCT_MEMBER(gfx::Rect, anchor_rect)
  IPC_STRUCT_MEMBER(blink::WebTextDirection, anchor_dir)
  IPC_STRUCT_MEMBER(gfx::Rect, focus_rect)
  IPC_STRUCT_MEMBER(blink::WebTextDirection, focus_dir)
  IPC_STRUCT_MEMBER(bool, is_anchor_first)
IPC_STRUCT_END()

// Traits for TextInputState.
IPC_ENUM_TRAITS_MAX_VALUE(ui::TextInputMode, ui::TEXT_INPUT_MODE_MAX)

IPC_STRUCT_TRAITS_BEGIN(content::TextInputState)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(mode)
  IPC_STRUCT_TRAITS_MEMBER(flags)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(selection_start)
  IPC_STRUCT_TRAITS_MEMBER(selection_end)
  IPC_STRUCT_TRAITS_MEMBER(composition_start)
  IPC_STRUCT_TRAITS_MEMBER(composition_end)
  IPC_STRUCT_TRAITS_MEMBER(can_compose_inline)
  IPC_STRUCT_TRAITS_MEMBER(show_ime_if_needed)
  IPC_STRUCT_TRAITS_MEMBER(reply_to_request)
IPC_STRUCT_TRAITS_END()

//
// Browser -> Renderer Messages.
//

// Sent to inform the renderer to invoke a context menu.
// The parameter specifies the location in the render widget's coordinates.
IPC_MESSAGE_ROUTED2(WidgetMsg_ShowContextMenu,
                    ui::MenuSourceType,
                    gfx::Point /* location where menu should be shown */)

// Tells the render widget to close.
// Expects a Close_ACK message when finished.
IPC_MESSAGE_ROUTED0(WidgetMsg_Close)

// Tells the renderer to update visual properties. The resulting
// CompositorFrame will produce a RenderFrameMetadata containing a new
// LocalSurfaceId. This acts as a form of ACK for this message.
IPC_MESSAGE_ROUTED1(WidgetMsg_SynchronizeVisualProperties,
                    content::VisualProperties /* params */)

// Enables device emulation. See WebDeviceEmulationParams for description.
IPC_MESSAGE_ROUTED1(WidgetMsg_EnableDeviceEmulation,
                    blink::WebDeviceEmulationParams /* params */)

// Disables device emulation, enabled previously by EnableDeviceEmulation.
IPC_MESSAGE_ROUTED0(WidgetMsg_DisableDeviceEmulation)

// Sent to inform the widget that it was hidden.  This allows it to reduce its
// resource utilization.
IPC_MESSAGE_ROUTED0(WidgetMsg_WasHidden)

// Tells the render view that it is no longer hidden (see WasHidden).
IPC_MESSAGE_ROUTED1(WidgetMsg_WasShown,
                    base::TimeTicks /* show_request_timestamp */)

// Activate/deactivate the RenderWidget (i.e., set its controls' tint
// accordingly, etc.).
IPC_MESSAGE_ROUTED1(WidgetMsg_SetActive, bool /* active */)

// Make the RenderWidget background transparent or opaque.
IPC_MESSAGE_ROUTED1(WidgetMsg_SetBackgroundOpaque, bool /* opaque */)

// Changes the text direction of the currently selected input field (if any).
IPC_MESSAGE_ROUTED1(WidgetMsg_SetTextDirection,
                    blink::WebTextDirection /* direction */)

// Reply to WidgetHostMsg_RequestSetBounds, WidgetHostMsg_ShowWidget, and
// FrameHostMsg_ShowCreatedWindow, to inform the renderer that the browser has
// processed the bounds-setting.  The browser may have ignored the new bounds,
// but it finished processing.  This is used because the renderer keeps a
// temporary cache of the widget position while these asynchronous operations
// are in progress.
IPC_MESSAGE_ROUTED0(WidgetMsg_SetBounds_ACK)

// Informs the RenderWidget of its position on the user's screen, as well as
// the position of the native window holding the RenderWidget.
IPC_MESSAGE_ROUTED2(WidgetMsg_UpdateScreenRects,
                    gfx::Rect /* widget_screen_rect */,
                    gfx::Rect /* window_screen_rect */)

// Sent by the browser to ask the renderer to redraw. Robust to events that can
// happen in renderer (abortion of the commit or draw, loss of output surface
// etc.).
IPC_MESSAGE_ROUTED1(WidgetMsg_ForceRedraw, int /* snapshot_id */)

// Sets the viewport intersection and compositor raster area on the widget for
// an out-of-process iframe.
IPC_MESSAGE_ROUTED3(WidgetMsg_SetViewportIntersection,
                    gfx::Rect /* viewport_intersection */,
                    gfx::Rect /* compositor_visible_rect */,
                    bool /* occluded or obscured */)

// Sent to an OOPIF widget when the browser receives a FrameHostMsg_SetIsInert
// from the target widget's embedding renderer changing its inertness. When a
// widget is inert, it is unable to process input events.
//
// https://html.spec.whatwg.org/multipage/interaction.html#inert
IPC_MESSAGE_ROUTED1(WidgetMsg_SetIsInert, bool /* inert */)

// Sets the inherited effective touch action on an out-of-process iframe.
IPC_MESSAGE_ROUTED1(WidgetMsg_SetInheritedEffectiveTouchAction, cc::TouchAction)

// Toggles render throttling for an out-of-process iframe.
IPC_MESSAGE_ROUTED2(WidgetMsg_UpdateRenderThrottlingStatus,
                    bool /* is_throttled */,
                    bool /* subtree_throttled */)

// Sent by the browser to synchronize with the next compositor frame by
// requesting an ACK be queued. Used only for tests.
IPC_MESSAGE_ROUTED1(WidgetMsg_WaitForNextFrameForTests,
                    int /* main_frame_thread_observer_routing_id */)

// Tells the render side that a WidgetHostMsg_LockMouse message has been
// processed. |succeeded| indicates whether the mouse has been successfully
// locked or not.
IPC_MESSAGE_ROUTED1(WidgetMsg_LockMouse_ACK, bool /* succeeded */)
// Tells the render side that the mouse has been unlocked.
IPC_MESSAGE_ROUTED0(WidgetMsg_MouseLockLost)

//
// Renderer -> Browser Messages.
//

// Sent by the renderer process to request that the browser close the widget.
// This corresponds to the window.close() API, and the browser may ignore
// this message.  Otherwise, the browser will generate a WidgetMsg_Close
// message to close the widget.
IPC_MESSAGE_ROUTED0(WidgetHostMsg_Close)

// Notification that the selection bounds have changed.
IPC_MESSAGE_ROUTED1(WidgetHostMsg_SelectionBoundsChanged,
                    WidgetHostMsg_SelectionBounds_Params)

// Sent in response to a WidgetMsg_UpdateScreenRects so that the renderer can
// throttle these messages.
IPC_MESSAGE_ROUTED0(WidgetHostMsg_UpdateScreenRects_ACK)

// Only used for SVGs inside of <object> and not for iframes. Informs the
// browser that hte current frame's intrinsic sizing info has changed. The
// browser can then notify a containing frame that the frame may need to
// trigger a new layout.
//
// Also see FrameMsg_IntrinsicSizingInfoOfChildChanged.
IPC_MESSAGE_ROUTED1(WidgetHostMsg_IntrinsicSizingInfoChanged,
                    blink::WebIntrinsicSizingInfo)

// Send the tooltip text for the current mouse position to the browser.
IPC_MESSAGE_ROUTED2(WidgetHostMsg_SetTooltipText,
                    base::string16 /* tooltip text string */,
                    blink::WebTextDirection /* text direction hint */)

// Updates the current cursor to be used by the browser for indicating the
// location of a pointing device.
IPC_MESSAGE_ROUTED1(WidgetHostMsg_SetCursor, content::WebCursor)

// Request a non-decelerating synthetic fling animation to be latched on the
// scroller at the start point, and whose velocity can be changed over time by
// sending multiple AutoscrollFling gestures.  Used for features like
// middle-click autoscroll.
IPC_MESSAGE_ROUTED1(WidgetHostMsg_AutoscrollStart, gfx::PointF /* start */)
IPC_MESSAGE_ROUTED1(WidgetHostMsg_AutoscrollFling,
                    gfx::Vector2dF /* velocity */)
IPC_MESSAGE_ROUTED0(WidgetHostMsg_AutoscrollEnd)

// Notifies the browser if the text input state has changed. Primarily useful
// for IME as they need to know of all changes to update their interpretation
// of the characters that have been input.
IPC_MESSAGE_ROUTED1(WidgetHostMsg_TextInputStateChanged,
                    content::TextInputState /* text_input_state */)

// Sent by the renderer process to request that the browser change the bounds of
// the widget. This corresponds to the window.resizeTo() and window.moveTo()
// APIs, and the browser may ignore this message.
IPC_MESSAGE_ROUTED1(WidgetHostMsg_RequestSetBounds, gfx::Rect /* bounds */)

// Requests to lock the mouse. Will result in a WidgetMsg_LockMouse_ACK message
// being sent back.
// |privileged| is used by Pepper Flash. If this flag is set to true, we won't
// pop up a bubble to ask for user permission or take mouse lock content into
// account.
IPC_MESSAGE_ROUTED2(WidgetHostMsg_LockMouse,
                    bool /* user_gesture */,
                    bool /* privileged */)

// Requests to unlock the mouse. A WidgetMsg_MouseLockLost message will be sent
// whenever the mouse is unlocked (which may or may not be caused by
// WidgetHostMsg_UnlockMouse).
IPC_MESSAGE_ROUTED0(WidgetHostMsg_UnlockMouse)

// Message sent from renderer to the browser when the element that is focused
// has been touched. A bool is passed in this message which indicates if the
// node is editable.
IPC_MESSAGE_ROUTED1(WidgetHostMsg_FocusedNodeTouched, bool /* editable */)

// Sent by the renderer process in response to an earlier WidgetMsg_ForceRedraw
// message. The reply includes the snapshot-id from the request.
IPC_MESSAGE_ROUTED1(WidgetHostMsg_ForceRedrawComplete, int /* snapshot_id */)

// Sends a set of queued messages that were being held until the next
// CompositorFrame is being submitted from the renderer. These messages are
// sent before the OnRenderFrameMetadataChanged message is sent (via mojo) and
// before the CompositorFrame is sent to the viz service. The |frame_token|
// will match the token in the about-to-be-submitted CompositorFrame.
IPC_MESSAGE_ROUTED2(WidgetHostMsg_FrameSwapMessages,
                    uint32_t /* frame_token */,
                    std::vector<IPC::Message> /* messages */)

// Indicates that the render widget has been closed in response to a
// Close message.
IPC_MESSAGE_CONTROL1(WidgetHostMsg_Close_ACK, int /* old_route_id */)

// Sent from an inactive renderer for the browser to route to the active
// renderer, instructing it to close.
//
// TODO(http://crbug.com/419087): Move this thing to Frame as it's a signal
// from a swapped out frame to the mainframe of the frame tree.
IPC_MESSAGE_ROUTED0(WidgetHostMsg_RouteCloseEvent)

// Sent in reply to WidgetMsg_WaitForNextFrameForTests.
IPC_MESSAGE_ROUTED0(WidgetHostMsg_WaitForNextFrameForTests_ACK)

// Sent once a paint happens after the first non empty layout. In other words,
// after the frame widget has painted something.
IPC_MESSAGE_ROUTED0(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint)

// Sent once the RenderWidgetCompositor issues a draw command.
IPC_MESSAGE_ROUTED0(WidgetHostMsg_DidCommitAndDrawCompositorFrame)

// Notifies whether there are JavaScript touch event handlers or not.
IPC_MESSAGE_ROUTED1(WidgetHostMsg_HasTouchEventHandlers,
                    bool /* has_handlers */)

#endif  //  CONTENT_COMMON_WIDGET_MESSAGES_H_
