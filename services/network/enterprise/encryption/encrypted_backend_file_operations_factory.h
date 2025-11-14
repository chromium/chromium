// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_BACKEND_FILE_OPERATIONS_FACTORY_H_
#define SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_BACKEND_FILE_OPERATIONS_FACTORY_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "net/disk_cache/disk_cache.h"

namespace network::enterprise {

// Factory to create `BackendFileOperations` instances with encryption support.
class COMPONENT_EXPORT(NETWORK_SERVICE) EncryptedBackendFileOperationsFactory
    : public disk_cache::BackendFileOperationsFactory {
 public:
  explicit EncryptedBackendFileOperationsFactory(
      scoped_refptr<disk_cache::BackendFileOperationsFactory>
          decorated_factory);

  EncryptedBackendFileOperationsFactory(
      const EncryptedBackendFileOperationsFactory&) = delete;
  EncryptedBackendFileOperationsFactory& operator=(
      const EncryptedBackendFileOperationsFactory&) = delete;

  std::unique_ptr<disk_cache::BackendFileOperations> Create(
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;
  std::unique_ptr<disk_cache::UnboundBackendFileOperations> CreateUnbound()
      override;

 protected:
  ~EncryptedBackendFileOperationsFactory() override;

 private:
  scoped_refptr<disk_cache::BackendFileOperationsFactory> decorated_factory_;
};

}  // namespace network::enterprise

#endif  // SERVICES_NETWORK_ENTERPRISE_ENCRYPTION_ENCRYPTED_BACKEND_FILE_OPERATIONS_FACTORY_H_
