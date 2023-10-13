// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_HOST_STARTER_H_
#define REMOTING_HOST_SETUP_HOST_STARTER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace remoting {

// An interface used to interact with a helper class which registers and starts
// a remote access host instance.
class HostStarter {
 public:
  enum Result {
    START_COMPLETE,
    NETWORK_ERROR,
    OAUTH_ERROR,
    START_ERROR,
  };
  struct Params {
    Params();
    Params(Params&& other);
    Params& operator=(Params&& other);
    ~Params();

    // Unique ID (GUID) used for the new host entry in the Directory.
    std::string id;
    // A user-friendly name which will be displayed in the website UI.
    std::string name;
    // 6+ digit PIN which is provided when accessing this machine remotely.
    std::string pin;
    // An OAuth2 authorization code generated for |owner_email| which is used to
    // associate the new host instance with that account in the Directory.
    std::string auth_code;
    // URL used to retrieve an OAuth2 access using |auth_code|.
    std::string redirect_url;

    // Specifies the account owner for the new remote access host instance.
    //
    // There are two methods for running this tool based on the flags passed in,
    // and |owner_email| is used differently in each context:
    //   - Passing an authorization code via the auth-code flag means that
    //     |owner_email| is an optional field which is used to verify that the
    //     |auth_code| was generated for that account.
    //
    //   - Providing a corp user email address via the corp-user flag means that
    //     |owner_email| is required and will be used to verify the account has
    //     access permissions and the value provided is also the account which
    //     the host will be associated with in the Directory.
    std::string owner_email;

    // Optional parameter used to indicate whether or not to enable crash
    // reporting. The default is true and can be opted-out via a command line
    // parameter.
    bool enable_crash_reporting = true;
  };

  typedef base::OnceCallback<void(Result)> CompletionCallback;

  // Creates a HostStarter instance.
  static std::unique_ptr<HostStarter> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  virtual ~HostStarter();

  // Registers a new host with the Chromoting service, and starts it.
  virtual void StartHost(Params params, CompletionCallback on_done) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_HOST_STARTER_H_
