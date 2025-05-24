// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_SESSION_AUTHZ_SERVICE_CLIENT_FACTORY_H_
#define REMOTING_BASE_SESSION_AUTHZ_SERVICE_CLIENT_FACTORY_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "remoting/base/authentication_method.h"
#include "remoting/base/session_authz_service_client.h"

namespace remoting {

// Interface for creating the SessionAuthz client.
class SessionAuthzServiceClientFactory
    : public base::RefCountedThreadSafe<SessionAuthzServiceClientFactory> {
 public:
  SessionAuthzServiceClientFactory() = default;

  virtual std::unique_ptr<SessionAuthzServiceClient> Create() = 0;
  virtual AuthenticationMethod method() = 0;

 protected:
  friend class base::RefCountedThreadSafe<SessionAuthzServiceClientFactory>;
  virtual ~SessionAuthzServiceClientFactory() = default;
};

}  // namespace remoting

#endif  // REMOTING_BASE_SESSION_AUTHZ_SERVICE_CLIENT_FACTORY_H_
