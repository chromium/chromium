// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/native_file_system_drag_drop_token_impl.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_drag_drop_token.mojom.h"

namespace content {

NativeFileSystemDragDropTokenImpl::NativeFileSystemDragDropTokenImpl(
    NativeFileSystemManagerImpl* manager,
    const base::FilePath& file_path,
    int renderer_process_id,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemDragDropToken> receiver)
    : manager_(manager),
      file_path_(file_path),
      renderer_process_id_(renderer_process_id),
      token_(base::UnguessableToken::Create()) {
  DCHECK(manager_);

  receivers_.set_disconnect_handler(
      base::BindRepeating(&NativeFileSystemDragDropTokenImpl::OnMojoDisconnect,
                          base::Unretained(this)));

  receivers_.Add(this, std::move(receiver));
}

NativeFileSystemDragDropTokenImpl::~NativeFileSystemDragDropTokenImpl() =
    default;

void NativeFileSystemDragDropTokenImpl::GetInternalId(
    GetInternalIdCallback callback) {
  std::move(callback).Run(token_);
}

void NativeFileSystemDragDropTokenImpl::Clone(
    mojo::PendingReceiver<blink::mojom::NativeFileSystemDragDropToken>
        clone_receiver) {
  receivers_.Add(this, std::move(clone_receiver));
}

void NativeFileSystemDragDropTokenImpl::OnMojoDisconnect() {
  if (receivers_.empty()) {
    manager_->RemoveDragDropToken(token_);
  }
}

}  // namespace content
