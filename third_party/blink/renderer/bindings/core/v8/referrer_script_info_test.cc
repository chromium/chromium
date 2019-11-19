// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "v8/include/v8.h"

namespace blink {

TEST(ReferrerScriptInfo, IsDefaultValue) {
  EXPECT_TRUE(ReferrerScriptInfo().IsDefaultValue());
  EXPECT_FALSE(ReferrerScriptInfo(KURL("http://example.com"),
                                  network::mojom::CredentialsMode::kInclude, "",
                                  kNotParserInserted,
                                  network::mojom::ReferrerPolicy::kDefault)
                   .IsDefaultValue());
}

TEST(ReferrerScriptInfo, ToFromV8) {
  V8TestingScope scope;
  const KURL url("http://example.com");

  EXPECT_TRUE(ReferrerScriptInfo()
                  .ToV8HostDefinedOptions(scope.GetIsolate())
                  .IsEmpty());

  ReferrerScriptInfo info(url, network::mojom::CredentialsMode::kInclude,
                          "foobar", kNotParserInserted,
                          network::mojom::ReferrerPolicy::kOrigin);
  v8::Local<v8::PrimitiveArray> v8_info =
      info.ToV8HostDefinedOptions(scope.GetIsolate());

  ReferrerScriptInfo decoded =
      ReferrerScriptInfo::FromV8HostDefinedOptions(scope.GetContext(), v8_info);
  EXPECT_EQ(url, decoded.BaseURL());
  EXPECT_EQ(network::mojom::CredentialsMode::kInclude,
            decoded.CredentialsMode());
  EXPECT_EQ("foobar", decoded.Nonce());
  EXPECT_EQ(kNotParserInserted, decoded.ParserState());
  EXPECT_EQ(network::mojom::ReferrerPolicy::kOrigin,
            decoded.GetReferrerPolicy());
}

}  // namespace blink
