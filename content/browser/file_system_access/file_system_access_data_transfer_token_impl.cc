// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_data_transfer_token_impl.h"

#include <utility>

#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_data_transfer_token.mojom.h"

namespace content {

FileSystemAccessDataTransferTokenImpl::FileSystemAccessDataTransferTokenImpl(
    FileSystemAccessManagerImpl* manager,
    const content::PathInfo& file_path_info,
    int renderer_process_id,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessDataTransferToken>
        receiver)
    : manager_(manager),
      file_path_info_(file_path_info),
      renderer_process_id_(renderer_process_id),
      token_(base::UnguessableToken::Create()) {
  DCHECK(manager_);

  receivers_.set_disconnect_handler(base::BindRepeating(
      &FileSystemAccessDataTransferTokenImpl::OnMojoDisconnect,
      base::Unretained(this)));

  receivers_.Add(this, std::move(receiver));
}

FileSystemAccessDataTransferTokenImpl::
    ~FileSystemAccessDataTransferTokenImpl() = default;

void FileSystemAccessDataTransferTokenImpl::GetInternalId(
    GetInternalIdCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(token_);
}

void FileSystemAccessDataTransferTokenImpl::Clone(
    mojo::PendingReceiver<blink::mojom::FileSystemAccessDataTransferToken>
        clone_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(clone_receiver));
}

void FileSystemAccessDataTransferTokenImpl::OnMojoDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (receivers_.empty()) {
    manager_->RemoveDataTransferToken(token_);
  }
}

}  // namespace content
