// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options_context.h"

namespace mojo {

using V8Context = blink::V8IdentityCredentialRequestOptionsContext;
using blink::mojom::blink::RpContext;

TEST(CredentialManagerTypeConvertersTest, RpContextTest) {
  EXPECT_EQ(RpContext::kSignIn,
            ConvertTo<RpContext>(V8Context(V8Context::Enum::kSignin)));
  EXPECT_EQ(RpContext::kSignUp,
            ConvertTo<RpContext>(V8Context(V8Context::Enum::kSignup)));
  EXPECT_EQ(RpContext::kUse,
            ConvertTo<RpContext>(V8Context(V8Context::Enum::kUse)));
  EXPECT_EQ(RpContext::kContinue,
            ConvertTo<RpContext>(V8Context(V8Context::Enum::kContinue)));
}

}  // namespace mojo
