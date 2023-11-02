// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/host_resource_var.h"

namespace content {

HostResourceVar::HostResourceVar() : pp_resource_(0) {}

HostResourceVar::HostResourceVar(PP_Resource pp_resource)
    : pp_resource_(pp_resource),
      pending_renderer_host_id_(0),
      pending_browser_host_id_(0) {}

HostResourceVar::HostResourceVar(int pending_renderer_host_id,
                                 const IPC::Message& creation_message)
    : pp_resource_(0),
      pending_renderer_host_id_(pending_renderer_host_id),
      pending_browser_host_id_(0),
      creation_message_(new IPC::Message(creation_message)) {}

PP_Resource HostResourceVar::GetPPResource() const { return pp_resource_; }

int HostResourceVar::GetPendingRendererHostId() const {
  return pending_renderer_host_id_;
}

int HostResourceVar::GetPendingBrowserHostId() const {
  return pending_browser_host_id_;
}

const IPC::Message* HostResourceVar::GetCreationMessage() const {
  return creation_message_.get();
}

bool HostResourceVar::IsPending() const {
  return pp_resource_ == 0 && creation_message_;
}

HostResourceVar::~HostResourceVar() {}

}  // namespace content
