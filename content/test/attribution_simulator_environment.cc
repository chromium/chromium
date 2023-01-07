// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/attribution_simulator_environment.h"

#include "content/public/common/network_service_util.h"
#include "content/public/test/test_content_client_initializer.h"
#include "mojo/core/embedder/embedder.h"

namespace content {

AttributionSimulatorEnvironment::AttributionSimulatorEnvironment(int argc,
                                                                 char** argv)
    : ContentTestSuiteBase(argc, argv) {
  Initialize();
  ForceInProcessNetworkService(true);
  // This initialization depends on the call to `Initialize()`, so we use a
  // `unique_ptr` to defer initialization instead of storing the field
  // directly.
  test_content_initializer_ = std::make_unique<TestContentClientInitializer>();

  mojo::core::Init();
}

AttributionSimulatorEnvironment::~AttributionSimulatorEnvironment() = default;

}  // namespace content
