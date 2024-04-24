// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_PROTOCOL_DOMAIN_HANDLER_H_
#define CONTENT_SHELL_BROWSER_PROTOCOL_DOMAIN_HANDLER_H_

#include "content/shell/browser/protocol/protocol.h"

namespace content::shell::protocol {

class DomainHandler {
 public:
  virtual ~DomainHandler() = default;
  virtual void Wire(UberDispatcher* dispatcher) = 0;
  virtual Response Disable() = 0;
};

}  // namespace content::shell::protocol

#endif  // CONTENT_SHELL_BROWSER_PROTOCOL_DOMAIN_HANDLER_H_
