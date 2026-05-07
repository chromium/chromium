// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/awc/additional_windowing_controls.h"

#include "base/test/gmock_callback_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

namespace {

using testing::_;

class MockChromeClient : public EmptyChromeClient {
 public:
  MOCK_METHOD(void,
              Maximize,
              (LocalFrame&, base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              Minimize,
              (LocalFrame&, base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              Restore,
              (LocalFrame&, base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void,
              SetResizable,
              (bool, LocalFrame&, base::OnceCallback<void(bool)>),
              (override));
};

class MockPermissionService final : public mojom::blink::PermissionService {
 public:
  MockPermissionService() = default;
  ~MockPermissionService() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(this, mojo::PendingReceiver<mojom::blink::PermissionService>(
                             std::move(handle)));
  }

  void SetPermissionStatus(mojom::blink::PermissionStatus status) {
    permission_ = status;
  }

  void Flush() {
    receivers_.FlushForTesting();
    if (observer_.is_bound()) {
      observer_.FlushForTesting();
    }
  }

  // mojom::blink::PermissionService implementation
  void HasPermission(mojom::blink::PermissionDescriptorPtr permission,
                     HasPermissionCallback callback) override {
    EXPECT_EQ(permission->name,
              mojom::blink::PermissionName::WINDOW_MANAGEMENT);
    has_permission_call_count_++;
    std::move(callback).Run(
        mojom::blink::PermissionStatusWithDetails::New(permission_, nullptr));
  }

  void RegisterPageEmbeddedPermissionControl(
      Vector<mojom::blink::PermissionDescriptorPtr> permissions,
      mojom::blink::EmbeddedPermissionRequestDescriptorPtr descriptor,
      mojo::PendingRemote<mojom::blink::EmbeddedPermissionControlClient> client)
      override {}

  void RequestPageEmbeddedPermission(
      Vector<mojom::blink::PermissionDescriptorPtr> descriptors,
      mojom::blink::EmbeddedPermissionRequestDescriptorPtr permissions,
      RequestPageEmbeddedPermissionCallback callback) override {}

  void RequestPermission(mojom::blink::PermissionDescriptorPtr permission,
                         RequestPermissionCallback callback) override {
    EXPECT_EQ(permission->name,
              mojom::blink::PermissionName::WINDOW_MANAGEMENT);
    request_permission_call_count_++;
    std::move(callback).Run(
        mojom::blink::PermissionStatusWithDetails::New(permission_, nullptr));
  }

  void RequestPermissions(
      Vector<mojom::blink::PermissionDescriptorPtr> permissions,
      RequestPermissionsCallback callback) override {}

  void RevokePermission(mojom::blink::PermissionDescriptorPtr permission,
                        RevokePermissionCallback callback) override {}

  void AddPermissionObserver(
      mojom::blink::PermissionDescriptorPtr permission,
      mojom::blink::PermissionStatusWithDetailsPtr last_known_status,
      mojo::PendingRemote<mojom::blink::PermissionObserver> observer) override {
    EXPECT_EQ(permission->name,
              mojom::blink::PermissionName::WINDOW_MANAGEMENT);
    observer_.Bind(std::move(observer));
  }

  void AddPageEmbeddedPermissionObserver(
      mojom::blink::PermissionDescriptorPtr permission,
      mojom::blink::PermissionStatus last_known_status,
      mojo::PendingRemote<mojom::blink::PermissionObserver> observer) override {
  }

  void NotifyEventListener(mojom::blink::PermissionDescriptorPtr permission,
                           const String& event_type,
                           bool is_added) override {}

  void NotifyObserver() {
    CHECK(observer_.is_bound());
    observer_->OnPermissionStatusChange(
        mojom::blink::PermissionStatusWithDetails::New(permission_, nullptr));
  }

  int GetHasPermissionCallCount() const { return has_permission_call_count_; }
  int GetRequestPermissionCallCount() const {
    return request_permission_call_count_;
  }

 private:
  int has_permission_call_count_ = 0;
  int request_permission_call_count_ = 0;

  mojom::blink::PermissionStatus permission_ =
      mojom::blink::PermissionStatus::GRANTED;

  mojo::ReceiverSet<mojom::blink::PermissionService> receivers_;
  mojo::Remote<mojom::blink::PermissionObserver> observer_;
};

}  // namespace

class AdditionalWindowingControlsTestBase : public testing::Test {
 protected:
  void InitializeBase(LocalDOMWindow* window, ScriptState* script_state) {
    window_ = window;
    script_state_ = script_state;

    window_->document()->SetURL(KURL("https://example.com"));
    window_->document()->GetSettings()->SetWebAppScope(
        window_->document()->Url().GetString());

    mock_permission_service_ = std::make_unique<MockPermissionService>();
    window_->GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::PermissionService::Name_,
        BindRepeating(&MockPermissionService::Bind,
                      Unretained(mock_permission_service_.get())));
  }

  void TearDown() override {
    if (window_) {
      window_->GetBrowserInterfaceBroker().SetBinderForTesting(
          mojom::blink::PermissionService::Name_, base::NullCallback());
    }
    window_.Clear();
    script_state_.Clear();
  }

  LocalDOMWindow& GetWindow() const { return *window_; }
  Document& GetDocument() const { return *window_->document(); }
  ScriptState* GetScriptState() const { return script_state_.Get(); }
  MockPermissionService& GetMockPermissionService() const {
    return *mock_permission_service_;
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<MockPermissionService> mock_permission_service_;

  Persistent<LocalDOMWindow> window_;
  Persistent<ScriptState> script_state_;
};

using AwcActionPtr = ScriptPromise<IDLUndefined> (*)(ScriptState*,
                                                     LocalDOMWindow&,
                                                     ExceptionState&);

struct AwcTestParam {
  AwcActionPtr action;
  ui::mojom::blink::WindowShowState initial_state;
  std::optional<ui::mojom::blink::WindowShowState> target_state;

  bool IsAlreadyInTargetState() const {
    return target_state.has_value() && initial_state == target_state.value();
  }
};

class AdditionalWindowingControlsTest
    : public AdditionalWindowingControlsTestBase,
      public testing::WithParamInterface<AwcTestParam> {
 protected:
  void SetUp() override {
    helper_.Initialize();

    auto* widget = helper_.LocalMainFrame()->FrameWidgetImpl();
    ASSERT_TRUE(widget);
    widget->SetWindowShowState(GetParam().initial_state);

    LocalDOMWindow* window = helper_.LocalMainFrame()->GetFrame()->DomWindow();
    ScriptState* script_state = ToScriptStateForMainWorld(window->GetFrame());

    InitializeBase(window, script_state);
  }

  ScriptPromise<IDLUndefined> RunAwcApiCall(ScriptState* script_state,
                                            LocalDOMWindow& window,
                                            ExceptionState& exception_state) {
    return GetParam().action(script_state, window, exception_state);
  }

  void ExpectDOMException(ScriptState* script_state,
                          const ScriptPromiseTester& tester,
                          const String& expected_name,
                          const String& expected_message) {
    auto* dom_exception = V8DOMException::ToWrappable(
        script_state->GetIsolate(), tester.Value().V8Value());
    ASSERT_TRUE(dom_exception);
    EXPECT_EQ(dom_exception->name(), expected_name);
    EXPECT_EQ(dom_exception->message(), expected_message);
  }

  frame_test_helpers::WebViewHelper helper_;
};

ScriptPromise<IDLUndefined> SetResizableTrueWrapper(
    ScriptState* script_state,
    LocalDOMWindow& window,
    ExceptionState& exception_state) {
  return AdditionalWindowingControls::setResizable(
      script_state, window, /*resizable=*/true, exception_state);
}

ScriptPromise<IDLUndefined> SetResizableFalseWrapper(
    ScriptState* script_state,
    LocalDOMWindow& window,
    ExceptionState& exception_state) {
  return AdditionalWindowingControls::setResizable(
      script_state, window, /*resizable=*/false, exception_state);
}

TEST_P(AdditionalWindowingControlsTest, RejectsIfFrameDetached) {
  ScriptState::Scope scope(GetScriptState());
  DummyExceptionStateForTesting exception_state;
  Persistent<LocalDOMWindow> window = &GetWindow();

  GetDocument().GetFrame()->Detach(FrameDetachType::kRemove);

  RunAwcApiCall(GetScriptState(), *window, exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST_P(AdditionalWindowingControlsTest, RejectsIfPrerendering) {
  ScriptState::Scope scope(GetScriptState());
  DummyExceptionStateForTesting exception_state;

  GetDocument().GetPage()->SetIsPrerendering(true);
  RunAwcApiCall(GetScriptState(), GetWindow(), exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
}

TEST_P(AdditionalWindowingControlsTest, RejectsInIframe) {
  frame_test_helpers::LoadHTMLString(
      helper_.LocalMainFrame(),
      "<body><iframe srcdoc='<div>child</div>'></iframe></body>",
      url_test_helpers::ToKURL("https://example.com"));

  auto* main_frame = helper_.LocalMainFrame()->GetFrame();
  auto* child_frame = To<LocalFrame>(main_frame->Tree().FirstChild());
  ASSERT_TRUE(child_frame);

  LocalDOMWindow* child_window = child_frame->DomWindow();
  ASSERT_TRUE(child_window);
  ASSERT_FALSE(child_frame->IsOutermostMainFrame());

  ScriptState* child_script_state = ToScriptStateForMainWorld(child_frame);
  ScriptState::Scope child_scope(child_script_state);
  DummyExceptionStateForTesting exception_state;

  RunAwcApiCall(child_script_state, *child_window, exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
  EXPECT_EQ(exception_state.Message(),
            "API is only supported in primary top-level browsing contexts.");
}

TEST_P(AdditionalWindowingControlsTest, RejectsIfNotInWebAppScope) {
  ScriptState::Scope scope(GetScriptState());
  DummyExceptionStateForTesting exception_state;

  // Clear the web app scope to simulate a standard browser tab.
  GetDocument().GetSettings()->SetWebAppScope(String());

  RunAwcApiCall(GetScriptState(), GetWindow(), exception_state);

  EXPECT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception_state.CodeAs<DOMExceptionCode>(),
            DOMExceptionCode::kInvalidStateError);
  EXPECT_EQ(exception_state.Message(), "API is only supported in web apps.");
}

TEST_P(AdditionalWindowingControlsTest, PermissionDenied) {
  ScriptState::Scope scope(GetScriptState());
  DummyExceptionStateForTesting exception_state;

  GetMockPermissionService().SetPermissionStatus(
      mojom::blink::PermissionStatus::DENIED);

  auto promise = RunAwcApiCall(GetScriptState(), GetWindow(), exception_state);
  GetMockPermissionService().Flush();

  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();

  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(tester.IsRejected());

  ExpectDOMException(GetScriptState(), tester, "NotAllowedError",
                     "Permission denied.");
}

TEST_P(AdditionalWindowingControlsTest, PermissionDeferred) {
  ScriptState::Scope scope(GetScriptState());
  DummyExceptionStateForTesting exception_state;

  GetMockPermissionService().SetPermissionStatus(
      mojom::blink::PermissionStatus::ASK);

  auto promise = RunAwcApiCall(GetScriptState(), GetWindow(), exception_state);
  GetMockPermissionService().Flush();

  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();

  EXPECT_FALSE(exception_state.HadException());
  EXPECT_TRUE(tester.IsRejected());

  ExpectDOMException(GetScriptState(), tester, "NotAllowedError",
                     "Permission decision deferred.");
}

TEST_P(AdditionalWindowingControlsTest,
       ContextDestroyedBeforePermissionResponse) {
  GetMockPermissionService().SetPermissionStatus(
      mojom::blink::PermissionStatus::GRANTED);
  {
    ScriptState::Scope scope(GetScriptState());
    DummyExceptionStateForTesting exception_state;

    RunAwcApiCall(GetScriptState(), GetWindow(), exception_state);
  }

  GetDocument().GetFrame()->Detach(FrameDetachType::kRemove);

  EXPECT_NO_FATAL_FAILURE(GetMockPermissionService().Flush());
}

TEST_P(AdditionalWindowingControlsTest,
       ConsumesTransientUserActivationAndRequestsPermission) {
  ScriptState::Scope scope(GetScriptState());
  DummyExceptionStateForTesting exception_state;

  GetMockPermissionService().SetPermissionStatus(
      mojom::blink::PermissionStatus::GRANTED);

  LocalFrame::NotifyUserActivation(
      GetWindow().GetFrame(),
      mojom::blink::UserActivationNotificationType::kTest);
  EXPECT_TRUE(LocalFrame::HasTransientUserActivation(GetWindow().GetFrame()));

  RunAwcApiCall(GetScriptState(), GetWindow(), exception_state);
  GetMockPermissionService().Flush();

  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(GetWindow().GetFrame()));

  EXPECT_EQ(GetMockPermissionService().GetRequestPermissionCallCount(), 1);
  EXPECT_EQ(GetMockPermissionService().GetHasPermissionCallCount(), 0);
}

TEST_P(AdditionalWindowingControlsTest, ChecksPermissionWithoutUserActivation) {
  ScriptState::Scope scope(GetScriptState());
  DummyExceptionStateForTesting exception_state;

  GetMockPermissionService().SetPermissionStatus(
      mojom::blink::PermissionStatus::GRANTED);

  EXPECT_FALSE(LocalFrame::HasTransientUserActivation(GetWindow().GetFrame()));

  RunAwcApiCall(GetScriptState(), GetWindow(), exception_state);
  GetMockPermissionService().Flush();

  EXPECT_EQ(GetMockPermissionService().GetHasPermissionCallCount(), 1);
  EXPECT_EQ(GetMockPermissionService().GetRequestPermissionCallCount(), 0);
}

TEST_P(AdditionalWindowingControlsTest,
       ResolvesImmediatelyIfAlreadyInTargetState) {
  if (!GetParam().IsAlreadyInTargetState()) {
    GTEST_SKIP() << "This test case only verifies behavior when the system is "
                    "already in the target state.";
  }

  ScriptState* script_state = GetScriptState();
  ScriptState::Scope scope(script_state);
  DummyExceptionStateForTesting exception_state;

  GetMockPermissionService().SetPermissionStatus(
      mojom::blink::PermissionStatus::GRANTED);

  LocalFrame::NotifyUserActivation(
      GetWindow().GetFrame(),
      mojom::blink::UserActivationNotificationType::kTest);

  auto promise = RunAwcApiCall(script_state, GetWindow(), exception_state);
  ASSERT_FALSE(exception_state.HadException())
      << exception_state.Message().Utf8();
  GetMockPermissionService().Flush();

  ScriptPromiseTester tester(script_state, promise);
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
  EXPECT_FALSE(exception_state.HadException());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AdditionalWindowingControlsTest,
    testing::Values(
        // Maximize Scenarios.
        AwcTestParam{
            .action = &AdditionalWindowingControls::maximize,
            .initial_state = ui::mojom::blink::WindowShowState::kNormal,
            .target_state = ui::mojom::blink::WindowShowState::kMaximized},
        AwcTestParam{
            .action = &AdditionalWindowingControls::maximize,
            .initial_state = ui::mojom::blink::WindowShowState::kMaximized,
            .target_state = ui::mojom::blink::WindowShowState::kMaximized},

        // Minimize Scenarios.
        AwcTestParam{
            .action = &AdditionalWindowingControls::minimize,
            .initial_state = ui::mojom::blink::WindowShowState::kNormal,
            .target_state = ui::mojom::blink::WindowShowState::kMinimized},
        AwcTestParam{
            .action = &AdditionalWindowingControls::minimize,
            .initial_state = ui::mojom::blink::WindowShowState::kMinimized,
            .target_state = ui::mojom::blink::WindowShowState::kMinimized},

        // Restore Scenarios.
        AwcTestParam{
            .action = &AdditionalWindowingControls::restore,
            .initial_state = ui::mojom::blink::WindowShowState::kMaximized,
            .target_state = ui::mojom::blink::WindowShowState::kNormal},
        AwcTestParam{
            .action = &AdditionalWindowingControls::restore,
            .initial_state = ui::mojom::blink::WindowShowState::kMinimized,
            .target_state = ui::mojom::blink::WindowShowState::kNormal},
        AwcTestParam{
            .action = &AdditionalWindowingControls::restore,
            .initial_state = ui::mojom::blink::WindowShowState::kNormal,
            .target_state = ui::mojom::blink::WindowShowState::kNormal},
        AwcTestParam{
            .action = &AdditionalWindowingControls::restore,
            .initial_state = ui::mojom::blink::WindowShowState::kDefault,
            .target_state = ui::mojom::blink::WindowShowState::kDefault},

        // SetResizable Scenarios.
        AwcTestParam{
            .action = &SetResizableTrueWrapper,
            .initial_state = ui::mojom::blink::WindowShowState::kDefault,
            .target_state = std::nullopt},
        AwcTestParam{
            .action = &SetResizableFalseWrapper,
            .initial_state = ui::mojom::blink::WindowShowState::kDefault,
            .target_state = std::nullopt}));

struct ExecutionTestParam {
  AwcActionPtr action;
  void (*setup_expectation)(MockChromeClient&, bool success);
  const char* expected_error_message;
  std::optional<ui::mojom::blink::WindowShowState> initial_state = std::nullopt;
};

class AdditionalWindowingControlsExecutionTest
    : public AdditionalWindowingControlsTestBase,
      public testing::WithParamInterface<ExecutionTestParam> {
 protected:
  void SetUp() override {
    mock_chrome_client_ = MakeGarbageCollected<MockChromeClient>();
    page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(800, 600),
                                                     mock_chrome_client_);

    if (GetParam().initial_state.has_value()) {
      if (auto* widget = page_holder_->GetFrame().GetWidgetForLocalRoot()) {
        static_cast<WebFrameWidgetImpl*>(widget)->SetWindowShowState(
            GetParam().initial_state.value());
      }
    }

    LocalDOMWindow* window = page_holder_->GetFrame().DomWindow();
    ScriptState* script_state =
        ToScriptStateForMainWorld(&page_holder_->GetFrame());

    InitializeBase(window, script_state);
  }

  MockChromeClient& GetMockChromeClient() const { return *mock_chrome_client_; }

  Persistent<MockChromeClient> mock_chrome_client_;
  std::unique_ptr<DummyPageHolder> page_holder_;
};

TEST_P(AdditionalWindowingControlsExecutionTest, ResolvesOnBrowserSuccess) {
  ScriptState::Scope scope(GetScriptState());
  DummyExceptionStateForTesting exception_state;

  GetMockPermissionService().SetPermissionStatus(
      mojom::blink::PermissionStatus::GRANTED);
  LocalFrame::NotifyUserActivation(
      GetWindow().GetFrame(),
      mojom::blink::UserActivationNotificationType::kTest);

  GetParam().setup_expectation(GetMockChromeClient(), /*success=*/true);

  auto promise =
      GetParam().action(GetScriptState(), GetWindow(), exception_state);
  ASSERT_FALSE(exception_state.HadException())
      << exception_state.Message().Utf8();
  GetMockPermissionService().Flush();

  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsFulfilled());
}

TEST_P(AdditionalWindowingControlsExecutionTest, RejectsOnBrowserFailure) {
  ScriptState::Scope scope(GetScriptState());
  DummyExceptionStateForTesting exception_state;

  GetMockPermissionService().SetPermissionStatus(
      mojom::blink::PermissionStatus::GRANTED);
  LocalFrame::NotifyUserActivation(
      GetWindow().GetFrame(),
      mojom::blink::UserActivationNotificationType::kTest);

  GetParam().setup_expectation(GetMockChromeClient(), /*success=*/false);

  auto promise =
      GetParam().action(GetScriptState(), GetWindow(), exception_state);

  ASSERT_FALSE(exception_state.HadException())
      << exception_state.Message().Utf8();

  GetMockPermissionService().Flush();

  ScriptPromiseTester tester(GetScriptState(), promise);
  tester.WaitUntilSettled();

  EXPECT_TRUE(tester.IsRejected());

  auto* dom_exception = V8DOMException::ToWrappable(
      GetScriptState()->GetIsolate(), tester.Value().V8Value());
  ASSERT_TRUE(dom_exception);
  EXPECT_EQ(dom_exception->name(), "NotAllowedError");
  EXPECT_EQ(dom_exception->message(), GetParam().expected_error_message);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    AdditionalWindowingControlsExecutionTest,
    testing::Values(
        ExecutionTestParam{
            .action = &AdditionalWindowingControls::maximize,
            .setup_expectation =
                [](MockChromeClient& mock, bool success) {
                  EXPECT_CALL(mock, Maximize(testing::_, testing::_))
                      .WillOnce(base::test::RunOnceCallback<1>(success));
                },
            .expected_error_message = "Could not maximize the window."},

        ExecutionTestParam{
            .action = &AdditionalWindowingControls::minimize,
            .setup_expectation =
                [](MockChromeClient& mock, bool success) {
                  EXPECT_CALL(mock, Minimize(testing::_, testing::_))
                      .WillOnce(base::test::RunOnceCallback<1>(success));
                },
            .expected_error_message = "Could not minimize the window."},

        ExecutionTestParam{
            .action = &AdditionalWindowingControls::restore,
            .setup_expectation =
                [](MockChromeClient& mock, bool success) {
                  EXPECT_CALL(mock, Restore(testing::_, testing::_))
                      .WillOnce(base::test::RunOnceCallback<1>(success));
                },
            .expected_error_message = "Could not restore the window.",
            .initial_state = ui::mojom::blink::WindowShowState::kMaximized},

        ExecutionTestParam{
            .action = &SetResizableTrueWrapper,
            .setup_expectation =
                [](MockChromeClient& mock, bool success) {
                  EXPECT_CALL(mock, SetResizable(true, testing::_, testing::_))
                      .WillOnce(base::test::RunOnceCallback<2>(success));
                },
            .expected_error_message =
                "Could not set the window to be resizable."},

        ExecutionTestParam{
            .action = &SetResizableFalseWrapper,
            .setup_expectation =
                [](MockChromeClient& mock, bool success) {
                  EXPECT_CALL(mock, SetResizable(false, testing::_, testing::_))
                      .WillOnce(base::test::RunOnceCallback<2>(success));
                },
            .expected_error_message =
                "Could not set the window to be non-resizable."}));

}  // namespace blink
