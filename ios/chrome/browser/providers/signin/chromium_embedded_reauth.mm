// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/functional/callback.h"
#import "base/notreached.h"
#import "ios/public/provider/chrome/browser/signin/embedded_reauth_api.h"

namespace ios::provider {
namespace {

class ChromiumEmbeddedReAuthInteractionManager final
    : public EmbeddedReAuthInteractionManager {
 public:
  ChromiumEmbeddedReAuthInteractionManager() = default;
  ~ChromiumEmbeddedReAuthInteractionManager() override = default;

  bool IsReAuthSupported() const override { return false; }

  void InitiateEmbeddedReAuth(id<SystemIdentity> identity,
                              NSString* reauth_token,
                              EmbeddedReAuthTokenType token_type,
                              UIViewController* presenting_view_controller,
                              ReAuthCompletionCallback callback) override {
    NOTREACHED();
  }
};

}  // namespace

std::unique_ptr<EmbeddedReAuthInteractionManager>
CreateEmbeddedReAuthInteractionManager() {
  return std::make_unique<ChromiumEmbeddedReAuthInteractionManager>();
}

}  // namespace ios::provider
