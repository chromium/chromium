// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_BROWSER_CONNECTION_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_BROWSER_CONNECTION_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "base/functional/callback.h"
#include "content/common/pepper_plugin.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"

class GURL;

namespace content {

// This class represents a connection from the renderer to the browser for
// sending/receiving pepper ResourceHost related messages. When the browser
// and renderer communicate about ResourceHosts, they should pass the plugin
// process ID to identify which plugin they are talking about.
class PepperBrowserConnection
    : public RenderFrameObserver,
      public RenderFrameObserverTracker<PepperBrowserConnection> {
 public:
  using PendingResourceIDCallback =
      base::OnceCallback<void(const std::vector<int>&)>;
  explicit PepperBrowserConnection(RenderFrame* render_frame);

  PepperBrowserConnection(const PepperBrowserConnection&) = delete;
  PepperBrowserConnection& operator=(const PepperBrowserConnection&) = delete;

  ~PepperBrowserConnection() override;

  bool OnMessageReceived(const IPC::Message& message) override;

  // TODO(teravest): Instead of having separate methods per message, we should
  // add generic functionality similar to PluginResource::Call().

  // Sends a request to the browser to create ResourceHosts for the given
  // |instance| of a plugin identified by |child_process_id|. |callback| will be
  // run when a reply is received with the pending resource IDs.
  void SendBrowserCreate(PP_Instance instance,
                         int child_process_id,
                         const std::vector<IPC::Message>& create_messages,
                         PendingResourceIDCallback callback);

  // Called when the renderer creates an in-process instance.
  void DidCreateInProcessInstance(PP_Instance instance,
                                  int render_frame_id,
                                  const GURL& document_url,
                                  const GURL& plugin_url);

  // Called when the renderer deletes an in-process instance.
  void DidDeleteInProcessInstance(PP_Instance instance);

  // Called when the renderer creates an out of process instance.
  void DidCreateOutOfProcessPepperInstance(int32_t plugin_child_id,
                                           int32_t pp_instance,
                                           bool is_external,
                                           int32_t render_frame_id,
                                           const GURL& document_url,
                                           const GURL& plugin_url,
                                           bool is_priviledged_context);

  // Called when the renderer deletes an out of process instance.
  void DidDeleteOutOfProcessPepperInstance(int32_t plugin_child_id,
                                           int32_t pp_instance,
                                           bool is_external);
  // Return a bound PepperHost.
  mojom::PepperHost* GetHost();

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Message handlers.
  void OnMsgCreateResourceHostsFromHostReply(
      int32_t sequence_number,
      const std::vector<int>& pending_resource_host_ids);

  // Return the next sequence number.
  int32_t GetNextSequence();

  // Sequence number to track pending callbacks.
  int32_t next_sequence_number_;

  // Maps a sequence number to the callback to be run.
  std::map<int32_t, PendingResourceIDCallback> pending_create_map_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_BROWSER_CONNECTION_H_
