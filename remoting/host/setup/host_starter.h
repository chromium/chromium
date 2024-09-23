// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SETUP_HOST_STARTER_H_
#define REMOTING_HOST_SETUP_HOST_STARTER_H_

#include <string>

#include "base/functional/callback.h"

namespace remoting {

// An interface used to interact with a helper class which registers and starts
// a remote access host instance.
class HostStarter {
 public:
  enum Result {
    NETWORK_ERROR,
    OAUTH_ERROR,
    PERMISSION_DENIED,
    REGISTRATION_ERROR,
    START_COMPLETE,
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

    // |owner_email| and |username| are used to associate the new remote access
    // host instance with a specific user account. In some cases, they are also
    // used to perform access permission checks. Note that these flags are
    // mutually exclusive, only one is set depending on the flags passed in to
    // configure the host.
    //
    // The three configuration workflows are:
    //   Gaia account via OAuth:
    //   - Caller provides an authorization code via |auth_code|. |owner_email|
    //     is an optional field which, if set, is used to ensure the |auth_code|
    //     was generated for the expected user account.
    //
    //   Corp user account:
    //   - Caller provides a Corp username via the corp-user flag. This means
    //     |username| will be populated and |owner_email| should be empty.
    //
    //   Cloud user account:
    //   - Caller provides an email address via the cloud-user flag which is set
    //     in the |owner_email| field.
    std::string owner_email;
    std::string username;

    // The API_KEY to use when calling the cloud registration service endpoint.
    // Not set for public, corp, or legacy cloud configurations.
    // TODO: joedow - Update this comment once the API_KEY flow is supported.
    std::string api_key;

    // Optional parameter used to indicate whether or not to enable crash
    // reporting. The default is true and can be opted-out via a command line
    // parameter.
    bool enable_crash_reporting = true;
  };

  typedef base::OnceCallback<void(Result)> CompletionCallback;

  virtual ~HostStarter();

  // Registers a new host with the Chromoting service, and starts it.
  virtual void StartHost(Params params, CompletionCallback on_done) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SETUP_HOST_STARTER_H_
