// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_INPUT_CLIENT_INPUT_INJECTOR_H_
#define REMOTING_CLIENT_INPUT_CLIENT_INPUT_INJECTOR_H_

#include <stdint.h>
#include <string>

namespace remoting {

// This is an interface used by key input strategies to send processed key
// events to the client side input injector.
class ClientInputInjector {
 public:
  virtual ~ClientInputInjector() {}

  // Sends the provided keyboard scan code to the host.
  virtual bool SendKeyEvent(int scan_code, int key_code, bool key_down) = 0;

  // Send utf8 encoded text to the host.
  virtual void SendTextEvent(const std::string& text) = 0;
};

}  // namespace remoting
#endif  // REMOTING_CLIENT_INPUT_CLIENT_INPUT_INJECTOR_H_
