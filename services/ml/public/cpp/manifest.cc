// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ml/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/ml/public/mojom/compilation.mojom.h"
#include "services/ml/public/mojom/constants.mojom.h"
#include "services/ml/public/mojom/execution.mojom.h"
#include "services/ml/public/mojom/model.mojom.h"
#include "services/ml/public/mojom/neuralnetwork.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace ml {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Machine Learning Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability(
              "neural_network",
              service_manager::Manifest::InterfaceList<mojom::NeuralNetwork>())
          .Build()};
  return *manifest;
}

}  // namespace ml
