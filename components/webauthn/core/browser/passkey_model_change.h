// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_CHANGE_H_
#define COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_CHANGE_H_

#include "components/sync/protocol/webauthn_credential_specifics.pb.h"

namespace webauthn {

// Represents an individual change to a passkey model.
class PasskeyModelChange {
 public:
  enum class ChangeType { ADD, UPDATE, REMOVE };

  PasskeyModelChange(ChangeType type,
                     sync_pb::WebauthnCredentialSpecifics passkey);
  PasskeyModelChange(const PasskeyModelChange& other) = default;
  PasskeyModelChange& operator=(const PasskeyModelChange& other) = default;

  ChangeType type() const { return type_; }
  const sync_pb::WebauthnCredentialSpecifics& passkey() const {
    return passkey_;
  }

 private:
  ChangeType type_;
  sync_pb::WebauthnCredentialSpecifics passkey_;
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_CORE_BROWSER_PASSKEY_MODEL_CHANGE_H_
