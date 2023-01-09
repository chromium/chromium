// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/disk_cache/mojo_backend_file_operations_factory.h"

#include "base/task/sequenced_task_runner.h"
#include "services/network/disk_cache/mojo_backend_file_operations.h"

using disk_cache::BackendFileOperations;
using disk_cache::UnboundBackendFileOperations;

namespace network {

MojoBackendFileOperationsFactory::MojoBackendFileOperationsFactory(
    mojo::PendingRemote<mojom::HttpCacheBackendFileOperationsFactory>
        pending_remote)
    : remote_(std::move(pending_remote)) {}
MojoBackendFileOperationsFactory::~MojoBackendFileOperationsFactory() = default;

std::unique_ptr<BackendFileOperations> MojoBackendFileOperationsFactory::Create(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  mojo::PendingRemote<mojom::HttpCacheBackendFileOperations> pending_remote;
  remote_->Create(pending_remote.InitWithNewPipeAndPassReceiver());

  return std::make_unique<MojoBackendFileOperations>(std::move(pending_remote),
                                                     std::move(task_runner));
}

std::unique_ptr<UnboundBackendFileOperations>
MojoBackendFileOperationsFactory::CreateUnbound() {
  mojo::PendingRemote<mojom::HttpCacheBackendFileOperations> pending_remote;
  remote_->Create(pending_remote.InitWithNewPipeAndPassReceiver());

  return std::make_unique<UnboundMojoBackendFileOperations>(
      std::move(pending_remote));
}

}  // namespace network
