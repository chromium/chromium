// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_HOST_WEIGHTS_FILE_CREATOR_IMPL_H_
#define SERVICES_WEBNN_HOST_WEIGHTS_FILE_CREATOR_IMPL_H_

#include <cstdint>

#include "base/byte_size.h"
#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "url/origin.h"

namespace webnn {

// Browser-process implementation of `WebNNWeightsFileCreator`. Bound once per
// `navigator.ml` (i.e. per renderer ExecutionContext); thin factory whose only
// responsibility is to spawn a self-owned `WeightsFileSessionImpl` per
// `OpenWeightsFile` call. All per-file state (tempfile, capacity, finalization)
// lives in the session, so multiple concurrent graph builds within a single
// `navigator.ml` each get their own session.
class COMPONENT_EXPORT(WEBNN_HOST) WeightsFileCreatorImpl
    : public mojom::WebNNWeightsFileCreator {
 public:
  // Maximum total bytes of weights files this browser process will grant to a
  // single origin at any one time. Per-context cap is enforced in
  // WeightsFileSessionImpl::kMaxWeightsBytesPerContext.
  static constexpr base::ByteSize kMaxBytesPerOrigin = base::GiBU(8);

  static void Create(
      mojo::PendingReceiver<mojom::WebNNWeightsFileCreator> receiver,
      const url::Origin& origin,
      bool is_incognito);

  WeightsFileCreatorImpl(const url::Origin& origin, bool is_incognito);
  ~WeightsFileCreatorImpl() override;

  WeightsFileCreatorImpl(const WeightsFileCreatorImpl&) = delete;
  WeightsFileCreatorImpl& operator=(const WeightsFileCreatorImpl&) = delete;

  // mojom::WebNNWeightsFileCreator:
  void OpenWeightsFile(OpenWeightsFileCallback callback) override;

 private:
  void DidOpenWeightsFile(OpenWeightsFileCallback callback,
                          base::File file,
                          base::FilePath path);

  const url::Origin origin_;
  // When true, no temporary weights file is created on disk; the in-renderer
  // TFLite backend will keep weights embedded in the in-memory model instead.
  const bool is_incognito_;

  base::WeakPtrFactory<WeightsFileCreatorImpl> weak_factory_{this};
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_HOST_WEIGHTS_FILE_CREATOR_IMPL_H_
