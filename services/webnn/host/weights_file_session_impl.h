// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_HOST_WEIGHTS_FILE_SESSION_IMPL_H_
#define SERVICES_WEBNN_HOST_WEIGHTS_FILE_SESSION_IMPL_H_

#include <cstdint>

#include "base/byte_size.h"
#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "url/origin.h"

namespace webnn {

// Browser-process implementation of WeightsFileSession.
//
// One instance is created per `OpenWeightsFile` call and owns:
//   * the writable tempfile (`tempfile_`),
//   * its on-disk path (`tempfile_path_`, kept so that `Finalize` can reopen
//     the file with `O_RDONLY` before unlinking),
//   * the per-context capacity counter (`granted_bytes_`),
//   * the per-origin budget reservation (released in the destructor).
//
// Lifetime: self-owned via `MakeSelfOwnedReceiver`. Destroyed when the
// Mojo pipe disconnects (renderer dropped it) or right after `Finalize`
// replies. The destructor:
//   * releases all reserved per-origin bytes back to the global tracker, and
//   * unlinks `tempfile_path_` if it has not already been consumed by
//     `Finalize` (covers crash / disconnect mid-build).
class COMPONENT_EXPORT(WEBNN_HOST) WeightsFileSessionImpl
    : public mojom::WeightsFileSession {
 public:
  // Maximum total bytes of weights a single context (= single session) may
  // write.
  static constexpr base::ByteSize kMaxWeightsBytesPerContext = base::GiBU(4);

  // Creates a self-owned session bound to `receiver`. Takes ownership of
  // `tempfile` (the writable browser-side fd) and `tempfile_path` (the on-disk
  // path, needed for the read-only reopen in `Finalize`).
  static void Create(mojo::PendingReceiver<mojom::WeightsFileSession> receiver,
                     base::File tempfile,
                     base::FilePath tempfile_path,
                     const url::Origin& origin);

  WeightsFileSessionImpl(base::File tempfile,
                         base::FilePath tempfile_path,
                         const url::Origin& origin);
  ~WeightsFileSessionImpl() override;

  WeightsFileSessionImpl(const WeightsFileSessionImpl&) = delete;
  WeightsFileSessionImpl& operator=(const WeightsFileSessionImpl&) = delete;

  // mojom::WeightsFileSession:
  void RequestCapacityChange(uint64_t new_size,
                             RequestCapacityChangeCallback callback) override;
  void Finalize(FinalizeCallback callback) override;

 private:
  base::File tempfile_;
  base::FilePath tempfile_path_;
  const url::Origin origin_;
  uint64_t granted_bytes_ = 0;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_HOST_WEIGHTS_FILE_SESSION_IMPL_H_
