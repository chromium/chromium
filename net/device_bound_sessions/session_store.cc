// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_store.h"

#include <memory>

#include "base/files/file_path.h"
#include "net/base/features.h"
#include "net/device_bound_sessions/session_store_impl.h"
#include "net/device_bound_sessions/unexportable_key_service_factory.h"

namespace net::device_bound_sessions {

std::unique_ptr<SessionStore> SessionStore::Create(
    const base::FilePath& db_storage_path) {
  unexportable_keys::UnexportableKeyService* key_service =
      UnexportableKeyServiceFactory::GetInstance()->GetShared();
  if (!key_service || db_storage_path.empty()) {
    return nullptr;
  }

  return std::make_unique<SessionStoreImpl>(db_storage_path, *key_service);
}

}  // namespace net::device_bound_sessions
