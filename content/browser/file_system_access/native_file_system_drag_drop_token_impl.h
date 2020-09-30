// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_DRAG_DROP_TOKEN_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_DRAG_DROP_TOKEN_IMPL_H_

#include "base/files/file_path.h"
#include "base/unguessable_token.h"
#include "content/browser/file_system_access/native_file_system_manager_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_drag_drop_token.mojom.h"

namespace content {

// This is the browser side implementation of the NativeFileSystemDragDropToken
// interface. This class stores the file path information of a single dragged
// and dropped file/directory in the browser process. The associated
// mojo::PendingRemote<NativeFileSystemDragDropToken>s are passed to the
// renderer process specified by `renderer_process_id` and can then be redeemed
// for NativeFileSystemEntry objects.
class CONTENT_EXPORT NativeFileSystemDragDropTokenImpl
    : public blink::mojom::NativeFileSystemDragDropToken {
 public:
  NativeFileSystemDragDropTokenImpl(
      NativeFileSystemManagerImpl* manager,
      const base::FilePath& file_path,
      int renderer_process_id,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemDragDropToken>
          receiver);

  ~NativeFileSystemDragDropTokenImpl() override;

  NativeFileSystemDragDropTokenImpl(const NativeFileSystemDragDropTokenImpl&) =
      delete;

  NativeFileSystemDragDropTokenImpl& operator=(
      const NativeFileSystemDragDropTokenImpl&) = delete;

  int renderer_process_id() const { return renderer_process_id_; }

  const base::FilePath& file_path() const { return file_path_; }

  const base::UnguessableToken& token() const { return token_; }

  // blink::mojom::NativeFileSystemDragDropToken:
  void GetInternalId(GetInternalIdCallback callback) override;

  void Clone(mojo::PendingReceiver<blink::mojom::NativeFileSystemDragDropToken>
                 receiver) override;

 private:
  void OnMojoDisconnect();

  // Raw pointer since NativeFileSystemManagerImpl owns `this`.
  NativeFileSystemManagerImpl* const manager_;
  const base::FilePath file_path_;
  const int renderer_process_id_;
  const base::UnguessableToken token_;

  // This token may contain multiple receivers, which includes a receiver for
  // the originally constructed instance and then additional receivers for
  // each clone. `manager_` must not remove this token until `receivers_` is
  // empty.
  mojo::ReceiverSet<blink::mojom::NativeFileSystemDragDropToken> receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_DRAG_DROP_TOKEN_IMPL_H_
