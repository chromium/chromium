// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_MOCK_FILE_SYSTEM_ACCESS_CONTEXT_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_MOCK_FILE_SYSTEM_ACCESS_CONTEXT_H_

#include <vector>

#include "components/services/storage/public/mojom/file_system_access_context.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content::indexed_db::test {

class MockFileSystemAccessContext
    : public ::storage::mojom::FileSystemAccessContext {
 public:
  MockFileSystemAccessContext();
  ~MockFileSystemAccessContext() override;

  void SerializeHandle(
      mojo::PendingRemote<::blink::mojom::FileSystemAccessTransferToken>
          pending_token,
      SerializeHandleCallback callback) override;

  void DeserializeHandle(
      const blink::StorageKey& storage_key,
      const std::vector<uint8_t>& bits,
      mojo::PendingReceiver<::blink::mojom::FileSystemAccessTransferToken>
          token) override;

  void Clone(mojo::PendingReceiver<::storage::mojom::FileSystemAccessContext>
                 receiver) override;

  const std::vector<
      mojo::Remote<::blink::mojom::FileSystemAccessTransferToken>>&
  writes() {
    return writes_;
  }

  void ClearWrites();

 private:
  std::vector<mojo::Remote<::blink::mojom::FileSystemAccessTransferToken>>
      writes_;
  mojo::ReceiverSet<::storage::mojom::FileSystemAccessContext> receivers_;
};

}  // namespace content::indexed_db::test

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_MOCK_FILE_SYSTEM_ACCESS_CONTEXT_H_
