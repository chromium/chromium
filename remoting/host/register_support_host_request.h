// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REGISTER_SUPPORT_HOST_REQUEST_H_
#define REMOTING_HOST_REGISTER_SUPPORT_HOST_REQUEST_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "remoting/base/rsa_key_pair.h"
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
  virtual ~RegisterSupportHostRequest() = default;

  virtual void StartRequest(SignalStrategy* signal_strategy,
                            scoped_refptr<RsaKeyPair> key_pair,
                            RegisterCallback callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(RegisterSupportHostRequest);
};

}  // namespace remoting

#endif  // REMOTING_HOST_REGISTER_SUPPORT_HOST_REQUEST_H_
