// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_MESSAGE_FILTER_H_

#include <stdint.h>

#include <map>
#include <set>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "content/browser/android/java/gin_java_bound_object.h"
#include "content/common/android/gin_java_bridge_errors.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/render_process_host_observer.h"

namespace base {
class ListValue;
}

namespace IPC {
class Message;
}

namespace content {

class GinJavaBridgeDispatcherHost;
class RenderFrameHost;

class GinJavaBridgeMessageFilter : public BrowserMessageFilter,
                                   public RenderProcessHostObserver {
 public:
  // BrowserMessageFilter
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;
  scoped_refptr<base::SequencedTaskRunner> OverrideTaskRunnerForMessage(
      const IPC::Message& message) override;

  // RenderProcessHostObserver
  void RenderProcessExited(RenderProcessHost* rph,
                           const ChildProcessTerminationInfo& info) override;

  // Called on the UI thread.
  void AddRoutingIdForHost(GinJavaBridgeDispatcherHost* host,
                           RenderFrameHost* render_frame_host);
  void RemoveHost(GinJavaBridgeDispatcherHost* host);

  static scoped_refptr<GinJavaBridgeMessageFilter> FromHost(
      GinJavaBridgeDispatcherHost* host, bool create_if_not_exists);

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<GinJavaBridgeMessageFilter>;

  // The filter keeps its own routing map of RenderFrames for two reasons:
  //  1. Message dispatching must be done on the background thread,
  //     without resorting to the UI thread, which can be in fact currently
  //     blocked, waiting for an event from an injected Java object.
  //  2. As RenderFrames pass away earlier than JavaScript wrappers,
  //     messages from the latter can arrive after the RenderFrame has been
  //     removed from the WebContents' routing table.
  typedef std::map<int32_t, scoped_refptr<GinJavaBridgeDispatcherHost>> HostMap;

  GinJavaBridgeMessageFilter();
  ~GinJavaBridgeMessageFilter() override;

  // Called on the background thread.
  scoped_refptr<GinJavaBridgeDispatcherHost> FindHost();
  void OnGetMethods(GinJavaBoundObject::ObjectID object_id,
                    std::set<std::string>* returned_method_names);
  void OnHasMethod(GinJavaBoundObject::ObjectID object_id,
                   const std::string& method_name,
                   bool* result);
  void OnInvokeMethod(GinJavaBoundObject::ObjectID object_id,
                      const std::string& method_name,
                      const base::ListValue& arguments,
                      base::ListValue* result,
                      content::GinJavaBridgeError* error_code);
  void OnObjectWrapperDeleted(GinJavaBoundObject::ObjectID object_id);

  // Accessed both from UI and background threads.
  HostMap hosts_ GUARDED_BY(hosts_lock_);
  base::Lock hosts_lock_;

  // The routing id of the RenderFrameHost whose request we are processing.
  // Used on the background thread.
  int32_t current_routing_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_JAVA_GIN_JAVA_BRIDGE_MESSAGE_FILTER_H_
