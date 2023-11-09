// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/public/cpp/on_device_model.h"

namespace on_device_model {

void OnDeviceModel::Session::AddContext(
    mojom::InputOptionsPtr input,
    mojo::PendingRemote<mojom::ContextClient> client) {
  // TODO(cduvall): Update internal repo and remove old AddContext().
  AddContext(std::move(input));
}

}  // namespace on_device_model
