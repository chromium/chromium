// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/frame/pending_beacon.h"

#include <tuple>
#include <vector>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pending_beacon_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/pending_beacon_dispatcher.h"
#include "third_party/blink/renderer/core/frame/pending_get_beacon.h"
#include "third_party/blink/renderer/core/frame/pending_post_beacon.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class PendingBeaconTestBase : public ::testing::Test {
 protected:
  PendingBeacon* CreatePendingBeacon(V8TestingScope& v8_scope,
                                     mojom::blink::BeaconMethod method,
                                     const WTF::String& url,
                                     PendingBeaconOptions* options) const {
    auto* ec = v8_scope.GetExecutionContext();
    if (method == mojom::blink::BeaconMethod::kGet) {
      return PendingGetBeacon::Create(ec, url, options);
    } else {
      return PendingPostBeacon::Create(ec, url, options);
    }
  }
  PendingBeacon* CreatePendingBeacon(V8TestingScope& v8_scope,
                                     mojom::blink::BeaconMethod method,
                                     const WTF::String& url) const {
    return CreatePendingBeacon(v8_scope, method, url,
                               PendingBeaconOptions::Create());
  }
  PendingBeacon* CreatePendingBeacon(V8TestingScope& v8_scope,
                                     mojom::blink::BeaconMethod method) const {
    return CreatePendingBeacon(v8_scope, method, GetTargetURL(),
                               PendingBeaconOptions::Create());
  }

  static const WTF::String& GetTargetURL() {
    DEFINE_STATIC_LOCAL(const AtomicString, kTargetURL,
                        ("/pending_beacon/send"));
    return kTargetURL;
  }
};

struct BeaconMethodTestType {
  const char* name;
  const mojom::blink::BeaconMethod method;

  WTF::AtomicString GetMethodString() const {
    switch (method) {
      case mojom::blink::BeaconMethod::kGet:
        return WTF::AtomicString("GET");
      case mojom::blink::BeaconMethod::kPost:
        return WTF::AtomicString("POST");
    }
    CHECK(false) << "Unsupported beacon method";
  }

  static const char* TestParamInfoToName(
      const ::testing::TestParamInfo<BeaconMethodTestType>& info) {
    return info.param.name;
  }
};
constexpr BeaconMethodTestType kPendingGetBeaconTestCase{
    "PendingGetBeacon", mojom::blink::BeaconMethod::kGet};
constexpr BeaconMethodTestType kPendingPostBeaconTestCase{
    "PendingPostBeacon", mojom::blink::BeaconMethod::kPost};

class PendingBeaconCreateTest
    : public PendingBeaconTestBase,
      public ::testing::WithParamInterface<BeaconMethodTestType> {};

INSTANTIATE_TEST_SUITE_P(All,
                         PendingBeaconCreateTest,
                         ::testing::Values(kPendingGetBeaconTestCase,
                                           kPendingPostBeaconTestCase),
                         BeaconMethodTestType::TestParamInfoToName);

TEST_P(PendingBeaconCreateTest, Create) {
  V8TestingScope v8_scope;
  const auto& method = GetParam().method;
  const auto& method_str = GetParam().GetMethodString();

  auto* beacon = CreatePendingBeacon(v8_scope, method);

  EXPECT_EQ(beacon->url(), GetTargetURL());
  ASSERT_EQ(beacon->method(), method_str);
  ASSERT_EQ(beacon->timeout(), -1);
  ASSERT_EQ(beacon->backgroundTimeout(), -1);
  ASSERT_TRUE(beacon->pending());
  ASSERT_TRUE(beacon->IsPending());
  ASSERT_TRUE(PendingBeaconDispatcher::From(*v8_scope.GetExecutionContext())
                  ->HasPendingBeaconForTesting(beacon));
}

class PendingBeaconURLTest
    : public PendingBeaconTestBase,
      public ::testing::WithParamInterface<BeaconMethodTestType> {
 protected:
  struct BeaconURLTestType {
    std::string name;
    std::string url_;
    WTF::String GetURL() const {
      return url_ == "null" ? WTF::String() : WTF::String(url_);
    }
  };
};

INSTANTIATE_TEST_SUITE_P(All,
                         PendingBeaconURLTest,
                         ::testing::Values(kPendingGetBeaconTestCase,
                                           kPendingPostBeaconTestCase),
                         BeaconMethodTestType::TestParamInfoToName);

TEST_P(PendingBeaconURLTest, CreateWithURL) {
  const std::vector<BeaconURLTestType> test_cases = {
      {"EmptyURL", ""},
      {"RootURL", "/"},
      {"RelativePathURL", "/path/to/page"},
      {"NullURL", "null"},
      {"RandomPhraseURL", "test"},
      {"LocalHostURL", "localhost"},
      {"AddressURL", "127.0.0.1"},
      {"HTTPURL", "http://example.com"},
      {"HTTPSURL", "https://example.com"},
  };
  const auto& method = GetParam().method;
  V8TestingScope v8_scope;

  for (const auto& test_case : test_cases) {
    const auto& url = test_case.GetURL();

    auto* beacon = CreatePendingBeacon(v8_scope, method, url);

    EXPECT_EQ(beacon->url(), url) << test_case.name;
  }
}

class PendingBeaconBasicOperationsTest
    : public PendingBeaconTestBase,
      public ::testing::WithParamInterface<BeaconMethodTestType> {};

INSTANTIATE_TEST_SUITE_P(All,
                         PendingBeaconBasicOperationsTest,
                         ::testing::Values(kPendingGetBeaconTestCase,
                                           kPendingPostBeaconTestCase),
                         BeaconMethodTestType::TestParamInfoToName);

TEST_P(PendingBeaconBasicOperationsTest, MarkNotPending) {
  V8TestingScope v8_scope;
  const auto& method = GetParam().method;

  auto* beacon = CreatePendingBeacon(v8_scope, method);
  ASSERT_TRUE(beacon->pending());
  ASSERT_TRUE(beacon->IsPending());

  beacon->MarkNotPending();

  ASSERT_FALSE(beacon->pending());
  ASSERT_FALSE(beacon->IsPending());
}

class PendingBeaconSendTest
    : public PendingBeaconTestBase,
      public ::testing::WithParamInterface<BeaconMethodTestType> {};

INSTANTIATE_TEST_SUITE_P(All,
                         PendingBeaconSendTest,
                         ::testing::Values(kPendingGetBeaconTestCase,
                                           kPendingPostBeaconTestCase),
                         BeaconMethodTestType::TestParamInfoToName);

TEST_P(PendingBeaconSendTest, Send) {
  V8TestingScope v8_scope;
  const auto& method = GetParam().method;
  auto* beacon = CreatePendingBeacon(v8_scope, method);
  auto* dispatcher =
      PendingBeaconDispatcher::From(*v8_scope.GetExecutionContext());
  ASSERT_TRUE(dispatcher);
  ASSERT_TRUE(dispatcher->HasPendingBeaconForTesting(beacon));
  ASSERT_TRUE(beacon->pending());
  EXPECT_TRUE(beacon->IsPending());

  beacon->Send();

  EXPECT_FALSE(dispatcher->HasPendingBeaconForTesting(beacon));
  EXPECT_FALSE(beacon->pending());
  EXPECT_FALSE(beacon->IsPending());
}

TEST_P(PendingBeaconSendTest, SendNow) {
  V8TestingScope v8_scope;
  const auto& method = GetParam().method;
  auto* beacon = CreatePendingBeacon(v8_scope, method);
  auto* dispatcher =
      PendingBeaconDispatcher::From(*v8_scope.GetExecutionContext());
  ASSERT_TRUE(dispatcher);
  ASSERT_TRUE(dispatcher->HasPendingBeaconForTesting(beacon));
  ASSERT_TRUE(beacon->pending());
  EXPECT_TRUE(beacon->IsPending());

  beacon->sendNow();

  EXPECT_FALSE(dispatcher->HasPendingBeaconForTesting(beacon));
  EXPECT_FALSE(beacon->pending());
  EXPECT_FALSE(beacon->IsPending());
}

TEST_P(PendingBeaconSendTest, SetNonPendingAfterTimeoutTimerStart) {
  V8TestingScope v8_scope;
  const auto& method = GetParam().method;
  auto* beacon = CreatePendingBeacon(v8_scope, method);
  auto* dispatcher =
      PendingBeaconDispatcher::From(*v8_scope.GetExecutionContext());
  ASSERT_TRUE(dispatcher);
  beacon->setTimeout(60000);  // 60s such that it can't be reached in this test.
  ASSERT_TRUE(dispatcher->HasPendingBeaconForTesting(beacon));
  ASSERT_TRUE(beacon->pending());

  beacon->MarkNotPending();

  EXPECT_FALSE(beacon->pending());
  // Unregistering is handled by dispatcher.
}

class PendingBeaconContextDestroyedTest
    : public PendingBeaconTestBase,
      public ::testing::WithParamInterface<BeaconMethodTestType> {};

INSTANTIATE_TEST_SUITE_P(All,
                         PendingBeaconContextDestroyedTest,
                         ::testing::Values(kPendingGetBeaconTestCase,
                                           kPendingPostBeaconTestCase),
                         BeaconMethodTestType::TestParamInfoToName);

TEST_P(PendingBeaconContextDestroyedTest,
       BecomeNonPendingAfterContextDestroyed) {
  PendingBeacon* beacon = nullptr;
  {
    V8TestingScope v8_scope;
    const auto& method = GetParam().method;
    beacon = CreatePendingBeacon(v8_scope, method);
    ASSERT_TRUE(beacon->pending());
  }
  // Lets `v8_scope` get destroyed to simulate unloading the document.

  EXPECT_FALSE(beacon->pending());
}
}  // namespace blink
