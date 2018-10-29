// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_CLIENT_ROOT_TEST_HELPER_H_
#define SERVICES_WS_CLIENT_ROOT_TEST_HELPER_H_

#include "base/macros.h"
#include "ui/events/event.h"

namespace aura {
class ClientSurfaceEmbedder;
}

namespace ws {

class ClientRoot;

// Used for accessing private members of ServerWindow in tests.
class ClientRootTestHelper {
 public:
  explicit ClientRootTestHelper(ClientRoot* client_root);
  ~ClientRootTestHelper();

  aura::ClientSurfaceEmbedder* GetClientSurfaceEmbedder();

 private:
  ClientRoot* client_root_;

  DISALLOW_COPY_AND_ASSIGN(ClientRootTestHelper);
};

}  // namespace ws

#endif  // SERVICES_WS_CLIENT_ROOT_TEST_HELPER_H_
