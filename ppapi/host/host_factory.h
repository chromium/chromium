// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_HOST_HOST_FACTORY_H_
#define PPAPI_HOST_HOST_FACTORY_H_

#include <memory>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"

namespace IPC {
class Message;
}

namespace ppapi {

namespace host {

class PpapiHost;
class ResourceHost;

// A host factory creates ResourceHosts for incoming create messages from
// the plugin. This allows us to implement the hosts at the chrome/content
// layer without the ppapi layer knowing about the details.
class HostFactory {
 public:
  virtual ~HostFactory() {}

  virtual std::unique_ptr<ResourceHost> CreateResourceHost(
      PpapiHost* host,
      PP_Resource resource,
      PP_Instance instance,
      const IPC::Message& message) = 0;
};

}  // namespace host
}  // namespace ppapi

#endif  // PPAPI_HOST_HOST_FACTORY_H_
