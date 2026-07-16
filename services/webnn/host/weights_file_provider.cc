// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/host/weights_file_provider.h"

#include <algorithm>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"

namespace webnn {

namespace {

// Returns true if the temp partition has at least `headroom` of free space,
// where `headroom` mirrors `storage::QuotaSettings::must_remain_available`
// (see //storage/browser/quota/quota_settings.cc): the smaller of a fixed
// reserve and a fraction of the total partition size.
bool HasSufficientFreeDiskSpace(const base::FilePath& temp_dir) {
  const std::optional<int64_t> free_bytes =
      base::SysInfo::AmountOfFreeDiskSpace(temp_dir);
  const std::optional<int64_t> total_bytes =
      base::SysInfo::AmountOfTotalDiskSpace(temp_dir);
  if (!free_bytes.has_value() || *free_bytes < 0 || !total_bytes.has_value() ||
      *total_bytes < 0) {
    VLOG(1) << "[WebNN] Could not query disk space for " << temp_dir
            << "; declining weights file.";
    return false;
  }

  const uint64_t headroom = std::min<uint64_t>(
      kWeightsFileMustRemainAvailableBytes.InBytes(),
      base::ClampFloor<uint64_t>(static_cast<double>(*total_bytes) *
                                 kWeightsFileMustRemainAvailableRatio));

  if (static_cast<uint64_t>(*free_bytes) < headroom) {
    VLOG(1) << "[WebNN] Not enough free disk space (" << *free_bytes
            << " B free, need " << headroom
            << " B headroom); declining weights file.";
    return false;
  }
  return true;
}

// Create a temporary file that will be passed to the GPU process and deleted
// on close.
//
// TODO(crbug.com/364445586): Once the LiteRT MLDrift delegate moves from GPU
// to the renderer process, this function (and the `CreateWebNNWeightsFile`
// host interface in `gpu_host.mojom` that wraps it) can be removed entirely.
base::File CreateTemporaryFile() {
  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir)) {
    return base::File();
  }
  if (!HasSufficientFreeDiskSpace(temp_dir)) {
    return base::File();
  }

  // The file may be passed to an untrusted process, so set
  // `FLAG_WIN_NO_EXECUTE` to match the flags added by
  // `base::File::AddFlagsForPassingToUntrustedProcess()` for read/write files.
  base::FilePath path;
  base::File weights_file = base::CreateAndOpenTemporaryFileInDir(
      temp_dir, &path,
      base::File::FLAG_WIN_TEMPORARY | base::File::FLAG_WIN_NO_EXECUTE);
  if (weights_file.IsValid()) {
    // On POSIX platforms we can just call unlink(2) immediately and the file
    // will be deleted when the FD is closed but on Windows instead set this
    // up explicitly.
#if BUILDFLAG(IS_WIN)
    weights_file.DeleteOnClose(true);
#else
    base::DeleteFile(path);
#endif
  }
  return weights_file;
}

// Holds the tempfile and its on-disk path. Both are empty when creation
// fails.
struct WeightsFileResult {
  base::File file;
  base::FilePath path;
};

// Returns the on-disk path alongside the writable handle so the caller
// (`WeightsFileSessionImpl`) can later reopen the file read-only and unlink it.
// The file is not self-deleting; the caller owns cleanup.
WeightsFileResult CreateTemporaryFileWithPath() {
  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir)) {
    return {};
  }
  if (!HasSufficientFreeDiskSpace(temp_dir)) {
    return {};
  }

  // The file may be passed to an untrusted process, so set
  // `FLAG_WIN_NO_EXECUTE` to match the flags added by
  // `base::File::AddFlagsForPassingToUntrustedProcess()` for read/write files.
  //
  // On Windows the writable handle is opened with a permissive sharing mode
  // (`FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE`) so that:
  //   * `WeightsFileSessionImpl::Finalize` can reopen the file by path with
  //     `FLAG_READ` to produce the read-only handle handed to LiteRT, and
  //   * `base::DeleteFile` can mark the path for deletion while either
  //     handle is still open.
  base::FilePath path;
#if BUILDFLAG(IS_WIN)
  base::File weights_file = base::CreateAndOpenTemporaryFileInDirWithFlags(
      temp_dir, &path,
      base::File::FLAG_READ | base::File::FLAG_WRITE |
          base::File::FLAG_WIN_SHARE_DELETE | base::File::FLAG_WIN_TEMPORARY |
          base::File::FLAG_WIN_NO_EXECUTE);
#else
  base::File weights_file =
      base::CreateAndOpenTemporaryFileInDir(temp_dir, &path);
#endif
  if (!weights_file.IsValid()) {
    return {};
  }
  return {std::move(weights_file), std::move(path)};
}

}  // namespace

void CreateWeightsFile(CreateWeightsFileCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&CreateTemporaryFile), std::move(callback));
}

void CreateWeightsFileWithPath(CreateWeightsFileWithPathCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()},
      base::BindOnce(&CreateTemporaryFileWithPath),
      base::BindOnce(
          [](CreateWeightsFileWithPathCallback cb, WeightsFileResult result) {
            std::move(cb).Run(std::move(result.file), std::move(result.path));
          },
          std::move(callback)));
}

}  // namespace webnn
