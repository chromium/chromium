// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/host/message_filter_host.h"

#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_message_filter.h"

namespace ppapi {
namespace host {

MessageFilterHost::MessageFilterHost(
    PpapiHost* host,
    PP_Instance instance,
    PP_Resource resource,
    const scoped_refptr<ResourceMessageFilter>& message_filter)
    : ResourceHost(host, instance, resource) {
  AddFilter(message_filter);
}

MessageFilterHost::~MessageFilterHost() {
}

}  // namespace host
}  // namespace ppapi
