// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_HOST_WEIGHTS_FILE_CREATOR_IMPL_H_
#define SERVICES_WEBNN_HOST_WEIGHTS_FILE_CREATOR_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"

namespace webnn {

// Browser-process implementation of WebNNWeightsFileCreator. Creates temporary
// files for storing WebNN model weights on behalf of sandboxed renderer
// processes that cannot create files directly.
class WeightsFileCreatorImpl : public mojom::WebNNWeightsFileCreator {
 public:
  static void Create(
      mojo::PendingReceiver<mojom::WebNNWeightsFileCreator> receiver,
      bool is_incognito);

  explicit WeightsFileCreatorImpl(bool is_incognito);
  ~WeightsFileCreatorImpl() override;

  WeightsFileCreatorImpl(const WeightsFileCreatorImpl&) = delete;
  WeightsFileCreatorImpl& operator=(const WeightsFileCreatorImpl&) = delete;

  // mojom::WebNNWeightsFileCreator:
  void CreateWeightsFile(CreateWeightsFileCallback callback) override;

 private:
  // When true, no temporary weights file is created on disk; the in-renderer
  // TFLite backend will keep weights embedded in the in-memory model instead.
  const bool is_incognito_;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_HOST_WEIGHTS_FILE_CREATOR_IMPL_H_
