// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fdio/namespace.h>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/strings/stringprintf.h"
#include "base/unguessable_token.h"
#include "services/network/public/cpp/transferable_directory.h"

namespace network {
namespace {

fdio_ns_t* GetNamespace() {
  fdio_ns_t* global_namespace = nullptr;
  zx_status_t status = fdio_ns_get_installed(&global_namespace);
  ZX_CHECK(status == ZX_OK, status) << "fdio_ns_get_installed";
  return global_namespace;
}

}  // namespace

void TransferableDirectory::OpenForTransfer() {
  handle_ = mojo::PlatformHandle(
      base::OpenWritableDirectoryHandle(path_).TakeChannel());
}

[[nodiscard]] base::OnceClosure TransferableDirectory::Mount() {
  DCHECK(NeedsMount());

  constexpr char kMountFormatString[] = "/sandbox-mnt/%s";
  path_ = base::FilePath(base::StringPrintf(
      kMountFormatString, base::UnguessableToken::Create().ToString().c_str()));
  CHECK(!base::PathExists(path_));

  zx_status_t status = fdio_ns_bind(GetNamespace(), path_.value().data(),
                                    handle_.ReleaseHandle());
  ZX_CHECK(status == ZX_OK, status) << "fdio_ns_bind";
  handle_ = {};
  DCHECK(base::PathExists(path_));

  return base::BindOnce(
      [](base::FilePath path) {
        zx_status_t status =
            fdio_ns_unbind(GetNamespace(), path.value().data());
        ZX_CHECK(status == ZX_OK, status) << "fdio_ns_unbind";
      },
      path_);
}

}  // namespace network
