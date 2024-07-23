// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/proxy/ppp_printing_proxy.h"

#include <string.h>

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace ppapi {
namespace proxy {

namespace {

#if !BUILDFLAG(IS_NACL)
bool HasPrintingPermission(PP_Instance instance) {
  Dispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return false;
  return dispatcher->permissions().HasPermission(PERMISSION_DEV);
}

uint32_t QuerySupportedFormats(PP_Instance instance) {
  if (!HasPrintingPermission(instance))
    return 0;
  uint32_t result = 0;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPrinting_QuerySupportedFormats(API_ID_PPP_PRINTING,
                                                     instance, &result));
  return result;
}

int32_t Begin(PP_Instance instance,
              const PP_PrintSettings_Dev* print_settings) {
  if (!HasPrintingPermission(instance))
    return 0;

  int32_t result = 0;
  HostDispatcher::GetForInstance(instance)->Send(new PpapiMsg_PPPPrinting_Begin(
      API_ID_PPP_PRINTING, instance, *print_settings, &result));
  return result;
}

PP_Resource PrintPages(PP_Instance instance,
                       const PP_PrintPageNumberRange_Dev* page_ranges,
                       uint32_t page_range_count) {
  if (!HasPrintingPermission(instance))
    return 0;
  std::vector<PP_PrintPageNumberRange_Dev> pages(
      page_ranges, page_ranges + page_range_count);

  HostResource result;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPrinting_PrintPages(API_ID_PPP_PRINTING,
                                          instance, pages, &result));

  // Explicilty don't add a reference to the received resource here. The plugin
  // adds a ref during creation of the resource and it will "abandon" the
  // resource to release it, which ensures that the initial ref won't be
  // decremented. See the comment below in OnPluginMsgPrintPages.

  return result.host_resource();
}

void End(PP_Instance instance) {
  if (!HasPrintingPermission(instance))
    return;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPrinting_End(API_ID_PPP_PRINTING, instance));
}

PP_Bool IsScalingDisabled(PP_Instance instance) {
  if (!HasPrintingPermission(instance))
    return PP_FALSE;
  bool result = false;
  HostDispatcher::GetForInstance(instance)->Send(
      new PpapiMsg_PPPPrinting_IsScalingDisabled(API_ID_PPP_PRINTING,
                                                 instance, &result));
  return PP_FromBool(result);
}

const PPP_Printing_Dev ppp_printing_interface = {
  &QuerySupportedFormats,
  &Begin,
  &PrintPages,
  &End,
  &IsScalingDisabled
};
#else
// The NaCl plugin doesn't need the host side interface - stub it out.
static const PPP_Printing_Dev ppp_printing_interface = {};
#endif  // !BUILDFLAG(IS_NACL)

}  // namespace

PPP_Printing_Proxy::PPP_Printing_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_printing_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_printing_impl_ = static_cast<const PPP_Printing_Dev*>(
        dispatcher->local_get_interface()(PPP_PRINTING_DEV_INTERFACE));
  }
}

PPP_Printing_Proxy::~PPP_Printing_Proxy() {
}

// static
const PPP_Printing_Dev* PPP_Printing_Proxy::GetProxyInterface() {
  return &ppp_printing_interface;
}

bool PPP_Printing_Proxy::OnMessageReceived(const IPC::Message& msg) {
  if (!dispatcher()->IsPlugin())
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Printing_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPrinting_QuerySupportedFormats,
                        OnPluginMsgQuerySupportedFormats)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPrinting_Begin,
                        OnPluginMsgBegin)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPrinting_PrintPages,
                        OnPluginMsgPrintPages)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPrinting_End,
                        OnPluginMsgEnd)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPPrinting_IsScalingDisabled,
                        OnPluginMsgIsScalingDisabled)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Printing_Proxy::OnPluginMsgQuerySupportedFormats(PP_Instance instance,
                                                          uint32_t* result) {
  if (ppp_printing_impl_) {
    *result = CallWhileUnlocked(ppp_printing_impl_->QuerySupportedFormats,
                                instance);
  } else {
    *result = 0;
  }
}

void PPP_Printing_Proxy::OnPluginMsgBegin(PP_Instance instance,
                                          const PP_PrintSettings_Dev& settings,
                                          int32_t* result) {
  if (!ppp_printing_impl_) {
    *result = 0;
    return;
  }

  *result = CallWhileUnlocked(ppp_printing_impl_->Begin, instance, &settings);
}

void PPP_Printing_Proxy::OnPluginMsgPrintPages(
    PP_Instance instance,
    const std::vector<PP_PrintPageNumberRange_Dev>& pages,
    HostResource* result) {
  if (!ppp_printing_impl_ || pages.empty())
    return;

  PP_Resource plugin_resource = CallWhileUnlocked(
      ppp_printing_impl_->PrintPages,
      instance, &pages[0], base::checked_cast<uint32_t>(pages.size()));
  ResourceTracker* resource_tracker = PpapiGlobals::Get()->GetResourceTracker();
  Resource* resource_object = resource_tracker->GetResource(plugin_resource);
  if (!resource_object)
    return;

  *result = resource_object->host_resource();

  // Abandon the resource on the plugin side. This releases a reference to the
  // resource and allows the plugin side of the resource (the proxy resource) to
  // be destroyed without sending a message to the renderer notifing it that the
  // plugin has released the resource. This used to call
  // ResourceTracker::ReleaseResource directly which would trigger an IPC to be
  // sent to the renderer to remove a ref to the resource. However due to
  // arbitrary ordering of received sync/async IPCs in the renderer, this
  // sometimes resulted in the resource being destroyed in the renderer before
  // the renderer had a chance to add a reference to it. See crbug.com/490611.
  static_cast<PluginResourceTracker*>(resource_tracker)
      ->AbandonResource(plugin_resource);
}

void PPP_Printing_Proxy::OnPluginMsgEnd(PP_Instance instance) {
  if (ppp_printing_impl_)
    CallWhileUnlocked(ppp_printing_impl_->End, instance);
}

void PPP_Printing_Proxy::OnPluginMsgIsScalingDisabled(PP_Instance instance,
                                                      bool* result) {
  if (ppp_printing_impl_) {
    *result = PP_ToBool(CallWhileUnlocked(ppp_printing_impl_->IsScalingDisabled,
                                          instance));
  } else {
    *result = false;
  }
}

}  // namespace proxy
}  // namespace ppapi
