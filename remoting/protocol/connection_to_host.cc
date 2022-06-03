// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/connection_to_host.h"

#include "base/notreached.h"

namespace remoting {
namespace protocol {

#define RETURN_STRING_LITERAL(x) \
case x: \
return #x;

const char* ConnectionToHost::StateToString(State state) {
  switch (state) {
    RETURN_STRING_LITERAL(INITIALIZING);
    RETURN_STRING_LITERAL(CONNECTING);
    RETURN_STRING_LITERAL(AUTHENTICATED);
    RETURN_STRING_LITERAL(CONNECTED);
    RETURN_STRING_LITERAL(CLOSED);
    RETURN_STRING_LITERAL(FAILED);
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace protocol
}  // namespace remoting
