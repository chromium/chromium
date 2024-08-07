// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/passkey_data_type_controller.h"

#include <utility>

#include "components/sync/base/data_type.h"
#include "components/sync/service/sync_service.h"

namespace webauthn {

PasskeyDataTypeController::PasskeyDataTypeController(
    syncer::SyncService* sync_service,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_transport_mode)
    : DataTypeController(syncer::WEBAUTHN_CREDENTIAL,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      sync_service_(sync_service) {}

}  // namespace webauthn
