// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/instance/mock_file_system_access_context.h"

#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom.h"

namespace content::indexed_db::test {

MockFileSystemAccessContext::MockFileSystemAccessContext() = default;
MockFileSystemAccessContext::~MockFileSystemAccessContext() = default;

void MockFileSystemAccessContext::SerializeHandle(
    mojo::PendingRemote<::blink::mojom::FileSystemAccessTransferToken>
        pending_token,
    SerializeHandleCallback callback) {
  writes_.emplace_back(std::move(pending_token));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback),
                                std::vector<uint8_t>{
                                    static_cast<uint8_t>(writes_.size() - 1)}));
}

void MockFileSystemAccessContext::DeserializeHandle(
    const blink::StorageKey& storage_key,
    const std::vector<uint8_t>& bits,
    mojo::PendingReceiver<::blink::mojom::FileSystemAccessTransferToken>
        token) {
  NOTREACHED();
}

void MockFileSystemAccessContext::Clone(
    mojo::PendingReceiver<::storage::mojom::FileSystemAccessContext> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void MockFileSystemAccessContext::ClearWrites() {
  writes_.clear();
}

}  // namespace content::indexed_db::test
