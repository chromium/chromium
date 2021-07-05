// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// IPC messages for extensions.

#ifndef EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_
#define EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/values.h"
#include "content/public/common/common_param_traits.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/common/activation_sequence.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/messaging_endpoint.h"
#include "extensions/common/api/messaging/port_context.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/common_param_traits.h"
#include "extensions/common/constants.h"
#include "extensions/common/draggable_region.h"
#include "extensions/common/event_filtering_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_guid.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/message_bundle.h"
#include "extensions/common/mojom/css_origin.mojom-shared.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "extensions/common/mojom/feature_session_type.mojom.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/host_id.mojom.h"
#include "extensions/common/mojom/injection_type.mojom-shared.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "extensions/common/permissions/socket_permission_data.h"
#include "extensions/common/permissions/usb_device_permission_data.h"
#include "extensions/common/stack_frame.h"
#include "extensions/common/user_script.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "ipc/ipc_message_utils.h"
#include "ui/accessibility/ax_param_traits.h"
#include "url/gurl.h"
#include "url/origin.h"

#define IPC_MESSAGE_START ExtensionMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(extensions::mojom::CSSOrigin,
                          extensions::mojom::CSSOrigin::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(content::SocketPermissionRequest::OperationType,
                          content::SocketPermissionRequest::OPERATION_TYPE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(extensions::mojom::RunLocation,
                          extensions::mojom::RunLocation::kMaxValue)

IPC_ENUM_TRAITS_MAX_VALUE(extensions::MessagingEndpoint::Type,
                          extensions::MessagingEndpoint::Type::kLast)

// Parameters structure for ExtensionHostMsg_AddAPIActionToActivityLog and
// ExtensionHostMsg_AddEventToActivityLog.
IPC_STRUCT_BEGIN(ExtensionHostMsg_APIActionOrEvent_Params)
  // API name.
  IPC_STRUCT_MEMBER(std::string, api_call)

  // List of arguments.
  IPC_STRUCT_MEMBER(base::ListValue, arguments)

  // Extra logging information.
  IPC_STRUCT_MEMBER(std::string, extra)
IPC_STRUCT_END()

// Parameters structure for ExtensionHostMsg_AddDOMActionToActivityLog.
IPC_STRUCT_BEGIN(ExtensionHostMsg_DOMAction_Params)
  // URL of the page.
  IPC_STRUCT_MEMBER(GURL, url)

  // Title of the page.
  IPC_STRUCT_MEMBER(std::u16string, url_title)

  // API name.
  IPC_STRUCT_MEMBER(std::string, api_call)

  // List of arguments.
  IPC_STRUCT_MEMBER(base::ListValue, arguments)

  // Type of DOM API call.
  IPC_STRUCT_MEMBER(int, call_type)
IPC_STRUCT_END()

// Parameters structure for ExtensionHostMsg_RequestWorker.
IPC_STRUCT_TRAITS_BEGIN(extensions::mojom::RequestParams)
  // Message name.
  IPC_STRUCT_TRAITS_MEMBER(name)

  // List of message arguments.
  IPC_STRUCT_TRAITS_MEMBER(arguments)

  // Extension ID this request was sent from. This can be empty, in the case
  // where we expose APIs to normal web pages using the extension function
  // system.
  IPC_STRUCT_TRAITS_MEMBER(extension_id)

  // URL of the frame the request was sent from. This isn't necessarily an
  // extension url. Extension requests can also originate from content scripts,
  // in which case extension_id will indicate the ID of the associated
  // extension. Or, they can originate from hosted apps or normal web pages.
  IPC_STRUCT_TRAITS_MEMBER(source_url)

  // Unique request id to match requests and responses.
  IPC_STRUCT_TRAITS_MEMBER(request_id)

  // True if request has a callback specified.
  IPC_STRUCT_TRAITS_MEMBER(has_callback)

  // True if request is executed in response to an explicit user gesture.
  IPC_STRUCT_TRAITS_MEMBER(user_gesture)

  // If this API call is for a service worker, then this is the worker thread
  // id. Otherwise, this is kMainThreadId.
  IPC_STRUCT_TRAITS_MEMBER(worker_thread_id)

  // If this API call is for a service worker, then this is the service
  // worker version id. Otherwise, this is set to
  // blink::mojom::kInvalidServiceWorkerVersionId.
  IPC_STRUCT_TRAITS_MEMBER(service_worker_version_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(extensions::mojom::DispatchEventParams)
  // If this event is for a service worker, then this is the worker thread
  // id. Otherwise, this is 0.
  IPC_STRUCT_TRAITS_MEMBER(worker_thread_id)

  // The id of the extension to dispatch the event to.
  IPC_STRUCT_TRAITS_MEMBER(extension_id)

  // The name of the event to dispatch.
  IPC_STRUCT_TRAITS_MEMBER(event_name)

  // The id of the event for use in the EventAck response message.
  IPC_STRUCT_TRAITS_MEMBER(event_id)

  // Whether or not the event is part of a user gesture.
  IPC_STRUCT_TRAITS_MEMBER(is_user_gesture)

  // Additional filtering info for the event.
  IPC_STRUCT_TRAITS_MEMBER(filtering_info)
IPC_STRUCT_TRAITS_END()

// Struct containing information about the sender of connect() calls that
// originate from a tab.
IPC_STRUCT_BEGIN(ExtensionMsg_TabConnectionInfo)
  // The tab from where the connection was created.
  IPC_STRUCT_MEMBER(base::DictionaryValue, tab)

  // The ID of the frame that initiated the connection.
  // 0 if main frame, positive otherwise. -1 if not initiated from a frame.
  IPC_STRUCT_MEMBER(int, frame_id)
IPC_STRUCT_END()

// Struct containing information about the destination of tab.connect().
IPC_STRUCT_BEGIN(ExtensionMsg_TabTargetConnectionInfo)
  // The destination tab's ID.
  IPC_STRUCT_MEMBER(int, tab_id)

  // Frame ID of the destination. -1 for all frames, 0 for main frame and
  // positive if the destination is a specific child frame.
  IPC_STRUCT_MEMBER(int, frame_id)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(extensions::MessagingEndpoint)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(extension_id)
  IPC_STRUCT_TRAITS_MEMBER(native_app_name)
IPC_STRUCT_TRAITS_END()

// Struct containing the data for external connections to extensions. Used to
// handle the IPCs initiated by both connect() and onConnect().
IPC_STRUCT_BEGIN(ExtensionMsg_ExternalConnectionInfo)
  // The ID of the extension that is the target of the request.
  IPC_STRUCT_MEMBER(std::string, target_id)

  // Specifies the type and the ID of the endpoint that initiated the request.
  IPC_STRUCT_MEMBER(extensions::MessagingEndpoint, source_endpoint)

  // The URL of the frame that initiated the request.
  IPC_STRUCT_MEMBER(GURL, source_url)

  // The origin of the object that initiated the request.
  IPC_STRUCT_MEMBER(absl::optional<url::Origin>, source_origin)

  // The process ID of the webview that initiated the request.
  IPC_STRUCT_MEMBER(int, guest_process_id)

  // The render frame routing ID of the webview that initiated the request.
  IPC_STRUCT_MEMBER(int, guest_render_frame_routing_id)
IPC_STRUCT_END()

IPC_STRUCT_TRAITS_BEGIN(extensions::DraggableRegion)
  IPC_STRUCT_TRAITS_MEMBER(draggable)
  IPC_STRUCT_TRAITS_MEMBER(bounds)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(content::SocketPermissionRequest)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(host)
  IPC_STRUCT_TRAITS_MEMBER(port)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(extensions::PortContext::FrameContext)
  IPC_STRUCT_TRAITS_MEMBER(routing_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(extensions::PortContext::WorkerContext)
  IPC_STRUCT_TRAITS_MEMBER(thread_id)
  IPC_STRUCT_TRAITS_MEMBER(version_id)
  IPC_STRUCT_TRAITS_MEMBER(extension_id)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(extensions::PortContext)
  IPC_STRUCT_TRAITS_MEMBER(frame)
  IPC_STRUCT_TRAITS_MEMBER(worker)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(extensions::SocketPermissionEntry)
  IPC_STRUCT_TRAITS_MEMBER(pattern_)
  IPC_STRUCT_TRAITS_MEMBER(match_subdomains_)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(extensions::SocketPermissionData)
  IPC_STRUCT_TRAITS_MEMBER(entry())
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(extensions::StackFrame)
  IPC_STRUCT_TRAITS_MEMBER(line_number)
  IPC_STRUCT_TRAITS_MEMBER(column_number)
  IPC_STRUCT_TRAITS_MEMBER(source)
  IPC_STRUCT_TRAITS_MEMBER(function)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(extensions::UsbDevicePermissionData)
  IPC_STRUCT_TRAITS_MEMBER(vendor_id())
  IPC_STRUCT_TRAITS_MEMBER(product_id())
  IPC_STRUCT_TRAITS_MEMBER(interface_class())
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(extensions::Message)
  IPC_STRUCT_TRAITS_MEMBER(data)
  IPC_STRUCT_TRAITS_MEMBER(user_gesture)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(extensions::PortId)
  IPC_STRUCT_TRAITS_MEMBER(context_id)
  IPC_STRUCT_TRAITS_MEMBER(port_number)
  IPC_STRUCT_TRAITS_MEMBER(is_opener)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(extensions::EventFilteringInfo)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(service_type)
  IPC_STRUCT_TRAITS_MEMBER(instance_id)
  IPC_STRUCT_TRAITS_MEMBER(window_type)
  IPC_STRUCT_TRAITS_MEMBER(window_exposed_by_default)
IPC_STRUCT_TRAITS_END()

// Singly-included section for custom IPC traits.
#ifndef INTERNAL_EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_
#define INTERNAL_EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_

// Map of extensions IDs to the executing script paths.
typedef std::map<std::string, std::set<std::string> > ExecutingScriptsMap;

struct ExtensionHostMsg_AutomationQuerySelector_Error {
  enum Value { kNone, kNoDocument, kNodeDestroyed };

  ExtensionHostMsg_AutomationQuerySelector_Error() : value(kNone) {}

  Value value;
};

#endif  // INTERNAL_EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_

IPC_ENUM_TRAITS_MAX_VALUE(
    ExtensionHostMsg_AutomationQuerySelector_Error::Value,
    ExtensionHostMsg_AutomationQuerySelector_Error::kNodeDestroyed)

IPC_STRUCT_TRAITS_BEGIN(ExtensionHostMsg_AutomationQuerySelector_Error)
IPC_STRUCT_TRAITS_MEMBER(value)
IPC_STRUCT_TRAITS_END()

// Messages sent from the browser to the renderer:

// Sent to the renderer to dispatch an event to an extension.
// Note: |event_args| is separate from the params to avoid having the message
// take ownership.
IPC_MESSAGE_CONTROL2(ExtensionMsg_DispatchEvent,
                     extensions::mojom::DispatchEventParams /* params */,
                     base::ListValue /* event_args */)

// Tell the render view which browser window it's being attached to.
IPC_MESSAGE_ROUTED1(ExtensionMsg_UpdateBrowserWindowId,
                    int /* id of browser window */)

// The browser's response to the ExtensionMsg_WakeEventPage IPC.
IPC_MESSAGE_CONTROL2(ExtensionMsg_WakeEventPageResponse,
                     int /* request_id */,
                     bool /* success */)

// Check whether the Port for extension messaging exists in a frame or a Service
// Worker. If the port ID is unknown, the frame replies with
// ExtensionHostMsg_CloseMessagePort.
IPC_MESSAGE_ROUTED2(ExtensionMsg_ValidateMessagePort,
                    // For main thread, this is kMainThreadId.
                    int /* worker_thread_id */,
                    extensions::PortId /* port_id */)

// Dispatch the Port.onConnect event for message channels.
IPC_MESSAGE_ROUTED5(ExtensionMsg_DispatchOnConnect,
                    // For main thread, this is kMainThreadId.
                    // TODO(lazyboy): Can this be absl::optional<int> instead?
                    int /* worker_thread_id */,
                    extensions::PortId /* target_port_id */,
                    std::string /* channel_name */,
                    ExtensionMsg_TabConnectionInfo /* source */,
                    ExtensionMsg_ExternalConnectionInfo)

// Deliver a message sent with ExtensionHostMsg_PostMessage.
IPC_MESSAGE_ROUTED3(ExtensionMsg_DeliverMessage,
                    // For main thread, this is kMainThreadId.
                    int /* worker_thread_id */,
                    extensions::PortId /* target_port_id */,
                    extensions::Message)

// Dispatch the Port.onDisconnect event for message channels.
IPC_MESSAGE_ROUTED3(ExtensionMsg_DispatchOnDisconnect,
                    // For main thread, this is kMainThreadId.
                    int /* worker_thread_id */,
                    extensions::PortId /* port_id */,
                    std::string /* error_message */)

// Messages sent from the renderer to the browser:

// Notify the browser that an event has finished being dispatched.
IPC_MESSAGE_ROUTED1(ExtensionHostMsg_EventAck, int /* message_id */)

// Open a channel to all listening contexts owned by the extension with
// the given ID. This responds asynchronously with ExtensionMsg_AssignPortId.
// If an error occurred, the opener will be notified asynchronously.
IPC_MESSAGE_CONTROL4(ExtensionHostMsg_OpenChannelToExtension,
                     extensions::PortContext /* source_context */,
                     ExtensionMsg_ExternalConnectionInfo,
                     std::string /* channel_name */,
                     extensions::PortId /* port_id */)

IPC_MESSAGE_CONTROL3(ExtensionHostMsg_OpenChannelToNativeApp,
                     extensions::PortContext /* source_context */,
                     std::string /* native_app_name */,
                     extensions::PortId /* port_id */)

// Get a port handle to the given tab.  The handle can be used for sending
// messages to the extension.
IPC_MESSAGE_CONTROL5(ExtensionHostMsg_OpenChannelToTab,
                     extensions::PortContext /* source_context */,
                     ExtensionMsg_TabTargetConnectionInfo,
                     std::string /* extension_id */,
                     std::string /* channel_name */,
                     extensions::PortId /* port_id */)

// Sent in response to ExtensionMsg_DispatchOnConnect when the port is accepted.
// The handle is the value returned by ExtensionHostMsg_OpenChannelTo*.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_OpenMessagePort,
                     extensions::PortContext /* port_context */,
                     extensions::PortId /* port_id */)

// Sent in response to ExtensionMsg_DispatchOnConnect and whenever the port is
// closed. The handle is the value returned by ExtensionHostMsg_OpenChannelTo*.
IPC_MESSAGE_CONTROL3(ExtensionHostMsg_CloseMessagePort,
                     extensions::PortContext /* port_context */,
                     extensions::PortId /* port_id */,
                     bool /* force_close */)

// Send a message to an extension process.  The handle is the value returned
// by ExtensionHostMsg_OpenChannelTo*.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_PostMessage,
                     extensions::PortId /* port_id */,
                     extensions::Message)

// Used to get the extension message bundle.
IPC_SYNC_MESSAGE_CONTROL1_1(
    ExtensionHostMsg_GetMessageBundle,
    std::string /* extension id */,
    extensions::MessageBundle::SubstitutionMap /* message bundle */)

// Sent from the renderer to the browser to notify that content scripts are
// running in the renderer that the IPC originated from.
IPC_MESSAGE_ROUTED2(ExtensionHostMsg_ContentScriptsExecuting,
                    ExecutingScriptsMap,
                    GURL /* url of the _topmost_ frame */)

// Optional Ack message sent to the browser to notify that the response to a
// function has been processed.
IPC_MESSAGE_ROUTED1(ExtensionHostMsg_ResponseAck,
                    int /* request_id */)

// Informs the browser to increment the keepalive count for the lazy background
// page, keeping it alive.
IPC_MESSAGE_ROUTED0(ExtensionHostMsg_IncrementLazyKeepaliveCount)

// Informs the browser there is one less thing keeping the lazy background page
// alive.
IPC_MESSAGE_ROUTED0(ExtensionHostMsg_DecrementLazyKeepaliveCount)

// Notify the browser that an app window is ready and can resume resource
// requests.
IPC_MESSAGE_ROUTED0(ExtensionHostMsg_AppWindowReady)

// Sent by the renderer when the draggable regions are updated.
IPC_MESSAGE_ROUTED1(ExtensionHostMsg_UpdateDraggableRegions,
                    std::vector<extensions::DraggableRegion> /* regions */)

// Sent by the renderer to log an API action to the extension activity log.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_AddAPIActionToActivityLog,
                     std::string /* extension_id */,
                     ExtensionHostMsg_APIActionOrEvent_Params)

// Sent by the renderer to log an event to the extension activity log.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_AddEventToActivityLog,
                    std::string /* extension_id */,
                    ExtensionHostMsg_APIActionOrEvent_Params)

// Sent by the renderer to log a DOM action to the extension activity log.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_AddDOMActionToActivityLog,
                     std::string /* extension_id */,
                     ExtensionHostMsg_DOMAction_Params)

// Asks the browser to wake the event page of an extension.
// The browser will reply with ExtensionHostMsg_WakeEventPageResponse.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_WakeEventPage,
                     int /* request_id */,
                     std::string /* extension_id */)

// Tells listeners that a detailed message was reported to the console by
// WebKit.
IPC_MESSAGE_ROUTED4(ExtensionHostMsg_DetailedConsoleMessageAdded,
                    std::u16string /* message */,
                    std::u16string /* source */,
                    extensions::StackTrace /* stack trace */,
                    int32_t /* severity level */)

// Sent when a query selector request is made from the automation API.
// acc_obj_id is the accessibility tree ID of the starting element.
IPC_MESSAGE_ROUTED3(ExtensionMsg_AutomationQuerySelector,
                    int /* request_id */,
                    int /* acc_obj_id */,
                    std::u16string /* selector */)

// Result of a query selector request.
// result_acc_obj_id is the accessibility tree ID of the result element; 0
// indicates no result.
IPC_MESSAGE_ROUTED3(ExtensionHostMsg_AutomationQuerySelector_Result,
                    int /* request_id */,
                    ExtensionHostMsg_AutomationQuerySelector_Error /* error */,
                    int /* result_acc_obj_id */)

// Messages related to Extension Service Worker.
#undef IPC_MESSAGE_START
#define IPC_MESSAGE_START ExtensionWorkerMsgStart
// A service worker thread sends this message when an extension service worker
// starts an API request. The browser will always respond with a
// ExtensionMsg_ResponseWorker.
IPC_MESSAGE_CONTROL1(ExtensionHostMsg_RequestWorker,
                     extensions::mojom::RequestParams)

// The browser sends this message in response to all service worker extension
// api calls. The response data (if any) is one of the base::Value subclasses,
// wrapped as the first element in a ListValue.
IPC_MESSAGE_CONTROL5(ExtensionMsg_ResponseWorker,
                     int /* thread_id */,
                     int /* request_id */,
                     bool /* success */,
                     base::ListValue /* response wrapper (see comment above) */,
                     std::string /* error */)

// Asks the browser to increment the pending activity count for
// the worker with version id |service_worker_version_id|.
// Each request to increment must use unique |request_uuid|. If a request with
// |request_uuid| is already in progress (due to race condition or renderer
// compromise), browser process ignores the IPC.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_IncrementServiceWorkerActivity,
                     int64_t /* service_worker_version_id */,
                     std::string /* request_uuid */)

// Asks the browser to decrement the pending activity count for
// the worker with version id |service_worker_version_id|.
// |request_uuid| must match the GUID of a previous request, otherwise the
// browser process ignores the IPC.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_DecrementServiceWorkerActivity,
                     int64_t /* service_worker_version_id */,
                     std::string /* request_uuid */)

// Tells the browser that an event with |event_id| was successfully dispatched
// to the worker with version |service_worker_version_id|.
IPC_MESSAGE_CONTROL4(ExtensionHostMsg_EventAckWorker,
                     std::string /* extension_id */,
                     int64_t /* service_worker_version_id */,
                     int /* worker_thread_id */,
                     int /* event_id */)

// Tells the browser that an extension service worker context was initialized,
// but possibly didn't start executing its top-level JavaScript.
IPC_MESSAGE_CONTROL3(ExtensionHostMsg_DidInitializeServiceWorkerContext,
                     std::string /* extension_id */,
                     int64_t /* service_worker_version_id */,
                     int /* worker_thread_id */)

// Tells the browser that an extension service worker context has started and
// finished executing its top-level JavaScript.
// Start corresponds to EmbeddedWorkerInstance::OnStarted notification.
//
// TODO(lazyboy): This is a workaround: ideally this IPC should be redundant
// because it directly corresponds to EmbeddedWorkerInstance::OnStarted message.
// However, because OnStarted message is on different mojo IPC pipe, and most
// extension IPCs are on legacy IPC pipe, this IPC is necessary to ensure FIFO
// ordering of this message with rest of the extension IPCs.
// Two possible solutions to this:
//   - Associate extension IPCs with Service Worker IPCs. This can be done (and
//     will be a requirement) when extension IPCs are moved to mojo, but
//     requires resolving or defining ordering dependencies amongst the
//     extension messages, and any additional messages in Chrome.
//   - Make Service Worker IPCs channel-associated so that there's FIFO
//     guarantee between extension IPCs and Service Worker IPCs. This isn't
//     straightforward as it changes SW IPC ordering with respect of rest of
//     Chrome.
// See https://crbug.com/879015#c4 for details.
IPC_MESSAGE_CONTROL5(ExtensionHostMsg_DidStartServiceWorkerContext,
                     std::string /* extension_id */,
                     extensions::ActivationSequence /* activation_sequence */,
                     GURL /* service_worker_scope */,
                     int64_t /* service_worker_version_id */,
                     int /* worker_thread_id */)

// Tells the browser that an extension service worker context has been
// destroyed.
IPC_MESSAGE_CONTROL5(ExtensionHostMsg_DidStopServiceWorkerContext,
                     std::string /* extension_id */,
                     extensions::ActivationSequence /* activation_sequence */,
                     GURL /* service_worker_scope */,
                     int64_t /* service_worker_version_id */,
                     int /* worker_thread_id */)

// Optional Ack message sent to the browser to notify that the response to a
// function has been processed.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_WorkerResponseAck,
                     int /* request_id */,
                     int64_t /* service_worker_version_id */)

IPC_STRUCT_BEGIN(ExtensionMsg_AccessibilityEventBundleParams)
  // ID of the accessibility tree that this event applies to.
  IPC_STRUCT_MEMBER(ui::AXTreeID, tree_id)

  // Zero or more updates to the accessibility tree to apply first.
  IPC_STRUCT_MEMBER(std::vector<ui::AXTreeUpdate>, updates)

  // Zero or more events to fire after the tree updates have been applied.
  IPC_STRUCT_MEMBER(std::vector<ui::AXEvent>, events)

  // The mouse location in screen coordinates.
  IPC_STRUCT_MEMBER(gfx::Point, mouse_location)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ExtensionMsg_AccessibilityLocationChangeParams)
  // ID of the accessibility tree that this event applies to.
  IPC_STRUCT_MEMBER(ui::AXTreeID, tree_id)

  // ID of the object whose location is changing.
  IPC_STRUCT_MEMBER(int, id)

  // The object's new location info.
  IPC_STRUCT_MEMBER(ui::AXRelativeBounds, new_location)
IPC_STRUCT_END()

// Forward an accessibility message to an extension process where an
// extension is using the automation API to listen for accessibility events.
IPC_MESSAGE_CONTROL2(ExtensionMsg_AccessibilityEventBundle,
                     ExtensionMsg_AccessibilityEventBundleParams /* events */,
                     bool /* is_active_profile */)

// Forward an accessibility location change message to an extension process
// where an extension is using the automation API to listen for
// accessibility events.
IPC_MESSAGE_CONTROL1(ExtensionMsg_AccessibilityLocationChange,
                     ExtensionMsg_AccessibilityLocationChangeParams)

#endif  // EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_
