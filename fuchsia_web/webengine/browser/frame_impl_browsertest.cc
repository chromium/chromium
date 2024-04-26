// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/element/cpp/fidl.h>
#include <fuchsia/ui/views/cpp/fidl.h>
#include <lib/zx/time.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "components/fuchsia_component_support/annotations_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_utils.h"
#include "fuchsia_web/common/string_util.h"
#include "fuchsia_web/common/test/fit_adapter.h"
#include "fuchsia_web/common/test/frame_for_test.h"
#include "fuchsia_web/common/test/frame_test_util.h"
#include "fuchsia_web/common/test/test_navigation_listener.h"
#include "fuchsia_web/webengine/browser/context_impl.h"
#include "fuchsia_web/webengine/browser/fake_semantics_manager.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "fuchsia_web/webengine/browser/frame_impl_browser_test_base.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/public/ozone_platform.h"

using testing::_;
using testing::AllOf;
using testing::AtLeast;
using testing::Contains;
using testing::Field;
using testing::InvokeWithoutArgs;
using testing::Key;
using testing::Mock;
using testing::Not;

// Use a shorter name for NavigationState, because it is referenced frequently
// in this file.
using NavigationDetails = fuchsia::web::NavigationState;
using OnNavigationStateChangedCallback =
    fuchsia::web::NavigationEventListener::OnNavigationStateChangedCallback;

namespace {

constexpr char kPage1Path[] = "/title1.html";
constexpr char kPage2Path[] = "/title2.html";
constexpr char kPage3Path[] = "/websql.html";
constexpr char kReportCloseEventsPath[] = "/report_close_events.html";
constexpr char kVisibilityPath[] = "/visibility.html";
constexpr char kWaitSizePath[] = "/wait-size.html";
constexpr char kPage1Title[] = "title 1";
constexpr char kPage2Title[] = "title 2";
constexpr char kPage3Title[] = "websql not available";
constexpr char kDataUrl[] =
    "data:text/html;base64,PGI+SGVsbG8sIHdvcmxkLi4uPC9iPg==";

MATCHER_P(NavigationHandleUrlEquals,
          url,
          "Checks equality with a NavigationHandle's URL.") {
  return arg->GetURL() == url;
}

class MockWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit MockWebContentsObserver(content::WebContents* web_contents) {
    Observe(web_contents);
  }

  ~MockWebContentsObserver() override = default;

  MOCK_METHOD1(DidStartNavigation, void(content::NavigationHandle*));

  MOCK_METHOD1(RenderViewDeleted,
               void(content::RenderViewHost* render_view_host));
};

std::string GetDocumentVisibilityState(fuchsia::web::Frame* frame) {
  auto visibility = base::MakeRefCounted<base::RefCountedData<std::string>>();
  base::RunLoop loop;
  frame->ExecuteJavaScript(
      {"*"}, base::MemBufferFromString("document.visibilityState;", "test"),
      [visibility, quit_loop = loop.QuitClosure()](
          fuchsia::web::Frame_ExecuteJavaScript_Result result) {
        ASSERT_TRUE(result.is_response());
        visibility->data = *base::StringFromMemBuffer(result.response().result);
        quit_loop.Run();
      });
  loop.Run();
  return visibility->data;
}

}  // namespace

// Defines a suite of tests that exercise Frame-level functionality, such as
// navigation commands and page events.
class FrameImplTest : public FrameImplTestBase {
 public:
  FrameImplTest() = default;
  ~FrameImplTest() override = default;

  FrameImplTest(const FrameImplTest&) = delete;
  FrameImplTest& operator=(const FrameImplTest&) = delete;

  MOCK_METHOD1(OnServeHttpRequest,
               void(const net::test_server::HttpRequest& request));
};

// Verifies that Frames are initially "hidden", changes to "visible" once the
// View is attached to a Presenter and back to "hidden" when the View is
// detached from the Presenter.
// TODO(crbug.com/42050537): Re-enable this test on Arm64 when femu is available
// for that architecture. This test requires Vulkan and Scenic to properly
// signal the Views visibility.
#if defined(ARCH_CPU_ARM_FAMILY)
#define MAYBE_VisibilityState DISABLED_VisibilityState
#else
#define MAYBE_VisibilityState VisibilityState
#endif
IN_PROC_BROWSER_TEST_F(FrameImplTest, MAYBE_VisibilityState) {
  // This test uses the `fuchsia.ui.composition` variant of
  // `Frame.CreateView*()`.
  ASSERT_EQ(ui::OzonePlatform::GetPlatformNameForTest(), "flatland");

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL page_url(embedded_test_server()->GetURL(kVisibilityPath));

  auto frame = FrameForTest::Create(context(), {});
  frame.ptr().set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status);
    ADD_FAILURE() << "Frame disconnected.";
  });
  base::RunLoop().RunUntilIdle();

  // Navigate to a page and wait for it to finish loading.
  ASSERT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(page_url);

  // Query the document.visibilityState before creating a View.
  EXPECT_EQ(GetDocumentVisibilityState(frame.ptr().get()), "\"hidden\"");

  // Query the document.visibilityState after creating the View, but without it
  // actually "attached" to the view tree.
  fuchsia::ui::views::ViewCreationToken view_token;
  fuchsia::ui::views::ViewportCreationToken viewport_token;
  auto status =
      zx::channel::create(0, &viewport_token.value, &view_token.value);
  ASSERT_EQ(ZX_OK, status);
  fuchsia::web::CreateView2Args create_view_args;
  create_view_args.set_view_creation_token(std::move(view_token));
  frame->CreateView2(std::move(create_view_args));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetDocumentVisibilityState(frame.ptr().get()), "\"hidden\"");

  // Attach the View to a Presenter, the page should be visible.
  auto annotations_manager =
      std::make_unique<fuchsia_component_support::AnnotationsManager>();
  fuchsia::element::AnnotationControllerHandle annotation_controller;
  annotations_manager->Connect(annotation_controller.NewRequest());
  auto presenter = base::ComponentContextForProcess()
                       ->svc()
                       ->Connect<::fuchsia::element::GraphicalPresenter>();
  presenter.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "GraphicalPresenter disconnected.";
    ADD_FAILURE();
  });
  fuchsia::element::ViewSpec view_spec;
  view_spec.set_viewport_creation_token(std::move(viewport_token));
  view_spec.set_annotations({});
  fuchsia::element::ViewControllerPtr view_controller;
  presenter->PresentView(std::move(view_spec), std::move(annotation_controller),
                         view_controller.NewRequest(),
                         [](auto result) { EXPECT_FALSE(result.is_err()); });
  frame.navigation_listener().RunUntilTitleEquals("visible");

  // TODO(fxbug.dev/114431): Flatland does not support dismissing a view through
  // the ViewController.
  // Detach the ViewController, causing the View to be
  // detached. This is a regression test for crbug.com/1141093, verifying that
  // the page receives a "not visible" event as a result.
  // view_controller->Dismiss();
  // frame.navigation_listener().RunUntilTitleEquals("hidden");
}

// Verifies that the browser will navigate and generate a navigation listener
// event when LoadUrl() is called.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigateFrame) {
  auto frame = FrameForTest::Create(context(), {});

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url::kAboutBlankURL));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(
      GURL(url::kAboutBlankURL), url::kAboutBlankURL);
}

// Verifies that the browser does not crash if the Renderer process exits while
// a navigation event listener is attached to the Frame.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigationListenerRendererProcessGone) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL("/nocontent"));

  // Create a Frame and navigate it to a URL that will not "commit".
  auto frame = FrameForTest::Create(context(), {});

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));

  // Terminate the Renderer process and run the browser until that is observed.
  content::RenderProcessHost* child_process =
      context_impl()
          ->GetFrameImplForTest(&frame.ptr())
          ->web_contents()
          ->GetPrimaryMainFrame()
          ->GetProcess();

  content::RenderProcessHostWatcher(
      child_process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_READY)
      .Wait();
  ASSERT_TRUE(child_process->IsReady());

  content::RenderProcessHostWatcher exit_observer(
      child_process, content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0);
  exit_observer.Wait();
  EXPECT_FALSE(exit_observer.did_exit_normally());

  // Spin the browser main loop to flush any queued work, or FIDL activity.
  base::RunLoop().RunUntilIdle();
}

// Verifies that the renderer process consumes more memory for document
// rendering.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigationIncreasesMemoryUsage) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  auto frame = FrameForTest::Create(context(), {});

  // Get the renderer size when no renderer process is active.
  base::test::TestFuture<uint64_t> before_nav_size;
  frame->GetPrivateMemorySize(
      CallbackToFitFunction(before_nav_size.GetCallback()));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, kPage1Title);

  base::test::TestFuture<uint64_t> after_nav_size;
  frame->GetPrivateMemorySize(
      CallbackToFitFunction(after_nav_size.GetCallback()));
  ASSERT_TRUE(after_nav_size.Wait());

  EXPECT_EQ(before_nav_size.Get(), 0u);  // No render process - zero bytes.
  EXPECT_GT(after_nav_size.Get(), 0u);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigateDataFrame) {
  auto frame = FrameForTest::Create(context(), {});

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       kDataUrl));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(GURL(kDataUrl),
                                                        kDataUrl);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, FrameDeletedBeforeContext) {
  auto frame = FrameForTest::Create(context(), {});

  // Process the frame creation message.
  base::RunLoop().RunUntilIdle();

  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame.ptr());
  MockWebContentsObserver deletion_observer(frame_impl->web_contents());
  base::RunLoop run_loop;
  EXPECT_CALL(deletion_observer, RenderViewDeleted(_))
      .WillOnce(InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url::kAboutBlankURL));

  frame.ptr().Unbind();
  run_loop.Run();

  // Check that |context| remains bound after the frame is closed.
  EXPECT_TRUE(context());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ContextDeletedBeforeFrame) {
  auto frame = FrameForTest::Create(context(), {});
  EXPECT_TRUE(frame.ptr());

  base::RunLoop run_loop;
  frame.ptr().set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  context().Unbind();
  run_loop.Run();
  EXPECT_FALSE(frame.ptr());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ContextDeletedBeforeFrameWithView) {
  auto frame = FrameForTest::Create(context(), {});
  EXPECT_TRUE(frame.ptr());
  base::RunLoop().RunUntilIdle();
  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame.ptr());

  fuchsia::ui::views::ViewCreationToken view_token;
  fuchsia::ui::views::ViewportCreationToken viewport_token;
  auto status =
      zx::channel::create(0, &viewport_token.value, &view_token.value);
  ZX_CHECK(status == ZX_OK, status);
  fuchsia::web::CreateView2Args create_view_args;
  create_view_args.set_view_creation_token(std::move(view_token));
  frame->CreateView2(std::move(create_view_args));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(frame_impl->has_view_for_test());

  base::RunLoop run_loop;
  frame.ptr().set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  context().Unbind();
  run_loop.Run();
  EXPECT_FALSE(frame.ptr());
}

// TODO(crbug.com/40507959): Remove this test when WebSQL is removed from
// Chrome.
IN_PROC_BROWSER_TEST_F(FrameImplTest, EnsureWebSqlDisabled) {
  auto frame = FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL title3(embedded_test_server()->GetURL(kPage3Path));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       title3.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(title3, kPage3Title);
}

namespace {

void VerifyCanGoBackAndForward(FrameForTest& frame,
                               bool can_go_back_expected,
                               bool can_go_forward_expected) {
  auto* state = frame.navigation_listener().current_state();
  EXPECT_TRUE(state->has_can_go_back());
  EXPECT_EQ(state->can_go_back(), can_go_back_expected);
  EXPECT_TRUE(state->has_can_go_forward());
  EXPECT_EQ(state->can_go_forward(), can_go_forward_expected);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(FrameImplTest, GoBackAndForward) {
  auto frame = FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       title1.spec()));
  frame.navigation_listener().RunUntilUrlTitleBackForwardEquals(
      title1, kPage1Title, false, false);

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       title2.spec()));
  frame.navigation_listener().RunUntilUrlTitleBackForwardEquals(
      title2, kPage2Title, true, false);

  frame.GetNavigationController()->GoBack();
  frame.navigation_listener().RunUntilUrlTitleBackForwardEquals(
      title1, kPage1Title, false, true);

  // At the top of the navigation entry list; this should be a no-op.
  frame.GetNavigationController()->GoBack();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();

  // Verify that the can-go-back/forward state has not changed.
  VerifyCanGoBackAndForward(frame, false, true);

  frame.GetNavigationController()->GoForward();
  frame.navigation_listener().RunUntilUrlTitleBackForwardEquals(
      title2, kPage2Title, true, false);

  // At the end of the navigation entry list; this should be a no-op.
  frame.GetNavigationController()->GoForward();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();

  // Back/forward state should not have changed, since the request was a
  // no-op.
  VerifyCanGoBackAndForward(frame, true, false);
}

namespace {

// An HTTP response stream whose response payload can be sent as "chunks"
// with indeterminate-length pauses in between.
class ChunkedHttpTransaction {
 public:
  explicit ChunkedHttpTransaction(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate)
      : io_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
        send_state_(SendState::IDLE),
        delegate_(delegate) {
    EXPECT_FALSE(current_instance_);

    current_instance_ = this;
  }

  ChunkedHttpTransaction(const ChunkedHttpTransaction&) = delete;
  ChunkedHttpTransaction& operator=(const ChunkedHttpTransaction&) = delete;

  static ChunkedHttpTransaction* current() {
    EXPECT_TRUE(current_instance_);
    return current_instance_;
  }

  void Close() {
    EnsureSendCompleted();
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&net::test_server::HttpResponseDelegate::FinishResponse,
                       delegate_));
    delete this;
  }

  void EnsureSendCompleted() {
    if (send_state_ == SendState::IDLE) {
      return;
    }

    base::RunLoop run_loop;
    send_chunk_complete_callback_ = run_loop.QuitClosure();
    run_loop.Run();
    EXPECT_EQ(send_state_, SendState::IDLE);
  }

  void SendChunk(const std::string& chunk) {
    EnsureSendCompleted();

    send_state_ = SendState::BLOCKED;

    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &net::test_server::HttpResponseDelegate::SendContents, delegate_,
            chunk,
            base::BindOnce(&ChunkedHttpTransaction::SendChunkCompleteOnIoThread,
                           base::Unretained(this),
                           base::SingleThreadTaskRunner::GetCurrentDefault())));
  }

 private:
  static ChunkedHttpTransaction* current_instance_;

  ~ChunkedHttpTransaction() { current_instance_ = nullptr; }

  void SendChunkCompleteOnIoThread(
      scoped_refptr<base::TaskRunner> ui_thread_task_runner) {
    ui_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&ChunkedHttpTransaction::SendChunkCompleteOnUiThread,
                       base::Unretained(this)));
  }

  void SendChunkCompleteOnUiThread() {
    send_state_ = SendState::IDLE;
    if (send_chunk_complete_callback_) {
      std::move(send_chunk_complete_callback_).Run();
    }
  }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Set by callers to SendChunk() waiting for the previous chunk to complete.
  base::OnceClosure send_chunk_complete_callback_;

  enum SendState { IDLE, BLOCKED };

  SendState send_state_;
  base::WeakPtr<net::test_server::HttpResponseDelegate> delegate_;
};

ChunkedHttpTransaction* ChunkedHttpTransaction::current_instance_ = nullptr;

class ChunkedHttpTransactionFactory : public net::test_server::HttpResponse {
 public:
  ChunkedHttpTransactionFactory() = default;

  ChunkedHttpTransactionFactory(const ChunkedHttpTransactionFactory&) = delete;
  ChunkedHttpTransactionFactory& operator=(
      const ChunkedHttpTransactionFactory&) = delete;

  ~ChunkedHttpTransactionFactory() override = default;

  void SetOnResponseCreatedCallback(base::OnceClosure on_response_created) {
    on_response_created_ = std::move(on_response_created);
  }

  // net::test_server::HttpResponse implementation.
  void SendResponse(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate) override {
    // The ChunkedHttpTransaction manages its own lifetime.
    new ChunkedHttpTransaction(delegate);

    if (on_response_created_) {
      std::move(on_response_created_).Run();
    }
  }

 private:
  base::OnceClosure on_response_created_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigationEventDuringPendingLoad) {
  auto frame = FrameForTest::Create(context(), {});

  auto factory = std::make_unique<ChunkedHttpTransactionFactory>();
  base::RunLoop transaction_created_run_loop;
  factory->SetOnResponseCreatedCallback(
      transaction_created_run_loop.QuitClosure());
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/pausable",
      base::BindLambdaForTesting(
          [&](const net::test_server::HttpRequest&)
              -> std::unique_ptr<net::test_server::HttpResponse> {
            CHECK(factory);
            return std::move(factory);
          })));

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL hung_url(embedded_test_server()->GetURL("/pausable"));
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       hung_url.spec()));
  fuchsia::web::NavigationState state_change;
  state_change.set_url(hung_url.spec());
  state_change.set_is_main_document_loaded(false);
  transaction_created_run_loop.Run();

  ChunkedHttpTransaction* transaction = ChunkedHttpTransaction::current();
  transaction->SendChunk(
      "HTTP/1.0 200 OK\r\n"
      "Host: localhost\r\n"
      "Content-Type: text/html\r\n\r\n"
      "<html><head><title>initial load</title>");
  state_change.set_title("initial load");
  state_change.set_is_main_document_loaded(false);
  frame.navigation_listener().RunUntilNavigationStateMatches(state_change);

  transaction->SendChunk(
      "<script>document.title='final load';</script><body></body>");
  transaction->Close();
  state_change.set_title("final load");
  state_change.set_is_main_document_loaded(true);
  frame.navigation_listener().RunUntilNavigationStateMatches(state_change);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ReloadFrame) {
  auto frame = FrameForTest::Create(context(), {});

  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &FrameImplTest::OnServeHttpRequest, base::Unretained(this)));

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  EXPECT_CALL(*this, OnServeHttpRequest(_)).Times(AtLeast(1));
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, kPage1Title);

  // Reload with NO_CACHE.
  {
    MockWebContentsObserver web_contents_observer(
        context_impl()->GetFrameImplForTest(&frame.ptr())->web_contents_.get());
    base::RunLoop run_loop;
    EXPECT_CALL(web_contents_observer,
                DidStartNavigation(NavigationHandleUrlEquals(url)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    frame.GetNavigationController()->Reload(fuchsia::web::ReloadType::NO_CACHE);
    run_loop.Run();
  }

  // Reload with PARTIAL_CACHE.
  {
    MockWebContentsObserver web_contents_observer(
        context_impl()->GetFrameImplForTest(&frame.ptr())->web_contents_.get());
    base::RunLoop run_loop;
    EXPECT_CALL(web_contents_observer,
                DidStartNavigation(NavigationHandleUrlEquals(url)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    frame.GetNavigationController()->Reload(
        fuchsia::web::ReloadType::PARTIAL_CACHE);
    run_loop.Run();
  }
}

// Verifies that NavigationState correctly reports when the Renderer terminates
// or crashes.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigationState_RendererGone) {
  auto frame = FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  // Navigate to a page.
  ASSERT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, kPage1Title);

  // Kill the renderer for the tab.
  auto* frame_impl = context_impl()->GetFrameImplForTest(&frame.ptr());
  auto* web_contents = frame_impl->web_contents_for_test();
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

    content::RenderFrameDeletedObserver crash_observer(
        web_contents->GetPrimaryMainFrame());
    web_contents->GetPrimaryMainFrame()->GetProcess()->Shutdown(1);
    crash_observer.WaitUntilDeleted();
  }

  // Wait for the NavigationListener to also observe the transition.
  fuchsia::web::NavigationState error_state;
  error_state.set_page_type(fuchsia::web::PageType::ERROR);
  frame.navigation_listener().RunUntilNavigationStateMatches(error_state);

  const fuchsia::web::NavigationState* current_state =
      frame.navigation_listener().current_state();
  ASSERT_TRUE(current_state->has_url());
  EXPECT_EQ(current_state->url(), url.spec());
  ASSERT_TRUE(current_state->has_title());
  EXPECT_EQ(current_state->title(), kPage1Title);
  ASSERT_TRUE(current_state->has_page_type());
  EXPECT_EQ(current_state->page_type(), fuchsia::web::PageType::ERROR);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, NoNavigationObserverAttached) {
  auto frame = FrameForTest::Create(context(), {});
  base::RunLoop().RunUntilIdle();

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  MockWebContentsObserver observer(
      context_impl()->GetFrameImplForTest(&frame.ptr())->web_contents_.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, DidStartNavigation(NavigationHandleUrlEquals(title1)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         title1.spec()));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, DidStartNavigation(NavigationHandleUrlEquals(title2)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         title2.spec()));
    run_loop.Run();
  }
}

// Verifies that a Frame will handle navigation listener disconnection events
// gracefully.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigationObserverDisconnected) {
  auto frame = FrameForTest::Create(context(), {});
  base::RunLoop().RunUntilIdle();

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  MockWebContentsObserver web_contents_observer(
      context_impl()->GetFrameImplForTest(&frame.ptr())->web_contents_.get());
  EXPECT_CALL(web_contents_observer,
              DidStartNavigation(NavigationHandleUrlEquals(title1)));

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       title1.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(title1, kPage1Title);

  // Disconnect the listener & spin the runloop to propagate the disconnection
  // event over IPC.
  frame.navigation_listener_binding().Close(ZX_ERR_PEER_CLOSED);
  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  EXPECT_CALL(web_contents_observer,
              DidStartNavigation(NavigationHandleUrlEquals(title2)))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       title2.spec()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, DelayedNavigationEventAck) {
  auto frame = FrameForTest::Create(context(), {});
  base::RunLoop().RunUntilIdle();

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  // Expect an navigation event here, but deliberately postpone acknowledgement
  // until the end of the test.
  OnNavigationStateChangedCallback captured_ack_cb;
  frame.navigation_listener().SetBeforeAckHook(base::BindRepeating(
      [](OnNavigationStateChangedCallback* dest_cb,
         const fuchsia::web::NavigationState& state,
         OnNavigationStateChangedCallback cb) { *dest_cb = std::move(cb); },
      base::Unretained(&captured_ack_cb)));
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       title1.spec()));
  fuchsia::web::NavigationState expected_state;
  expected_state.set_url(title1.spec());
  frame.navigation_listener().RunUntilNavigationStateMatches(expected_state);
  EXPECT_TRUE(captured_ack_cb);
  frame.navigation_listener().SetBeforeAckHook({});

  // Navigate to a second page.
  {
    // Since we have blocked NavigationEventObserver's flow, we must observe the
    // lower level browser navigation events directly from the WebContents.
    MockWebContentsObserver web_contents_observer(
        context_impl()->GetFrameImplForTest(&frame.ptr())->web_contents_.get());

    base::RunLoop run_loop;
    EXPECT_CALL(web_contents_observer,
                DidStartNavigation(NavigationHandleUrlEquals(title2)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         title2.spec()));
    run_loop.Run();
  }

  // Navigate to the first page.
  {
    MockWebContentsObserver web_contents_observer(
        context_impl()->GetFrameImplForTest(&frame.ptr())->web_contents_.get());
    base::RunLoop run_loop;
    EXPECT_CALL(web_contents_observer,
                DidStartNavigation(NavigationHandleUrlEquals(title1)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                         fuchsia::web::LoadUrlParams(),
                                         title1.spec()));
    run_loop.Run();
  }

  // Since there was no observable change in navigation state since the last
  // ack, there should be no more NavigationEvents generated.
  captured_ack_cb();
  frame.navigation_listener().RunUntilUrlAndTitleEquals(title1, kPage1Title);
}

namespace {

// Observes events specific to the Stop() test case.
struct WebContentsObserverForStop : public content::WebContentsObserver {
  using content::WebContentsObserver::Observe;
  MOCK_METHOD1(DidStartNavigation, void(content::NavigationHandle*));
  MOCK_METHOD0(NavigationStopped, void());
};

}  // namespace

IN_PROC_BROWSER_TEST_F(FrameImplTest, Stop) {
  auto frame = FrameForTest::Create(context(), {});
  base::RunLoop().RunUntilIdle();

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  // Use a request handler that will accept the connection and stall
  // indefinitely.
  GURL hung_url(embedded_test_server()->GetURL("/hung"));

  {
    base::RunLoop run_loop;
    WebContentsObserverForStop observer;
    observer.Observe(
        context_impl()->GetFrameImplForTest(&frame.ptr())->web_contents_.get());
    EXPECT_CALL(observer, DidStartNavigation(_))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    frame.GetNavigationController()->LoadUrl(
        hung_url.spec(), fuchsia::web::LoadUrlParams(),
        [](fuchsia::web::NavigationController_LoadUrl_Result) {});
    run_loop.Run();
  }

  EXPECT_TRUE(context_impl()
                  ->GetFrameImplForTest(&frame.ptr())
                  ->web_contents_->IsLoading());

  {
    base::RunLoop run_loop;
    WebContentsObserverForStop observer;
    observer.Observe(
        context_impl()->GetFrameImplForTest(&frame.ptr())->web_contents_.get());
    EXPECT_CALL(observer, NavigationStopped())
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    frame.GetNavigationController()->Stop();
    run_loop.Run();
  }

  EXPECT_FALSE(context_impl()
                   ->GetFrameImplForTest(&frame.ptr())
                   ->web_contents_->IsLoading());
}

// TODO(crbug.com/42050537): Enable on ARM64 when bots support Vulkan.
// This test requires Vulkan and Scenic to properly signal the Views visibility.
#if defined(ARCH_CPU_ARM_FAMILY)
#define MAYBE_SetPageScale DISABLED_SetPageScale
#else
// TODO(crbug.com/42050328): SetPageScale/ExecuteJavaScript is racey, causing
// the test to flake.
#define MAYBE_SetPageScale DISABLED_SetPageScale
#endif
IN_PROC_BROWSER_TEST_F(FrameImplTest, MAYBE_SetPageScale) {
  ASSERT_EQ(ui::OzonePlatform::GetInstance()->GetPlatformNameForTest(),
            "flatland");

  auto frame = FrameForTest::Create(context(), {});

  fuchsia::ui::views::ViewCreationToken view_token;
  fuchsia::ui::views::ViewportCreationToken viewport_token;
  auto status =
      zx::channel::create(0, &viewport_token.value, &view_token.value);
  ZX_CHECK(status == ZX_OK, status);
  fuchsia::web::CreateView2Args create_view_args;
  create_view_args.set_view_creation_token(std::move(view_token));
  frame->CreateView2(std::move(create_view_args));

  // Attach the View to a Presenter, the page should be visible.
  auto presenter = base::ComponentContextForProcess()
                       ->svc()
                       ->Connect<::fuchsia::element::GraphicalPresenter>();
  presenter.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "GraphicalPresenter disconnected.";
    ADD_FAILURE();
  });

  ::fuchsia::element::ViewSpec view_spec;
  view_spec.set_viewport_creation_token(std::move(viewport_token));
  view_spec.set_annotations({});
  ::fuchsia::element::ViewControllerPtr view_controller;
  presenter->PresentView(std::move(view_spec), nullptr,
                         view_controller.NewRequest(),
                         [](auto result) { EXPECT_FALSE(result.is_err()); });

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url = embedded_test_server()->GetURL(kWaitSizePath);

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "done");

  std::optional<base::Value> default_dpr =
      ExecuteJavaScript(frame.ptr().get(), "window.devicePixelRatio");
  ASSERT_TRUE(default_dpr);

  EXPECT_EQ(default_dpr->GetDouble(), 1.0f);

  // Update scale and verify that devicePixelRatio is updated accordingly.
  const float kZoomInScale = 1.5;
  fuchsia::web::ContentAreaSettings settings;
  settings.set_page_scale(kZoomInScale);
  frame->SetContentAreaSettings(std::move(settings));

  std::optional<base::Value> scaled_dpr =
      ExecuteJavaScript(frame.ptr().get(), "window.devicePixelRatio");
  ASSERT_TRUE(scaled_dpr);

  EXPECT_EQ(scaled_dpr->GetDouble(), kZoomInScale);

  // Navigate to the same page on http://localhost. This is a different site,
  // so it will be loaded in a new renderer process. Page scale value should be
  // preserved.
  GURL url2 = embedded_test_server()->GetURL("localhost", kWaitSizePath);
  EXPECT_NE(url.host(), url2.host());
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url2.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url2, "done");

  std::optional<base::Value> dpr_after_navigation =
      ExecuteJavaScript(frame.ptr().get(), "window.devicePixelRatio");
  ASSERT_TRUE(scaled_dpr);

  EXPECT_EQ(dpr_after_navigation, scaled_dpr);

  // Reset the scale to 1.0 (default) and verify that reported DPR is updated
  // to 1.0.
  const float kDefaultScale = 1.0;
  fuchsia::web::ContentAreaSettings settings2;
  settings2.set_page_scale(kDefaultScale);
  frame->SetContentAreaSettings(std::move(settings2));

  std::optional<base::Value> dpr_after_reset =
      ExecuteJavaScript(frame.ptr().get(), "window.devicePixelRatio");
  ASSERT_TRUE(dpr_after_reset);

  EXPECT_EQ(dpr_after_reset->GetDouble(), kDefaultScale);

  // Zoom out by setting scale to 0.5.
  const float kZoomOutScale = 0.5;
  fuchsia::web::ContentAreaSettings settings3;
  settings3.set_page_scale(kZoomOutScale);
  frame->SetContentAreaSettings(std::move(settings3));

  std::optional<base::Value> zoomed_out_dpr =
      ExecuteJavaScript(frame.ptr().get(), "window.devicePixelRatio");
  ASSERT_TRUE(zoomed_out_dpr);

  EXPECT_EQ(zoomed_out_dpr->GetDouble(), kZoomOutScale);

  // Create another frame. Verify that the scale factor is not applied to the
  // new frame.
  auto frame2 = FrameForTest::Create(context(), {});
  status = zx::channel::create(0, &viewport_token.value, &view_token.value);
  ZX_CHECK(status == ZX_OK, status);
  create_view_args.set_view_creation_token(std::move(view_token));
  frame2->CreateView2(std::move(create_view_args));

  view_spec.set_viewport_creation_token(std::move(viewport_token));
  view_spec.set_annotations({});
  presenter->PresentView(std::move(view_spec), nullptr,
                         view_controller.NewRequest(), [](auto) {});

  EXPECT_TRUE(LoadUrlAndExpectResponse(frame2.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       url.spec()));
  frame2.navigation_listener().RunUntilUrlAndTitleEquals(url, "done");

  std::optional<base::Value> frame2_dpr =
      ExecuteJavaScript(frame2.ptr().get(), "window.devicePixelRatio");
  ASSERT_TRUE(frame2_dpr);

  EXPECT_EQ(frame2_dpr->GetDouble(), 1.0);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, RecreateView) {
  auto frame = FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  // Process the Frame creation request, and verify we can get the FrameImpl.
  base::RunLoop().RunUntilIdle();
  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame.ptr());
  ASSERT_TRUE(frame_impl);
  EXPECT_FALSE(frame_impl->has_view_for_test());

  // Verify that the Frame can navigate, prior to the View being created.
  const GURL page1_url(embedded_test_server()->GetURL(kPage1Path));
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page1_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(page1_url, kPage1Title);

  // Request a View from the Frame, and pump the loop to process the request.
  fuchsia::ui::views::ViewCreationToken view_token;
  fuchsia::ui::views::ViewportCreationToken viewport_token;
  auto status =
      zx::channel::create(0, &viewport_token.value, &view_token.value);
  ZX_CHECK(status == ZX_OK, status);
  fuchsia::web::CreateView2Args create_view_args;
  create_view_args.set_view_creation_token(std::move(view_token));
  frame->CreateView2(std::move(create_view_args));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(frame_impl->has_view_for_test());

  // Verify that the Frame still works, by navigating to Page #2.
  const GURL page2_url(embedded_test_server()->GetURL(kPage2Path));
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page2_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(page2_url, kPage2Title);

  // Create new View tokens and request a new view.
  status = zx::channel::create(0, &viewport_token.value, &view_token.value);
  ZX_CHECK(status == ZX_OK, status);
  create_view_args.set_view_creation_token(std::move(view_token));
  frame->CreateView2(std::move(create_view_args));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(frame_impl->has_view_for_test());

  // Verify that the Frame still works, by navigating back to Page #1.
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page1_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(page1_url, kPage1Title);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, CreateViewMissingArgs) {
  auto frame = FrameForTest::Create(context(), {});

  // Close the NavigationEventListener to avoid a test failure resulting, when
  // it is disconnected as a result of the Frame closing.
  frame.navigation_listener_binding().Close(ZX_OK);

  // Create a view with GFX, without supplying a valid view token.
  base::test::TestFuture<zx_status_t> frame_status;
  frame.ptr().set_error_handler(
      CallbackToFitFunction(frame_status.GetCallback()));

  frame->CreateView({});

  EXPECT_EQ(frame_status.Get(), ZX_ERR_INVALID_ARGS);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, CreateView2MissingArgs) {
  auto frame = FrameForTest::Create(context(), {});

  // Close the NavigationEventListener to avoid a test failure resulting, when
  // it is disconnected as a result of the Frame closing.
  frame.navigation_listener_binding().Close(ZX_OK);

  // Create a view with GFX, without supplying a valid view token.
  base::test::TestFuture<zx_status_t> frame_status;
  frame.ptr().set_error_handler(
      CallbackToFitFunction(frame_status.GetCallback()));

  frame->CreateView2({});

  EXPECT_EQ(frame_status.Get(), ZX_ERR_INVALID_ARGS);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ChildFrameNavigationIgnored) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL page_url(embedded_test_server()->GetURL("/creates_child_frame.html"));

  // Navigate to a page and wait for the navigation to complete.
  auto frame = FrameForTest::Create(context(), {});
  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  fuchsia::web::NavigationState expected_state;
  expected_state.set_url(page_url.spec());
  expected_state.set_title("main frame");
  expected_state.set_is_main_document_loaded(true);
  frame.navigation_listener().RunUntilNavigationStateMatches(
      std::move(expected_state));

  // Notify the page so that it constructs a child iframe.
  fuchsia::web::WebMessage message;
  message.set_data(base::MemBufferFromString("test", "test"));
  base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
  frame->PostMessage(embedded_test_server()->GetOrigin().Serialize(),
                     std::move(message),
                     CallbackToFitFunction(post_result.GetCallback()));

  frame.navigation_listener().SetBeforeAckHook(
      base::BindRepeating([](const fuchsia::web::NavigationState& change,
                             OnNavigationStateChangedCallback callback) {
        // The child iframe's loading status should not affect the
        // is_main_document_loaded() bit.
        if (change.has_is_main_document_loaded()) {
          ADD_FAILURE();
        }

        callback();
      }));

  frame.navigation_listener().RunUntilUrlAndTitleEquals(page_url,
                                                        "iframe loaded");
}

// Tests SetNavigationEventListener() immediately returns a NavigationEvent,
// even in the absence of a new navigation.
IN_PROC_BROWSER_TEST_F(FrameImplTest, ImmediateNavigationEvent) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL page_url(embedded_test_server()->GetURL(kPage1Path));

  // The first NavigationState received should be empty.
  base::RunLoop run_loop;
  auto frame = FrameForTest::Create(context(), {});
  frame.navigation_listener().SetBeforeAckHook(base::BindRepeating(
      [](base::OnceClosure quit_loop,
         const fuchsia::web::NavigationState& change,
         OnNavigationStateChangedCallback callback) {
        std::move(quit_loop).Run();
        EXPECT_TRUE(change.IsEmpty());
        callback();
      },
      run_loop.QuitClosure()));
  run_loop.Run();
  frame.navigation_listener().SetBeforeAckHook({});

  // Navigate to a page and wait for the navigation to complete.
  EXPECT_TRUE(LoadUrlAndExpectResponse(frame.GetNavigationController(),
                                       fuchsia::web::LoadUrlParams(),
                                       page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(page_url);

  // Attach a new navigation listener, we should get the new page state, even if
  // no new navigation occurred.
  frame.CreateAndAttachNavigationListener(/*flags=*/{});
  frame.navigation_listener().RunUntilUrlAndTitleEquals(page_url, kPage1Title);
}

// Check loading an invalid URL in NavigationController.LoadUrl() sets the right
// error.
IN_PROC_BROWSER_TEST_F(FrameImplTest, InvalidUrl) {
  auto frame = FrameForTest::Create(context(), {});

  base::test::TestFuture<fuchsia::web::NavigationController_LoadUrl_Result>
      result;
  auto controller = frame.GetNavigationController();
  controller->LoadUrl("http:google.com:foo", fuchsia::web::LoadUrlParams(),
                      CallbackToFitFunction(result.GetCallback()));
  ASSERT_TRUE(result.Wait());

  ASSERT_TRUE(result.Get().is_err());
  EXPECT_EQ(result.Get().err(),
            fuchsia::web::NavigationControllerError::INVALID_URL);
}

// Check setting invalid headers in NavigationController.LoadUrl() sets the
// right error.
IN_PROC_BROWSER_TEST_F(FrameImplTest, InvalidHeader) {
  auto frame = FrameForTest::Create(context(), {});

  {
    // Set an invalid header name.
    fuchsia::web::LoadUrlParams load_url_params;
    fuchsia::net::http::Header header;
    header.name = StringToBytes("Invalid:Header");
    header.value = StringToBytes("1");
    load_url_params.set_headers({header});

    base::test::TestFuture<fuchsia::web::NavigationController_LoadUrl_Result>
        result;
    auto controller = frame.GetNavigationController();
    controller->LoadUrl("http://site.ext/", std::move(load_url_params),
                        CallbackToFitFunction(result.GetCallback()));
    ASSERT_TRUE(result.Wait());

    ASSERT_TRUE(result.Get().is_err());
    EXPECT_EQ(result.Get().err(),
              fuchsia::web::NavigationControllerError::INVALID_HEADER);
  }

  {
    // Set an invalid header value.
    fuchsia::web::LoadUrlParams load_url_params;
    fuchsia::net::http::Header header;
    header.name = StringToBytes("Header");
    header.value = StringToBytes("Invalid\rValue");
    load_url_params.set_headers({header});

    base::test::TestFuture<fuchsia::web::NavigationController_LoadUrl_Result>
        result;
    auto controller = frame.GetNavigationController();
    controller->LoadUrl("http://site.ext/", std::move(load_url_params),
                        CallbackToFitFunction(result.GetCallback()));
    ASSERT_TRUE(result.Wait());

    ASSERT_TRUE(result.Get().is_err());
    EXPECT_EQ(result.Get().err(),
              fuchsia::web::NavigationControllerError::INVALID_HEADER);
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ContentAreaSettings) {
  auto frame = FrameForTest::Create(context(), {});
  base::RunLoop().RunUntilIdle();
  auto* frame_impl = context_impl()->GetFrameImplForTest(&frame.ptr());
  auto* web_contents = frame_impl->web_contents_for_test();

  // Frame should start with default values in web_contents.
  {
    blink::web_pref::WebPreferences web_prefs =
        web_contents->GetOrCreateWebPreferences();
    EXPECT_FALSE(web_prefs.hide_scrollbars);
    EXPECT_EQ(web_prefs.autoplay_policy,
              blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired);
    EXPECT_EQ(web_prefs.preferred_color_scheme,
              blink::mojom::PreferredColorScheme::kLight);
  }

  // SetContentAreaSettings with empty settings object should not change any
  // existing settings.
  {
    fuchsia::web::ContentAreaSettings settings;
    frame->SetContentAreaSettings(std::move(settings));
    base::RunLoop().RunUntilIdle();

    blink::web_pref::WebPreferences web_prefs =
        web_contents->GetOrCreateWebPreferences();
    EXPECT_FALSE(web_prefs.hide_scrollbars);
    EXPECT_EQ(web_prefs.autoplay_policy,
              blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired);
    EXPECT_EQ(web_prefs.preferred_color_scheme,
              blink::mojom::PreferredColorScheme::kLight);
  }

  // Set hide_scrollbars field and expect that it's reflected in web_contents.
  // Expect other fields to be unchanged.
  {
    fuchsia::web::ContentAreaSettings settings;
    settings.set_hide_scrollbars(true);
    frame->SetContentAreaSettings(std::move(settings));
    base::RunLoop().RunUntilIdle();

    blink::web_pref::WebPreferences web_prefs =
        web_contents->GetOrCreateWebPreferences();
    EXPECT_TRUE(web_prefs.hide_scrollbars);
    EXPECT_EQ(web_prefs.autoplay_policy,
              blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired);
    EXPECT_EQ(web_prefs.preferred_color_scheme,
              blink::mojom::PreferredColorScheme::kLight);
  }

  // ResetContentAreaSettings should revert preferences to their default values
  // in web_contents.
  {
    frame->ResetContentAreaSettings();
    base::RunLoop().RunUntilIdle();

    blink::web_pref::WebPreferences web_prefs =
        web_contents->GetOrCreateWebPreferences();
    EXPECT_FALSE(web_prefs.hide_scrollbars);
    EXPECT_EQ(web_prefs.autoplay_policy,
              blink::mojom::AutoplayPolicy::kDocumentUserActivationRequired);
    EXPECT_EQ(web_prefs.preferred_color_scheme,
              blink::mojom::PreferredColorScheme::kLight);
  }
}

// Verifies that a `Frame` closes correctly if `Close()` is called before any
// page is loaded.
IN_PROC_BROWSER_TEST_F(FrameImplTest, Close_BeforeNavigation) {
  auto frame = FrameForTest::Create(context(), {});

  base::RunLoop loop;
  frame.ptr().set_error_handler(
      [quit = loop.QuitClosure()](zx_status_t status) {
        EXPECT_EQ(status, ZX_OK);
        std::move(quit).Run();
      });

  // Request to gracefully close the Frame, which should be immediate since
  // nothing has been loaded into it yet.
  frame->Close({});

  loop.Run();
}

namespace {

// Helper class for `Frame.Close()` tests, that navigates the `Frame` to an
// event-recording page, and connects to accumulate the list of events it
// receives.
// If `hang_on_event` is true then the web page will be configured to busy-loop
// as soon as any event is received, to allow timeouts to be simulated.
class FrameForTestWithMessageLog : public FrameForTest {
 public:
  FrameForTestWithMessageLog(FrameForTest frame,
                             net::EmbeddedTestServer& test_server,
                             bool hang_on_event = false)
      : FrameForTest(std::move(frame)) {
    // Navigate to a page that will record the relevant events.
    const GURL page_url(test_server.GetURL(kReportCloseEventsPath));
    if (!LoadUrlAndExpectResponse(GetNavigationController(),
                                  fuchsia::web::LoadUrlParams(),
                                  page_url.spec())) {
      ADD_FAILURE();
      loop_.Quit();
      return;
    }
    navigation_listener().RunUntilUrlEquals(page_url);

    // Connect a message port over which to receive notifications of events.
    fuchsia::web::WebMessage message;
    message.set_data(base::MemBufferFromString(
        hang_on_event ? "hang_on_event" : "init", "test"));
    message.mutable_outgoing_transfer()->push_back(
        std::move(fuchsia::web::OutgoingTransferable().set_message_port(
            message_port_.NewRequest())));

    base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
    get()->PostMessage(test_server.GetOrigin().Serialize(), std::move(message),
                       CallbackToFitFunction(post_result.GetCallback()));
    if (!post_result.Wait()) {
      ADD_FAILURE();
      loop_.Quit();
      return;
    }

    // If the FrameForTest becomes disconnected then store the epitaph, for
    // tests to verify.
    ptr().set_error_handler(CallbackToFitFunction(epitaph_.GetCallback()));

    // Don't error-out if the Frame disconnects.
    navigation_listener_binding().set_error_handler([](zx_status_t) {});

    // Start reading messages from the port into a queue, until the port closes.
    message_port_.set_error_handler(
        [quit = loop_.QuitClosure()](zx_status_t) { std::move(quit).Run(); });
    message_port_->ReceiveMessage(
        fit::bind_member(this, &FrameForTestWithMessageLog::OnMessage));
  }

  void RunUntilMessagePortClosed() { loop_.Run(); }

  base::test::TestFuture<zx_status_t>& epitaph() { return epitaph_; }

  const std::vector<std::string>& events() const { return events_; }

  std::string EventsString() const {
    return "[" + base::JoinString(events_, ", ") + "]";
  }

 private:
  void OnMessage(fuchsia::web::WebMessage message) {
    events_.push_back(std::move(*base::StringFromMemBuffer(message.data())));
    message_port_->ReceiveMessage(
        fit::bind_member(this, &FrameForTestWithMessageLog::OnMessage));
  }

  base::RunLoop loop_;
  fuchsia::web::MessagePortPtr message_port_;
  std::vector<std::string> events_;
  base::test::TestFuture<zx_status_t> epitaph_;
};

constexpr char kBeforeUnloadEventName[] = "window.beforeunload";
constexpr char kUnloadEventName[] = "window.unload";
constexpr char kPageHideEventName[] = "window.pagehide";

}  // namespace

// Verifies that `Close()`ing a `Frame` without an explicit timeout allows
// graceful teardown, including firing the expected set of events
// ("beforeunload", "pagehide" and "onunload").
IN_PROC_BROWSER_TEST_F(FrameImplTest, Close_EventsWithDefaultTimeout) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  FrameForTestWithMessageLog frame(FrameForTest::Create(context(), {}),
                                   *embedded_test_server());

  // Request to gracefully close the Frame, which should trigger the
  // relevant event handlers, and then quickly tear-down.
  frame->Close({});

  frame.RunUntilMessagePortClosed();

  // Verify that the expected events were delivered!
  ASSERT_EQ(frame.events().size(), 3u) << frame.EventsString();
  EXPECT_EQ(frame.events()[0], kBeforeUnloadEventName);
  EXPECT_EQ(frame.events()[1], kPageHideEventName);
  EXPECT_EQ(frame.events()[2], kUnloadEventName);

  EXPECT_EQ(frame.epitaph().Get(), ZX_OK);
}

// Verifies that `Close()`ing a `Frame` with an explicit timeout allows
// graceful teardown, dispatching the expected events.
IN_PROC_BROWSER_TEST_F(FrameImplTest, Close_EventsWithNonZeroTimeout) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  FrameForTestWithMessageLog frame(FrameForTest::Create(context(), {}),
                                   *embedded_test_server());

  // Request to gracefully close the Frame, which should trigger the
  // relevant event handlers, and then quickly tear-down.
  // Use a timeout long enough that the test would have timed-out if
  // teardown had taken so long.
  frame->Close(std::move(fuchsia::web::FrameCloseRequest().set_timeout(
      TestTimeouts::action_max_timeout().ToZxDuration())));

  frame.RunUntilMessagePortClosed();

  // Verify that the expected events were delivered!
  ASSERT_EQ(frame.events().size(), 3u) << frame.EventsString();
  EXPECT_EQ(frame.events()[0], kBeforeUnloadEventName);
  EXPECT_EQ(frame.events()[1], kPageHideEventName);
  EXPECT_EQ(frame.events()[2], kUnloadEventName);

  EXPECT_EQ(frame.epitaph().Get(), ZX_OK);
}

// Verifies that if a `Frame` is `Close()`d with a timeout and takes too long
// to close then `ZX_ERR_TIMED_OUT` is reported.
IN_PROC_BROWSER_TEST_F(FrameImplTest, Close_WithInsufficientTimeout) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  FrameForTestWithMessageLog frame(FrameForTest::Create(context(), {}),
                                   *embedded_test_server(),
                                   /* hang_on_event= */ true);

  // Request to gracefully close the Frame, by specifying a non-zero timeout.
  // This can be arbitrarily short, since we are deliberately provoking timeout.
  frame->Close(std::move(fuchsia::web::FrameCloseRequest().set_timeout(
      base::Microseconds(1u).ToZxDuration())));

  // Don't wait for the MessagePort to close, since that doesn't happen in
  // ASAN builds, for some reason (crbug.com/1400304).

  // Regardless of how many events may have been delivered, teardown should
  // have timed-out.
  EXPECT_EQ(frame.epitaph().Get(), ZX_ERR_TIMED_OUT);
}

// Verifies that `Close()`ing a `Frame` with a zero timeout takes immediate
// effect.
IN_PROC_BROWSER_TEST_F(FrameImplTest, Close_NoEventsWithZeroTimeout) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  FrameForTestWithMessageLog frame(FrameForTest::Create(context(), {}),
                                   *embedded_test_server());

  // Request to close the frame immediately.
  frame->Close(std::move(fuchsia::web::FrameCloseRequest().set_timeout(0u)));

  frame.RunUntilMessagePortClosed();

  // In practice it is possible for content to have time to receive & process
  // visibility and unload events, so just check for "beforeunload".
  EXPECT_THAT(frame.events(), Not(Contains(kBeforeUnloadEventName)))
      << frame.EventsString();

  EXPECT_EQ(frame.epitaph().Get(), ZX_OK);
}

// Verifies that disconnecting a `Frame` takes immediate effect.
IN_PROC_BROWSER_TEST_F(FrameImplTest, Disconnect_NoEvents) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  FrameForTestWithMessageLog frame(FrameForTest::Create(context(), {}),
                                   *embedded_test_server());

  // Request to close the frame immediately.
  frame.ptr() = nullptr;

  frame.RunUntilMessagePortClosed();

  // In practice it is possible for content to have time to receive & process
  // visibility and unload events, so just check for "beforeunload".
  EXPECT_THAT(frame.events(), Not(Contains(kBeforeUnloadEventName)))
      << frame.EventsString();
}
