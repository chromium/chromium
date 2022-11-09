// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/renderer/core/frame/pending_beacon.h"

#include <tuple>

#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_pending_beacon_options.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/pending_beacon_dispatcher.h"
#include "third_party/blink/renderer/core/frame/pending_get_beacon.h"
#include "third_party/blink/renderer/core/frame/pending_post_beacon.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

using ::testing::Not;

MATCHER_P(HasTypeError,
          expected_message,
          base::StrCat({"has ", negation ? "no " : "", "TypeError('",
                        expected_message, "')"})) {
  const ExceptionState& exception_state = arg;
  if (!exception_state.HadException()) {
    *result_listener << "no exception";
    return false;
  }
  if (exception_state.CodeAs<ESErrorType>() != ESErrorType::kTypeError) {
    *result_listener << "exception is not TypeError";
    return false;
  }
  if (exception_state.Message() != expected_message) {
    *result_listener << "unexpected message from TypeError: "
                     << exception_state.Message();
    return false;
  }
  return true;
}

MATCHER(HasSecureContext,
        base::StrCat({"has ", negation ? "no " : "", "SecureContext"})) {
  const V8TestingScope& scope = arg;
  if (scope.GetExecutionContext()
          ->GetSecurityContext()
          .GetSecureContextMode() == SecureContextMode::kSecureContext) {
    return true;
  }
  *result_listener << "got InsecureContext";
  return false;
}

}  // namespace

class PendingBeaconTestBase : public ::testing::Test {
 public:
  static const WTF::String GetDefaultTargetURL() {
    return AtomicString("/pending_beacon/send");
  }

  static const WTF::String GetSourceURL() {
    return AtomicString("https://example.com");
  }

 protected:
  PendingBeacon* CreatePendingBeacon(V8TestingScope& v8_scope,
                                     mojom::blink::BeaconMethod method,
                                     const WTF::String& url,
                                     PendingBeaconOptions* options) const {
    auto* ec = v8_scope.GetExecutionContext();
    auto& exception_state = v8_scope.GetExceptionState();
    if (method == mojom::blink::BeaconMethod::kGet) {
      return PendingGetBeacon::Create(ec, url, options, exception_state);
    } else {
      return PendingPostBeacon::Create(ec, url, options, exception_state);
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
    return CreatePendingBeacon(v8_scope, method, GetDefaultTargetURL(),
                               PendingBeaconOptions::Create());
  }
  PendingGetBeacon* CreatePendingGetBeacon(V8TestingScope& v8_scope) const {
    auto* ec = v8_scope.GetExecutionContext();
    auto& exception_state = v8_scope.GetExceptionState();
    return PendingGetBeacon::Create(ec, GetDefaultTargetURL(),
                                    PendingBeaconOptions::Create(),
                                    exception_state);
  }
};

class PendingBeaconTestingScope : public V8TestingScope {
  STACK_ALLOCATED();

 public:
  PendingBeaconTestingScope()
      : V8TestingScope(KURL(PendingBeaconTestBase::GetSourceURL())) {}
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

TEST_P(PendingBeaconCreateTest, CreateFromSecureContext) {
  const auto& method = GetParam().method;
  const auto& method_str = GetParam().GetMethodString();
  PendingBeaconTestingScope v8_scope;
  ASSERT_THAT(v8_scope, HasSecureContext());

  auto* beacon = CreatePendingBeacon(v8_scope, method);

  EXPECT_EQ(beacon->url(), GetDefaultTargetURL());
  ASSERT_EQ(beacon->method(), method_str);
  ASSERT_EQ(beacon->timeout(), -1);
  ASSERT_EQ(beacon->backgroundTimeout(), -1);
  ASSERT_TRUE(beacon->pending());
  ASSERT_TRUE(beacon->IsPending());
  ASSERT_TRUE(PendingBeaconDispatcher::From(*v8_scope.GetExecutionContext())
                  ->HasPendingBeaconForTesting(beacon));
}

struct BeaconURLTestType {
  const char* name;
  const char* url_;
  bool expect_supported;
  const char* error_msg;
  WTF::String url() const {
    return strcmp(url_, "<null>") == 0 ? WTF::String() : WTF::String(url_);
  }
};

constexpr BeaconURLTestType kBeaconURLTestCases[] = {
    {"EMPTY_URL", "", true, ""},
    {"ROOT_URL", "/", true, ""},
    {"RELATIVE_PATH_URL", "/path/to/page", true, ""},
    {"NULL_STRING_URL", "null", true, ""},
    {"NULL_URL", "<null>", false,
     "The URL argument is ill-formed or unsupported."},
    {"RANDOM_PHRASE_URL", "test", true, ""},
    {"HTTPS_LOCALHOST_URL", "https://localhost", true, ""},
    // Results in a request to https://a.test/127.0.0.1.
    {"IP_URL", "127.0.0.1", true, ""},
    {"HTTP_IP_URL", "http://127.0.0.1", false,
     "PendingBeacons are only supported over HTTPS."},
    {"HTTPS_IP_URL", "https://127.0.0.1", true, ""},
    {"HTTP_URL", "http://example.com", false,
     "PendingBeacons are only supported over HTTPS."},
    {"HTTPS_URL", "https://example.com", true, ""},
    {"FILE_URL", "file://tmp", false,
     "PendingBeacons are only supported over HTTPS."},
    {"SSH_URL", "ssh://example.com", false,
     "PendingBeacons are only supported over HTTPS."},
    {"ABOUT_BLANK_URL", "about:blank", false,
     "PendingBeacons are only supported over HTTPS."},
    {"JAVASCRIPT_URL", "javascript:alert('');", false,
     "PendingBeacons are only supported over HTTPS."},
};

class PendingBeaconURLTest
    : public PendingBeaconTestBase,
      public ::testing::WithParamInterface<
          std::tuple<BeaconMethodTestType, BeaconURLTestType>> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    PendingBeaconURLTest,
    ::testing::Combine(::testing::Values(kPendingGetBeaconTestCase,
                                         kPendingPostBeaconTestCase),
                       ::testing::ValuesIn(kBeaconURLTestCases)),
    [](const ::testing::TestParamInfo<
        std::tuple<BeaconMethodTestType, BeaconURLTestType>>& info) {
      return base::StrCat(
          {std::get<0>(info.param).name, "_", std::get<1>(info.param).name});
    });

TEST_P(PendingBeaconURLTest, CreateWithURL) {
  const auto& method = std::get<0>(GetParam()).method;
  const auto& url = std::get<1>(GetParam()).url();
  const bool expect_supported = std::get<1>(GetParam()).expect_supported;
  const auto* error_msg = std::get<1>(GetParam()).error_msg;
  PendingBeaconTestingScope v8_scope;
  ASSERT_THAT(v8_scope, HasSecureContext());
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  ASSERT_FALSE(exception_state.HadException());

  auto* beacon = CreatePendingBeacon(v8_scope, method, url);

  if (expect_supported) {
    EXPECT_EQ(beacon->url(), url);
    EXPECT_FALSE(exception_state.HadException());
  } else {
    EXPECT_EQ(beacon, nullptr);
    EXPECT_THAT(exception_state, HasTypeError(error_msg));
  }
}

TEST_P(PendingBeaconURLTest, SetURL) {
  const auto& method = std::get<0>(GetParam()).method;
  const auto& url = std::get<1>(GetParam()).url();
  const bool expect_supported = std::get<1>(GetParam()).expect_supported;
  const auto* error_msg = std::get<1>(GetParam()).error_msg;
  if (method != mojom::blink::BeaconMethod::kGet) {
    return;
  }

  PendingBeaconTestingScope v8_scope;
  ASSERT_THAT(v8_scope, HasSecureContext());
  ExceptionState& exception_state = v8_scope.GetExceptionState();
  ASSERT_FALSE(exception_state.HadException());

  auto* get_beacon = CreatePendingGetBeacon(v8_scope);
  get_beacon->setURL(url, exception_state);

  if (expect_supported) {
    EXPECT_EQ(get_beacon->url(), url);
    EXPECT_FALSE(exception_state.HadException());
  } else {
    EXPECT_NE(get_beacon->url(), url);
    EXPECT_EQ(get_beacon->url(), GetDefaultTargetURL());
    EXPECT_THAT(exception_state, HasTypeError(error_msg));
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
  const auto& method = GetParam().method;
  PendingBeaconTestingScope v8_scope;
  ASSERT_THAT(v8_scope, HasSecureContext());

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
  const auto& method = GetParam().method;
  PendingBeaconTestingScope v8_scope;
  ASSERT_THAT(v8_scope, HasSecureContext());
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
  const auto& method = GetParam().method;
  PendingBeaconTestingScope v8_scope;
  ASSERT_THAT(v8_scope, HasSecureContext());
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
  const auto& method = GetParam().method;
  PendingBeaconTestingScope v8_scope;
  ASSERT_THAT(v8_scope, HasSecureContext());
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
    const auto& method = GetParam().method;
    PendingBeaconTestingScope v8_scope;
    ASSERT_THAT(v8_scope, HasSecureContext());
    beacon = CreatePendingBeacon(v8_scope, method);
    ASSERT_TRUE(beacon->pending());
  }
  // Lets `v8_scope` get destroyed to simulate unloading the document.

  EXPECT_FALSE(beacon->pending());
}
}  // namespace blink
