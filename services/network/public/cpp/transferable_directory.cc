// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/transferable_directory.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "services/network/public/cpp/network_service_buildflags.h"

namespace network {

TransferableDirectory::TransferableDirectory() = default;

TransferableDirectory::TransferableDirectory(TransferableDirectory&& other) {
  *this = std::move(other);
}

TransferableDirectory::TransferableDirectory(const base::FilePath& path)
    : path_(path) {}

TransferableDirectory::TransferableDirectory(mojo::PlatformHandle handle)
    : handle_(std::move(handle)) {}

TransferableDirectory::~TransferableDirectory() = default;

void TransferableDirectory::operator=(TransferableDirectory&& other) {
  if (other.HasValidHandle()) {
    handle_ = other.TakeHandle();
    path_ = {};
  } else {
    handle_ = {};
    path_ = other.path();
  }
}

void TransferableDirectory::operator=(const base::FilePath& path) {
  // Path cannot be mutated if OpenForTransfer() was called.
  DCHECK(!HasValidHandle());

  path_ = path;
}

const base::FilePath& TransferableDirectory::path() const {
  return path_;
}

void TransferableDirectory::ClearPath() {
  path_ = {};
}

// static
bool TransferableDirectory::IsOpenForTransferRequired() {
#if BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)
  return true;
#else
  return false;
#endif
}

bool TransferableDirectory::NeedsMount() const {
  if (HasValidHandle()) {
    return true;
  }
  return false;
}

mojo::PlatformHandle TransferableDirectory::TakeHandle() {
  DCHECK(HasValidHandle());

  mojo::PlatformHandle output;
  std::swap(handle_, output);
  return output;
}

#if !BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)

void TransferableDirectory::OpenForTransfer() {
  NOTREACHED_IN_MIGRATION()
      << "Directory transfer not supported on this platform.";
}

[[nodiscard]] base::OnceClosure TransferableDirectory::Mount() {
  NOTREACHED_IN_MIGRATION()
      << "Directory transfer not supported on this platform.";
  return {};
}

#endif  // BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)

}  // namespace network
