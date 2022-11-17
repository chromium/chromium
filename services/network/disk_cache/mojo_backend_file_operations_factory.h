// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DISK_CACHE_MOJO_BACKEND_FILE_OPERATIONS_FACTORY_H_
#define SERVICES_NETWORK_DISK_CACHE_MOJO_BACKEND_FILE_OPERATIONS_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/disk_cache/disk_cache.h"
#include "services/network/public/mojom/http_cache_backend_file_operations.mojom.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace network {

// A BackendFileOperationsFactory implementation that creates
// MojoBackendFileOperations on Create.
class COMPONENT_EXPORT(NETWORK_SERVICE) MojoBackendFileOperationsFactory final
    : public disk_cache::BackendFileOperationsFactory {
 public:
  explicit MojoBackendFileOperationsFactory(
      mojo::PendingRemote<mojom::HttpCacheBackendFileOperationsFactory>
          pending_remote);

  // BackendFileOperationsFactory implementation:
  std::unique_ptr<disk_cache::BackendFileOperations> Create(
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;
  std::unique_ptr<disk_cache::UnboundBackendFileOperations> CreateUnbound()
      override;

 private:
  ~MojoBackendFileOperationsFactory() override;

  mojo::Remote<mojom::HttpCacheBackendFileOperationsFactory> remote_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_DISK_CACHE_MOJO_BACKEND_FILE_OPERATIONS_FACTORY_H_
