// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_IDENTITY_IDENTITY_SERVICE_H_
#define SERVICES_IDENTITY_IDENTITY_SERVICE_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/identity/public/mojom/identity_service.mojom.h"

namespace mojom {
class IdentityAccessor;
}

namespace signin {
class IdentityManager;
}

namespace identity {
class IdentityService : public mojom::IdentityService {
 public:
  IdentityService(signin::IdentityManager* identity_manager,
                  mojo::PendingReceiver<mojom::IdentityService> receiver);
  ~IdentityService() override;

 private:
  // mojom::IdentityService:
  void BindIdentityAccessor(
      mojo::PendingReceiver<mojom::IdentityAccessor> receiver) override;

  // Shuts down this instance, blocking it from serving any pending or future
  // requests. Safe to call multiple times; will be a no-op after the first
  // call.
  void ShutDown();
  bool IsShutDown();

  mojo::Receiver<mojom::IdentityService> receiver_;

  signin::IdentityManager* identity_manager_;

  mojo::UniqueReceiverSet<mojom::IdentityAccessor> identity_accessors_;

  DISALLOW_COPY_AND_ASSIGN(IdentityService);
};

}  // namespace identity

#endif  // SERVICES_IDENTITY_IDENTITY_SERVICE_H_
