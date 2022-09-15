// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_HOST_INSTANCE_MESSAGE_FILTER_H_
#define PPAPI_HOST_INSTANCE_MESSAGE_FILTER_H_

#include "ppapi/host/ppapi_host_export.h"

namespace IPC {
class Message;
}

namespace ppapi {
namespace host {

class PpapiHost;

class PPAPI_HOST_EXPORT InstanceMessageFilter {
 public:
  explicit InstanceMessageFilter(PpapiHost* host);

  InstanceMessageFilter(const InstanceMessageFilter&) = delete;
  InstanceMessageFilter& operator=(const InstanceMessageFilter&) = delete;

  virtual ~InstanceMessageFilter();

  // Processes an instance message from the plugin process. Returns true if the
  // message was handled. On false, the PpapiHost will forward the message to
  // the next filter.
  virtual bool OnInstanceMessageReceived(const IPC::Message& msg) = 0;

  PpapiHost* host() { return host_; }

 private:
  PpapiHost* host_;
};

}  // namespace host
}  // namespace ppapi

#endif  // PPAPI_HOST_INSTANCE_MESSAGE_FILTER_H_
