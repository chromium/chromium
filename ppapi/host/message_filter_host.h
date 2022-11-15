// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_HOST_MESSAGE_FILTER_HOST_H_
#define PPAPI_HOST_MESSAGE_FILTER_HOST_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_refptr.h"
#include "ppapi/host/ppapi_host_export.h"
#include "ppapi/host/resource_host.h"

namespace ppapi {
namespace host {

class PpapiHost;
class ResourceMessageFilter;

// This class is a generic ResourceHost that is composed of a single
// ResourceMessageFilter. There are cases where ResourceHosts only serve the
// purpose of passing messages onto a message filter to be handled on another
// thread. This class can be used as the host in those cases.
class PPAPI_HOST_EXPORT MessageFilterHost : public ResourceHost {
 public:
  MessageFilterHost(PpapiHost* host,
                    PP_Instance instance,
                    PP_Resource resource,
                    const scoped_refptr<ResourceMessageFilter>& message_filter);

  MessageFilterHost(const MessageFilterHost&) = delete;
  MessageFilterHost& operator=(const MessageFilterHost&) = delete;

  virtual ~MessageFilterHost();
};

}  // namespace host
}  // namespace ppapi

#endif  // PPAPI_HOST_MESSAGE_FILTER_HOST_H_
