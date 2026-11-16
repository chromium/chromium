// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/signin/test_embedded_reauth.h"

#import <memory>
#import <optional>
#import <utility>

#import "base/check.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/location.h"
#import "base/no_destructor.h"
#import "base/task/sequenced_task_runner.h"
#import "base/types/expected.h"
#import "ios/public/provider/chrome/browser/signin/embedded_reauth_api.h"

namespace ios::provider {
namespace {

struct TestEmbeddedReAuthConfig {
  NSString* reauth_proof_token = nil;
  NSError* error = nil;
  bool is_supported = true;
  NSString* last_reauth_token = nil;
  std::optional<EmbeddedReAuthTokenType> last_token_type;
};

TestEmbeddedReAuthConfig& GetTestEmbeddedReAuthConfig() {
  static base::NoDestructor<TestEmbeddedReAuthConfig> config;
  return *config;
}

class TestEmbeddedReAuthInteractionManager final
    : public EmbeddedReAuthInteractionManager {
 public:
  TestEmbeddedReAuthInteractionManager() = default;
  ~TestEmbeddedReAuthInteractionManager() override = default;

  bool IsReAuthSupported() const override {
    return GetTestEmbeddedReAuthConfig().is_supported;
  }

  void InitiateEmbeddedReAuth(id<SystemIdentity> identity,
                              NSString* reauth_token,
                              EmbeddedReAuthTokenType token_type,
                              UIViewController* presenting_view_controller,
                              ReAuthCompletionCallback callback) override {
    TestEmbeddedReAuthConfig& config = GetTestEmbeddedReAuthConfig();
    CHECK(config.is_supported);
    CHECK(identity);
    CHECK_GT(reauth_token.length, 0u);
    CHECK(presenting_view_controller);
    CHECK(callback);

    config.last_reauth_token = [reauth_token copy];
    config.last_token_type = token_type;

    base::expected<NSString*, NSError*> result;
    if (!config.error && config.reauth_proof_token.length > 0) {
      result = base::ok(config.reauth_proof_token);
    } else {
      NSError* error =
          config.error
              ? config.error
              : [NSError errorWithDomain:@"TestEmbeddedReAuthErrorDomain"
                                    code:-1
                                userInfo:nil];
      result = base::unexpected(error);
    }
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
  }
};

}  // namespace

namespace test {

void SetEmbeddedReAuthSupported(bool supported) {
  GetTestEmbeddedReAuthConfig().is_supported = supported;
}

void SetEmbeddedReAuthResult(NSString* reauth_proof_token, NSError* error) {
  TestEmbeddedReAuthConfig& config = GetTestEmbeddedReAuthConfig();
  config.reauth_proof_token = [reauth_proof_token copy];
  config.error = error;
}

NSString* GetLastEmbeddedReAuthToken() {
  return GetTestEmbeddedReAuthConfig().last_reauth_token;
}

std::optional<EmbeddedReAuthTokenType> GetLastEmbeddedReAuthTokenType() {
  return GetTestEmbeddedReAuthConfig().last_token_type;
}

void ResetEmbeddedReAuthState() {
  GetTestEmbeddedReAuthConfig() = TestEmbeddedReAuthConfig();
}

}  // namespace test

std::unique_ptr<EmbeddedReAuthInteractionManager>
CreateEmbeddedReAuthInteractionManager() {
  return std::make_unique<TestEmbeddedReAuthInteractionManager>();
}

}  // namespace ios::provider
