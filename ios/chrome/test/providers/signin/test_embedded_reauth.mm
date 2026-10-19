// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <utility>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/task/sequenced_task_runner.h"
#import "base/types/expected.h"
#import "ios/public/provider/chrome/browser/signin/embedded_reauth_api.h"

namespace ios::provider {
namespace {

class TestEmbeddedReAuthInteractionManager final
    : public EmbeddedReAuthInteractionManager {
 public:
  TestEmbeddedReAuthInteractionManager() = default;
  ~TestEmbeddedReAuthInteractionManager() override = default;

  bool IsReAuthSupported() const override { return true; }

  void InitiateEmbeddedReAuth(id<SystemIdentity> identity,
                              NSString* reauth_token,
                              EmbeddedReAuthTokenType token_type,
                              UIViewController* presenting_view_controller,
                              ReAuthCompletionCallback callback) override {
    NSError* error = [NSError errorWithDomain:@"TestEmbeddedReAuthErrorDomain"
                                         code:-1
                                     userInfo:nil];
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), base::unexpected(error)));
  }
};

}  // namespace

std::unique_ptr<EmbeddedReAuthInteractionManager>
CreateEmbeddedReAuthInteractionManager() {
  return std::make_unique<TestEmbeddedReAuthInteractionManager>();
}

}  // namespace ios::provider
