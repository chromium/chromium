// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REGISTER_SUPPORT_HOST_REQUEST_H_
#define REMOTING_HOST_REGISTER_SUPPORT_HOST_REQUEST_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/host/chromeos/chromeos_enterprise_params.h"
#include "remoting/protocol/errors.h"

namespace remoting {

class SignalStrategy;

// RegisterSupportHostRequest sends a request to register the host for
// a SupportID. When a response is received, it calls the callback specified in
// the StartRequest() method.
class RegisterSupportHostRequest {
 public:
  // First  parameter is the new SessionID received from the bot. Second
  // parameter is the amount of time until that id expires. Third parameter
  // is an error message if the request failed, or null if it succeeded.
  using RegisterCallback =
      base::OnceCallback<void(const std::string&,
                              const base::TimeDelta&,
                              protocol::ErrorCode error_code)>;

  RegisterSupportHostRequest() = default;

  RegisterSupportHostRequest(const RegisterSupportHostRequest&) = delete;
  RegisterSupportHostRequest& operator=(const RegisterSupportHostRequest&) =
      delete;

  virtual ~RegisterSupportHostRequest() = default;

  virtual void StartRequest(SignalStrategy* signal_strategy,
                            scoped_refptr<RsaKeyPair> key_pair,
                            const std::string& authorized_helper,
                            std::optional<ChromeOsEnterpriseParams> params,
                            RegisterCallback callback) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_REGISTER_SUPPORT_HOST_REQUEST_H_
