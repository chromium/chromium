// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DATA_TRANSFER_TOKEN_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DATA_TRANSFER_TOKEN_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/thread_annotations.h"
#include "base/unguessable_token.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom.h"

namespace content {

// This is the browser side implementation of the
// FileSystemAccessDataTransferToken interface. This class stores the file path
// information of a single dragged and dropped or copied and pasted
// file/directory in the browser process. The associated
// mojo::PendingRemote<FileSystemAccessDataTransferToken>s are passed to the
// renderer process specified by `renderer_process_id` and can then be redeemed
// for FileSystemAccessEntry objects.
class CONTENT_EXPORT FileSystemAccessDataTransferTokenImpl
    : public blink::mojom::FileSystemAccessDataTransferToken {
 public:
  FileSystemAccessDataTransferTokenImpl(
      FileSystemAccessManagerImpl* manager,
      const content::PathInfo& file_path_info,
      int renderer_process_id,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessDataTransferToken>
          receiver);

  ~FileSystemAccessDataTransferTokenImpl() override;

  FileSystemAccessDataTransferTokenImpl(
      const FileSystemAccessDataTransferTokenImpl&) = delete;

  FileSystemAccessDataTransferTokenImpl& operator=(
      const FileSystemAccessDataTransferTokenImpl&) = delete;

  int renderer_process_id() const { return renderer_process_id_; }

  const content::PathInfo& file_path_info() const { return file_path_info_; }

  const base::UnguessableToken& token() const { return token_; }

  // blink::mojom::FileSystemAccessDataTransferToken:
  void GetInternalId(GetInternalIdCallback callback) override;

  void Clone(
      mojo::PendingReceiver<blink::mojom::FileSystemAccessDataTransferToken>
          receiver) override;

 private:
  void OnMojoDisconnect();

  SEQUENCE_CHECKER(sequence_checker_);

  // Raw pointer since FileSystemAccessManagerImpl owns `this`.
  const raw_ptr<FileSystemAccessManagerImpl> manager_ = nullptr;
  const content::PathInfo file_path_info_;
  const int renderer_process_id_;
  const base::UnguessableToken token_;

  // This token may contain multiple receivers, which includes a receiver for
  // the originally constructed instance and then additional receivers for
  // each clone. `manager_` must not remove this token until `receivers_` is
  // empty.
  mojo::ReceiverSet<blink::mojom::FileSystemAccessDataTransferToken> receivers_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_DATA_TRANSFER_TOKEN_IMPL_H_
