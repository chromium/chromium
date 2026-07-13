// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/web_install/navigator_web_install.h"

#include <memory>

#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_install_params.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_web_install_result.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/permissions_policy/permissions_policy_parser.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

namespace {

class MockWebInstallService : public mojom::blink::WebInstallService {
 public:
  MockWebInstallService() = default;
  ~MockWebInstallService() override = default;

  void BindHandle(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(this, mojo::PendingReceiver<mojom::blink::WebInstallService>(
                             std::move(handle)));
  }

  void IsInstalled(mojom::blink::InstallOptionsPtr options,
                   IsInstalledCallback callback) override {
    std::move(callback).Run(false);
  }

  void Install(mojom::blink::InstallOptionsPtr options,
               InstallCallback callback) override {
    CHECK(!callback_) << "Keep the tests simple: one call at a time.";
    options_ = std::move(options);
    callback_ = std::move(callback);
    called_.SetValue();
  }

  void InstallFromElement(mojom::blink::InstallOptionsPtr options,
                          InstallCallback callback) override {
    NOTIMPLEMENTED();
  }

  void InstallFromManifest(mojom::blink::ManifestInstallOptionsPtr options,
                           InstallFromManifestCallback callback) override {
    CHECK(!manifest_callback_) << "Keep the tests simple: one call at a time.";
    manifest_options_ = std::move(options);
    manifest_callback_ = std::move(callback);
    manifest_called_.SetValue();
  }

  void ElementInstallFromManifest(
      mojom::blink::ManifestInstallOptionsPtr options,
      InstallFromManifestCallback callback) override {
    NOTIMPLEMENTED();
  }

  void WaitForCall() { EXPECT_TRUE(called_.Wait()); }
  void WaitForManifestCall() { EXPECT_TRUE(manifest_called_.Wait()); }

  const mojom::blink::ManifestInstallOptions* manifest_options() const {
    return manifest_options_.get();
  }

  void RespondWithSuccess(const KURL& manifest_id = KURL()) {
    CHECK(callback_);
    std::move(callback_).Run(mojom::blink::WebInstallServiceResult::kSuccess,
                             manifest_id);
    called_.Clear();
  }

  void RespondWithAbortError() {
    CHECK(callback_);
    std::move(callback_).Run(mojom::blink::WebInstallServiceResult::kAbortError,
                             KURL());
    called_.Clear();
  }

  void RespondWithDataError() {
    CHECK(callback_);
    std::move(callback_).Run(mojom::blink::WebInstallServiceResult::kDataError,
                             KURL());
    called_.Clear();
  }

  void RespondToManifestInstallWithSuccess() {
    CHECK(manifest_callback_);
    std::move(manifest_callback_)
        .Run(mojom::blink::WebInstallServiceResult::kSuccess);
    manifest_called_.Clear();
  }

  void RespondToManifestInstallWithAbortError() {
    CHECK(manifest_callback_);
    std::move(manifest_callback_)
        .Run(mojom::blink::WebInstallServiceResult::kAbortError);
    manifest_called_.Clear();
  }

  void RespondToManifestInstallWithDataError() {
    CHECK(manifest_callback_);
    std::move(manifest_callback_)
        .Run(mojom::blink::WebInstallServiceResult::kDataError);
    manifest_called_.Clear();
  }

 private:
  mojo::ReceiverSet<mojom::blink::WebInstallService> receivers_;
  mojom::blink::InstallOptionsPtr options_;
  InstallCallback callback_;
  base::test::TestFuture<void> called_;
  mojom::blink::ManifestInstallOptionsPtr manifest_options_;
  InstallFromManifestCallback manifest_callback_;
  base::test::TestFuture<void> manifest_called_;
};

}  // namespace

class NavigatorWebInstallTest : public testing::Test {
 public:
  NavigatorWebInstallTest()
      : holder_(std::make_unique<DummyPageHolder>()),
        handle_scope_(GetScriptState()->GetIsolate()),
        context_(GetScriptState()->GetContext()),
        context_scope_(context_) {}

  LocalFrame& GetFrame() { return holder_->GetFrame(); }

  ScriptState* GetScriptState() const {
    return ToScriptStateForMainWorld(&holder_->GetFrame());
  }

  Navigator* GetNavigator() { return GetFrame().DomWindow()->navigator(); }

 protected:
  void SetUp() override {
    GetFrame().Loader().CommitNavigation(
        WebNavigationParams::CreateWithEmptyHTMLForTesting(
            KURL("https://example.com")),
        /*extra_data=*/nullptr);
    test::RunPendingTasks();

    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::WebInstallService::Name_,
        BindRepeating(&MockWebInstallService::BindHandle,
                      Unretained(&mock_service_)));
  }

  void TearDown() override {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::WebInstallService::Name_, {});
  }

  MockWebInstallService& mock_service() { return mock_service_; }

 private:
  test::TaskEnvironment task_environment_;
  MockWebInstallService mock_service_;

  std::unique_ptr<DummyPageHolder> holder_;
  v8::HandleScope handle_scope_;
  v8::Local<v8::Context> context_;
  v8::Context::Scope context_scope_;
};

TEST_F(NavigatorWebInstallTest, Success) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  NonThrowableExceptionState exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester tester(GetScriptState(), promise);

  mock_service().WaitForCall();
  mock_service().RespondWithSuccess();

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
}

TEST_F(NavigatorWebInstallTest, AbortError) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  NonThrowableExceptionState exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester tester(GetScriptState(), promise);

  mock_service().WaitForCall();
  mock_service().RespondWithAbortError();

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());

  auto* dom_exception = V8DOMException::ToWrappable(
      GetScriptState()->GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(dom_exception->name(), "AbortError");
}

TEST_F(NavigatorWebInstallTest, DataError) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  NonThrowableExceptionState exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester tester(GetScriptState(), promise);

  mock_service().WaitForCall();
  mock_service().RespondWithDataError();

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());

  auto* dom_exception = V8DOMException::ToWrappable(
      GetScriptState()->GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(dom_exception->name(), "DataError");
}

TEST_F(NavigatorWebInstallTest, BlockedInSandbox) {
  GetFrame().DomWindow()->GetSecurityContext().SetSandboxFlags(
      network::mojom::blink::WebSandboxFlags::kAll);

  DummyExceptionStateForTesting exception_state;
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(DOMExceptionCode::kSecurityError,
            exception_state.CodeAs<DOMExceptionCode>());
  EXPECT_TRUE(promise.IsEmpty());
}

TEST_F(NavigatorWebInstallTest, BlockedByPermissionsPolicy) {
  network::ParsedPermissionsPolicy parsed_policy;
  DisallowFeature(network::mojom::PermissionsPolicyFeature::kWebAppInstallation,
                  parsed_policy);
  auto origin = SecurityOrigin::CreateFromString("https://example.com");
  GetFrame().DomWindow()->GetSecurityContext().SetPermissionsPolicy(
      network::PermissionsPolicy::CreateFromParsedPolicy(
          parsed_policy, origin->ToUrlOrigin()));

  DummyExceptionStateForTesting exception_state;
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(DOMExceptionCode::kSecurityError,
            exception_state.CodeAs<DOMExceptionCode>());
  EXPECT_TRUE(promise.IsEmpty());
}

TEST_F(NavigatorWebInstallTest, BlockedWithoutUserActivation) {
  DummyExceptionStateForTesting exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(DOMExceptionCode::kNotAllowedError,
            exception_state.CodeAs<DOMExceptionCode>());
  EXPECT_TRUE(promise.IsEmpty());
}

TEST_F(NavigatorWebInstallTest, EmptyInstallUrl) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);

  DummyExceptionStateForTesting exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              String(""), exception_state);
  ASSERT_FALSE(exception_state.HadException());

  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(NavigatorWebInstallTest, InvalidInstallUrl) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);

  DummyExceptionStateForTesting exception_state;
  auto promise = NavigatorWebInstall::install(
      GetScriptState(), *GetNavigator(), String("://invalid"), exception_state);
  ASSERT_FALSE(exception_state.HadException());

  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(NavigatorWebInstallTest, WhitespaceOnlyInstallUrl) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);

  DummyExceptionStateForTesting exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              String("   "), exception_state);
  ASSERT_FALSE(exception_state.HadException());

  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(NavigatorWebInstallTest, EmptyManifestId) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);

  DummyExceptionStateForTesting exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              String("https://example.com"),
                                              String(""), exception_state);
  ASSERT_FALSE(exception_state.HadException());

  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(NavigatorWebInstallTest, InstallFromManifest_Success) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* params = MakeGarbageCollected<InstallParams>();
  params->setManifest("https://example.com/manifest.json");

  NonThrowableExceptionState exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              params, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester tester(GetScriptState(), promise);

  mock_service().WaitForManifestCall();
  ASSERT_TRUE(mock_service().manifest_options());
  EXPECT_EQ(mock_service().manifest_options()->manifest_url,
            KURL("https://example.com/manifest.json"));
  EXPECT_FALSE(mock_service().manifest_options()->manifest_id);
  mock_service().RespondToManifestInstallWithSuccess();

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
}

TEST_F(NavigatorWebInstallTest, InstallFromManifest_WithIdSuccess) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* params = MakeGarbageCollected<InstallParams>();
  params->setManifest("https://example.com/manifest.json");
  params->setId("https://example.com/app/");

  NonThrowableExceptionState exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              params, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester tester(GetScriptState(), promise);

  mock_service().WaitForManifestCall();
  ASSERT_TRUE(mock_service().manifest_options());
  EXPECT_EQ(mock_service().manifest_options()->manifest_url,
            KURL("https://example.com/manifest.json"));
  EXPECT_EQ(mock_service().manifest_options()->manifest_id,
            KURL("https://example.com/app/"));
  mock_service().RespondToManifestInstallWithSuccess();

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
}

TEST_F(NavigatorWebInstallTest, InstallFromManifest_AbortError) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* params = MakeGarbageCollected<InstallParams>();
  params->setManifest("https://example.com/manifest.json");

  NonThrowableExceptionState exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              params, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester tester(GetScriptState(), promise);

  mock_service().WaitForManifestCall();
  mock_service().RespondToManifestInstallWithAbortError();

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());

  auto* dom_exception = V8DOMException::ToWrappable(
      GetScriptState()->GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(dom_exception->name(), "AbortError");
}

TEST_F(NavigatorWebInstallTest, InstallFromManifest_DataError) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* params = MakeGarbageCollected<InstallParams>();
  params->setManifest("https://example.com/manifest.json");

  NonThrowableExceptionState exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              params, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester tester(GetScriptState(), promise);

  mock_service().WaitForManifestCall();
  mock_service().RespondToManifestInstallWithDataError();

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());

  auto* dom_exception = V8DOMException::ToWrappable(
      GetScriptState()->GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(dom_exception->name(), "DataError");
}

TEST_F(NavigatorWebInstallTest, InstallFromManifest_EmptyManifestUrl) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* params = MakeGarbageCollected<InstallParams>();
  params->setManifest("");

  NonThrowableExceptionState exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              params, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(NavigatorWebInstallTest, InstallFromManifest_InvalidManifestUrl) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* params = MakeGarbageCollected<InstallParams>();
  params->setManifest("://not-a-url");

  NonThrowableExceptionState exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              params, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(NavigatorWebInstallTest, InstallFromManifest_EmptyId) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* params = MakeGarbageCollected<InstallParams>();
  params->setManifest("https://example.com/manifest.json");
  params->setId("");

  NonThrowableExceptionState exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              params, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(NavigatorWebInstallTest, InstallFromManifest_NullIdTreatedAsAbsent) {
  // `id` is declared `USVString?` in install_params.idl. When JS passes `null`,
  // the binding produces a null `String`; this should be treated the same as
  // omitting `id` entirely, and no `manifest_id` should be set on the mojo
  // options struct.
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* params = MakeGarbageCollected<InstallParams>();
  params->setManifest("https://example.com/manifest.json");
  params->setId(String());

  NonThrowableExceptionState exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              params, exception_state);
  ASSERT_FALSE(exception_state.HadException());
  ScriptPromiseTester tester(GetScriptState(), promise);

  mock_service().WaitForManifestCall();
  ASSERT_TRUE(mock_service().manifest_options());
  EXPECT_EQ(mock_service().manifest_options()->manifest_url,
            KURL("https://example.com/manifest.json"));
  EXPECT_FALSE(mock_service().manifest_options()->manifest_id);
  mock_service().RespondToManifestInstallWithSuccess();

  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsFulfilled());
}

TEST_F(NavigatorWebInstallTest, InstallFromManifest_InvalidId) {
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);
  auto* params = MakeGarbageCollected<InstallParams>();
  params->setManifest("https://example.com/manifest.json");
  params->setId("://invalid");

  DummyExceptionStateForTesting exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              params, exception_state);
  ASSERT_FALSE(exception_state.HadException());

  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(NavigatorWebInstallTest,
       InstallFromManifest_BlockedWithoutUserActivation) {
  auto* params = MakeGarbageCollected<InstallParams>();
  params->setManifest("https://example.com/manifest.json");

  DummyExceptionStateForTesting exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              params, exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(DOMExceptionCode::kNotAllowedError,
            exception_state.CodeAs<DOMExceptionCode>());
  EXPECT_TRUE(promise.IsEmpty());
}

TEST_F(NavigatorWebInstallTest, InstallFromManifest_BlockedInSandbox) {
  GetFrame().DomWindow()->GetSecurityContext().SetSandboxFlags(
      network::mojom::blink::WebSandboxFlags::kAll);
  LocalFrame::NotifyUserActivation(
      &GetFrame(), mojom::UserActivationNotificationType::kTest);

  auto* params = MakeGarbageCollected<InstallParams>();
  params->setManifest("https://example.com/manifest.json");

  DummyExceptionStateForTesting exception_state;
  auto promise = NavigatorWebInstall::install(GetScriptState(), *GetNavigator(),
                                              params, exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(DOMExceptionCode::kSecurityError,
            exception_state.CodeAs<DOMExceptionCode>());
  EXPECT_TRUE(promise.IsEmpty());
}

}  // namespace blink
