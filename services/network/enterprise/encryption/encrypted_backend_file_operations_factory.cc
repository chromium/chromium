// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/encrypted_backend_file_operations_factory.h"

#include <memory>
#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "services/network/enterprise/encryption/encrypted_backend_file_operations.h"

namespace network::enterprise {

EncryptedBackendFileOperationsFactory::EncryptedBackendFileOperationsFactory(
    scoped_refptr<BackendFileOperationsFactory> decorated_factory)
    : decorated_factory_(std::move(decorated_factory)) {}

EncryptedBackendFileOperationsFactory::
    ~EncryptedBackendFileOperationsFactory() = default;

std::unique_ptr<disk_cache::BackendFileOperations>
EncryptedBackendFileOperationsFactory::Create(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return std::make_unique<EncryptedBackendFileOperations>(
      decorated_factory_->Create(std::move(task_runner)));
}

std::unique_ptr<disk_cache::UnboundBackendFileOperations>
EncryptedBackendFileOperationsFactory::CreateUnbound() {
  return std::make_unique<UnboundEncryptedBackendFileOperations>(
      decorated_factory_->CreateUnbound());
}

}  // namespace network::enterprise
