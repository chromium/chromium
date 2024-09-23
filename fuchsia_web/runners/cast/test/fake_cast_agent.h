// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_TEST_FAKE_CAST_AGENT_H_
#define FUCHSIA_WEB_RUNNERS_CAST_TEST_FAKE_CAST_AGENT_H_

#include <chromium/cast/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/component/cpp/testing/realm_builder.h>

#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "fuchsia_web/runners/cast/test/fake_application_config_manager.h"

namespace test {

// LocalComponentImpl implementation that offers some fake services that the
// runner normally expects to have provided by the Cast "agent".
class FakeCastAgent final : public ::component_testing::LocalComponentImpl,
                            public chromium::cast::CorsExemptHeaderProvider {
 public:
  FakeCastAgent();
  ~FakeCastAgent() override;

  FakeCastAgent(const FakeCastAgent&) = delete;
  FakeCastAgent& operator=(const FakeCastAgent&) = delete;

  // Registers a callback to be invoked every time the specified service is
  // requested. This can be combined with e.g:
  // - Expect[Not]RunClosure() to express simple expectations on whether
  //   services are connected-to.
  // - DoNothing() to prevent default services (e.g. CorsExemptHeaderProvider)
  //   being handled by the fake.
  void RegisterOnConnectClosure(std::string_view service,
                                base::RepeatingClosure callback);

  // ::component_testing::LocalComponentImpl implementation.
  void OnStart() override;

  // Returns the fake ApplicationConfigManager implementation that will be
  // served by this fake CastAgent component.
  FakeApplicationConfigManager& app_config_manager() {
    return app_config_manager_;
  }

 private:
  // chromium::cast::CorsExemptHeaderProvider implementation.
  void GetCorsExemptHeaderNames(
      GetCorsExemptHeaderNamesCallback callback) override;

  // Adds the service provided by the supplied `request_handler` to the
  // fake component's outgoing service directory, unless the caller has
  // registered an on-connect closure for that service already.
  template <class T>
  void MaybeAddDefaultService(fidl::InterfaceRequestHandler<T> request_handler);

  bool is_started_ = false;

  // Used to publish a stub CorsExemptHeaderProvider to the Cast runtime.
  fidl::BindingSet<chromium::cast::CorsExemptHeaderProvider>
      cors_exempt_header_provider_bindings_;

  // Used to configure the `ApplicationConfig`s reported to the Cast runtime.
  FakeApplicationConfigManager app_config_manager_;
  fidl::BindingSet<FakeApplicationConfigManager> app_config_manager_bindings_;

  // Used by individual tests to very that other services are connected-to.
  base::flat_map<std::string, base::RepeatingClosure> on_connect_;
};

}  // namespace test

#endif  // FUCHSIA_WEB_RUNNERS_CAST_TEST_FAKE_CAST_AGENT_H_
