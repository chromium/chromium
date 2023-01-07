// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_ENVIRONMENT_H_
#define CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_ENVIRONMENT_H_

#include <memory>

#include "content/public/test/content_test_suite_base.h"

namespace content {

class TestContentClientInitializer;

// Initializes various environment settings to be able to run the attribution
// simulator in both fuzzers and standalone binaries.
class AttributionSimulatorEnvironment : public content::ContentTestSuiteBase {
 public:
  AttributionSimulatorEnvironment(int argc, char** argv);
  ~AttributionSimulatorEnvironment() override;

  AttributionSimulatorEnvironment(const AttributionSimulatorEnvironment&) =
      delete;
  AttributionSimulatorEnvironment(AttributionSimulatorEnvironment&&) = delete;

  AttributionSimulatorEnvironment& operator=(
      const AttributionSimulatorEnvironment&) = delete;
  AttributionSimulatorEnvironment& operator=(
      AttributionSimulatorEnvironment&&) = delete;

 private:
  std::unique_ptr<TestContentClientInitializer> test_content_initializer_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_ATTRIBUTION_SIMULATOR_ENVIRONMENT_H_
