// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_HOST_RESOURCE_HOST_H_
#define PPAPI_HOST_RESOURCE_HOST_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/host/ppapi_host_export.h"
#include "ppapi/host/resource_message_handler.h"
#include "ppapi/shared_impl/host_resource.h"

namespace IPC {
class Message;
}

namespace ppapi {
namespace host {

struct HostMessageContext;
class PpapiHost;
class ResourceMessageFilter;

// Some (but not all) resources have a corresponding object in the host side
// that is kept alive as long as the resource in the plugin is alive. This is
// the base class for such objects.
class PPAPI_HOST_EXPORT ResourceHost : public ResourceMessageHandler {
 public:
  ResourceHost(PpapiHost* host, PP_Instance instance, PP_Resource resource);

  ResourceHost(const ResourceHost&) = delete;
  ResourceHost& operator=(const ResourceHost&) = delete;

  ~ResourceHost() override;

  PpapiHost* host() { return host_; }
  PP_Instance pp_instance() const { return pp_instance_; }
  PP_Resource pp_resource() const { return pp_resource_; }

  // This runs any message filters in |message_filters_|. If the message is not
  // handled by these filters then the host's own message handler is run. True
  // is always returned (the message will always be handled in some way).
  bool HandleMessage(const IPC::Message& msg,
                     HostMessageContext* context) override;

  // Sets the PP_Resource ID when the plugin attaches to a pending resource
  // host. This will notify subclasses by calling
  // DidConnectPendingHostToResource.
  //
  // The current PP_Resource for all pending hosts should be 0. See
  // PpapiHostMsg_AttachToPendingHost.
  void SetPPResourceForPendingHost(PP_Resource pp_resource);

  void SendReply(const ReplyMessageContext& context,
                 const IPC::Message& msg) override;

  // Simple RTTI. A subclass that is a host for one of these APIs will override
  // the appropriate function and return true.
  virtual bool IsFileRefHost();
  virtual bool IsFileSystemHost();
  virtual bool IsGraphics2DHost();
  virtual bool IsMediaStreamVideoTrackHost();

 protected:
  // Adds a ResourceMessageFilter to handle resource messages. Incoming
  // messages will be passed to the handlers of these filters before being
  // handled by the resource host's own message handler. This allows
  // ResourceHosts to easily handle messages on other threads.
  void AddFilter(scoped_refptr<ResourceMessageFilter> filter);

  // Called when this resource host is pending and the corresponding plugin has
  // just connected to it. The host resource subclass can implement this
  // function if it wants to do processing (typically sending queued data).
  //
  // The PP_Resource will be valid for this call but not before.
  virtual void DidConnectPendingHostToResource() {}

 private:
  // The host that owns this object.
  PpapiHost* host_;

  PP_Instance pp_instance_;
  PP_Resource pp_resource_;

  // A vector of message filters which the host will forward incoming resource
  // messages to.
  std::vector<scoped_refptr<ResourceMessageFilter> > message_filters_;
};

}  // namespace host
}  // namespace ppapi

#endif  // PPAPI_HOST_RESOURCE_HOST_H_
