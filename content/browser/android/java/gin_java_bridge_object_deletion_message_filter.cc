// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/gin_java_bridge_object_deletion_message_filter.h"

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "content/browser/android/java/gin_java_bridge_dispatcher_host.h"
#include "content/browser/android/java/java_bridge_thread.h"
#include "content/common/gin_java_bridge_messages.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"

#if !BUILDFLAG(IS_ANDROID)
#error "JavaBridge only supports OS_ANDROID"
#endif

namespace {

const char kGinJavaBridgeObjectDeletionMessageFilterKey[] =
    "GinJavaBridgeObjectDeletionMessageFilter";

}  // namespace

namespace content {

GinJavaBridgeObjectDeletionMessageFilter::
    GinJavaBridgeObjectDeletionMessageFilter(
        base::PassKey<GinJavaBridgeObjectDeletionMessageFilter> pass_key)
    : BrowserMessageFilter(GinJavaBridgeMsgStart),
      current_routing_id_(MSG_ROUTING_NONE) {
  DCHECK(base::FeatureList::IsEnabled(features::kMBIMode));
}

GinJavaBridgeObjectDeletionMessageFilter::
    ~GinJavaBridgeObjectDeletionMessageFilter() {}

void GinJavaBridgeObjectDeletionMessageFilter::OnDestruct() const {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    delete this;
  } else {
    GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE, this);
  }
}

bool GinJavaBridgeObjectDeletionMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  base::AutoReset<int32_t> routing_id(&current_routing_id_,
                                      message.routing_id());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GinJavaBridgeObjectDeletionMessageFilter, message)
    IPC_MESSAGE_HANDLER(GinJavaBridgeHostMsg_ObjectWrapperDeleted,
                        OnObjectWrapperDeleted)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

scoped_refptr<base::SequencedTaskRunner>
GinJavaBridgeObjectDeletionMessageFilter::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  // As the filter is only invoked for the messages of the particular class,
  // we can return the task runner unconditionally.
  return JavaBridgeThread::GetTaskRunner();
}

void GinJavaBridgeObjectDeletionMessageFilter::AddRoutingIdForHost(
    GinJavaBridgeDispatcherHost* host,
    RenderFrameHost* render_frame_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock locker(hosts_lock_);
  hosts_[render_frame_host->GetRoutingID()] = host;
}

void GinJavaBridgeObjectDeletionMessageFilter::RemoveHost(
    GinJavaBridgeDispatcherHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock locker(hosts_lock_);
  auto iter = hosts_.begin();
  while (iter != hosts_.end()) {
    if (iter->second == host)
      hosts_.erase(iter++);
    else
      ++iter;
  }
}

void GinJavaBridgeObjectDeletionMessageFilter::RenderProcessExited(
    RenderProcessHost* rph,
    const ChildProcessTerminationInfo& info) {
#if DCHECK_IS_ON()
  {
    scoped_refptr<GinJavaBridgeObjectDeletionMessageFilter> filter =
        base::UserDataAdapter<GinJavaBridgeObjectDeletionMessageFilter>::Get(
            rph, kGinJavaBridgeObjectDeletionMessageFilterKey);
    DCHECK_EQ(this, filter.get());
  }
#endif
  rph->RemoveObserver(this);
  rph->RemoveUserData(kGinJavaBridgeObjectDeletionMessageFilterKey);

  // DO NOT use `this` from here on, as the object has been destroyed.
}

// static
scoped_refptr<GinJavaBridgeObjectDeletionMessageFilter>
GinJavaBridgeObjectDeletionMessageFilter::FromHost(RenderProcessHost* rph,
                                                   bool create_if_not_exists) {
  DCHECK(base::FeatureList::IsEnabled(features::kMBIMode));
  scoped_refptr<GinJavaBridgeObjectDeletionMessageFilter> filter =
      base::UserDataAdapter<GinJavaBridgeObjectDeletionMessageFilter>::Get(
          rph, kGinJavaBridgeObjectDeletionMessageFilterKey);
  if (!filter && create_if_not_exists) {
    filter = base::MakeRefCounted<GinJavaBridgeObjectDeletionMessageFilter>(
        base::PassKey<GinJavaBridgeObjectDeletionMessageFilter>());
    rph->AddFilter(filter.get());
    rph->AddObserver(filter.get());

    rph->SetUserData(
        kGinJavaBridgeObjectDeletionMessageFilterKey,
        std::make_unique<
            base::UserDataAdapter<GinJavaBridgeObjectDeletionMessageFilter>>(
            filter.get()));
  }
  return filter;
}

scoped_refptr<GinJavaBridgeDispatcherHost>
GinJavaBridgeObjectDeletionMessageFilter::FindHost() {
  base::AutoLock locker(hosts_lock_);
  auto iter = hosts_.find(current_routing_id_);
  if (iter != hosts_.end())
    return iter->second;
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

void GinJavaBridgeObjectDeletionMessageFilter::OnObjectWrapperDeleted(
    GinJavaBoundObject::ObjectID object_id) {
  DCHECK(JavaBridgeThread::CurrentlyOn());
  scoped_refptr<GinJavaBridgeDispatcherHost> host = FindHost();
  if (host)
    host->OnObjectWrapperDeleted(current_routing_id_, object_id);
}

}  // namespace content
