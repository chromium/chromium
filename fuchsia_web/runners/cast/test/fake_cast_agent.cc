// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/test/fake_cast_agent.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "fuchsia_web/common/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace test {

FakeCastAgent::FakeCastAgent() = default;

FakeCastAgent::~FakeCastAgent() = default;

void FakeCastAgent::RegisterOnConnectClosure(base::StringPiece service,
                                             base::RepeatingClosure callback) {
  DCHECK(!handles_);

  std::string name{service};
  DCHECK(!base::Contains(on_connect_, name));

  on_connect_[std::move(name)] = std::move(callback);
}

void FakeCastAgent::Start(
    std::unique_ptr<::component_testing::LocalComponentHandles> handles) {
  handles_ = std::move(handles);

  ASSERT_EQ(handles_->outgoing()->AddPublicService(
                cors_exempt_header_provider_bindings_.GetHandler(this)),
            ZX_OK);

  for (const auto& [name, on_connect_closure] : on_connect_) {
    ASSERT_EQ(handles_->outgoing()->AddPublicService(
                  std::make_unique<vfs::Service>(
                      [on_connect_closure = on_connect_closure](
                          zx::channel, async_dispatcher_t*) {
                        on_connect_closure.Run();
                      }),
                  name),
              ZX_OK);
  }
}

void FakeCastAgent::GetCorsExemptHeaderNames(
    GetCorsExemptHeaderNamesCallback callback) {
  callback({StringToBytes("Test")});
}

}  // namespace test
