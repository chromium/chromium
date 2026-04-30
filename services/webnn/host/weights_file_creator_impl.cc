// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/host/weights_file_creator_impl.h"

#include <utility>

#include "base/files/file.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/host/weights_file_provider.h"

namespace webnn {

// static
void WeightsFileCreatorImpl::Create(
    mojo::PendingReceiver<mojom::WebNNWeightsFileCreator> receiver,
    bool is_incognito) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<WeightsFileCreatorImpl>(is_incognito),
      std::move(receiver));
}

WeightsFileCreatorImpl::WeightsFileCreatorImpl(bool is_incognito)
    : is_incognito_(is_incognito) {}

WeightsFileCreatorImpl::~WeightsFileCreatorImpl() = default;

void WeightsFileCreatorImpl::CreateWeightsFile(
    CreateWeightsFileCallback callback) {
  // In incognito mode, do not create a temporary weights file on disk. Return
  // an invalid `base::File` so the renderer-side TFLite backend keeps the
  // weights embedded in the in-memory Flatbuffer model.
  if (is_incognito_) {
    std::move(callback).Run(base::File());
    return;
  }
  // TODO(crbug.com/507502295): Use file manager for weights files.
  webnn::CreateWeightsFile(std::move(callback));
}

}  // namespace webnn
