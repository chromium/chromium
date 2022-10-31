// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_TEST_FAKE_CAST_AGENT_H_
#define FUCHSIA_WEB_RUNNERS_CAST_TEST_FAKE_CAST_AGENT_H_

#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/component/cpp/testing/realm_builder.h>

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/strings/string_piece.h"
#include "fuchsia_web/runners/cast/fidl/fidl/chromium/cast/cpp/fidl.h"

namespace test {

// LocalComponent implementation that offers some fake services that the
// runner normally expects to have provided by the Cast "agent".
class FakeCastAgent final : public ::component_testing::LocalComponent,
                            public chromium::cast::CorsExemptHeaderProvider {
 public:
  FakeCastAgent();
  ~FakeCastAgent() override;

  FakeCastAgent(const FakeCastAgent&) = delete;
  FakeCastAgent& operator=(const FakeCastAgent&) = delete;

  // Registers a callback to be invoked every time the specified service is
  // requested. This can be combined with Expect[Not]RunClosure() to express
  // simple expectations on whether services are connected-to.
  void RegisterOnConnectClosure(base::StringPiece service,
                                base::RepeatingClosure callback);

  // ::component_testing::LocalComponent implementation.
  void Start(std::unique_ptr<::component_testing::LocalComponentHandles>
                 handles) override;

 private:
  // chromium::cast::CorsExemptHeaderProvider implementation.
  void GetCorsExemptHeaderNames(
      GetCorsExemptHeaderNamesCallback callback) override;

  fidl::BindingSet<chromium::cast::CorsExemptHeaderProvider>
      cors_exempt_header_provider_bindings_;

  base::flat_map<std::string, base::RepeatingClosure> on_connect_;

  std::unique_ptr<::component_testing::LocalComponentHandles> handles_;
};

}  // namespace test

#endif  // FUCHSIA_WEB_RUNNERS_CAST_TEST_FAKE_CAST_AGENT_H_
