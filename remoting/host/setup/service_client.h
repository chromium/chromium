// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_SERVICE_CLIENT_H_
#define REMOTING_HOST_SETUP_SERVICE_CLIENT_H_

#include <string>

#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

// A class that gives access to the Chromoting service.
namespace remoting {

class ServiceClient {
 public:
  // TODO(simonmorris): Consider using a Callback instead of a delegate.
  class Delegate {
   public:
    // Invoked when a host has been registered.
    virtual void OnHostRegistered(const std::string& authorization_code) = 0;
    // Invoked when a host has been unregistered.
    virtual void OnHostUnregistered() = 0;
    // Invoked when there is an OAuth error.
    virtual void OnOAuthError() = 0;
    // Invoked when there is a network error or upon receiving an invalid
    // response.
    virtual void OnNetworkError(int response_code) = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit ServiceClient(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ServiceClient(const ServiceClient&) = delete;
  ServiceClient& operator=(const ServiceClient&) = delete;

  ~ServiceClient();

  // Register a host.
  void RegisterHost(const std::string& host_id,
                    const std::string& host_name,
                    const std::string& public_key,
                    const std::string& host_client_id,
                    const std::string& oauth_access_token,
                    Delegate* delegate);
  // Unregister a host.
  void UnregisterHost(const std::string& host_id,
                      const std::string& oauth_access_token,
                      Delegate* delegate);

 private:
  // The guts of the implementation live in this class.
  class Core;
  scoped_refptr<Core> core_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_SERVICE_CLIENT_H_
