// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/file_utilities_host_impl.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

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
    std::move(callback).Run(absl::nullopt);
    return;
  }

  base::File::Info info;
  if (base::GetFileInfo(path, &info)) {
    std::move(callback).Run(info);
  } else {
    std::move(callback).Run(absl::nullopt);
  }
}

#if BUILDFLAG(IS_MAC)
void FileUtilitiesHostImpl::SetLength(base::File file,
                                      const int64_t length,
                                      SetLengthCallback callback) {
  if (base::mac::IsAtLeastOS10_15()) {
    mojo::ReportBadMessage("SetLength() disabled on this OS.");
    // No error message is specified as the ReportBadMessage() call should close
    // the pipe and kill the renderer.
    std::move(callback).Run(std::move(file), false);
    return;
  }
  bool result = file.SetLength(length);
  std::move(callback).Run(std::move(file), result);
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace content
