// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_PROTOCOL_DOMAIN_HANDLER_H_
#define HEADLESS_LIB_BROWSER_PROTOCOL_DOMAIN_HANDLER_H_

#include "headless/lib/browser/protocol/protocol.h"

namespace headless {

namespace protocol {

class DomainHandler {
 public:
  virtual ~DomainHandler() = default;
  virtual void Wire(UberDispatcher* dispatcher) = 0;
  virtual Response Disable() = 0;
};

}  // namespace protocol
}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_PROTOCOL_DOMAIN_HANDLER_H_
