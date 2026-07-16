// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_HOST_WEIGHTS_FILE_PROVIDER_H_
#define SERVICES_WEBNN_HOST_WEIGHTS_FILE_PROVIDER_H_

#include <cstdint>

#include "base/byte_size.h"
#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"

namespace webnn {

using CreateWeightsFileCallback = base::OnceCallback<void(base::File)>;

// The file is not unlinked at creation: the caller is responsible for
// cleaning up `path` (e.g. via `base::DeleteFile` on POSIX) when it is
// finished with the file.
using CreateWeightsFileWithPathCallback =
    base::OnceCallback<void(base::File, base::FilePath)>;

// Absolute floor of free disk space to preserve when creating a weights
// file. The shape mirrors `storage::QuotaSettings::must_remain_available`
// in //storage/browser/quota/quota_settings.cc: `min(fixed bytes,
// ratio * total)`.
//
// We pick a much larger fixed reserve than the storage subsystem because WebNN
// does not serialize concurrent weights-file creations: multiple origins /
// contexts can race on the same `AmountOfFreeDiskSpace` snapshot. With
// per-context cap 4 GiB and per-origin cap
// (`WeightsFileCreatorImpl::kMaxBytesPerOrigin`) 8 GiB, a single origin can
// split its 8 GiB into many smaller per-call requests that all observe the same
// free-space snapshot and pass the check; the actual total written is bounded
// by the per-origin cap. So the headroom must satisfy `H >= kMaxBytesPerOrigin
// = 8 GiB` to avoid no space.
inline constexpr base::ByteSize kWeightsFileMustRemainAvailableBytes =
    base::GiBU(10);

// Fraction of total disk space that must remain available. Used together
// with `kWeightsFileMustRemainAvailableBytes` as `min(fixed, ratio *
// total)` so small partitions (e.g. 8 GiB embedded device) scale the
// headroom down instead of refusing all allocations there.
inline constexpr double kWeightsFileMustRemainAvailableRatio = 0.10;

// Create a file in browser process to save all weights in the WebNN service.
COMPONENT_EXPORT(WEBNN_HOST)
void CreateWeightsFile(CreateWeightsFileCallback callback);

// The temp partition must have at least `headroom` of free space, where
// `headroom` is computed at call time as
// `min(kWeightsFileMustRemainAvailableBytes,
//      total_disk * kWeightsFileMustRemainAvailableRatio)`.
//
// The file does not unlink (POSIX) or mark `DeleteOnClose` (Windows) the
// tempfile. The callback receives the on-disk path so that the caller can later
// reopen the file (e.g. to obtain a read-only handle) and is responsible for
// deleting the path when done.
COMPONENT_EXPORT(WEBNN_HOST)
void CreateWeightsFileWithPath(CreateWeightsFileWithPathCallback callback);

}  // namespace webnn

#endif  // SERVICES_WEBNN_HOST_WEIGHTS_FILE_PROVIDER_H_
