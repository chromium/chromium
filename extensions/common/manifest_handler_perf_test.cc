// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/common_manifest_handlers.h"
#include "extensions/common/manifest_handler.h"
#include "extensions/common/scoped_testing_manifest_handler_registry.h"
#include "extensions/test/logging_timer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

// This and the following test are used to monitor the performance
// of the manifest handler registry initialization path, since
// it was determined to be a large part of the extensions system
// startup cost. They are prefixed with "MANUAL_" since they are
// not run like regular unit tests. They can be run like this:
// extensions_unittests  --gtest_filter="ManifestHandlerPerfTest.*"
// and should be run after any substantial changes to the related
// code.
TEST(ManifestHandlerPerfTest, MANUAL_CommonInitialize) {
  ScopedTestingManifestHandlerRegistry scoped_registry;
  static constexpr char kTimerId[] = "CommonInitialize";
  for (int i = 0; i < 100000; ++i) {
    {
      LoggingTimer timer(kTimerId);
      RegisterCommonManifestHandlers();
      ManifestHandler::FinalizeRegistration();
    }
    ManifestHandlerRegistry::ResetForTesting();
  }
  LoggingTimer::Print();
}

TEST(ManifestHandlerPerfTest, MANUAL_LookupTest) {
  ScopedTestingManifestHandlerRegistry scoped_registry;
  RegisterCommonManifestHandlers();
  ManifestHandler::FinalizeRegistration();
  ManifestHandlerRegistry* registry = ManifestHandlerRegistry::Get();
  ASSERT_TRUE(registry);
  std::vector<std::string> handler_names;
  handler_names.reserve(registry->handlers_.size());
  for (const auto& entry : registry->handlers_) {
    handler_names.push_back(entry.first);
  }
  static constexpr char kTimerId[] = "LookupTest";
  for (int i = 0; i < 100000; ++i) {
    LoggingTimer timer(kTimerId);
    for (const auto& name : handler_names) {
      registry->handlers_.find(name);
    }
  }
  LoggingTimer::Print();
}

TEST(ManifestHandlerPerfTest, MANUAL_CommonMeasureFinalization) {
  ScopedTestingManifestHandlerRegistry scoped_registry;
  static constexpr char kTimerId[] = "Finalize";
  for (int i = 0; i < 100000; ++i) {
    {
      RegisterCommonManifestHandlers();
      LoggingTimer timer(kTimerId);
      ManifestHandler::FinalizeRegistration();
    }
    ManifestHandlerRegistry::ResetForTesting();
  }
  LoggingTimer::Print();
}

}  // namespace extensions
