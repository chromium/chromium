// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_core_proxy.h"

#include <stdint.h>
#include <stdlib.h>  // For malloc

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/time_conversion.h"

namespace ppapi {
namespace proxy {

namespace {

void AddRefResource(PP_Resource resource) {
  ppapi::ProxyAutoLock lock;
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(resource);
}

void ReleaseResource(PP_Resource resource) {
  ppapi::ProxyAutoLock lock;
  PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(resource);
}

double GetTime() {
  return TimeToPPTime(base::Time::Now());
}

double GetTimeTicks() {
  return TimeTicksToPPTimeTicks(base::TimeTicks::Now());
}

void CallbackWrapper(PP_CompletionCallback callback, int32_t result) {
  TRACE_EVENT2("ppapi_proxy", "CallOnMainThread callback", "Func",
               reinterpret_cast<void*>(callback.func), "UserData",
               callback.user_data);
  CallWhileUnlocked(PP_RunCompletionCallback, &callback, result);
}

void CallOnMainThread(int delay_in_ms,
                      PP_CompletionCallback callback,
                      int32_t result) {
  DCHECK(callback.func);
#if BUILDFLAG(IS_NACL)
  // Some NaCl apps pass a negative delay, so we just sanitize to 0, to run as
  // soon as possible. MessageLoop checks that the delay is non-negative.
  if (delay_in_ms < 0)
    delay_in_ms = 0;
#endif
  if (!callback.func)
    return;
  ProxyAutoLock lock;

  // If the plugin attempts to call CallOnMainThread from a background thread
  // at shutdown, it's possible that the PpapiGlobals object or the main loop
  // has been destroyed.
  if (!PpapiGlobals::Get() || !PpapiGlobals::Get()->GetMainThreadMessageLoop())
    return;

  PpapiGlobals::Get()->GetMainThreadMessageLoop()->PostDelayedTask(
      FROM_HERE,
      RunWhileLocked(base::BindOnce(&CallbackWrapper, callback, result)),
      base::Milliseconds(delay_in_ms));
}

PP_Bool IsMainThread() {
  return PP_FromBool(PpapiGlobals::Get()->
      GetMainThreadMessageLoop()->BelongsToCurrentThread());
}

const PPB_Core core_interface = {
  &AddRefResource,
  &ReleaseResource,
  &GetTime,
  &GetTimeTicks,
  &CallOnMainThread,
  &IsMainThread
};

}  // namespace

PPB_Core_Proxy::PPB_Core_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppb_core_impl_(NULL) {
  if (!dispatcher->IsPlugin()) {
    ppb_core_impl_ = static_cast<const PPB_Core*>(
        dispatcher->local_get_interface()(PPB_CORE_INTERFACE));
  }
}

PPB_Core_Proxy::~PPB_Core_Proxy() {
}

// static
const PPB_Core* PPB_Core_Proxy::GetPPB_Core_Interface() {
  return &core_interface;
}

bool PPB_Core_Proxy::OnMessageReceived(const IPC::Message& msg) {
#if BUILDFLAG(IS_NACL)
  return false;
#else
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Core_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCore_AddRefResource,
                        OnMsgAddRefResource)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBCore_ReleaseResource,
                        OnMsgReleaseResource)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
#endif
}

#if !BUILDFLAG(IS_NACL)
void PPB_Core_Proxy::OnMsgAddRefResource(const HostResource& resource) {
  ppb_core_impl_->AddRefResource(resource.host_resource());
}

void PPB_Core_Proxy::OnMsgReleaseResource(const HostResource& resource) {
  ppb_core_impl_->ReleaseResource(resource.host_resource());
}
#endif  // !BUILDFLAG(IS_NACL)

}  // namespace proxy
}  // namespace ppapi
