// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_VIEW_MESSAGES_H_
#define CONTENT_COMMON_VIEW_MESSAGES_H_

// IPC messages for page rendering.

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <string>
#include <vector>

#include "base/memory/shared_memory.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "cc/input/touch_action.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "content/common/content_export.h"
#include "content/common/content_param_traits.h"
#include "content/common/date_time_suggestion.h"
#include "content/common/frame_replication_state.h"
#include "content/common/navigation_gesture.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/menu_item.h"
#include "content/public/common/page_state.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/referrer.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/three_d_api_types.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "media/base/ipc/media_param_traits.h"
#include "net/base/network_change_notifier.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/manifest/web_display_mode.h"
#include "third_party/blink/public/web/web_plugin_action.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"

#if defined(OS_MACOSX)
#include "third_party/blink/public/platform/mac/web_scrollbar_theme.h"
#include "third_party/blink/public/platform/web_scrollbar_buttons_placement.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT

#define IPC_MESSAGE_START ViewMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(blink::WebPluginAction::Type,
                          blink::WebPluginAction::Type::kTypeLast)
IPC_ENUM_TRAITS_MAX_VALUE(content::MenuItem::Type, content::MenuItem::TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(content::NavigationGesture,
                          content::NavigationGestureLast)
IPC_ENUM_TRAITS_MIN_MAX_VALUE(content::PageZoom,
                              content::PageZoom::PAGE_ZOOM_OUT,
                              content::PageZoom::PAGE_ZOOM_IN)
IPC_ENUM_TRAITS_MAX_VALUE(content::ThreeDAPIType,
                          content::THREE_D_API_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(ui::TextInputType, ui::TEXT_INPUT_TYPE_MAX)

#if defined(OS_MACOSX)
IPC_ENUM_TRAITS_MAX_VALUE(
    blink::WebScrollbarButtonsPlacement,
    blink::WebScrollbarButtonsPlacement::kWebScrollbarButtonsPlacementLast)

IPC_ENUM_TRAITS_MAX_VALUE(blink::ScrollerStyle, blink::kScrollerStyleOverlay)
#endif

IPC_STRUCT_TRAITS_BEGIN(blink::WebPluginAction)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(enable)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::MenuItem)
  IPC_STRUCT_TRAITS_MEMBER(label)
  IPC_STRUCT_TRAITS_MEMBER(tool_tip)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(action)
  IPC_STRUCT_TRAITS_MEMBER(rtl)
  IPC_STRUCT_TRAITS_MEMBER(has_directional_override)
  IPC_STRUCT_TRAITS_MEMBER(enabled)
  IPC_STRUCT_TRAITS_MEMBER(checked)
  IPC_STRUCT_TRAITS_MEMBER(submenu)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::DateTimeSuggestion)
  IPC_STRUCT_TRAITS_MEMBER(value)
  IPC_STRUCT_TRAITS_MEMBER(localized_value)
  IPC_STRUCT_TRAITS_MEMBER(label)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(ViewHostMsg_DateTimeDialogValue_Params)
  IPC_STRUCT_MEMBER(ui::TextInputType, dialog_type)
  IPC_STRUCT_MEMBER(double, dialog_value)
  IPC_STRUCT_MEMBER(double, minimum)
  IPC_STRUCT_MEMBER(double, maximum)
  IPC_STRUCT_MEMBER(double, step)
  IPC_STRUCT_MEMBER(std::vector<content::DateTimeSuggestion>, suggestions)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer.

#if defined(OS_ANDROID)
// Tells the renderer to cancel an opened date/time dialog.
IPC_MESSAGE_ROUTED0(ViewMsg_CancelDateTimeDialog)

// Replaces a date time input field.
IPC_MESSAGE_ROUTED1(ViewMsg_ReplaceDateTime,
                    double /* dialog_value */)

#endif

// Sends updated preferences to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_SetRendererPrefs,
                    content::RendererPreferences)

// This passes a set of webkit preferences down to the renderer.
IPC_MESSAGE_ROUTED1(ViewMsg_UpdateWebPreferences,
                    content::WebPreferences)

// Tells the renderer to focus the first (last if reverse is true) focusable
// node.
IPC_MESSAGE_ROUTED1(ViewMsg_SetInitialFocus,
                    bool /* reverse */)

// Tells the renderer to perform the given action on the plugin located at
// the given point.
IPC_MESSAGE_ROUTED2(ViewMsg_PluginActionAt,
                    gfx::Point, /* location */
                    blink::WebPluginAction)

// Sets the page scale for the current main frame to the given page scale.
IPC_MESSAGE_ROUTED1(ViewMsg_SetPageScale, float /* page_scale_factor */)

// Tell the renderer to add a property to the WebUI binding object.  This
// only works if we allowed WebUI bindings.
IPC_MESSAGE_ROUTED2(ViewMsg_SetWebUIProperty,
                    std::string /* property_name */,
                    std::string /* property_value_json */)

// Used to notify the render-view that we have received a target URL. Used
// to prevent target URLs spamming the browser.
IPC_MESSAGE_ROUTED0(ViewMsg_UpdateTargetURL_ACK)

// Provides the results of directory enumeration.
IPC_MESSAGE_ROUTED2(ViewMsg_EnumerateDirectoryResponse,
                    int /* request_id */,
                    std::vector<base::FilePath> /* files_in_directory */)

// Instructs the renderer to close the current page, including running the
// onunload event handler.
//
// Expects a ClosePage_ACK message when finished.
IPC_MESSAGE_ROUTED0(ViewMsg_ClosePage)

// Notification that a move or resize renderer's containing window has
// started.
IPC_MESSAGE_ROUTED0(ViewMsg_MoveOrResizeStarted)

// Used to instruct the RenderView to send back updates to the preferred size.
IPC_MESSAGE_ROUTED0(ViewMsg_EnablePreferredSizeChangedMode)

// Response message to ViewHostMsg_CreateWorker.
// Sent when the worker has started.
IPC_MESSAGE_ROUTED0(ViewMsg_WorkerCreated)

// Sent when the worker failed to load the worker script.
// In normal cases, this message is sent after ViewMsg_WorkerCreated is sent.
// But if the shared worker of the same URL already exists and it has failed
// to load the script, when the renderer send ViewHostMsg_CreateWorker before
// the shared worker is killed only ViewMsg_WorkerScriptLoadFailed is sent.
IPC_MESSAGE_ROUTED0(ViewMsg_WorkerScriptLoadFailed)

// Sent when the worker has connected.
// This message is sent only if the worker successfully loaded the script.
// |used_features| is the set of features that the worker has used. The values
// must be from blink::UseCounter::Feature enum.
IPC_MESSAGE_ROUTED1(ViewMsg_WorkerConnected,
                    std::set<uint32_t> /* used_features */)

// Sent when the worker is destroyed.
IPC_MESSAGE_ROUTED0(ViewMsg_WorkerDestroyed)

// Sent when the worker calls API that should be recored in UseCounter.
// |feature| must be one of the values from blink::UseCounter::Feature
// enum.
IPC_MESSAGE_ROUTED1(ViewMsg_CountFeatureOnSharedWorker,
                    uint32_t /* feature */)

#if BUILDFLAG(ENABLE_PLUGINS)
// Reply to ViewHostMsg_OpenChannelToPpapiBroker
// Tells the renderer that the channel to the broker has been created.
IPC_MESSAGE_ROUTED2(ViewMsg_PpapiBrokerChannelCreated,
                    base::ProcessId /* broker_pid */,
                    IPC::ChannelHandle /* handle */)

// Reply to ViewHostMsg_RequestPpapiBrokerPermission.
// Tells the renderer whether permission to access to PPAPI broker was granted
// or not.
IPC_MESSAGE_ROUTED1(ViewMsg_PpapiBrokerPermissionResult,
                    bool /* result */)
#endif

// -----------------------------------------------------------------------------
// Messages sent from the renderer to the browser.

// These two messages are sent to the parent RenderViewHost to display a widget
// that was created by CreateWidget/CreateFullscreenWidget. |route_id| refers
// to the id that was returned from the corresponding Create message above.
// |initial_rect| is in screen coordinates.
IPC_MESSAGE_ROUTED2(ViewHostMsg_ShowWidget,
                    int /* route_id */,
                    gfx::Rect /* initial_rect */)

// Message to show a full screen widget.
IPC_MESSAGE_ROUTED1(ViewHostMsg_ShowFullscreenWidget,
                    int /* route_id */)

// Indicates that the current page has been closed, after a ClosePage
// message.
IPC_MESSAGE_ROUTED0(ViewHostMsg_ClosePage_ACK)

// Notifies the browser that we want to show a destination url for a potential
// action (e.g. when the user is hovering over a link).
IPC_MESSAGE_ROUTED1(ViewHostMsg_UpdateTargetURL,
                    GURL)

// Sent when the document element is available for the top-level frame.  This
// happens after the page starts loading, but before all resources are
// finished.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DocumentAvailableInMainFrame,
                    bool /* uses_temporary_zoom_level */)

IPC_MESSAGE_ROUTED0(ViewHostMsg_Focus)

// Get the list of proxies to use for |url|, as a semicolon delimited list
// of "<TYPE> <HOST>:<PORT>" | "DIRECT".
IPC_SYNC_MESSAGE_CONTROL1_2(ViewHostMsg_ResolveProxy,
                            GURL /* url */,
                            bool /* result */,
                            std::string /* proxy list */)

// Tells the browser that a specific Appcache manifest in the current page
// was accessed.
IPC_MESSAGE_ROUTED2(ViewHostMsg_AppCacheAccessed,
                    GURL /* manifest url */,
                    bool /* blocked by policy */)

// Used to go to the session history entry at the given offset (ie, -1 will
// return the "back" item).
IPC_MESSAGE_ROUTED2(ViewHostMsg_GoToEntryAtOffset,
                    int /* offset (from current) of history item to get */,
                    bool /* has_user_gesture */)

// Notifies that the preferred size of the content changed.
IPC_MESSAGE_ROUTED1(ViewHostMsg_DidContentsPreferredSizeChange,
                    gfx::Size /* pref_size */)

#if BUILDFLAG(ENABLE_PLUGINS)
// A renderer sends this to the browser process when it wants to access a PPAPI
// broker. In contrast to FrameHostMsg_OpenChannelToPpapiBroker, this is called
// for every connection.
// The browser will respond with ViewMsg_PpapiBrokerPermissionResult.
IPC_MESSAGE_ROUTED3(ViewHostMsg_RequestPpapiBrokerPermission,
                    int /* routing_id */,
                    GURL /* document_url */,
                    base::FilePath /* plugin_path */)
#endif  // BUILDFLAG(ENABLE_PLUGINS)

// Asks the browser to enumerate a directory.  This is equivalent to running
// the file chooser in directory-enumeration mode and having the user select
// the given directory.  The result is returned in a
// ViewMsg_EnumerateDirectoryResponse message.
IPC_MESSAGE_ROUTED2(ViewHostMsg_EnumerateDirectory,
                    int /* request_id */,
                    base::FilePath /* file_path */)

// When the renderer needs the browser to transfer focus cross-process on its
// behalf in the focus hierarchy. This may focus an element in the browser ui or
// a cross-process frame, as appropriate.
IPC_MESSAGE_ROUTED1(ViewHostMsg_TakeFocus,
                    bool /* reverse */)

// Required for opening a date/time dialog
IPC_MESSAGE_ROUTED1(ViewHostMsg_OpenDateTimeDialog,
                    ViewHostMsg_DateTimeDialogValue_Params /* value */)

// Sent when the renderer changes its page scale factor.
IPC_MESSAGE_ROUTED1(ViewHostMsg_PageScaleFactorChanged,
                    float /* page_scale_factor */)

// Updates the minimum/maximum allowed zoom percent for this tab from the
// default values.  If |remember| is true, then the zoom setting is applied to
// other pages in the site and is saved, otherwise it only applies to this
// tab.
IPC_MESSAGE_ROUTED2(ViewHostMsg_UpdateZoomLimits,
                    int /* minimum_percent */,
                    int /* maximum_percent */)

// Send back a string to be recorded by UserMetrics.
IPC_MESSAGE_CONTROL1(ViewHostMsg_UserMetricsRecordAction,
                     std::string /* action */)

// Notifies the browser of an event occurring in the media pipeline.
IPC_MESSAGE_CONTROL1(ViewHostMsg_MediaLogEvents,
                     std::vector<media::MediaLogEvent> /* events */)

// Sent once a paint happens after the first non empty layout. In other words,
// after the frame widget has painted something.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidFirstVisuallyNonEmptyPaint)

// Sent once the RenderWidgetCompositor issues a draw command.
IPC_MESSAGE_ROUTED0(ViewHostMsg_DidCommitAndDrawCompositorFrame)

// Adding a new message? Stick to the sort order above: first platform
// independent ViewMsg, then ifdefs for platform specific ViewMsg, then platform
// independent ViewHostMsg, then ifdefs for platform specific ViewHostMsg.

#endif  // CONTENT_COMMON_VIEW_MESSAGES_H_
