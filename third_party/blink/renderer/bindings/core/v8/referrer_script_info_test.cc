// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "v8/include/v8.h"

namespace blink {

TEST(ReferrerScriptInfo, HasReferencingScriptWithDefaultValue) {
  const KURL script_origin_resource_name("http://example.org/script.js");

  auto info_no_script = ReferrerScriptInfo::CreateNoReferencingScript();
  EXPECT_FALSE(info_no_script.HasReferencingScript());
  EXPECT_FALSE(info_no_script.HasReferencingScriptWithDefaultValue(
      script_origin_resource_name));

  auto info_default_script = ReferrerScriptInfo::CreateWithReferencingScript(
      script_origin_resource_name, ScriptFetchOptions());
  EXPECT_TRUE(info_default_script.HasReferencingScript());
  EXPECT_TRUE(info_default_script.HasReferencingScriptWithDefaultValue(
      script_origin_resource_name));

  auto info_null_url_script = ReferrerScriptInfo::CreateWithReferencingScript(
      KURL(), ScriptFetchOptions());
  EXPECT_TRUE(info_null_url_script.HasReferencingScript());
  EXPECT_FALSE(info_null_url_script.HasReferencingScriptWithDefaultValue(
      script_origin_resource_name));

  auto info_script = ReferrerScriptInfo::CreateWithReferencingScript(
      KURL("http://example.com"), ScriptFetchOptions());
  EXPECT_TRUE(info_script.HasReferencingScript());
  EXPECT_FALSE(info_script.HasReferencingScriptWithDefaultValue(
      script_origin_resource_name));

  auto info_nondefault_options =
      ReferrerScriptInfo::CreateWithReferencingScript(
          KURL("http://example.com"),
          ScriptFetchOptions("", {}, {}, kNotParserInserted,
                             network::mojom::CredentialsMode::kInclude,
                             network::mojom::ReferrerPolicy::kDefault,
                             mojom::FetchImportanceMode::kImportanceAuto,
                             RenderBlockingBehavior::kUnset));
  EXPECT_TRUE(info_nondefault_options.HasReferencingScript());
  EXPECT_FALSE(info_nondefault_options.HasReferencingScriptWithDefaultValue(
      script_origin_resource_name));
}

TEST(ReferrerScriptInfo, ToFromV8NoReferencingScript) {
  V8TestingScope scope;
  const KURL script_origin_resource_name("http://example.org/script.js");

  v8::Local<v8::PrimitiveArray> v8_info =
      ReferrerScriptInfo::CreateNoReferencingScript().ToV8HostDefinedOptions(
          scope.GetIsolate(), script_origin_resource_name);

  EXPECT_TRUE(v8_info.IsEmpty());

  ReferrerScriptInfo decoded = ReferrerScriptInfo::FromV8HostDefinedOptions(
      scope.GetContext(), v8_info, script_origin_resource_name);

  EXPECT_FALSE(decoded.HasReferencingScript());
}

TEST(ReferrerScriptInfo, ToFromV8ScriptOriginBaseUrl) {
  V8TestingScope scope;
  const KURL script_origin_resource_name("http://example.org/script.js");

  v8::Local<v8::PrimitiveArray> v8_info =
      ReferrerScriptInfo::CreateWithReferencingScript(
          script_origin_resource_name, ScriptFetchOptions())
          .ToV8HostDefinedOptions(scope.GetIsolate(),
                                  script_origin_resource_name);

  ASSERT_FALSE(v8_info.IsEmpty());
  EXPECT_EQ(v8_info->Length(), 1);

  ReferrerScriptInfo decoded = ReferrerScriptInfo::FromV8HostDefinedOptions(
      scope.GetContext(), v8_info, script_origin_resource_name);

  EXPECT_TRUE(decoded.HasReferencingScript());
  EXPECT_EQ(script_origin_resource_name, decoded.BaseURL());
}

TEST(ReferrerScriptInfo, ToFromV8ScriptNullBaseUrl) {
  V8TestingScope scope;
  const KURL script_origin_resource_name("http://example.org/script.js");

  v8::Local<v8::PrimitiveArray> v8_info =
      ReferrerScriptInfo::CreateWithReferencingScript(KURL(),
                                                      ScriptFetchOptions())
          .ToV8HostDefinedOptions(scope.GetIsolate(),
                                  script_origin_resource_name);

  ASSERT_FALSE(v8_info.IsEmpty());
  EXPECT_GT(v8_info->Length(), 1);

  ReferrerScriptInfo decoded = ReferrerScriptInfo::FromV8HostDefinedOptions(
      scope.GetContext(), v8_info, script_origin_resource_name);

  EXPECT_TRUE(decoded.HasReferencingScript());
  EXPECT_EQ(KURL(), decoded.BaseURL());
}

TEST(ReferrerScriptInfo, ToFromV8) {
  V8TestingScope scope;
  const KURL script_origin_resource_name("http://example.org/script.js");
  const KURL url("http://example.com");

  auto info = ReferrerScriptInfo::CreateWithReferencingScript(
      url, ScriptFetchOptions("foobar", {}, {}, kNotParserInserted,
                              network::mojom::CredentialsMode::kInclude,
                              network::mojom::ReferrerPolicy::kOrigin,
                              mojom::FetchImportanceMode::kImportanceAuto,
                              RenderBlockingBehavior::kUnset));

  v8::Local<v8::PrimitiveArray> v8_info = info.ToV8HostDefinedOptions(
      scope.GetIsolate(), script_origin_resource_name);

  ASSERT_FALSE(v8_info.IsEmpty());
  EXPECT_GT(v8_info->Length(), 1);

  ReferrerScriptInfo decoded = ReferrerScriptInfo::FromV8HostDefinedOptions(
      scope.GetContext(), v8_info, script_origin_resource_name);

  EXPECT_TRUE(decoded.HasReferencingScript());
  EXPECT_EQ(url, decoded.BaseURL());
  EXPECT_EQ(network::mojom::CredentialsMode::kInclude,
            decoded.CredentialsMode());
  EXPECT_EQ("foobar", decoded.Nonce());
  EXPECT_EQ(kNotParserInserted, decoded.ParserState());
  EXPECT_EQ(network::mojom::ReferrerPolicy::kOrigin,
            decoded.GetReferrerPolicy());
}

}  // namespace blink
