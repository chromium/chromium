// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/test/fake_cast_agent.h"

#include <lib/vfs/cpp/service.h>

#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "fuchsia_web/common/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace test {

FakeCastAgent::FakeCastAgent() = default;

FakeCastAgent::~FakeCastAgent() = default;

void FakeCastAgent::RegisterOnConnectClosure(std::string_view service,
                                             base::RepeatingClosure callback) {
  DCHECK(!is_started_);

  std::string name{service};
  DCHECK(!base::Contains(on_connect_, name));

  on_connect_[std::move(name)] = std::move(callback);
}

void FakeCastAgent::OnStart() {
  MaybeAddDefaultService(
      cors_exempt_header_provider_bindings_.GetHandler(this));
  MaybeAddDefaultService(
      app_config_manager_bindings_.GetHandler(&app_config_manager_));

  for (const auto& [name, on_connect_closure] : on_connect_) {
    ASSERT_EQ(outgoing()->AddPublicService(
                  std::make_unique<vfs::Service>(
                      [closure = on_connect_closure](
                          zx::channel, async_dispatcher_t*) { closure.Run(); }),
                  name),
              ZX_OK);
  }

  is_started_ = true;
}

void FakeCastAgent::GetCorsExemptHeaderNames(
    GetCorsExemptHeaderNamesCallback callback) {
  callback({StringToBytes("Test")});
}

template <class T>
void FakeCastAgent::MaybeAddDefaultService(
    fidl::InterfaceRequestHandler<T> request_handler) {
  if (!base::Contains(on_connect_, T::Name_)) {
    ASSERT_EQ(outgoing()->AddPublicService(std::move(request_handler)), ZX_OK);
  }
}

}  // namespace test
