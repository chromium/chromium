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
#include "extensions/common/extensions_client.h"
#include "extensions/common/host_id.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/socket_permission_data.h"
#include "extensions/common/permissions/usb_device_permission_data.h"
#include "extensions/common/stack_frame.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"
#include "extensions/common/view_type.h"
#include "ipc/ipc_message_macros.h"
#include "ui/accessibility/ax_param_traits.h"
#include "url/gurl.h"
#include "url/origin.h"

#define IPC_MESSAGE_START ExtensionMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(extensions::CSSOrigin, extensions::CSS_ORIGIN_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(extensions::ViewType, extensions::VIEW_TYPE_LAST)
IPC_ENUM_TRAITS_MAX_VALUE(content::SocketPermissionRequest::OperationType,
                          content::SocketPermissionRequest::OPERATION_TYPE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(extensions::UserScript::InjectionType,
                          extensions::UserScript::INJECTION_TYPE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(extensions::UserScript::RunLocation,
                          extensions::UserScript::RUN_LOCATION_LAST - 1)

IPC_ENUM_TRAITS_MAX_VALUE(extensions::UserScript::ActionType,
                          extensions::UserScript::ACTION_TYPE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(extensions::MessagingEndpoint::Type,
                          extensions::MessagingEndpoint::Type::kLast)

IPC_ENUM_TRAITS_MAX_VALUE(HostID::HostType, HostID::HOST_TYPE_LAST)

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
  IPC_STRUCT_MEMBER(base::string16, url_title)

  // API name.
  IPC_STRUCT_MEMBER(std::string, api_call)

  // List of arguments.
  IPC_STRUCT_MEMBER(base::ListValue, arguments)

  // Type of DOM API call.
  IPC_STRUCT_MEMBER(int, call_type)
IPC_STRUCT_END()

// Parameters structure for ExtensionHostMsg_Request.
IPC_STRUCT_BEGIN(ExtensionHostMsg_Request_Params)
  // Message name.
  IPC_STRUCT_MEMBER(std::string, name)

  // List of message arguments.
  IPC_STRUCT_MEMBER(base::ListValue, arguments)

  // Extension ID this request was sent from. This can be empty, in the case
  // where we expose APIs to normal web pages using the extension function
  // system.
  IPC_STRUCT_MEMBER(std::string, extension_id)

  // URL of the frame the request was sent from. This isn't necessarily an
  // extension url. Extension requests can also originate from content scripts,
  // in which case extension_id will indicate the ID of the associated
  // extension. Or, they can originate from hosted apps or normal web pages.
  IPC_STRUCT_MEMBER(GURL, source_url)

  // Unique request id to match requests and responses.
  IPC_STRUCT_MEMBER(int, request_id)

  // True if request has a callback specified.
  IPC_STRUCT_MEMBER(bool, has_callback)

  // True if request is executed in response to an explicit user gesture.
  IPC_STRUCT_MEMBER(bool, user_gesture)

  // If this API call is for a service worker, then this is the worker thread
  // id. Otherwise, this is kMainThreadId.
  IPC_STRUCT_MEMBER(int, worker_thread_id)

  // If this API call is for a service worker, then this is the service
  // worker version id. Otherwise, this is set to
  // blink::mojom::kInvalidServiceWorkerVersionId.
  IPC_STRUCT_MEMBER(int64_t, service_worker_version_id)
IPC_STRUCT_END()

IPC_STRUCT_BEGIN(ExtensionMsg_DispatchEvent_Params)
  // If this event is for a service worker, then this is the worker thread
  // id. Otherwise, this is 0.
  IPC_STRUCT_MEMBER(int, worker_thread_id)

  // The id of the extension to dispatch the event to.
  IPC_STRUCT_MEMBER(std::string, extension_id)

  // The name of the event to dispatch.
  IPC_STRUCT_MEMBER(std::string, event_name)

  // The id of the event for use in the EventAck response message.
  IPC_STRUCT_MEMBER(int, event_id)

  // Whether or not the event is part of a user gesture.
  IPC_STRUCT_MEMBER(bool, is_user_gesture)

  // Additional filtering info for the event.
  IPC_STRUCT_MEMBER(extensions::EventFilteringInfo, filtering_info)
IPC_STRUCT_END()

// Allows an extension to execute code in a tab.
IPC_STRUCT_BEGIN(ExtensionMsg_ExecuteCode_Params)
  // The extension API request id, for responding.
  IPC_STRUCT_MEMBER(int, request_id)

  // The ID of the requesting injection host.
  IPC_STRUCT_MEMBER(HostID, host_id)

  // Whether the code is JavaScript or CSS.
  IPC_STRUCT_MEMBER(extensions::UserScript::ActionType, action_type)

  // String of code to execute.
  IPC_STRUCT_MEMBER(std::string, code)

  // The webview guest source who calls to execute code.
  IPC_STRUCT_MEMBER(GURL, webview_src)

  // Whether to inject into about:blank (sub)frames.
  IPC_STRUCT_MEMBER(bool, match_about_blank)

  // When to inject the code.
  IPC_STRUCT_MEMBER(extensions::UserScript::RunLocation, run_at)

  // Whether the request is coming from a <webview>.
  IPC_STRUCT_MEMBER(bool, is_web_view)

  // Whether the caller is interested in the result value. Manifest-declared
  // content scripts and executeScript() calls without a response callback
  // are examples of when this will be false.
  IPC_STRUCT_MEMBER(bool, wants_result)

  // The URL of the script that was injected, if any.
  IPC_STRUCT_MEMBER(GURL, script_url)

  // Whether the code to be executed should be associated with a user gesture.
  IPC_STRUCT_MEMBER(bool, user_gesture)

  // The origin of the CSS.
  IPC_STRUCT_MEMBER(base::Optional<extensions::CSSOrigin>, css_origin)

  // The autogenerated key for the CSS injection.
  IPC_STRUCT_MEMBER(base::Optional<std::string>, injection_key)
IPC_STRUCT_END()

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
  IPC_STRUCT_MEMBER(base::Optional<url::Origin>, source_origin)

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

// Identifier containing info about a service worker, used in event listener
// IPCs.
IPC_STRUCT_BEGIN(ServiceWorkerIdentifier)
  IPC_STRUCT_MEMBER(GURL, scope)
  IPC_STRUCT_MEMBER(int64_t, version_id)
  IPC_STRUCT_MEMBER(int, thread_id)
IPC_STRUCT_END()

// Singly-included section for custom IPC traits.
#ifndef INTERNAL_EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_
#define INTERNAL_EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_

// IPC_MESSAGE macros choke on extra , in the std::map, when expanding. We need
// to typedef it to avoid that.
// Substitution map for l10n messages.
typedef std::map<std::string, std::string> SubstitutionMap;

// Map of extensions IDs to the executing script paths.
typedef std::map<std::string, std::set<std::string> > ExecutingScriptsMap;

struct ExtensionMsg_PermissionSetStruct {
  ExtensionMsg_PermissionSetStruct();
  explicit ExtensionMsg_PermissionSetStruct(
      const extensions::PermissionSet& permissions);
  ~ExtensionMsg_PermissionSetStruct();

  ExtensionMsg_PermissionSetStruct(ExtensionMsg_PermissionSetStruct&& other);
  ExtensionMsg_PermissionSetStruct& operator=(
      ExtensionMsg_PermissionSetStruct&& other);

  std::unique_ptr<const extensions::PermissionSet> ToPermissionSet() const;

  extensions::APIPermissionSet apis;
  extensions::ManifestPermissionSet manifest_permissions;
  extensions::URLPatternSet explicit_hosts;
  extensions::URLPatternSet scriptable_hosts;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMsg_PermissionSetStruct);
};

struct ExtensionMsg_Loaded_Params {
  ExtensionMsg_Loaded_Params();
  ~ExtensionMsg_Loaded_Params();
  ExtensionMsg_Loaded_Params(const extensions::Extension* extension,
                             bool include_tab_permissions,
                             base::Optional<extensions::ActivationSequence>
                                 worker_activation_sequence);

  ExtensionMsg_Loaded_Params(ExtensionMsg_Loaded_Params&& other);
  ExtensionMsg_Loaded_Params& operator=(ExtensionMsg_Loaded_Params&& other);

  // Creates a new extension from the data in this object.
  // A context_id needs to be passed because each browser context can have
  // different values for default_policy_blocked/allowed_hosts.
  // (see extension_util.cc#GetBrowserContextId)
  scoped_refptr<extensions::Extension> ConvertToExtension(
      int context_id,
      std::string* error) const;

  // The subset of the extension manifest data we send to renderers.
  base::DictionaryValue manifest;

  // The location the extension was installed from.
  extensions::Manifest::Location location;

  // The path the extension was loaded from. This is used in the renderer only
  // to generate the extension ID for extensions that are loaded unpacked.
  base::FilePath path;

  // The extension's active and withheld permissions.
  ExtensionMsg_PermissionSetStruct active_permissions;
  ExtensionMsg_PermissionSetStruct withheld_permissions;
  std::map<int, ExtensionMsg_PermissionSetStruct> tab_specific_permissions;

  // Contains URLPatternSets defining which URLs an extension may not interact
  // with by policy.
  extensions::URLPatternSet policy_blocked_hosts;
  extensions::URLPatternSet policy_allowed_hosts;

  // If the extension uses the default list of blocked / allowed URLs.
  bool uses_default_policy_blocked_allowed_hosts = true;

  // We keep this separate so that it can be used in logging.
  std::string id;

  // If this extension is Service Worker based, then this contains the
  // activation sequence of the extension.
  base::Optional<extensions::ActivationSequence> worker_activation_sequence;

  // Send creation flags so extension is initialized identically.
  int creation_flags;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionMsg_Loaded_Params);
};

struct ExtensionHostMsg_AutomationQuerySelector_Error {
  enum Value { kNone, kNoDocument, kNodeDestroyed };

  ExtensionHostMsg_AutomationQuerySelector_Error() : value(kNone) {}

  Value value;
};

namespace IPC {

template <>
struct ParamTraits<URLPattern> {
  typedef URLPattern param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<extensions::URLPatternSet> {
  typedef extensions::URLPatternSet param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<extensions::APIPermission::ID> {
  typedef extensions::APIPermission::ID param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<extensions::APIPermissionSet> {
  typedef extensions::APIPermissionSet param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<extensions::ManifestPermissionSet> {
  typedef extensions::ManifestPermissionSet param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<HostID> {
  typedef HostID param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ExtensionMsg_PermissionSetStruct> {
  typedef ExtensionMsg_PermissionSetStruct param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct ParamTraits<ExtensionMsg_Loaded_Params> {
  typedef ExtensionMsg_Loaded_Params param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // INTERNAL_EXTENSIONS_COMMON_EXTENSION_MESSAGES_H_

IPC_ENUM_TRAITS_MAX_VALUE(
    ExtensionHostMsg_AutomationQuerySelector_Error::Value,
    ExtensionHostMsg_AutomationQuerySelector_Error::kNodeDestroyed)

IPC_STRUCT_TRAITS_BEGIN(ExtensionHostMsg_AutomationQuerySelector_Error)
IPC_STRUCT_TRAITS_MEMBER(value)
IPC_STRUCT_TRAITS_END()

// Parameters structure for ExtensionMsg_UpdatePermissions.
IPC_STRUCT_BEGIN(ExtensionMsg_UpdatePermissions_Params)
  IPC_STRUCT_MEMBER(std::string, extension_id)
  IPC_STRUCT_MEMBER(ExtensionMsg_PermissionSetStruct, active_permissions)
  IPC_STRUCT_MEMBER(ExtensionMsg_PermissionSetStruct, withheld_permissions)
  IPC_STRUCT_MEMBER(extensions::URLPatternSet, policy_blocked_hosts)
  IPC_STRUCT_MEMBER(extensions::URLPatternSet, policy_allowed_hosts)
  IPC_STRUCT_MEMBER(bool, uses_default_policy_host_restrictions)
IPC_STRUCT_END()

// Parameters structure for ExtensionMsg_UpdateDefaultPolicyHostRestrictions.
IPC_STRUCT_BEGIN(ExtensionMsg_UpdateDefaultPolicyHostRestrictions_Params)
  IPC_STRUCT_MEMBER(extensions::URLPatternSet, default_policy_blocked_hosts)
  IPC_STRUCT_MEMBER(extensions::URLPatternSet, default_policy_allowed_hosts)
IPC_STRUCT_END()

// Messages sent from the browser to the renderer:

// The browser sends this message in response to all extension api calls. The
// response data (if any) is one of the base::Value subclasses, wrapped as the
// first element in a ListValue.
IPC_MESSAGE_ROUTED4(ExtensionMsg_Response,
                    int /* request_id */,
                    bool /* success */,
                    base::ListValue /* response wrapper (see comment above) */,
                    std::string /* error */)

// Sent to the renderer to dispatch an event to an extension.
// Note: |event_args| is separate from the params to avoid having the message
// take ownership.
IPC_MESSAGE_CONTROL2(ExtensionMsg_DispatchEvent,
                     ExtensionMsg_DispatchEvent_Params /* params */,
                     base::ListValue /* event_args */)

// This message is optionally routed.  If used as a control message, it will
// call a javascript function |function_name| from module |module_name| in
// every registered context in the target process.  If routed, it will be
// restricted to the contexts that are part of the target RenderView.
//
// If |extension_id| is non-empty, the function will be invoked only in
// contexts owned by the extension. |args| is a list of primitive Value types
// that are passed to the function.
IPC_MESSAGE_ROUTED4(ExtensionMsg_MessageInvoke,
                    std::string /* extension_id */,
                    std::string /* module_name */,
                    std::string /* function_name */,
                    base::ListValue /* args */)

// Set the top-level frame to the provided name.
IPC_MESSAGE_ROUTED1(ExtensionMsg_SetFrameName,
                    std::string /* frame_name */)

// Tell the renderer process the platforms system font.
IPC_MESSAGE_CONTROL2(ExtensionMsg_SetSystemFont,
                     std::string /* font_family */,
                     std::string /* font_size */)

// Marks an extension as 'active' in an extension process. 'Active' extensions
// have more privileges than other extension content that might end up running
// in the process (e.g. because of iframes or content scripts).
IPC_MESSAGE_CONTROL1(ExtensionMsg_ActivateExtension,
                     std::string /* extension_id */)

// Notifies the renderer that extensions were loaded in the browser.
IPC_MESSAGE_CONTROL1(ExtensionMsg_Loaded,
                     std::vector<ExtensionMsg_Loaded_Params>)

// Notifies the renderer that an extension was unloaded in the browser.
IPC_MESSAGE_CONTROL1(ExtensionMsg_Unloaded,
                     std::string)

// Updates the scripting allowlist for extensions in the render process. This is
// only used for testing.
IPC_MESSAGE_CONTROL1(ExtensionMsg_SetScriptingAllowlist,
                     // extension ids
                     extensions::ExtensionsClient::ScriptingAllowlist)

// Notification that renderer should run some JavaScript code.
IPC_MESSAGE_ROUTED1(ExtensionMsg_ExecuteCode,
                    ExtensionMsg_ExecuteCode_Params)

// Notification that the user scripts have been updated. It has one
// ReadOnlySharedMemoryRegion argument consisting of the pickled script data.
// This memory region is valid in the context of the renderer.
// If |owner| is not empty, then the shared memory handle refers to |owner|'s
// programmatically-defined scripts. Otherwise, the handle refers to all
// hosts' statically defined scripts. So far, only extension-hosts support
// statically defined scripts; WebUI-hosts don't.
// If |changed_hosts| is not empty, only the host in that set will
// be updated. Otherwise, all hosts that have scripts in the shared memory
// region will be updated. Note that the empty set => all hosts case is not
// supported for per-extension programmatically-defined script regions; in such
// regions, the owner is expected to list itself as the only changed host.
// If |whitelisted_only| is true, this process should only run whitelisted
// scripts and not all user scripts.
IPC_MESSAGE_CONTROL4(ExtensionMsg_UpdateUserScripts,
                     base::ReadOnlySharedMemoryRegion,
                     HostID /* owner */,
                     std::set<HostID> /* changed hosts */,
                     bool /* whitelisted_only */)

// Trigger to execute declarative content script under browser control.
IPC_MESSAGE_ROUTED4(ExtensionMsg_ExecuteDeclarativeScript,
                    int /* tab identifier */,
                    extensions::ExtensionId /* extension identifier */,
                    int /* script identifier */,
                    GURL /* page URL where script should be injected */)

// Tell the render view which browser window it's being attached to.
IPC_MESSAGE_ROUTED1(ExtensionMsg_UpdateBrowserWindowId,
                    int /* id of browser window */)

// Tell the render view what its tab ID is.
IPC_MESSAGE_ROUTED1(ExtensionMsg_SetTabId,
                    int /* id of tab */)

// Tell the renderer to update an extension's permission set.
IPC_MESSAGE_CONTROL1(ExtensionMsg_UpdatePermissions,
                     ExtensionMsg_UpdatePermissions_Params)

// Tell the renderer to update an extension's policy_blocked_hosts set.
IPC_MESSAGE_CONTROL1(ExtensionMsg_UpdateDefaultPolicyHostRestrictions,
                     ExtensionMsg_UpdateDefaultPolicyHostRestrictions_Params)

// Tell the render view about new tab-specific permissions for an extension.
IPC_MESSAGE_CONTROL5(ExtensionMsg_UpdateTabSpecificPermissions,
                     GURL /* url */,
                     std::string /* extension_id */,
                     extensions::URLPatternSet /* hosts */,
                     bool /* update origin whitelist */,
                     int /* tab_id */)

// Tell the render view to clear tab-specific permissions for some extensions.
IPC_MESSAGE_CONTROL3(ExtensionMsg_ClearTabSpecificPermissions,
                     std::vector<std::string> /* extension_ids */,
                     bool /* update origin whitelist */,
                     int /* tab_id */)

// Tell the renderer which type this view is.
IPC_MESSAGE_ROUTED1(ExtensionMsg_NotifyRenderViewType,
                    extensions::ViewType /* view_type */)

// The browser's response to the ExtensionMsg_WakeEventPage IPC.
IPC_MESSAGE_CONTROL2(ExtensionMsg_WakeEventPageResponse,
                     int /* request_id */,
                     bool /* success */)

// Ask the lazy background page if it is ready to be suspended. This is sent
// when the page is considered idle. The renderer will reply with the same
// sequence_id so that we can tell which message it is responding to.
IPC_MESSAGE_CONTROL2(ExtensionMsg_ShouldSuspend,
                     std::string /* extension_id */,
                     uint64_t /* sequence_id */)

// If we complete a round of ShouldSuspend->ShouldSuspendAck messages without
// the lazy background page becoming active again, we are ready to unload. This
// message tells the page to dispatch the suspend event.
IPC_MESSAGE_CONTROL1(ExtensionMsg_Suspend,
                     std::string /* extension_id */)

// The browser changed its mind about suspending this extension.
IPC_MESSAGE_CONTROL1(ExtensionMsg_CancelSuspend,
                     std::string /* extension_id */)

// Response to the renderer for ExtensionHostMsg_GetAppInstallState.
IPC_MESSAGE_ROUTED2(ExtensionMsg_GetAppInstallStateResponse,
                    std::string /* state */,
                    int32_t /* callback_id */)

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
                    // TODO(lazyboy): Can this be base::Optional<int> instead?
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

// Informs the renderer what channel (dev, beta, stable, etc) and user session
// type is running.
IPC_MESSAGE_CONTROL3(ExtensionMsg_SetSessionInfo,
                     version_info::Channel /* channel */,
                     extensions::FeatureSessionType /* session_type */,
                     bool /* is_lock_screen_context */)

// Notify the renderer that its window has closed.
IPC_MESSAGE_ROUTED1(ExtensionMsg_AppWindowClosed, bool /* send_onclosed */)

// Notify the renderer that an extension wants notifications when certain
// searches match the active page.  This message replaces the old set of
// searches, and triggers ExtensionHostMsg_OnWatchedPageChange messages from
// each tab to keep the browser updated about changes.
IPC_MESSAGE_CONTROL1(ExtensionMsg_WatchPages,
                     std::vector<std::string> /* CSS selectors */)

// Send by the browser to indicate a Blob handle has been transferred to the
// renderer. This is sent after the actual extension response, and depends on
// the sequential nature of IPCs so that the blob has already been caught.
// This is a separate control message, so that the renderer process will send
// an acknowledgement even if the RenderView has closed or navigated away.
IPC_MESSAGE_CONTROL1(ExtensionMsg_TransferBlobs,
                     std::vector<std::string> /* blob_uuids */)

// Report the WebView partition ID to the WebView guest renderer process.
IPC_MESSAGE_CONTROL1(ExtensionMsg_SetWebViewPartitionID,
                     std::string /* webview_partition_id */)

// Enable or disable spatial navigation.
IPC_MESSAGE_ROUTED1(ExtensionMsg_SetSpatialNavigationEnabled,
                    bool /* spatial_nav_enabled */)

// Messages sent from the renderer to the browser:

// A renderer sends this message when an extension process starts an API
// request. The browser will always respond with a ExtensionMsg_Response.
IPC_MESSAGE_ROUTED1(ExtensionHostMsg_Request,
                    ExtensionHostMsg_Request_Params)

// Notify the browser that the given extension added a listener to an event.
IPC_MESSAGE_CONTROL5(ExtensionHostMsg_AddListener,
                     std::string /* extension_id */,
                     GURL /* listener_or_worker_scope_url */,
                     std::string /* name */,
                     int64_t /* service_worker_version_id */,
                     int /* worker_thread_id */)

// Notify the browser that the given extension removed a listener from an
// event.
IPC_MESSAGE_CONTROL5(ExtensionHostMsg_RemoveListener,
                     std::string /* extension_id */,
                     GURL /* listener_or_worker_scope_url */,
                     std::string /* name */,
                     int64_t /* service_worker_version_id */,
                     int /* worker_thread_id */)

// Notify the browser that the given extension added a listener to an event from
// a lazy background page.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_AddLazyListener,
                     std::string /* extension_id */,
                     std::string /* name */)

// Notify the browser that the given extension is no longer interested in
// receiving the given event from a lazy background page.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_RemoveLazyListener,
                     std::string /* extension_id */,
                     std::string /* event_name */)

// Notify the browser that the given extension added a listener to an event from
// an extension service worker.
IPC_MESSAGE_CONTROL3(ExtensionHostMsg_AddLazyServiceWorkerListener,
                     std::string /* extension_id */,
                     std::string /* name */,
                     GURL /* service_worker_scope */)

// Notify the browser that the given extension is no longer interested in
// receiving the given event from an extension service worker.
IPC_MESSAGE_CONTROL3(ExtensionHostMsg_RemoveLazyServiceWorkerListener,
                     std::string /* extension_id */,
                     std::string /* name */,
                     GURL /* service_worker_scope */)

// Notify the browser that the given extension added a listener to instances of
// the named event that satisfy the filter.
// If |sw_identifier| is specified, it implies that the listener is for a
// service worker, and the param is used to identify the worker.
IPC_MESSAGE_CONTROL5(
    ExtensionHostMsg_AddFilteredListener,
    std::string /* extension_id */,
    std::string /* name */,
    base::Optional<ServiceWorkerIdentifier> /* sw_identifier */,
    base::DictionaryValue /* filter */,
    bool /* lazy */)

// Notify the browser that the given extension is no longer interested in
// instances of the named event that satisfy the filter.
// If |sw_identifier| is specified, it implies that the listener is for a
// service worker, and the param is used to identify the worker.
IPC_MESSAGE_CONTROL5(
    ExtensionHostMsg_RemoveFilteredListener,
    std::string /* extension_id */,
    std::string /* name */,
    base::Optional<ServiceWorkerIdentifier> /* sw_identifier */,
    base::DictionaryValue /* filter */,
    bool /* lazy */)

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
IPC_SYNC_MESSAGE_CONTROL1_1(ExtensionHostMsg_GetMessageBundle,
                            std::string /* extension id */,
                            SubstitutionMap /* message bundle */)

// Sent from the renderer to the browser to return the script running result.
IPC_MESSAGE_ROUTED4(
    ExtensionHostMsg_ExecuteCodeFinished,
    int /* request id */,
    std::string /* error; empty implies success */,
    GURL /* URL of the code executed on. May be empty if unsuccessful. */,
    base::ListValue /* result of the script */)

// Sent from the renderer to the browser to notify that content scripts are
// running in the renderer that the IPC originated from.
IPC_MESSAGE_ROUTED2(ExtensionHostMsg_ContentScriptsExecuting,
                    ExecutingScriptsMap,
                    GURL /* url of the _topmost_ frame */)

// Sent from the renderer to the browser to request permission for a script
// injection.
// If request id is -1, this signals that the request has already ran, and this
// merely serves as a notification. This happens when the feature to disable
// scripts running without user consent is not enabled.
IPC_MESSAGE_ROUTED4(ExtensionHostMsg_RequestScriptInjectionPermission,
                    std::string /* extension id */,
                    extensions::UserScript::InjectionType /* script type */,
                    extensions::UserScript::RunLocation /* run location */,
                    int64_t /* request id */)

// Sent from the browser to the renderer in reply to a
// RequestScriptInjectionPermission message, granting permission for a script
// script to run.
IPC_MESSAGE_ROUTED1(ExtensionMsg_PermitScriptInjection,
                    int64_t /* request id */)

// Sent by the renderer when a web page is checking if its app is installed.
IPC_MESSAGE_ROUTED3(ExtensionHostMsg_GetAppInstallState,
                    GURL /* requestor_url */,
                    int32_t /* return_route_id */,
                    int32_t /* callback_id */)

// Optional Ack message sent to the browser to notify that the response to a
// function has been processed.
IPC_MESSAGE_ROUTED1(ExtensionHostMsg_ResponseAck,
                    int /* request_id */)

// Response to ExtensionMsg_ShouldSuspend.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_ShouldSuspendAck,
                     std::string /* extension_id */,
                     uint64_t /* sequence_id */)

// Response to ExtensionMsg_Suspend, after we dispatch the suspend event.
IPC_MESSAGE_CONTROL1(ExtensionHostMsg_SuspendAck,
                     std::string /* extension_id */)

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

// Notifies the browser process that a tab has started or stopped matching
// certain conditions.  This message is sent in response to several events:
//
// * ExtensionMsg_WatchPages was received, updating the set of conditions.
// * A new page is loaded.  This will be sent after
//   mojom::FrameHost::DidCommitProvisionalLoad. Currently this only fires for
//   the main frame.
// * Something changed on an existing frame causing the set of matching searches
//   to change.
IPC_MESSAGE_ROUTED1(ExtensionHostMsg_OnWatchedPageChange,
                    std::vector<std::string> /* Matching CSS selectors */)

// Sent by the renderer when it has received a Blob handle from the browser.
IPC_MESSAGE_CONTROL1(ExtensionHostMsg_TransferBlobsAck,
                     std::vector<std::string> /* blob_uuids */)

// Asks the browser to wake the event page of an extension.
// The browser will reply with ExtensionHostMsg_WakeEventPageResponse.
IPC_MESSAGE_CONTROL2(ExtensionHostMsg_WakeEventPage,
                     int /* request_id */,
                     std::string /* extension_id */)

// Tells listeners that a detailed message was reported to the console by
// WebKit.
IPC_MESSAGE_ROUTED4(ExtensionHostMsg_DetailedConsoleMessageAdded,
                    base::string16 /* message */,
                    base::string16 /* source */,
                    extensions::StackTrace /* stack trace */,
                    int32_t /* severity level */)

// Sent when a query selector request is made from the automation API.
// acc_obj_id is the accessibility tree ID of the starting element.
IPC_MESSAGE_ROUTED3(ExtensionMsg_AutomationQuerySelector,
                    int /* request_id */,
                    int /* acc_obj_id */,
                    base::string16 /* selector */)

// Result of a query selector request.
// result_acc_obj_id is the accessibility tree ID of the result element; 0
// indicates no result.
IPC_MESSAGE_ROUTED3(ExtensionHostMsg_AutomationQuerySelector_Result,
                    int /* request_id */,
                    ExtensionHostMsg_AutomationQuerySelector_Error /* error */,
                    int /* result_acc_obj_id */)

// Tells the renderer whether or not activity logging is enabled. This is only
// sent if logging is or was previously enabled; not being enabled is assumed
// otherwise.
IPC_MESSAGE_CONTROL1(ExtensionMsg_SetActivityLoggingEnabled, bool /* enabled */)

// Messages related to Extension Service Worker.
#undef IPC_MESSAGE_START
#define IPC_MESSAGE_START ExtensionWorkerMsgStart
// A service worker thread sends this message when an extension service worker
// starts an API request. The browser will always respond with a
// ExtensionMsg_ResponseWorker.
IPC_MESSAGE_CONTROL1(ExtensionHostMsg_RequestWorker,
                     ExtensionHostMsg_Request_Params)

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
