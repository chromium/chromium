// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/passkey_model_type_controller.h"

#include <utility>

#include "build/build_config.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace webauthn {

PasskeyModelTypeController::PasskeyModelTypeController(
    syncer::SyncService* sync_service,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_full_sync_mode,
    std::unique_ptr<syncer::ModelTypeControllerDelegate>
        delegate_for_transport_mode)
    : ModelTypeController(syncer::WEBAUTHN_CREDENTIAL,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      sync_service_(sync_service) {}

bool PasskeyModelTypeController::ShouldRunInTransportOnlyMode() const {
#if !BUILDFLAG(IS_IOS)
  // Outside iOS, passphrase errors aren't reported in the UI, so it doesn't
  // make sense to enable this datatype.
  if (sync_service_->GetUserSettings()->IsUsingExplicitPassphrase()) {
    return false;
  }
#endif  // !BUILDFLAG(IS_IOS)
  return true;
}

}  // namespace webauthn
