// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/file_utilities_host_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/files/file_util.h"
#include "build/build_config.h"
#include "content/browser/security/cpsp/child_process_security_policy_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

FileUtilitiesHostImpl::FileUtilitiesHostImpl(ChildProcessId process_id)
    : process_id_(process_id) {}

FileUtilitiesHostImpl::~FileUtilitiesHostImpl() = default;

void FileUtilitiesHostImpl::Create(
    ChildProcessId process_id,
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
    std::move(callback).Run(std::nullopt);
    return;
  }

  base::File::Info info;
#if BUILDFLAG(IS_WIN)
  // On Windows, base::GetFileInfo() uses GetFileAttributesEx(), which can
  // report a stub/placeholder size for cloud-backed files that are not fully
  // present locally (such as OneDrive Files On-Demand files), particularly when
  // the file is protected with a sensitivity label. Relying on that truncated
  // size causes only part of the file to be read (e.g., dragged and dropped or
  // uploaded to a web page). base::GetHydratedFileInfo() opens such placeholder
  // files to force the cloud provider to hydrate (and decrypt) them, so the
  // reported size reflects the full file.
  const bool got_info = base::GetHydratedFileInfo(path, &info);
#else
  const bool got_info = base::GetFileInfo(path, &info);
#endif  // BUILDFLAG(IS_WIN)

  if (got_info) {
    std::move(callback).Run(info);
  } else {
    std::move(callback).Run(std::nullopt);
  }
}

}  // namespace content
