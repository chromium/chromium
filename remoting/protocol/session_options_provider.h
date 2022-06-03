// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_SESSION_OPTIONS_PROVIDER_H_
#define REMOTING_PROTOCOL_SESSION_OPTIONS_PROVIDER_H_

#include "base/memory/ref_counted.h"

namespace remoting {

class SessionOptions;

namespace protocol {

// Interface for getting session options for the current session.
class SessionOptionsProvider {
 public:
  ~SessionOptionsProvider() = default;

  // Returns session options for the current session.
  virtual const SessionOptions& session_options() const = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SESSION_OPTIONS_PROVIDER_H_