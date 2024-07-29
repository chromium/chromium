// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/session_service.h"

#include "net/device_bound_sessions/unexportable_key_service_factory.h"
#include "net/test/test_with_task_environment.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::device_bound_sessions {

namespace {

unexportable_keys::UnexportableKeyService* GetUnexportableKeyFactoryNull() {
  return nullptr;
}

class ScopedNullUnexportableKeyFactory {
 public:
  ScopedNullUnexportableKeyFactory() {
    UnexportableKeyServiceFactory::GetInstance()
        ->SetUnexportableKeyFactoryForTesting(GetUnexportableKeyFactoryNull);
  }
  ScopedNullUnexportableKeyFactory(const ScopedNullUnexportableKeyFactory&) =
      delete;
  ScopedNullUnexportableKeyFactory(ScopedNullUnexportableKeyFactory&&) = delete;
  ~ScopedNullUnexportableKeyFactory() {
    UnexportableKeyServiceFactory::GetInstance()
        ->SetUnexportableKeyFactoryForTesting(nullptr);
  }
};

class SessionServiceTest : public TestWithTaskEnvironment {
 protected:
  SessionServiceTest()
      : context_(CreateTestURLRequestContextBuilder()->Build()) {}

  std::unique_ptr<URLRequestContext> context_;
};

TEST_F(SessionServiceTest, HasService) {
  auto service = SessionService::Create(context_.get());
  EXPECT_TRUE(service);
}

TEST_F(SessionServiceTest, NoService) {
  ScopedNullUnexportableKeyFactory null_factory;
  auto service = SessionService::Create(context_.get());
  EXPECT_FALSE(service);
}
}  // namespace

}  // namespace net::device_bound_sessions
