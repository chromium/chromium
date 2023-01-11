// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_READER_H_
#define REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_READER_H_

#include "base/functional/callback_forward.h"
#include "remoting/host/security_key/security_key_message.h"

namespace remoting {

// Interface used for listening for security key messages and notifying
// listeners when one is received.
class SecurityKeyMessageReader {
 public:
  virtual ~SecurityKeyMessageReader() {}

  // Starts the process of listening for security key messages.
  // |message_callback| is called for each received message.
  // |error_callback| is called in case of an error or the file is closed.
  // This method is asynchronous, callbacks will be called on the thread this
  // method is called on.  These callbacks can be called up to the point this
  // instance is destroyed and may be destroyed as a result of the callback
  // being invoked.
  virtual void Start(const SecurityKeyMessageCallback& message_callback,
                     base::OnceClosure error_callback) = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_SECURITY_KEY_SECURITY_KEY_MESSAGE_READER_H_
