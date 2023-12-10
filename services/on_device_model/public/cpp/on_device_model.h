// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_ON_DEVICE_MODEL_H_
#define SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_ON_DEVICE_MODEL_H_

#include "base/component_export.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"

namespace on_device_model {

// An interface mirroring mojom::OnDeviceModel to avoid having the internal
// library depend on the mojom interfaces directly.
class COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP) OnDeviceModel {
 public:
  virtual ~OnDeviceModel() = default;

  // An interface mirroring mojom::Session to avoid having the internal library
  // depend on the mojom interfaces directly.
  class COMPONENT_EXPORT(ON_DEVICE_MODEL_CPP) Session {
   public:
    virtual ~Session() = default;

    virtual void AddContext(
        mojom::InputOptionsPtr input,
        mojo::PendingRemote<mojom::ContextClient> client) = 0;
    virtual void Execute(
        mojom::InputOptionsPtr input,
        mojo::PendingRemote<mojom::StreamingResponder> response) = 0;
  };

  virtual std::unique_ptr<Session> CreateSession() = 0;
};

}  // namespace on_device_model

#endif  //  SERVICES_ON_DEVICE_MODEL_PUBLIC_CPP_ON_DEVICE_MODEL_H_
