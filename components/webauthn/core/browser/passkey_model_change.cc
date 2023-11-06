// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/passkey_model_change.h"

#include "components/sync/protocol/webauthn_credential_specifics.pb.h"

namespace webauthn {

PasskeyModelChange::PasskeyModelChange(
    ChangeType type,
    sync_pb::WebauthnCredentialSpecifics passkey)
    : type_(type), passkey_(std::move(passkey)) {}

}  // namespace webauthn
