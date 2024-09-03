// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/proxy/ppp_instance_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/check.h"
#include "base/functional/bind.h"
#include "build/build_config.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_proxy_delegate.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/url_loader_resource.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_view_api.h"

namespace ppapi {
namespace proxy {

using thunk::EnterInstanceAPINoLock;
using thunk::EnterInstanceNoLock;
using thunk::EnterResourceNoLock;
using thunk::PPB_Instance_API;
using thunk::PPB_View_API;

namespace {

#if !BUILDFLAG(IS_NACL)
PP_Bool DidCreate(PP_Instance instance,
                  uint32_t argc,
                  const char* argn[],
                  const char* argv[]) {
  std::vector<std::string> argn_vect;
  std::vector<std::string> argv_vect;
  for (uint32_t i = 0; i < argc; i++) {
    argn_vect.push_back(std::string(argn[i]));
    argv_vect.push_back(std::string(argv[i]));
  }

  PP_Bool result = PP_FALSE;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPInstance_DidCreate(API_ID_PPP_INSTANCE, instance,
                                         argn_vect, argv_vect, &result));
  return result;
}

void DidDestroy(PP_Instance instance) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPInstance_DidDestroy(API_ID_PPP_INSTANCE, instance));
}

void DidChangeView(PP_Instance instance, PP_Resource view_resource) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);

  EnterResourceNoLock<PPB_View_API> enter_view(view_resource, false);
  CHECK(!enter_view.failed());

  EnterInstanceNoLock enter_instance(instance);
  dispatcher->Send(new PpapiMsg_PPPInstance_DidChangeView(
      API_ID_PPP_INSTANCE, instance, enter_view.object()->GetData(),
      /*flash_fullscreen=*/PP_FALSE));
}

void DidChangeFocus(PP_Instance instance, PP_Bool has_focus) {
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPInstance_DidChangeFocus(API_ID_PPP_INSTANCE,
                                              instance, has_focus));
}

PP_Bool HandleDocumentLoad(PP_Instance instance, PP_Resource url_loader) {
  // This should never get called. Out-of-process document loads are handled
  // specially.
  NOTREACHED();
}

static const PPP_Instance_1_1 instance_interface = {
  &DidCreate,
  &DidDestroy,
  &DidChangeView,
  &DidChangeFocus,
  &HandleDocumentLoad
};
#endif  // !BUILDFLAG(IS_NACL)

}  // namespace

PPP_Instance_Proxy::PPP_Instance_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
  if (dispatcher->IsPlugin()) {
    // The PPP_Instance proxy works by always proxying the 1.1 version of the
    // interface, and then detecting in the plugin process which one to use.
    // PPP_Instance_Combined handles dispatching to whatever interface is
    // supported.
    //
    // This means that if the plugin supports either 1.0 or 1.1 version of
    // the interface, we want to say it supports the 1.1 version since we'll
    // convert it here. This magic conversion code is hardcoded into
    // PluginDispatcher::OnMsgSupportsInterface.
    combined_interface_.reset(PPP_Instance_Combined::Create(
        base::BindRepeating(dispatcher->local_get_interface())));
  }
}

PPP_Instance_Proxy::~PPP_Instance_Proxy() {
}

#if !BUILDFLAG(IS_NACL)
// static
const PPP_Instance* PPP_Instance_Proxy::GetInstanceInterface() {
  return &instance_interface;
}
#endif  // !BUILDFLAG(IS_NACL)

bool PPP_Instance_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Instance_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_DidCreate,
                        OnPluginMsgDidCreate)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_DidDestroy,
                        OnPluginMsgDidDestroy)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_DidChangeView,
                        OnPluginMsgDidChangeView)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_DidChangeFocus,
                        OnPluginMsgDidChangeFocus)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPInstance_HandleDocumentLoad,
                        OnPluginMsgHandleDocumentLoad)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Instance_Proxy::OnPluginMsgDidCreate(
    PP_Instance instance,
    const std::vector<std::string>& argn,
    const std::vector<std::string>& argv,
    PP_Bool* result) {
  *result = PP_FALSE;
  if (argn.size() != argv.size())
    return;

  // Set up the routing associating this new instance with the dispatcher we
  // just got the message from. This must be done before calling into the
  // plugin so it can in turn call PPAPI functions.
  PluginDispatcher* plugin_dispatcher =
      static_cast<PluginDispatcher*>(dispatcher());
  plugin_dispatcher->DidCreateInstance(instance);
  PpapiGlobals::Get()->GetResourceTracker()->DidCreateInstance(instance);

  // Make sure the arrays always have at least one element so we can take the
  // address below.
  std::vector<const char*> argn_array(
      std::max(static_cast<size_t>(1), argn.size()));
  std::vector<const char*> argv_array(
      std::max(static_cast<size_t>(1), argn.size()));
  for (size_t i = 0; i < argn.size(); i++) {
    argn_array[i] = argn[i].c_str();
    argv_array[i] = argv[i].c_str();
  }

  DCHECK(combined_interface_.get());
  *result = combined_interface_->DidCreate(instance,
                                           static_cast<uint32_t>(argn.size()),
                                           &argn_array[0], &argv_array[0]);
}

void PPP_Instance_Proxy::OnPluginMsgDidDestroy(PP_Instance instance) {
  combined_interface_->DidDestroy(instance);

  PpapiGlobals* globals = PpapiGlobals::Get();
  globals->GetResourceTracker()->DidDeleteInstance(instance);
  globals->GetVarTracker()->DidDeleteInstance(instance);

  static_cast<PluginDispatcher*>(dispatcher())->DidDestroyInstance(instance);
}

void PPP_Instance_Proxy::OnPluginMsgDidChangeView(
    PP_Instance instance,
    const ViewData& new_data,
    PP_Bool /*flash_fullscreen*/) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  InstanceData* data = dispatcher->GetInstanceData(instance);
  if (!data)
    return;
  data->view = new_data;

  ScopedPPResource resource(
      ScopedPPResource::PassRef(),
      (new PPB_View_Shared(OBJECT_IS_PROXY,
                           instance, new_data))->GetReference());

  combined_interface_->DidChangeView(instance, resource,
                                     &new_data.rect,
                                     &new_data.clip_rect);
}

void PPP_Instance_Proxy::OnPluginMsgDidChangeFocus(PP_Instance instance,
                                                   PP_Bool has_focus) {
  combined_interface_->DidChangeFocus(instance, has_focus);
}

void PPP_Instance_Proxy::OnPluginMsgHandleDocumentLoad(
    PP_Instance instance,
    int pending_loader_host_id,
    const URLResponseInfoData& data) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return;
  Connection connection(PluginGlobals::Get()->GetBrowserSender(),
                        dispatcher->sender());

  scoped_refptr<URLLoaderResource> loader_resource(
      new URLLoaderResource(connection, instance,
                            pending_loader_host_id, data));

  PP_Resource loader_pp_resource = loader_resource->GetReference();
  if (!combined_interface_->HandleDocumentLoad(instance, loader_pp_resource))
    loader_resource->Close();
  // We don't pass a ref into the plugin, if it wants one, it will have taken
  // an additional one.
  PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(
      loader_pp_resource);
}

}  // namespace proxy
}  // namespace ppapi
