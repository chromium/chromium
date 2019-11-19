// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/file_utilities_host_impl.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/optional.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

FileUtilitiesHostImpl::FileUtilitiesHostImpl(int process_id)
    : process_id_(process_id) {}

FileUtilitiesHostImpl::~FileUtilitiesHostImpl() = default;

void FileUtilitiesHostImpl::Create(
    int process_id,
    mojo::PendingReceiver<blink::mojom::FileUtilitiesHost> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<FileUtilitiesHostImpl>(process_id), std::move(receiver));
}

void FileUtilitiesHostImpl::GetFileInfo(const base::FilePath& path,
                                        GetFileInfoCallback callback) {
  // Get file metadata only when the child process has been granted
  // permission to read the file.
  auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!security_policy->CanReadFile(process_id_, path)) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  base::File::Info info;
  if (base::GetFileInfo(path, &info)) {
    std::move(callback).Run(info);
  } else {
    std::move(callback).Run(base::nullopt);
  }
}

}  // namespace content
