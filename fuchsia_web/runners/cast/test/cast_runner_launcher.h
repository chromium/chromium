// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_LAUNCHER_H_
#define FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_LAUNCHER_H_

#include <lib/sys/component/cpp/testing/realm_builder.h>
#include <lib/sys/cpp/service_directory.h>

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "fuchsia_web/common/test/test_realm_root.h"
#include "fuchsia_web/runners/cast/test/cast_runner_features.h"
#include "fuchsia_web/runners/cast/test/fake_cast_agent.h"

namespace test {

// Test helper that arranges to launch an isolated `cast_runner.cm` instance
// with the specified features enabled.  The instance will start only when
// one of the capabilities that it offers is actually connected-to, via the
// `exposed_services()`.
class CastRunnerLauncher {
 public:
  // Name of a component collection defined under this launcher's `Realm`,
  // into which Cast activities may be launched using the `cast_runner`
  // managed by this launcher.
  static constexpr char kTestCollectionName[] = "cast-test-collection";

  // Name of the CastRunner's Realm protocol, as exposed via the returned
  // service directory.
  static constexpr char kCastRunnerRealmProtocol[] =
      "fuchsia.component.Realm-runner";

  explicit CastRunnerLauncher(CastRunnerFeatures runner_features);
  ~CastRunnerLauncher();

  CastRunnerLauncher(const CastRunnerLauncher&) = delete;
  CastRunnerLauncher& operator=(const CastRunnerLauncher&) = delete;

  // Registers a component moniker (relative to this launcher's Realm, e.g.
  // "cast_runner") that is expected to terminate abnormally.
  void ExpectAbnormalTermination(std::string_view component_name) {
    realm_root_.value().ExpectAbnormalTermination(component_name);
  }

  // Returns the moniker prefix for components within this launcher's Realm.
  const std::string& realm_prefix() const {
    return realm_root_.value().realm_prefix();
  }

  // Returns a reference to the set of services exposed by the launcher, which
  // includes both the capabilities exposed by the `cast_runner` component, and
  // a `Realm` containing the test collection into which Cast activities may
  // be launched.
  sys::ServiceDirectory& exposed_services() { return *exposed_services_; }

  // Returns a fake through which Cast-specific services such as the
  // ApplicationConfigManager may be configured by tests.
  FakeCastAgent& fake_cast_agent() { return *fake_cast_agent_; }

 private:
  std::optional<TestRealmRoot> realm_root_;

  std::unique_ptr<sys::ServiceDirectory> exposed_services_;

  raw_ptr<FakeCastAgent> fake_cast_agent_ = nullptr;
};

}  // namespace test

#endif  // FUCHSIA_WEB_RUNNERS_CAST_TEST_CAST_RUNNER_LAUNCHER_H_
