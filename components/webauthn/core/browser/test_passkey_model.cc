// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/core/browser/test_passkey_model.h"

#include "base/containers/cxx20_erase_unordered_set.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"

TestPasskeyModel::TestPasskeyModel() = default;
TestPasskeyModel::~TestPasskeyModel() = default;

base::WeakPtr<syncer::ModelTypeControllerDelegate>
TestPasskeyModel::GetModelTypeControllerDelegate() {
  NOTIMPLEMENTED();
  return nullptr;
}

base::flat_set<std::string> TestPasskeyModel::GetAllSyncIds() const {
  base::flat_set<std::string> ids;
  for (const auto& credential : credentials_) {
    ids.emplace(credential.sync_id());
  }
  return ids;
}

std::vector<sync_pb::WebauthnCredentialSpecifics>
TestPasskeyModel::GetAllPasskeys() const {
  return credentials_;
}

std::string TestPasskeyModel::AddNewPasskeyForTesting(
    sync_pb::WebauthnCredentialSpecifics passkey) {
  credentials_.push_back(std::move(passkey));
  return credentials_.back().credential_id();
}

bool TestPasskeyModel::DeletePasskey(const std::string& credential_id) {
  NOTIMPLEMENTED();
  return false;
}
