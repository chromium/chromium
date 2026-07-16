// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/host/weights_file_session_impl.h"

#include <map>
#include <utility>

#include "base/check_op.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/synchronization/lock.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/host/weights_file_creator_impl.h"

namespace webnn {

namespace {

// Process-global per-origin accounting of reserved weights-file bytes.
// Lock-guarded because sessions for the same origin can live on different
// sequences (one per `OpenWeightsFile` call).
class OriginUsageTracker {
 public:
  static OriginUsageTracker& Get() {
    static base::NoDestructor<OriginUsageTracker> instance;
    return *instance;
  }

  bool TryReserve(const url::Origin& origin, uint64_t bytes) {
    base::AutoLock lock(lock_);
    auto [it, inserted] = usage_.try_emplace(origin, 0);
    base::CheckedNumeric<uint64_t> updated = it->second;
    updated += bytes;
    uint64_t new_used = 0;
    if (!updated.AssignIfValid(&new_used) ||
        new_used > WeightsFileCreatorImpl::kMaxBytesPerOrigin.InBytes()) {
      if (inserted) {
        usage_.erase(it);
      }
      return false;
    }
    it->second = new_used;
    return true;
  }

  void Release(const url::Origin& origin, uint64_t bytes) {
    if (bytes == 0) {
      return;
    }
    base::AutoLock lock(lock_);
    auto it = usage_.find(origin);
    CHECK(it != usage_.end());
    CHECK_GE(it->second, bytes);
    it->second -= bytes;
    if (it->second == 0) {
      usage_.erase(it);
    }
  }

 private:
  base::Lock lock_;
  std::map<url::Origin, uint64_t> usage_ GUARDED_BY(lock_);
};

}  // namespace

// static
void WeightsFileSessionImpl::Create(
    mojo::PendingReceiver<mojom::WeightsFileSession> receiver,
    base::File tempfile,
    base::FilePath tempfile_path,
    const url::Origin& origin) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WeightsFileSessionImpl>(
          std::move(tempfile), std::move(tempfile_path), origin),
      std::move(receiver));
}

WeightsFileSessionImpl::WeightsFileSessionImpl(base::File tempfile,
                                               base::FilePath tempfile_path,
                                               const url::Origin& origin)
    : tempfile_(std::move(tempfile)),
      tempfile_path_(std::move(tempfile_path)),
      origin_(origin) {}

WeightsFileSessionImpl::~WeightsFileSessionImpl() {
  // Release the per-origin reservation. The renderer's read-only fd no longer
  // counts against the quota.
  if (granted_bytes_ > 0) {
    OriginUsageTracker::Get().Release(origin_, granted_bytes_);
  }

  // If `Finalize` never ran (e.g. renderer disconnected mid-build) the tempfile
  // is still on disk because the unlink is deferred to support the read-only
  // reopen.
  if (!tempfile_path_.empty()) {
    tempfile_.Close();
    base::DeleteFile(tempfile_path_);
    tempfile_path_.clear();
  }
}

void WeightsFileSessionImpl::RequestCapacityChange(
    uint64_t new_size,
    RequestCapacityChangeCallback callback) {
  int64_t current_length = tempfile_.GetLength();
  if (current_length < 0 ||
      static_cast<uint64_t>(current_length) > granted_bytes_) {
    mojo::ReportBadMessage("WebNN: renderer wrote past granted capacity.");
    std::move(callback).Run(false);
    return;
  }

  if (new_size <= granted_bytes_) {
    // No new capacity needed.
    std::move(callback).Run(true);
    return;
  }

  uint64_t delta = new_size - granted_bytes_;

  // Per-context cap.
  base::CheckedNumeric<uint64_t> projected_context = granted_bytes_;
  projected_context += delta;
  uint64_t projected_context_value = 0;
  if (!projected_context.AssignIfValid(&projected_context_value) ||
      projected_context_value > kMaxWeightsBytesPerContext.InBytes()) {
    VLOG(1) << "[WebNN] Per-context weights budget exceeded (have "
            << granted_bytes_ << " B, want +" << delta << " B)";
    std::move(callback).Run(false);
    return;
  }

  // Per-origin cap.
  if (!OriginUsageTracker::Get().TryReserve(origin_, delta)) {
    VLOG(1) << "[WebNN] Per-origin weights budget exceeded for "
            << origin_.Serialize() << " (want +" << delta << " B)";
    std::move(callback).Run(false);
    return;
  }

  granted_bytes_ = projected_context_value;
  std::move(callback).Run(true);
}

void WeightsFileSessionImpl::Finalize(FinalizeCallback callback) {
  if (!tempfile_.IsValid()) {
    mojo::ReportBadMessage("WebNN: Finalize called without an open tempfile.");
    std::move(callback).Run(base::File());
    return;
  }

  int64_t current_length = tempfile_.GetLength();
  if (current_length < 0 ||
      static_cast<uint64_t>(current_length) > granted_bytes_) {
    mojo::ReportBadMessage("WebNN: Finalize file exceeds granted capacity.");
    std::move(callback).Run(base::File());
    return;
  }

  // Reopen by path with `FLAG_READ`, then unlink the file disappears once the
  // last fd closes. On Windows both handles include `FLAG_WIN_SHARE_DELETE` so
  // `DeleteFile` can mark the file for deletion.
  //
  // `FLAG_NO_FOLLOW` (POSIX-only, ignored on Windows) defends against a
  // same-uid attacker with write access to the temp directory replacing
  // `tempfile_path_` with a symlink to a sensitive file between the writable
  // handle's creation and this reopen. `FLAG_WIN_NO_EXECUTE` matches
  // `AddFlagsForPassingToUntrustedProcess` since this handle is handed to the
  // renderer.
  base::File ro_file;
  if (!tempfile_path_.empty()) {
    uint32_t ro_flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
                        base::File::FLAG_NO_FOLLOW |
                        base::File::FLAG_WIN_NO_EXECUTE;
#if BUILDFLAG(IS_WIN)
    ro_flags |= base::File::FLAG_WIN_SHARE_DELETE;
#endif
    ro_file = base::File(tempfile_path_, ro_flags);
    base::DeleteFile(tempfile_path_);
    tempfile_path_.clear();
  }
  tempfile_.Close();

  std::move(callback).Run(ro_file.IsValid() ? std::move(ro_file)
                                            : base::File());
}

}  // namespace webnn
