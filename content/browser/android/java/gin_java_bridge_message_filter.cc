// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/gin_java_bridge_message_filter.h"

#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "content/browser/android/java/gin_java_bridge_dispatcher_host.h"
#include "content/browser/android/java/java_bridge_thread.h"
#include "content/browser/renderer_host/agent_scheduling_group_host.h"
#include "content/common/gin_java_bridge_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#error "JavaBridge only supports OS_ANDROID"
#endif

namespace {

const char kGinJavaBridgeMessageFilterKey[] = "GinJavaBridgeMessageFilter";

}  // namespace

namespace content {

GinJavaBridgeMessageFilter::GinJavaBridgeMessageFilter(
    base::PassKey<GinJavaBridgeMessageFilter> pass_key,
    AgentSchedulingGroupHost& agent_scheduling_group)
    : BrowserMessageFilter(GinJavaBridgeMsgStart),
      agent_scheduling_group_(agent_scheduling_group),
      current_routing_id_(MSG_ROUTING_NONE) {}

GinJavaBridgeMessageFilter::~GinJavaBridgeMessageFilter() {
}

void GinJavaBridgeMessageFilter::OnDestruct() const {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    delete this;
  } else {
    GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE, this);
  }
}

bool GinJavaBridgeMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  base::AutoReset<int32_t> routing_id(&current_routing_id_,
                                      message.routing_id());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GinJavaBridgeMessageFilter, message)
    IPC_MESSAGE_HANDLER(GinJavaBridgeHostMsg_GetMethods, OnGetMethods)
    IPC_MESSAGE_HANDLER(GinJavaBridgeHostMsg_HasMethod, OnHasMethod)
    IPC_MESSAGE_HANDLER(GinJavaBridgeHostMsg_InvokeMethod, OnInvokeMethod)
    IPC_MESSAGE_HANDLER(GinJavaBridgeHostMsg_ObjectWrapperDeleted,
                        OnObjectWrapperDeleted)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

scoped_refptr<base::SequencedTaskRunner>
GinJavaBridgeMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  // As the filter is only invoked for the messages of the particular class,
  // we can return the task runner unconditionally.
  return JavaBridgeThread::GetTaskRunner();
}

void GinJavaBridgeMessageFilter::AddRoutingIdForHost(
      GinJavaBridgeDispatcherHost* host,
      RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int routing_id = render_frame_host->GetRoutingID();

  base::AutoLock locker(hosts_lock_);
  hosts_[routing_id] = host;
  hosts_is_in_primary_main_frame_[routing_id] =
      render_frame_host->IsInPrimaryMainFrame();
}

void GinJavaBridgeMessageFilter::RemoveHost(GinJavaBridgeDispatcherHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock locker(hosts_lock_);
  auto iter = hosts_.begin();
  while (iter != hosts_.end()) {
    if (iter->second == host) {
      hosts_is_in_primary_main_frame_.erase(iter->first);
      hosts_.erase(iter++);
    } else {
      ++iter;
    }
  }
}

void GinJavaBridgeMessageFilter::RenderProcessExited(
    RenderProcessHost* rph,
    const ChildProcessTerminationInfo& info) {
#if DCHECK_IS_ON()
  {
    scoped_refptr<GinJavaBridgeMessageFilter> filter =
        base::UserDataAdapter<GinJavaBridgeMessageFilter>::Get(
            &*agent_scheduling_group_, kGinJavaBridgeMessageFilterKey);
    DCHECK_EQ(this, filter.get());
  }
#endif
  // Since the message filter is tied to the `agent_scheduling_group_`,
  // introducing the concept of an AgentSchedulingGroupHostObserver was under
  // consideration, but we did not go with this because:
  //   - For now, this would be the only user of it
  //   - We'd still be interested in responding to RenderProcessExited in the
  //     same way, so the new observer's event would end up just duplicating
  //     this observer method, which seemed unnecessary.
  // We may introduce this interface later once crbug.com/1141459 is fixed.
  rph->RemoveObserver(this);
  // While we're observing the `RenderProcessHost`, the message filter is 1:1
  // with `agent_scheduling_group_`, so at this point we must remove ourselves
  // from the user data, thus destroying `this`. If later the render process is
  // restarted and new frames are created, a new `GinJavaBridgeMessageFilter`
  // will be created and installed on the IPC channel associated with
  // `agent_scheduling_group_`.
  agent_scheduling_group_->RemoveUserData(kGinJavaBridgeMessageFilterKey);

  // DO NOT use `this` from here on, as the object has been destroyed.
}

// static
scoped_refptr<GinJavaBridgeMessageFilter> GinJavaBridgeMessageFilter::FromHost(
    AgentSchedulingGroupHost& agent_scheduling_group,
    bool create_if_not_exists) {
  scoped_refptr<GinJavaBridgeMessageFilter> filter =
      base::UserDataAdapter<GinJavaBridgeMessageFilter>::Get(
          &agent_scheduling_group, kGinJavaBridgeMessageFilterKey);
  if (!filter && create_if_not_exists) {
    filter = base::MakeRefCounted<GinJavaBridgeMessageFilter>(
        base::PassKey<GinJavaBridgeMessageFilter>(), agent_scheduling_group);
    agent_scheduling_group.AddFilter(filter.get());
    agent_scheduling_group.GetProcess()->AddObserver(filter.get());

    agent_scheduling_group.SetUserData(
        kGinJavaBridgeMessageFilterKey,
        std::make_unique<base::UserDataAdapter<GinJavaBridgeMessageFilter>>(
            filter.get()));
  }
  return filter;
}

scoped_refptr<GinJavaBridgeDispatcherHost> GinJavaBridgeMessageFilter::FindHost(
    bool* is_in_primary_main_frame) {
  base::AutoLock locker(hosts_lock_);
  auto iter = hosts_.find(current_routing_id_);
  if (iter != hosts_.end()) {
    if (is_in_primary_main_frame) {
      auto main_frame_iter =
          hosts_is_in_primary_main_frame_.find(current_routing_id_);
      CHECK(main_frame_iter != hosts_is_in_primary_main_frame_.end());

      *is_in_primary_main_frame = main_frame_iter->second;
    }

    return iter->second;
  }

  // Not being able to find a host is OK -- we can receive messages from
  // RenderFrames for which the corresponding host part has already been
  // destroyed. That means, any references to Java objects that the host was
  // holding were already released (with the death of WebContents), so we
  // can just ignore such messages.
  // RenderProcessHostImpl does the same -- if it can't find a listener
  // for the message's routing id, it just drops the message silently.
  // The only action RenderProcessHostImpl does is sending a reply to incoming
  // synchronous messages, but as we handle all our messages using
  // IPC_MESSAGE_HANDLER, the reply will be sent automatically.
  return nullptr;
}

void GinJavaBridgeMessageFilter::OnGetMethods(
    GinJavaBoundObject::ObjectID object_id,
    std::set<std::string>* returned_method_names) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  scoped_refptr<GinJavaBridgeDispatcherHost> host = FindHost();
  if (host) {
    host->OnGetMethods(object_id, returned_method_names);
  } else {
    *returned_method_names = std::set<std::string>();
  }
}

void GinJavaBridgeMessageFilter::OnHasMethod(
    GinJavaBoundObject::ObjectID object_id,
    const std::string& method_name,
    bool* result) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  scoped_refptr<GinJavaBridgeDispatcherHost> host = FindHost();
  if (host) {
    host->OnHasMethod(object_id, method_name, result);
  } else {
    *result = false;
  }
}

void GinJavaBridgeMessageFilter::OnInvokeMethod(
    GinJavaBoundObject::ObjectID object_id,
    const std::string& method_name,
    const base::Value::List& arguments,
    base::Value::List* wrapped_result,
    content::GinJavaBridgeError* error_code) {
  DCHECK(JavaBridgeThread::CurrentlyOn());

  bool is_in_primary_main_frame = false;
  scoped_refptr<GinJavaBridgeDispatcherHost> host =
      FindHost(&is_in_primary_main_frame);

  if (host) {
    UMA_HISTOGRAM_BOOLEAN(
        "Android.WebView.JavaBridge.InvocationIsInPrimaryMainFrame",
        is_in_primary_main_frame);
    host->OnInvokeMethod(current_routing_id_, object_id, method_name, arguments,
                         wrapped_result, error_code);
  } else {
    wrapped_result->Append(base::Value());
    *error_code = kGinJavaBridgeRenderFrameDeleted;
  }
}

void GinJavaBridgeMessageFilter::OnObjectWrapperDeleted(
    GinJavaBoundObject::ObjectID object_id) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  scoped_refptr<GinJavaBridgeDispatcherHost> host = FindHost();
  if (host)
    host->OnObjectWrapperDeleted(current_routing_id_, object_id);
}

}  // namespace content
