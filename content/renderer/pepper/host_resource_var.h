// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_HOST_RESOURCE_VAR_H_
#define CONTENT_RENDERER_PEPPER_HOST_RESOURCE_VAR_H_

#include <memory>

#include "ipc/ipc_message.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/shared_impl/resource_var.h"
#include "ppapi/shared_impl/var.h"

namespace content {

// Represents a resource var, usable on the host side. Can either have a
// plugin-side resource or a pending resource host.
class HostResourceVar : public ppapi::ResourceVar {
 public:
  // Makes a null resource var.
  HostResourceVar();

  // Makes a resource var with an existing plugin-side resource.
  explicit HostResourceVar(PP_Resource pp_resource);

  // Makes a resource var with a pending resource host.
  // The |pending_renderer_host_id| is a pending resource host ID in the
  // renderer to attach from the plugin. Depending on the type of resource, this
  // may be 0. The |creation_message| contains additional data needed to create
  // the plugin-side resource. Its type depends on the type of resource.
  HostResourceVar(int pending_renderer_host_id,
                  const IPC::Message& creation_message);

  HostResourceVar(const HostResourceVar&) = delete;
  HostResourceVar& operator=(const HostResourceVar&) = delete;

  // ResourceVar override.
  PP_Resource GetPPResource() const override;
  int GetPendingRendererHostId() const override;
  int GetPendingBrowserHostId() const override;
  const IPC::Message* GetCreationMessage() const override;
  bool IsPending() const override;

  void set_pending_browser_host_id(int id) { pending_browser_host_id_ = id; }

 protected:
  ~HostResourceVar() override;

 private:
  // Real resource ID in the plugin. 0 if one has not yet been created
  // (indicating that there is a pending resource host).
  PP_Resource pp_resource_;

  // Pending resource host ID in the renderer.
  int pending_renderer_host_id_;

  // Pending resource host ID in the browser.
  int pending_browser_host_id_;

  // If the plugin-side resource has not yet been created, carries a message to
  // create a resource of the specific type on the plugin side. Otherwise, NULL.
  std::unique_ptr<IPC::Message> creation_message_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_HOST_RESOURCE_VAR_H_
