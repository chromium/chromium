// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_TYPE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "components/sync/service/model_type_controller.h"

namespace syncer {
class SyncService;
}

namespace webauthn {

// A class that manages the startup and shutdown of passkey sync.
class PasskeyModelTypeController : public syncer::ModelTypeController {
 public:
  PasskeyModelTypeController(
      syncer::SyncService* sync_service,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_transport_mode);

  PasskeyModelTypeController(const PasskeyModelTypeController&) = delete;
  PasskeyModelTypeController& operator=(const PasskeyModelTypeController&) =
      delete;

  ~PasskeyModelTypeController() override = default;

 private:
  const raw_ptr<syncer::SyncService> sync_service_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_TYPE_CONTROLLER_H_
