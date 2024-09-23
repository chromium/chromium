// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_DATA_TYPE_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "components/sync/service/data_type_controller.h"

namespace syncer {
class SyncService;
}

namespace webauthn {

// A class that manages the startup and shutdown of passkey sync.
class PasskeyDataTypeController : public syncer::DataTypeController {
 public:
  PasskeyDataTypeController(syncer::SyncService* sync_service,
                            std::unique_ptr<syncer::DataTypeControllerDelegate>
                                delegate_for_full_sync_mode,
                            std::unique_ptr<syncer::DataTypeControllerDelegate>
                                delegate_for_transport_mode);

  PasskeyDataTypeController(const PasskeyDataTypeController&) = delete;
  PasskeyDataTypeController& operator=(const PasskeyDataTypeController&) =
      delete;

  ~PasskeyDataTypeController() override = default;

 private:
  const raw_ptr<syncer::SyncService> sync_service_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_DATA_TYPE_CONTROLLER_H_
