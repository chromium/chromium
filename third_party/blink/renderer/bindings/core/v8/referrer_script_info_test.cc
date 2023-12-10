// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/referrer_script_info.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "v8/include/v8.h"

namespace blink {

TEST(ReferrerScriptInfo, IsDefaultValue) {
  test::TaskEnvironment task_environment;
  const KURL script_origin_resource_name("http://example.org/script.js");

  // TODO(https://crbug.com/1114993): There three cases should be distinguished.
  EXPECT_TRUE(ReferrerScriptInfo().IsDefaultValue(script_origin_resource_name));
  EXPECT_TRUE(
      ReferrerScriptInfo(script_origin_resource_name, ScriptFetchOptions())
          .IsDefaultValue(script_origin_resource_name));
  EXPECT_TRUE(ReferrerScriptInfo(KURL(), ScriptFetchOptions())
                  .IsDefaultValue(script_origin_resource_name));

  EXPECT_FALSE(
      ReferrerScriptInfo(KURL("http://example.com"), ScriptFetchOptions())
          .IsDefaultValue(script_origin_resource_name));
  EXPECT_FALSE(ReferrerScriptInfo(KURL("http://example.com"),
                                  network::mojom::CredentialsMode::kInclude, "",
                                  kNotParserInserted,
                                  network::mojom::ReferrerPolicy::kDefault)
                   .IsDefaultValue(script_origin_resource_name));
}

TEST(ReferrerScriptInfo, ToFromV8NoReferencingScript) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL script_origin_resource_name("http://example.org/script.js");

  v8::Local<v8::Data> v8_info = ReferrerScriptInfo().ToV8HostDefinedOptions(
      scope.GetIsolate(), script_origin_resource_name);

  EXPECT_TRUE(v8_info.IsEmpty());

  ReferrerScriptInfo decoded = ReferrerScriptInfo::FromV8HostDefinedOptions(
      scope.GetContext(), v8_info, script_origin_resource_name);

  // TODO(https://crbug.com/1235202): This should be null URL.
  EXPECT_EQ(script_origin_resource_name, decoded.BaseURL());
}

TEST(ReferrerScriptInfo, ToFromV8ScriptOriginBaseUrl) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL script_origin_resource_name("http://example.org/script.js");

  v8::Local<v8::Data> v8_info =
      ReferrerScriptInfo(script_origin_resource_name, ScriptFetchOptions())
          .ToV8HostDefinedOptions(scope.GetIsolate(),
                                  script_origin_resource_name);

  EXPECT_TRUE(v8_info.IsEmpty());

  ReferrerScriptInfo decoded = ReferrerScriptInfo::FromV8HostDefinedOptions(
      scope.GetContext(), v8_info, script_origin_resource_name);

  EXPECT_EQ(script_origin_resource_name, decoded.BaseURL());
}

TEST(ReferrerScriptInfo, ToFromV8ScriptNullBaseUrl) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL script_origin_resource_name("http://example.org/script.js");

  v8::Local<v8::Data> v8_info =
      ReferrerScriptInfo(KURL(), ScriptFetchOptions())
          .ToV8HostDefinedOptions(scope.GetIsolate(),
                                  script_origin_resource_name);

  EXPECT_TRUE(v8_info.IsEmpty());

  ReferrerScriptInfo decoded = ReferrerScriptInfo::FromV8HostDefinedOptions(
      scope.GetContext(), v8_info, script_origin_resource_name);

  // TODO(https://crbug.com/1235202): This should be null URL.
  EXPECT_EQ(script_origin_resource_name, decoded.BaseURL());
}

TEST(ReferrerScriptInfo, ToFromV8) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const KURL script_origin_resource_name("http://example.org/script.js");
  const KURL url("http://example.com");

  ReferrerScriptInfo info(url, network::mojom::CredentialsMode::kInclude,
                          "foobar", kNotParserInserted,
                          network::mojom::ReferrerPolicy::kOrigin);
  v8::Local<v8::Data> v8_info = info.ToV8HostDefinedOptions(
      scope.GetIsolate(), script_origin_resource_name);

  ReferrerScriptInfo decoded = ReferrerScriptInfo::FromV8HostDefinedOptions(
      scope.GetContext(), v8_info, script_origin_resource_name);
  EXPECT_EQ(url, decoded.BaseURL());
  EXPECT_EQ(network::mojom::CredentialsMode::kInclude,
            decoded.CredentialsMode());
  EXPECT_EQ("foobar", decoded.Nonce());
  EXPECT_EQ(kNotParserInserted, decoded.ParserState());
  EXPECT_EQ(network::mojom::ReferrerPolicy::kOrigin,
            decoded.GetReferrerPolicy());
}

}  // namespace blink
