// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/host/resource_host.h"

#include <stddef.h>

#include "base/check.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_message_filter.h"

namespace ppapi {
namespace host {

ResourceHost::ResourceHost(PpapiHost* host,
                           PP_Instance instance,
                           PP_Resource resource)
    : host_(host),
      pp_instance_(instance),
      pp_resource_(resource) {
}

ResourceHost::~ResourceHost() {
  for (size_t i = 0; i < message_filters_.size(); ++i)
    message_filters_[i]->OnFilterDestroyed();
}

bool ResourceHost::HandleMessage(const IPC::Message& msg,
                                 HostMessageContext* context) {
  // First see if the message is handled off-thread by message filters.
  for (size_t i = 0; i < message_filters_.size(); ++i) {
    if (message_filters_[i]->HandleMessage(msg, context))
      return true;
  }
  // Run this ResourceHosts message handler.
  RunMessageHandlerAndReply(msg, context);
  return true;
}

void ResourceHost::SetPPResourceForPendingHost(PP_Resource pp_resource) {
  DCHECK(!pp_resource_);
  pp_resource_ = pp_resource;
  DidConnectPendingHostToResource();
}

void ResourceHost::SendReply(const ReplyMessageContext& context,
                             const IPC::Message& msg) {
  host_->SendReply(context, msg);
}

bool ResourceHost::IsFileRefHost() {
  return false;
}

bool ResourceHost::IsFileSystemHost() {
  return false;
}

bool ResourceHost::IsMediaStreamVideoTrackHost() {
  return false;
}

bool ResourceHost::IsGraphics2DHost() {
  return false;
}

void ResourceHost::AddFilter(scoped_refptr<ResourceMessageFilter> filter) {
  message_filters_.push_back(filter);
  filter->OnFilterAdded(this);
}

}  // namespace host
}  // namespace ppapi
