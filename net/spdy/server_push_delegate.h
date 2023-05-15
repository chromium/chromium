// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SERVER_PUSH_DELEGATE_H_
#define NET_SPDY_SERVER_PUSH_DELEGATE_H_

#include <memory>

#include "net/base/net_export.h"
#include "net/log/net_log_with_source.h"
#include "url/gurl.h"

namespace net {

// An interface to a class that should be notified when session receives server
// push.
// TODO(https://crbug.com/1426477): Remove.
class NET_EXPORT_PRIVATE ServerPushDelegate {
 public:
  // An interface to a class that reflects information on the pushed request.
  class NET_EXPORT ServerPushHelper {
   public:
    virtual ~ServerPushHelper() = default;

    // Cancels the push if it is not claimed yet.
    virtual void Cancel() = 0;

    // Gets the URL of the pushed request.
    virtual const GURL& GetURL() const = 0;

    // Gets the network anonymization key for the pushed request.
    virtual NetworkAnonymizationKey GetNetworkAnonymizationKey() const = 0;
  };

  virtual ~ServerPushDelegate() = default;

  // Invoked by session when a push promise has been received.
  virtual void OnPush(std::unique_ptr<ServerPushHelper> push_helper,
                      const NetLogWithSource& session_net_log) = 0;
};

}  // namespace net

#endif  // NET_SPDY_SERVER_PUSH_DELEGATE_H_
