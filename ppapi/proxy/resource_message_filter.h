// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_RESOURCE_MESSAGE_FILTER_H_
#define PPAPI_PROXY_RESOURCE_MESSAGE_FILTER_H_

#include "base/memory/ref_counted.h"
#include "ppapi/proxy/ppapi_proxy_export.h"

namespace IPC {
class Message;
}

namespace ppapi {
namespace proxy {
class ResourceMessageReplyParams;

// A ResourceMessageFilter lives on the IO thread and handles messages for a
// particular resource type. This is necessary in some cases where we want to
// reduce latency by doing some work on the IO thread rather than having to
// PostTask to the main Pepper thread.
//
// Note: In some cases we can rely on a reply being associated with a
// particular TrackedCallback, in which case we can dispatch directly to the
// TrackedCallback's thread. See ReplyThreadRegistrar. That should be the first
// choice for avoiding an unecessary jump to the main-thread.
//
// ResourceMessageFilter is for cases where there is not a one-to-one
// relationship between a reply message and a TrackedCallback. For example, for
// UDP Socket resources, the browser pushes data to the plugin even when the
// plugin does not have a pending callback. We can't use the
// ReplyThreadRegistrar, because data may arrive when there's not yet a
// TrackedCallback to tell us what thread to use. So instead, we define a
// UDPSocketFilter which accepts and queues UDP data on the IO thread.
class PPAPI_PROXY_EXPORT ResourceMessageFilter
    : public base::RefCountedThreadSafe<ResourceMessageFilter> {
 public:
  virtual bool OnResourceReplyReceived(
      const ResourceMessageReplyParams& reply_params,
      const IPC::Message& nested_msg) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ResourceMessageFilter>;
  virtual ~ResourceMessageFilter() {}
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_RESOURCE_MESSAGE_FILTER_H_
