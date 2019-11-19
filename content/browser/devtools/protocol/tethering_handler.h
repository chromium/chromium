// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TETHERING_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TETHERING_HANDLER_H_

#include <stdint.h>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/tethering.h"

namespace net {
class ServerSocket;
}

namespace content {
namespace protocol {

// This class implements reversed tethering handler.
class TetheringHandler : public DevToolsDomainHandler,
                         public Tethering::Backend {
 public:
  using CreateServerSocketCallback =
      base::Callback<std::unique_ptr<net::ServerSocket>(std::string*)>;

  TetheringHandler(const CreateServerSocketCallback& socket_callback,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~TetheringHandler() override;

  void Wire(UberDispatcher* dispatcher) override;

  void Bind(int port, std::unique_ptr<BindCallback> callback) override;
  void Unbind(int port, std::unique_ptr<UnbindCallback> callback) override;

 private:
  class TetheringImpl;

  void Accepted(uint16_t port, const std::string& name);
  bool Activate();

  std::unique_ptr<Tethering::Frontend> frontend_;
  CreateServerSocketCallback socket_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool is_active_;
  base::WeakPtrFactory<TetheringHandler> weak_factory_{this};

  static TetheringImpl* impl_;

  DISALLOW_COPY_AND_ASSIGN(TetheringHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_TETHERING_HANDLER_H_
