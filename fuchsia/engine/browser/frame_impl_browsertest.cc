// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/ui/policy/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>

#include <string>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/fuchsia/mem_buffer_util.h"
#include "base/fuchsia/process_context.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_utils.h"
#include "fuchsia/base/string_util.h"
#include "fuchsia/base/test/fit_adapter.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/base/test/test_navigation_listener.h"
#include "fuchsia/base/test/url_request_rewrite_test_util.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/browser/fake_semantics_manager.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/browser/frame_impl_browser_test_base.h"
#include "fuchsia/engine/switches.h"
#include "fuchsia/engine/test/frame_for_test.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/url_request/url_request_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

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

const char kPage1Path[] = "/title1.html";
const char kPage2Path[] = "/title2.html";
const char kPage3Path[] = "/websql.html";
const char kPage4Path[] = "/image.html";
const char kPage4ImgPath[] = "/img.png";
const char kDynamicTitlePath[] = "/dynamic_title.html";
const char kPopupParentPath[] = "/popup_parent.html";
const char kPopupRedirectPath[] = "/popup_child.html";
const char kPopupMultiplePath[] = "/popup_multiple.html";
const char kSetHeaderRequestPath[] = "/set_header_request.html";
const char kVisibilityPath[] = "/visibility.html";
const char kWaitSizePath[] = "/wait-size.html";
const char kPage1Title[] = "title 1";
const char kPage2Title[] = "title 2";
const char kPage3Title[] = "websql not available";
const char kDataUrl[] =
    "data:text/html;base64,PGI+SGVsbG8sIHdvcmxkLi4uPC9iPg==";
const int64_t kOnLoadScriptId = 0;
const char kChildQueryParamName[] = "child_url";
const char kPopupChildFile[] = "popup_child.html";
const char kAutoplayFileAndQuery[] = "play_video.html?autoplay=1&codecs=vp8";
const char kAutoPlayBlockedTitle[] = "blocked";
const char kAutoPlaySuccessTitle[] = "playing";

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

std::string StringFromMemBufferOrDie(const fuchsia::mem::Buffer& buffer) {
  absl::optional<std::string> output = base::StringFromMemBuffer(buffer);
  CHECK(output.has_value());
  return std::move(*output);
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

std::string GetDocumentVisibilityState(fuchsia::web::Frame* frame) {
  auto visibility = base::MakeRefCounted<base::RefCountedData<std::string>>();
  base::RunLoop loop;
  frame->ExecuteJavaScript(
      {"*"}, base::MemBufferFromString("document.visibilityState;", "test"),
      [visibility, quit_loop = loop.QuitClosure()](
          fuchsia::web::Frame_ExecuteJavaScript_Result result) {
        ASSERT_TRUE(result.is_response());
        visibility->data = StringFromMemBufferOrDie(result.response().result);
        quit_loop.Run();
      });
  loop.Run();
  return visibility->data;
}

// Verifies that Frames are initially "hidden", changes to "visible" once the
// View is attached to a Presenter and back to "hidden" when the View is
// detached from the Presenter.
// TODO(crbug.com/1058247): Re-enable this test on Arm64 when femu is available
// for that architecture. This test requires Vulkan and Scenic to properly
// signal the Views visibility.
#if defined(ARCH_CPU_ARM_FAMILY)
#define MAYBE_VisibilityState DISABLED_VisibilityState
#else
#define MAYBE_VisibilityState VisibilityState
#endif
IN_PROC_BROWSER_TEST_F(FrameImplTest, MAYBE_VisibilityState) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL page_url(embedded_test_server()->GetURL(kVisibilityPath));

  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});
  base::RunLoop().RunUntilIdle();
  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame.ptr());

  // CreateView() will cause the AccessibilityBridge to be created.
  FakeSemanticsManager fake_semantics_manager;
  frame_impl->set_semantics_manager_for_test(&fake_semantics_manager);

  // Navigate to a page and wait for it to finish loading.
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(page_url);

  // Query the document.visibilityState before creating a View.
  EXPECT_EQ(GetDocumentVisibilityState(frame.ptr().get()), "\"hidden\"");

  // Query the document.visibilityState after creating the View, but without it
  // actually "attached" to the view tree.
  auto view_tokens = scenic::ViewTokenPair::New();
  frame->CreateView(std::move(view_tokens.view_token));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(GetDocumentVisibilityState(frame.ptr().get()), "\"hidden\"");

  // Attach the View to a Presenter, the page should be visible.
  auto presenter = base::ComponentContextForProcess()
                       ->svc()
                       ->Connect<::fuchsia::ui::policy::Presenter>();
  presenter.set_error_handler(
      [](zx_status_t) { ADD_FAILURE() << "Presenter disconnected."; });
  presenter->PresentOrReplaceView(std::move(view_tokens.view_holder_token),
                                  nullptr);
  frame.navigation_listener().RunUntilTitleEquals("visible");

  // Attach a new View to the Presenter, the page should be hidden again.
  // This part of the test is a regression test for crbug.com/1141093.
  auto view_tokens2 = scenic::ViewTokenPair::New();
  presenter->PresentOrReplaceView(std::move(view_tokens2.view_holder_token),
                                  nullptr);
  frame.navigation_listener().RunUntilTitleEquals("hidden");
}

void VerifyCanGoBackAndForward(
    const fuchsia::web::NavigationControllerPtr& controller,
    bool can_go_back_expected,
    bool can_go_forward_expected) {
  base::test::TestFuture<fuchsia::web::NavigationState> visible_entry;
  controller->GetVisibleEntry(
      cr_fuchsia::CallbackToFitFunction(visible_entry.GetCallback()));
  ASSERT_TRUE(visible_entry.Wait());
  EXPECT_TRUE(visible_entry.Get().has_can_go_back());
  EXPECT_EQ(visible_entry.Get().can_go_back(), can_go_back_expected);
  EXPECT_TRUE(visible_entry.Get().has_can_go_forward());
  EXPECT_EQ(visible_entry.Get().can_go_forward(), can_go_forward_expected);
}

// Verifies that the browser will navigate and generate a navigation listener
// event when LoadUrl() is called.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigateFrame) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url::kAboutBlankURL));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(
      GURL(url::kAboutBlankURL), url::kAboutBlankURL);
}

// Verifies that the renderer process consumes more memory for document
// rendering.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigationIncreasesMemoryUsage) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  // Get the renderer size when no renderer process is active.
  base::test::TestFuture<uint64_t> before_nav_size;
  frame->GetPrivateMemorySize(
      cr_fuchsia::CallbackToFitFunction(before_nav_size.GetCallback()));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, kPage1Title);

  base::test::TestFuture<uint64_t> after_nav_size;
  frame->GetPrivateMemorySize(
      cr_fuchsia::CallbackToFitFunction(after_nav_size.GetCallback()));
  ASSERT_TRUE(after_nav_size.Wait());

  EXPECT_EQ(before_nav_size.Get(), 0u);  // No render process - zero bytes.
  EXPECT_GT(after_nav_size.Get(), 0u);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigateDataFrame) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      kDataUrl));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(GURL(kDataUrl),
                                                        kDataUrl);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, FrameDeletedBeforeContext) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  // Process the frame creation message.
  base::RunLoop().RunUntilIdle();

  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame.ptr());
  MockWebContentsObserver deletion_observer(frame_impl->web_contents());
  base::RunLoop run_loop;
  EXPECT_CALL(deletion_observer, RenderViewDeleted(_))
      .WillOnce(InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url::kAboutBlankURL));

  frame.ptr().Unbind();
  run_loop.Run();

  // Check that |context| remains bound after the frame is closed.
  EXPECT_TRUE(context());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ContextDeletedBeforeFrame) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});
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
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});
  EXPECT_TRUE(frame.ptr());
  base::RunLoop().RunUntilIdle();
  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame.ptr());

  // CreateView() will cause the AccessibilityBridge to be created.
  FakeSemanticsManager fake_semantics_manager;
  frame_impl->set_semantics_manager_for_test(&fake_semantics_manager);

  auto view_tokens = scenic::ViewTokenPair::New();
  frame->CreateView(std::move(view_tokens.view_token));
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

// TODO(https://crbug.com/695592): Remove this test when WebSQL is removed from
// Chrome.
IN_PROC_BROWSER_TEST_F(FrameImplTest, EnsureWebSqlDisabled) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL title3(embedded_test_server()->GetURL(kPage3Path));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      title3.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(title3, kPage3Title);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, GoBackAndForward) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      title1.spec()));
  frame.navigation_listener().RunUntilUrlTitleBackForwardEquals(
      title1, kPage1Title, false, false);

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      title2.spec()));
  frame.navigation_listener().RunUntilUrlTitleBackForwardEquals(
      title2, kPage2Title, true, false);

  VerifyCanGoBackAndForward(frame.GetNavigationController(), true, false);
  frame.GetNavigationController()->GoBack();
  frame.navigation_listener().RunUntilUrlTitleBackForwardEquals(
      title1, kPage1Title, false, true);

  // At the top of the navigation entry list; this should be a no-op.
  VerifyCanGoBackAndForward(frame.GetNavigationController(), false, true);
  frame.GetNavigationController()->GoBack();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();

  VerifyCanGoBackAndForward(frame.GetNavigationController(), false, true);
  frame.GetNavigationController()->GoForward();
  frame.navigation_listener().RunUntilUrlTitleBackForwardEquals(
      title2, kPage2Title, true, false);

  // At the end of the navigation entry list; this should be a no-op.
  VerifyCanGoBackAndForward(frame.GetNavigationController(), true, false);
  frame.GetNavigationController()->GoForward();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();
}

// An HTTP response stream whose response payload can be sent as "chunks"
// with indeterminate-length pauses in between.
class ChunkedHttpTransaction {
 public:
  explicit ChunkedHttpTransaction(
      base::WeakPtr<net::test_server::HttpResponseDelegate> delegate)
      : io_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        send_state_(SendState::IDLE),
        delegate_(delegate) {
    DCHECK(!current_instance_);

    current_instance_ = this;
  }

  static ChunkedHttpTransaction* current() {
    DCHECK(current_instance_);
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
    if (send_state_ == SendState::IDLE)
      return;

    base::RunLoop run_loop;
    send_chunk_complete_callback_ = run_loop.QuitClosure();
    run_loop.Run();
    DCHECK_EQ(send_state_, SendState::IDLE);
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
                           base::ThreadTaskRunnerHandle::Get())));
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
    if (send_chunk_complete_callback_)
      std::move(send_chunk_complete_callback_).Run();
  }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Set by callers to SendChunk() waiting for the previous chunk to complete.
  base::OnceClosure send_chunk_complete_callback_;

  enum SendState { IDLE, BLOCKED };

  SendState send_state_;
  base::WeakPtr<net::test_server::HttpResponseDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChunkedHttpTransaction);
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

    if (on_response_created_)
      std::move(on_response_created_).Run();
  }

 private:
  base::OnceClosure on_response_created_;
};

IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigationEventDuringPendingLoad) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  ChunkedHttpTransactionFactory* factory = new ChunkedHttpTransactionFactory;
  base::RunLoop transaction_created_run_loop;
  factory->SetOnResponseCreatedCallback(
      transaction_created_run_loop.QuitClosure());
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &net::test_server::HandlePrefixedRequest, "/pausable",
      base::BindRepeating(
          [](std::unique_ptr<ChunkedHttpTransactionFactory> out_factory,
             const net::test_server::HttpRequest&)
              -> std::unique_ptr<net::test_server::HttpResponse> {
            return out_factory;
          },
          base::Passed(base::WrapUnique(factory)))));

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL hung_url(embedded_test_server()->GetURL("/pausable"));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
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
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &FrameImplTest::OnServeHttpRequest, base::Unretained(this)));

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  EXPECT_CALL(*this, OnServeHttpRequest(_)).Times(AtLeast(1));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
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

IN_PROC_BROWSER_TEST_F(FrameImplTest, GetVisibleEntry) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  // Verify that a Frame returns an empty NavigationState prior to receiving any
  // LoadUrl() calls.
  {
    base::test::TestFuture<fuchsia::web::NavigationState> result;
    auto controller = frame.GetNavigationController();
    controller->GetVisibleEntry(
        cr_fuchsia::CallbackToFitFunction(result.GetCallback()));
    ASSERT_TRUE(result.Wait());
    EXPECT_FALSE(result.Get().has_title());
    EXPECT_FALSE(result.Get().has_url());
    EXPECT_FALSE(result.Get().has_page_type());
  }

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  // Navigate to a page.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      title1.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(title1, kPage1Title);

  // Verify that GetVisibleEntry() reflects the new Frame navigation state.
  {
    base::test::TestFuture<fuchsia::web::NavigationState> result;
    auto controller = frame.GetNavigationController();
    controller->GetVisibleEntry(
        cr_fuchsia::CallbackToFitFunction(result.GetCallback()));
    ASSERT_TRUE(result.Wait());
    ASSERT_TRUE(result.Get().has_url());
    EXPECT_EQ(result.Get().url(), title1.spec());
    ASSERT_TRUE(result.Get().has_title());
    EXPECT_EQ(result.Get().title(), kPage1Title);
    ASSERT_TRUE(result.Get().has_page_type());
    EXPECT_EQ(result.Get().page_type(), fuchsia::web::PageType::NORMAL);
  }

  // Navigate to another page.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      title2.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(title2, kPage2Title);

  // Verify the navigation with GetVisibleEntry().
  {
    base::test::TestFuture<fuchsia::web::NavigationState> result;
    auto controller = frame.GetNavigationController();
    controller->GetVisibleEntry(
        cr_fuchsia::CallbackToFitFunction(result.GetCallback()));
    ASSERT_TRUE(result.Wait());
    ASSERT_TRUE(result.Get().has_url());
    EXPECT_EQ(result.Get().url(), title2.spec());
    ASSERT_TRUE(result.Get().has_title());
    EXPECT_EQ(result.Get().title(), kPage2Title);
    ASSERT_TRUE(result.Get().has_page_type());
    EXPECT_EQ(result.Get().page_type(), fuchsia::web::PageType::NORMAL);
  }

  // Navigate back to the first page.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      title1.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(title1, kPage1Title);

  // Verify the navigation with GetVisibleEntry().
  {
    base::test::TestFuture<fuchsia::web::NavigationState> result;
    auto controller = frame.GetNavigationController();
    controller->GetVisibleEntry(
        cr_fuchsia::CallbackToFitFunction(result.GetCallback()));
    ASSERT_TRUE(result.Wait());
    ASSERT_TRUE(result.Get().has_url());
    EXPECT_EQ(result.Get().url(), title1.spec());
    ASSERT_TRUE(result.Get().has_title());
    EXPECT_EQ(result.Get().title(), kPage1Title);
    ASSERT_TRUE(result.Get().has_page_type());
    EXPECT_EQ(result.Get().page_type(), fuchsia::web::PageType::NORMAL);
  }
}

// Verifies that NavigationState correctly reports when the Renderer terminates
// or crashes. Also verifies that GetVisibleEntry() reports the same state.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigationState_RendererGone) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  // Navigate to a page.
  ASSERT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, kPage1Title);

  // Kill the renderer for the tab.
  auto* frame_impl = context_impl()->GetFrameImplForTest(&frame.ptr());
  auto* web_contents = frame_impl->web_contents_for_test();
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;

    content::RenderFrameDeletedObserver crash_observer(
        web_contents->GetMainFrame());
    web_contents->GetMainFrame()->GetProcess()->Shutdown(1);
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

  // Verify that GetVisibleEntry() also reflects the expected error state.
  {
    base::test::TestFuture<fuchsia::web::NavigationState> result;
    auto controller = frame.GetNavigationController();
    controller->GetVisibleEntry(
        cr_fuchsia::CallbackToFitFunction(result.GetCallback()));
    ASSERT_TRUE(result.Wait());
    ASSERT_TRUE(result.Get().has_url());
    EXPECT_EQ(result.Get().url(), url.spec());
    ASSERT_TRUE(result.Get().has_title());
    EXPECT_EQ(result.Get().title(), kPage1Title);
    ASSERT_TRUE(result.Get().has_page_type());
    EXPECT_EQ(result.Get().page_type(), fuchsia::web::PageType::ERROR);
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, NoNavigationObserverAttached) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});
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
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        title1.spec()));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, DidStartNavigation(NavigationHandleUrlEquals(title2)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        title2.spec()));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScript) {
  constexpr int64_t kBindingsId = 1234;

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kBindingsId, {url.GetOrigin().spec()},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(url);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScriptUpdated) {
  constexpr int64_t kBindingsId = 1234;

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kBindingsId, {url.GetOrigin().spec()},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Verify that this script clobbers the previous script, as opposed to being
  // injected alongside it. (The latter would result in the title being
  // "helloclobber").
  frame->AddBeforeLoadJavaScript(
      kBindingsId, {url.GetOrigin().spec()},
      base::MemBufferFromString("stashed_title = document.title + 'clobber';",
                                "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "clobber");
}

// Verifies that bindings are injected in order by producing a cumulative,
// non-commutative result.
IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScriptOrdered) {
  constexpr int64_t kBindingsId1 = 1234;
  constexpr int64_t kBindingsId2 = 5678;

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kBindingsId1, {url.GetOrigin().spec()},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });
  frame->AddBeforeLoadJavaScript(
      kBindingsId2, {url.GetOrigin().spec()},
      base::MemBufferFromString("stashed_title += ' there';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "hello there");
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScriptRemoved) {
  constexpr int64_t kBindingsId1 = 1234;
  constexpr int64_t kBindingsId2 = 5678;

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kBindingsId1, {url.GetOrigin().spec()},
      base::MemBufferFromString("stashed_title = 'foo';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Add a script which clobbers "foo".
  frame->AddBeforeLoadJavaScript(
      kBindingsId2, {url.GetOrigin().spec()},
      base::MemBufferFromString("stashed_title = 'bar';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Deletes the clobbering script.
  frame->RemoveBeforeLoadJavaScript(kBindingsId2);

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "foo");
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScriptRemoveInvalidId) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kPage1Path));
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  frame->RemoveBeforeLoadJavaScript(kOnLoadScriptId);

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, kPage1Title);
}

// Test JS injection using ExecuteJavaScriptNoResult() to set a value, and
// ExecuteJavaScript() to retrieve that value.
IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavaScript) {
  constexpr char kJsonStringLiteral[] = "\"I am a literal, literally\"";
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  const GURL kUrl(embedded_test_server()->GetURL(kPage1Path));

  // Navigate to a page and wait for it to finish loading.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      kUrl.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(kUrl, kPage1Title);

  // Execute with no result to set the variable.
  frame->ExecuteJavaScriptNoResult(
      {kUrl.GetOrigin().spec()},
      base::MemBufferFromString(
          base::StringPrintf("my_variable = %s;", kJsonStringLiteral), "test"),
      [](fuchsia::web::Frame_ExecuteJavaScriptNoResult_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Execute a script snippet to return the variable's value.
  base::RunLoop loop;
  frame->ExecuteJavaScript(
      {kUrl.GetOrigin().spec()},
      base::MemBufferFromString("my_variable;", "test"),
      [&](fuchsia::web::Frame_ExecuteJavaScript_Result result) {
        ASSERT_TRUE(result.is_response());
        std::string result_json =
            StringFromMemBufferOrDie(result.response().result);
        EXPECT_EQ(result_json, kJsonStringLiteral);
        loop.Quit();
      });
  loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScriptVmoDestroyed) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId, {url.GetOrigin().spec()},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "hello");
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScriptWrongOrigin) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId, {"http://example.com"},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Expect that the original HTML title is used, because we didn't inject a
  // script with a replacement title.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(
      url, "Welcome to Stan the Offline Dino's Homepage");
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScriptWildcardOrigin) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId, {"*"},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Test script injection for the origin 127.0.0.1.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "hello");

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url::kAboutBlankURL));
  frame.navigation_listener().RunUntilUrlEquals(GURL(url::kAboutBlankURL));

  // Test script injection using a different origin ("localhost"), which should
  // still be picked up by the wildcard.
  GURL alt_url = embedded_test_server()->GetURL("localhost", kDynamicTitlePath);
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      alt_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(alt_url, "hello");
}

// Test that we can inject scripts before and after RenderFrame creation.
IN_PROC_BROWSER_TEST_F(FrameImplTest,
                       BeforeLoadScriptEarlyAndLateRegistrations) {
  constexpr int64_t kOnLoadScriptId2 = kOnLoadScriptId + 1;

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId, {url.GetOrigin().spec()},
      base::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "hello");

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId2, {url.GetOrigin().spec()},
      base::MemBufferFromString("stashed_title += ' there';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Navigate away to clean the slate.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url::kAboutBlankURL));
  frame.navigation_listener().RunUntilUrlEquals(GURL(url::kAboutBlankURL));

  // Navigate back and see if both scripts are working.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "hello there");
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavaScriptBadEncoding) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, kPage1Title);

  base::RunLoop run_loop;

  // 0xFE is an illegal UTF-8 byte; it should cause UTF-8 conversion to fail.
  frame->ExecuteJavaScriptNoResult(
      {url.GetOrigin().spec()}, base::MemBufferFromString("true;\xfe", "test"),
      [&run_loop](fuchsia::web::Frame_ExecuteJavaScriptNoResult_Result result) {
        EXPECT_TRUE(result.is_err());
        EXPECT_EQ(result.err(), fuchsia::web::FrameError::BUFFER_NOT_UTF8);
        run_loop.Quit();
      });
  run_loop.Run();
}

// Verifies that a Frame will handle navigation listener disconnection events
// gracefully.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigationObserverDisconnected) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});
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

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
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
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      title2.spec()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, DelayedNavigationEventAck) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});
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
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
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
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
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
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        title1.spec()));
    run_loop.Run();
  }

  // Since there was no observable change in navigation state since the last
  // ack, there should be no more NavigationEvents generated.
  captured_ack_cb();
  frame.navigation_listener().RunUntilUrlAndTitleEquals(title1, kPage1Title);
}

// Observes events specific to the Stop() test case.
struct WebContentsObserverForStop : public content::WebContentsObserver {
  using content::WebContentsObserver::Observe;
  MOCK_METHOD1(DidStartNavigation, void(content::NavigationHandle*));
  MOCK_METHOD0(NavigationStopped, void());
};

IN_PROC_BROWSER_TEST_F(FrameImplTest, Stop) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});
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

IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessage) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL post_message_url(
      embedded_test_server()->GetURL("/window_post_message.html"));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      post_message_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(post_message_url,
                                                        "postmessage");

  fuchsia::web::WebMessage message;
  message.set_data(base::MemBufferFromString(kPage1Path, "test"));
  base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
  frame->PostMessage(
      post_message_url.GetOrigin().spec(), std::move(message),
      cr_fuchsia::CallbackToFitFunction(post_result.GetCallback()));
  ASSERT_TRUE(post_result.Wait());

  frame.navigation_listener().RunUntilUrlAndTitleEquals(
      embedded_test_server()->GetURL(kPage1Path), kPage1Title);

  EXPECT_TRUE(post_result.Get().is_response());
}

// Send a MessagePort to the content, then perform bidirectional messaging
// through the port.
IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessagePassMessagePort) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      post_message_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(post_message_url,
                                                        "messageport");

  fuchsia::web::MessagePortPtr message_port;
  {
    base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
    frame->PostMessage(
        post_message_url.GetOrigin().spec(),
        cr_fuchsia::CreateWebMessageWithMessagePortRequest(
            message_port.NewRequest(), base::MemBufferFromString("hi", "test")),
        cr_fuchsia::CallbackToFitFunction(post_result.GetCallback()));

    base::test::TestFuture<fuchsia::web::WebMessage> receiver;
    message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetCallback()));
    ASSERT_TRUE(receiver.Wait());
    ASSERT_TRUE(receiver.Get().has_data());
    EXPECT_EQ("got_port", StringFromMemBufferOrDie(receiver.Get().data()));
  }

  {
    fuchsia::web::WebMessage msg;
    msg.set_data(base::MemBufferFromString("ping", "test"));
    base::test::TestFuture<fuchsia::web::MessagePort_PostMessage_Result>
        post_result;
    message_port->PostMessage(std::move(msg), cr_fuchsia::CallbackToFitFunction(
                                                  post_result.GetCallback()));
    base::test::TestFuture<fuchsia::web::WebMessage> receiver;
    message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetCallback()));
    ASSERT_TRUE(post_result.Wait());
    ASSERT_TRUE(receiver.Wait());
    ASSERT_TRUE(receiver.Get().has_data());
    EXPECT_EQ("ack ping", StringFromMemBufferOrDie(receiver.Get().data()));
    EXPECT_TRUE(post_result.Get().is_response());
  }
}

// TODO(crbug.com/1058247): Re-enable this test on Arm64 when femu is available
// for that architecture. This test requires Vulkan and Scenic to properly
// signal the Views visibility.
#if defined(ARCH_CPU_ARM_FAMILY)
#define MAYBE_SetPageScale DISABLED_SetPageScale
#else
#define MAYBE_SetPageScale SetPageScale
#endif
// TODO(crbug.com/1239135): SetPageScale/ExecuteJavaScript is racey, causing
// the test to flake.
IN_PROC_BROWSER_TEST_F(FrameImplTest, DISABLED_SetPageScale) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  auto view_tokens = scenic::ViewTokenPair::New();
  frame->CreateView(std::move(view_tokens.view_token));

  // Attach the View to a Presenter, the page should be visible.
  auto presenter = base::ComponentContextForProcess()
                       ->svc()
                       ->Connect<::fuchsia::ui::policy::Presenter>();
  presenter.set_error_handler(
      [](zx_status_t) { ADD_FAILURE() << "Presenter disconnected."; });
  presenter->PresentOrReplaceView(std::move(view_tokens.view_holder_token),
                                  nullptr);

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL url = embedded_test_server()->GetURL(kWaitSizePath);

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url, "done");

  absl::optional<base::Value> default_dpr = cr_fuchsia::ExecuteJavaScript(
      frame.ptr().get(), "window.devicePixelRatio");
  ASSERT_TRUE(default_dpr);

  EXPECT_EQ(default_dpr->GetDouble(), 1.0f);

  // Update scale and verify that devicePixelRatio is updated accordingly.
  const float kZoomInScale = 1.5;
  frame->SetPageScale(kZoomInScale);

  absl::optional<base::Value> scaled_dpr = cr_fuchsia::ExecuteJavaScript(
      frame.ptr().get(), "window.devicePixelRatio");
  ASSERT_TRUE(scaled_dpr);

  EXPECT_EQ(scaled_dpr->GetDouble(), kZoomInScale);

  // Navigate to the same page on http://localhost. This is a different site,
  // so it will be loaded in a new renderer process. Page scale value should be
  // preserved.
  GURL url2 = embedded_test_server()->GetURL("localhost", kWaitSizePath);
  EXPECT_NE(url.host(), url2.host());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url2.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(url2, "done");

  absl::optional<base::Value> dpr_after_navigation =
      cr_fuchsia::ExecuteJavaScript(frame.ptr().get(),
                                    "window.devicePixelRatio");
  ASSERT_TRUE(scaled_dpr);

  EXPECT_EQ(dpr_after_navigation, scaled_dpr);

  // Reset the scale to 1.0 (default) and verify that reported DPR is updated
  // to 1.0.
  frame->SetPageScale(1.0);
  absl::optional<base::Value> dpr_after_reset = cr_fuchsia::ExecuteJavaScript(
      frame.ptr().get(), "window.devicePixelRatio");
  ASSERT_TRUE(dpr_after_reset);

  EXPECT_EQ(dpr_after_reset->GetDouble(), 1.0);

  // Zoom out by setting scale to 0.5.
  const float kZoomOutScale = 0.5;
  frame->SetPageScale(kZoomOutScale);

  absl::optional<base::Value> zoomed_out_dpr = cr_fuchsia::ExecuteJavaScript(
      frame.ptr().get(), "window.devicePixelRatio");
  ASSERT_TRUE(zoomed_out_dpr);

  EXPECT_EQ(zoomed_out_dpr->GetDouble(), kZoomOutScale);

  // Create another frame. Verify that the scale factor is not applied to the
  // new frame.
  auto frame2 = cr_fuchsia::FrameForTest::Create(context(), {});

  view_tokens = scenic::ViewTokenPair::New();
  frame2->CreateView(std::move(view_tokens.view_token));

  presenter->PresentOrReplaceView(std::move(view_tokens.view_holder_token),
                                  nullptr);

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame2.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      url.spec()));
  frame2.navigation_listener().RunUntilUrlAndTitleEquals(url, "done");

  absl::optional<base::Value> frame2_dpr = cr_fuchsia::ExecuteJavaScript(
      frame2.ptr().get(), "window.devicePixelRatio");
  ASSERT_TRUE(frame2_dpr);

  EXPECT_EQ(frame2_dpr->GetDouble(), 1.0);
}

// Send a MessagePort to the content, then perform bidirectional messaging
// over its channel.
IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessageMessagePortDisconnected) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      post_message_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(post_message_url,
                                                        "messageport");

  fuchsia::web::MessagePortPtr message_port;
  {
    base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
    frame->PostMessage(
        post_message_url.GetOrigin().spec(),
        cr_fuchsia::CreateWebMessageWithMessagePortRequest(
            message_port.NewRequest(), base::MemBufferFromString("hi", "test")),
        cr_fuchsia::CallbackToFitFunction(post_result.GetCallback()));

    base::test::TestFuture<fuchsia::web::WebMessage> receiver;
    message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetCallback()));
    ASSERT_TRUE(post_result.Wait());
    ASSERT_TRUE(receiver.Wait());
    ASSERT_TRUE(receiver.IsReady());
    EXPECT_EQ("got_port", StringFromMemBufferOrDie(receiver.Get().data()));
    EXPECT_TRUE(post_result.Get().is_response());
  }

  // Navigating off-page should tear down the Mojo channel, thereby causing the
  // MessagePortImpl to self-destruct and tear down its FIDL channel.
  {
    base::RunLoop run_loop;
    message_port.set_error_handler(
        [&run_loop](zx_status_t) { run_loop.Quit(); });
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        url::kAboutBlankURL));
    run_loop.Run();
  }
}

// Send a MessagePort to the content, and through that channel, receive a
// different MessagePort that was created by the content. Verify the second
// channel's liveness by sending a ping to it.
IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessageUseContentProvidedPort) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      post_message_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(post_message_url,
                                                        "messageport");

  fuchsia::web::MessagePortPtr incoming_message_port;
  {
    fuchsia::web::MessagePortPtr message_port;
    base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
    frame->PostMessage(
        "*",
        cr_fuchsia::CreateWebMessageWithMessagePortRequest(
            message_port.NewRequest(), base::MemBufferFromString("hi", "test")),
        cr_fuchsia::CallbackToFitFunction(post_result.GetCallback()));

    base::test::TestFuture<fuchsia::web::WebMessage> receiver;
    message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetCallback()));
    ASSERT_TRUE(receiver.Wait());
    ASSERT_TRUE(receiver.Get().has_data());
    EXPECT_EQ("got_port", StringFromMemBufferOrDie(receiver.Get().data()));
    ASSERT_TRUE(receiver.Get().has_incoming_transfer());
    ASSERT_EQ(receiver.Get().incoming_transfer().size(), 1u);
    incoming_message_port = receiver.Take()
                                .mutable_incoming_transfer()
                                ->at(0)
                                .message_port()
                                .Bind();
    EXPECT_TRUE(post_result.Get().is_response());
  }

  // Get the content to send three 'ack ping' messages, which will accumulate in
  // the MessagePortImpl buffer.
  for (int i = 0; i < 3; ++i) {
    base::test::TestFuture<fuchsia::web::MessagePort_PostMessage_Result>
        post_result;
    fuchsia::web::WebMessage msg;
    msg.set_data(base::MemBufferFromString("ping", "test"));
    incoming_message_port->PostMessage(
        std::move(msg),
        cr_fuchsia::CallbackToFitFunction(post_result.GetCallback()));
    ASSERT_TRUE(post_result.Wait());
    EXPECT_TRUE(post_result.Get().is_response());
  }

  // Receive another acknowledgement from content on a side channel to ensure
  // that all the "ack pings" are ready to be consumed.
  {
    fuchsia::web::MessagePortPtr ack_message_port;

    // Quit the runloop only after we've received a WebMessage AND a PostMessage
    // result.
    base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
    frame->PostMessage(
        "*",
        cr_fuchsia::CreateWebMessageWithMessagePortRequest(
            ack_message_port.NewRequest(),
            base::MemBufferFromString("hi", "test")),
        cr_fuchsia::CallbackToFitFunction(post_result.GetCallback()));
    base::test::TestFuture<fuchsia::web::WebMessage> receiver;
    ack_message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetCallback()));
    ASSERT_TRUE(receiver.Wait());
    ASSERT_TRUE(receiver.Get().has_data());
    EXPECT_EQ("got_port", StringFromMemBufferOrDie(receiver.Get().data()));
    EXPECT_TRUE(post_result.Get().is_response());
  }

  // Pull the three 'ack ping's from the buffer.
  for (int i = 0; i < 3; ++i) {
    base::test::TestFuture<fuchsia::web::WebMessage> receiver;
    incoming_message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetCallback()));
    ASSERT_TRUE(receiver.Wait());
    ASSERT_TRUE(receiver.Get().has_data());
    EXPECT_EQ("ack ping", StringFromMemBufferOrDie(receiver.Get().data()));
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessageBadOriginDropped) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      post_message_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(post_message_url,
                                                        "messageport");

  // PostMessage() to invalid origins should be ignored. We pass in a
  // MessagePort but nothing should happen to it.
  fuchsia::web::MessagePortPtr bad_origin_incoming_message_port;
  base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result>
      unused_post_result;
  frame->PostMessage(
      "https://example.com",
      cr_fuchsia::CreateWebMessageWithMessagePortRequest(
          bad_origin_incoming_message_port.NewRequest(),
          base::MemBufferFromString("bad origin, bad!", "test")),
      cr_fuchsia::CallbackToFitFunction(unused_post_result.GetCallback()));
  base::test::TestFuture<fuchsia::web::WebMessage> unused_message_read;
  bad_origin_incoming_message_port->ReceiveMessage(
      cr_fuchsia::CallbackToFitFunction(unused_message_read.GetCallback()));

  // PostMessage() with a valid origin should succeed.
  // Verify it by looking for an ack message on the MessagePort we passed in.
  // Since message events are handled in order, observing the result of this
  // operation will verify whether the previous PostMessage() was received but
  // discarded.
  fuchsia::web::MessagePortPtr incoming_message_port;
  fuchsia::web::MessagePortPtr message_port;
  base::test::TestFuture<fuchsia::web::Frame_PostMessage_Result> post_result;
  frame->PostMessage(
      "*",
      cr_fuchsia::CreateWebMessageWithMessagePortRequest(
          message_port.NewRequest(),
          base::MemBufferFromString("good origin", "test")),
      cr_fuchsia::CallbackToFitFunction(post_result.GetCallback()));
  base::test::TestFuture<fuchsia::web::WebMessage> receiver;
  message_port->ReceiveMessage(
      cr_fuchsia::CallbackToFitFunction(receiver.GetCallback()));
  ASSERT_TRUE(receiver.Wait());
  ASSERT_TRUE(receiver.Get().has_data());
  EXPECT_EQ("got_port", StringFromMemBufferOrDie(receiver.Get().data()));
  ASSERT_TRUE(receiver.Get().has_incoming_transfer());
  ASSERT_EQ(receiver.Get().incoming_transfer().size(), 1u);
  incoming_message_port =
      receiver.Take().mutable_incoming_transfer()->at(0).message_port().Bind();
  EXPECT_TRUE(post_result.Get().is_response());

  // Verify that the first PostMessage() call wasn't handled.
  EXPECT_FALSE(unused_message_read.IsReady());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, RecreateView) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());

  // Process the Frame creation request, and verify we can get the FrameImpl.
  base::RunLoop().RunUntilIdle();
  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame.ptr());
  ASSERT_TRUE(frame_impl);
  EXPECT_FALSE(frame_impl->has_view_for_test());

  // CreateView() will cause the AccessibilityBridge to be created.
  FakeSemanticsManager fake_semantics_manager;
  frame_impl->set_semantics_manager_for_test(&fake_semantics_manager);

  // Verify that the Frame can navigate, prior to the View being created.
  const GURL page1_url(embedded_test_server()->GetURL(kPage1Path));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page1_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(page1_url, kPage1Title);

  // Request a View from the Frame, and pump the loop to process the request.
  auto view_tokens = scenic::ViewTokenPair::New();
  frame->CreateView(std::move(view_tokens.view_token));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(frame_impl->has_view_for_test());

  // Verify that the Frame still works, by navigating to Page #2.
  const GURL page2_url(embedded_test_server()->GetURL(kPage2Path));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page2_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(page2_url, kPage2Title);

  // Create new View tokens and request a new view.
  auto view_tokens2 = scenic::ViewTokenPair::New();
  frame->CreateView(std::move(view_tokens2.view_token));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(frame_impl->has_view_for_test());

  // Verify that the Frame still works, by navigating back to Page #1.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page1_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(page1_url, kPage1Title);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ChildFrameNavigationIgnored) {
  net::test_server::EmbeddedTestServerHandle test_server_handle;
  ASSERT_TRUE(test_server_handle =
                  embedded_test_server()->StartAndReturnHandle());
  GURL page_url(embedded_test_server()->GetURL("/creates_child_frame.html"));

  // Navigate to a page and wait for the navigation to complete.
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});
  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
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
  frame->PostMessage(
      page_url.GetOrigin().spec(), std::move(message),
      cr_fuchsia::CallbackToFitFunction(post_result.GetCallback()));

  frame.navigation_listener().SetBeforeAckHook(
      base::BindRepeating([](const fuchsia::web::NavigationState& change,
                             OnNavigationStateChangedCallback callback) {
        // The child iframe's loading status should not affect the
        // is_main_document_loaded() bit.
        if (change.has_is_main_document_loaded())
          ADD_FAILURE();

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
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});
  frame.navigation_listener().SetBeforeAckHook(base::BindRepeating(
      [](base::RunLoop* run_loop, const fuchsia::web::NavigationState& change,
         OnNavigationStateChangedCallback callback) {
        EXPECT_TRUE(change.IsEmpty());
        run_loop->Quit();
        callback();
      },
      base::Unretained(&run_loop)));
  run_loop.Run();
  frame.navigation_listener().SetBeforeAckHook({});

  // Navigate to a page and wait for the navigation to complete.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(page_url);

  // Attach a new navigation listener, we should get the new page state, even if
  // no new navigation occurred.
  cr_fuchsia::TestNavigationListener navigation_listener;
  fidl::Binding<fuchsia::web::NavigationEventListener>
      navigation_listener_binding(&navigation_listener);
  frame.ptr()->SetNavigationEventListener(
      navigation_listener_binding.NewBinding());
  navigation_listener.RunUntilUrlAndTitleEquals(page_url, kPage1Title);
}

// Check loading an invalid URL in NavigationController.LoadUrl() sets the right
// error.
IN_PROC_BROWSER_TEST_F(FrameImplTest, InvalidUrl) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  base::test::TestFuture<fuchsia::web::NavigationController_LoadUrl_Result>
      result;
  auto controller = frame.GetNavigationController();
  controller->LoadUrl("http:google.com:foo", fuchsia::web::LoadUrlParams(),
                      cr_fuchsia::CallbackToFitFunction(result.GetCallback()));
  ASSERT_TRUE(result.Wait());

  ASSERT_TRUE(result.Get().is_err());
  EXPECT_EQ(result.Get().err(),
            fuchsia::web::NavigationControllerError::INVALID_URL);
}

// Check setting invalid headers in NavigationController.LoadUrl() sets the
// right error.
IN_PROC_BROWSER_TEST_F(FrameImplTest, InvalidHeader) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  {
    // Set an invalid header name.
    fuchsia::web::LoadUrlParams load_url_params;
    fuchsia::net::http::Header header;
    header.name = cr_fuchsia::StringToBytes("Invalid:Header");
    header.value = cr_fuchsia::StringToBytes("1");
    load_url_params.set_headers({header});

    base::test::TestFuture<fuchsia::web::NavigationController_LoadUrl_Result>
        result;
    auto controller = frame.GetNavigationController();
    controller->LoadUrl(
        "http://site.ext/", std::move(load_url_params),
        cr_fuchsia::CallbackToFitFunction(result.GetCallback()));
    ASSERT_TRUE(result.Wait());

    ASSERT_TRUE(result.Get().is_err());
    EXPECT_EQ(result.Get().err(),
              fuchsia::web::NavigationControllerError::INVALID_HEADER);
  }

  {
    // Set an invalid header value.
    fuchsia::web::LoadUrlParams load_url_params;
    fuchsia::net::http::Header header;
    header.name = cr_fuchsia::StringToBytes("Header");
    header.value = cr_fuchsia::StringToBytes("Invalid\rValue");
    load_url_params.set_headers({header});

    base::test::TestFuture<fuchsia::web::NavigationController_LoadUrl_Result>
        result;
    auto controller = frame.GetNavigationController();
    controller->LoadUrl(
        "http://site.ext/", std::move(load_url_params),
        cr_fuchsia::CallbackToFitFunction(result.GetCallback()));
    result.Wait();

    ASSERT_TRUE(result.Get().is_err());
    EXPECT_EQ(result.Get().err(),
              fuchsia::web::NavigationControllerError::INVALID_HEADER);
  }
}

class RequestMonitoringFrameImplBrowserTest : public FrameImplTest {
 public:
  RequestMonitoringFrameImplBrowserTest() = default;

 protected:
  void SetUpOnMainThread() override {
    // Accumulate all http requests made to |embedded_test_server| into
    // |accumulated_requests_| container.
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &RequestMonitoringFrameImplBrowserTest::MonitorRequestOnIoThread,
        base::Unretained(this), base::SequencedTaskRunnerHandle::Get()));

    ASSERT_TRUE(test_server_handle_ =
                    embedded_test_server()->StartAndReturnHandle());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Needed for UrlRequestRewriteAddHeaders.
    command_line->AppendSwitchNative(switches::kCorsExemptHeaders, "Test");
  }

  std::map<GURL, net::test_server::HttpRequest> accumulated_requests_;

 private:
  void MonitorRequestOnIoThread(
      const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
      const net::test_server::HttpRequest& request) {
    main_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RequestMonitoringFrameImplBrowserTest::MonitorRequestOnMainThread,
            base::Unretained(this), request));
  }

  void MonitorRequestOnMainThread(
      const net::test_server::HttpRequest& request) {
    accumulated_requests_[request.GetURL()] = request;
  }

  net::test_server::EmbeddedTestServerHandle test_server_handle_;

  DISALLOW_COPY_AND_ASSIGN(RequestMonitoringFrameImplBrowserTest);
};

IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest, ExtraHeaders) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  const GURL page_url(embedded_test_server()->GetURL(kPage1Path));
  fuchsia::web::LoadUrlParams load_url_params;
  fuchsia::net::http::Header header1;
  header1.name = cr_fuchsia::StringToBytes("X-ExtraHeaders");
  header1.value = cr_fuchsia::StringToBytes("1");
  fuchsia::net::http::Header header2;
  header2.name = cr_fuchsia::StringToBytes("X-2ExtraHeaders");
  header2.value = cr_fuchsia::StringToBytes("2");
  load_url_params.set_headers({header1, header2});

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), std::move(load_url_params),
      page_url.spec()));
  frame.navigation_listener().RunUntilUrlAndTitleEquals(page_url, kPage1Title);

  // At this point, the page should be loaded, the server should have received
  // the request and the request should be in the map.
  const auto iter = accumulated_requests_.find(page_url);
  ASSERT_NE(iter, accumulated_requests_.end());
  EXPECT_THAT(iter->second.headers, Contains(Key("X-ExtraHeaders")));
  EXPECT_THAT(iter->second.headers, Contains(Key("X-2ExtraHeaders")));
}

// Tests that UrlRequestActions can be set up to deny requests to specific
// hosts.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteDeny) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_hosts_filter({"127.0.0.1"});
  rule.set_action(fuchsia::web::UrlRequestAction::DENY);
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // 127.0.0.1 should be blocked.
  const GURL page_url(embedded_test_server()->GetURL(kPage4Path));
  {
    fuchsia::web::NavigationState error_state;
    error_state.set_page_type(fuchsia::web::PageType::ERROR);
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        page_url.spec()));
    frame.navigation_listener().RunUntilNavigationStateMatches(error_state);
  }

  // However, "localhost" is not blocked, so this request should be allowed.
  {
    GURL::Replacements replacements;
    replacements.SetHostStr("localhost");
    GURL page_url_localhost = page_url.ReplaceComponents(replacements);
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        page_url_localhost.spec()));
    frame.navigation_listener().RunUntilUrlEquals(page_url_localhost);
  }
}

// Tests that a UrlRequestAction with no filter criteria will apply to all
// requests.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteDenyAll) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  // No filter criteria are set, so everything is denied.
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_action(fuchsia::web::UrlRequestAction::DENY);
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // 127.0.0.1 should be blocked.
  const GURL page_url(embedded_test_server()->GetURL(kPage4Path));
  {
    fuchsia::web::NavigationState error_state;
    error_state.set_page_type(fuchsia::web::PageType::ERROR);
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        page_url.spec()));
    frame.navigation_listener().RunUntilNavigationStateMatches(error_state);
  }

  // "localhost" should be blocked.
  {
    GURL::Replacements replacements;
    replacements.SetHostStr("localhost");
    GURL page_url_localhost = page_url.ReplaceComponents(replacements);
    fuchsia::web::NavigationState error_state;
    error_state.set_page_type(fuchsia::web::PageType::ERROR);
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        page_url.spec()));
    frame.navigation_listener().RunUntilNavigationStateMatches(error_state);
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        page_url_localhost.spec()));
    frame.navigation_listener().RunUntilNavigationStateMatches(error_state);
  }
}

// Tests that UrlRequestActions can be set up to only allow requests for a
// single host, while denying everything else.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteSelectiveAllow) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  // Allow 127.0.0.1.
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_hosts_filter({"127.0.0.1"});
  rule.set_action(fuchsia::web::UrlRequestAction::ALLOW);
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));

  // Deny everything else.
  rule = {};
  rule.set_action(fuchsia::web::UrlRequestAction::DENY);
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // 127.0.0.1 should be allowed.
  const GURL page_url(embedded_test_server()->GetURL(kPage4Path));
  {
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        page_url.spec()));
    frame.navigation_listener().RunUntilUrlEquals(page_url);
  }

  // "localhost" should be blocked.
  {
    GURL::Replacements replacements;
    replacements.SetHostStr("localhost");
    GURL page_url_localhost = page_url.ReplaceComponents(replacements);
    fuchsia::web::NavigationState error_state;
    error_state.set_page_type(fuchsia::web::PageType::ERROR);
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        page_url_localhost.spec()));
    frame.navigation_listener().RunUntilNavigationStateMatches(error_state);
  }
}

// Tests the URLRequestRewrite API properly adds headers on every requests.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteAddHeaders) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get the additional header on the main request and the
  // image request.
  const GURL page_url(embedded_test_server()->GetURL(kPage4Path));
  const GURL img_url(embedded_test_server()->GetURL(kPage4ImgPath));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(page_url);

  {
    const auto iter = accumulated_requests_.find(page_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, Contains(Key("Test")));
  }
  {
    const auto iter = accumulated_requests_.find(img_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, Contains(Key("Test")));
  }
}

// Tests the URLRequestRewrite API properly adds headers on every requests.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteAddExistingHeader) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate. The first page request should have the "Test" header set to
  // "Value". The second one should have the "Test" header set to "SetByJS" via
  // JavaScript and not be overridden by the rewrite rule.
  const GURL page_url(embedded_test_server()->GetURL(kSetHeaderRequestPath));
  const GURL img_url(embedded_test_server()->GetURL(kPage4Path));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  frame.navigation_listener().RunUntilTitleEquals("loaded");

  {
    const auto iter = accumulated_requests_.find(page_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    ASSERT_THAT(iter->second.headers, Contains(Key("Test")));
    EXPECT_EQ(iter->second.headers["Test"], "Value");
  }
  {
    const auto iter = accumulated_requests_.find(img_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    ASSERT_THAT(iter->second.headers, Contains(Key("Test")));
    EXPECT_EQ(iter->second.headers["Test"], "SetByJS");
  }
}

// Tests the URLRequestRewrite API properly removes headers on every requests.
// Also tests that rewrites are applied properly in succession.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteRemoveHeader) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test", "Value"));
  rewrites.push_back(
      cr_fuchsia::CreateRewriteRemoveHeader(absl::nullopt, "Test"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get no "Test" header.
  const GURL page_url(embedded_test_server()->GetURL(kPage4Path));
  const GURL img_url(embedded_test_server()->GetURL(kPage4ImgPath));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(page_url);

  {
    const auto iter = accumulated_requests_.find(page_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, Not(Contains(Key("Test"))));
  }
  {
    const auto iter = accumulated_requests_.find(img_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, Not(Contains(Key("Test"))));
  }
}

// Tests the URLRequestRewrite API properly removes headers, based on the
// presence of a string in the query.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteRemoveHeaderWithQuery) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  const GURL page_url(embedded_test_server()->GetURL("/page?stuff=[pattern]"));

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test", "Value"));
  rewrites.push_back(cr_fuchsia::CreateRewriteRemoveHeader(
      absl::make_optional("[pattern]"), "Test"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get no "Test" header.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(page_url);

  const auto iter = accumulated_requests_.find(page_url);
  ASSERT_NE(iter, accumulated_requests_.end());
  EXPECT_THAT(iter->second.headers, Not(Contains(Key("Test"))));
}

// Tests the URLRequestRewrite API properly handles query substitution.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteSubstituteQueryPattern) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(cr_fuchsia::CreateRewriteSubstituteQueryPattern(
      "[pattern]", "substitution"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get to the URL with the modified request.
  const GURL page_url(embedded_test_server()->GetURL("/page?[pattern]"));
  const GURL final_url(embedded_test_server()->GetURL("/page?substitution"));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(final_url);

  EXPECT_THAT(accumulated_requests_, Contains(Key(final_url)));
}

// Tests the URLRequestRewrite API properly handles URL replacement.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteReplaceUrl) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  const GURL page_url(embedded_test_server()->GetURL(kPage1Path));
  const GURL final_url(embedded_test_server()->GetURL(kPage2Path));

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(
      cr_fuchsia::CreateRewriteReplaceUrl(kPage1Path, final_url.spec()));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get to the replaced URL.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(final_url);

  EXPECT_THAT(accumulated_requests_, Contains(Key(final_url)));
}

// Tests the URLRequestRewrite API properly handles URL replacement when the
// original request URL contains a query and a fragment string.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteReplaceUrlQueryRef) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  const GURL page_url(
      embedded_test_server()->GetURL(std::string(kPage1Path) + "?query#ref"));
  const GURL replacement_url(embedded_test_server()->GetURL(kPage2Path));
  const GURL final_url_with_ref(
      embedded_test_server()->GetURL(std::string(kPage2Path) + "?query#ref"));
  const GURL final_url(
      embedded_test_server()->GetURL(std::string(kPage2Path) + "?query"));

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(
      cr_fuchsia::CreateRewriteReplaceUrl(kPage1Path, replacement_url.spec()));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get to the replaced URL.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(final_url_with_ref);

  EXPECT_THAT(accumulated_requests_, Contains(Key(final_url)));
}

// Tests the URLRequestRewrite API properly handles adding a query.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteAddQuery) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(cr_fuchsia::CreateRewriteAppendToQuery("query"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  {
    // Add a query to a URL with no query.
    const GURL page_url(embedded_test_server()->GetURL(kPage1Path));
    const GURL expected_url(
        embedded_test_server()->GetURL(std::string(kPage1Path) + "?query"));

    // Navigate, we should get to the URL with the query.
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        page_url.spec()));
    frame.navigation_listener().RunUntilUrlEquals(expected_url);

    EXPECT_THAT(accumulated_requests_, Contains(Key(expected_url)));
  }

  {
    // Add a quest to a URL that has an empty query.
    const std::string original_path = std::string(kPage1Path) + "?";
    const GURL page_url(embedded_test_server()->GetURL(original_path));
    const GURL expected_url(
        embedded_test_server()->GetURL(original_path + "query"));

    // Navigate, we should get to the URL with the query, but no "&".
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        page_url.spec()));
    frame.navigation_listener().RunUntilUrlEquals(expected_url);

    EXPECT_THAT(accumulated_requests_, Contains(Key(expected_url)));
  }

  {
    // Add a query to a URL that already has a query.
    const std::string original_path =
        std::string(kPage1Path) + "?original_query=value";
    const GURL page_url(embedded_test_server()->GetURL(original_path));
    const GURL expected_url(
        embedded_test_server()->GetURL(original_path + "&query"));

    // Navigate, we should get to the URL with the appended query.
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        page_url.spec()));
    frame.navigation_listener().RunUntilUrlEquals(expected_url);

    EXPECT_THAT(accumulated_requests_, Contains(Key(expected_url)));
  }

  {
    // Add a query to a URL that has a ref.
    const GURL page_url(
        embedded_test_server()->GetURL(std::string(kPage1Path) + "#ref"));
    const GURL expected_url(
        embedded_test_server()->GetURL(std::string(kPage1Path) + "?query#ref"));

    // Navigate, we should get to the URL with the query.
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
        page_url.spec()));
    frame.navigation_listener().RunUntilUrlEquals(expected_url);
  }
}

// Tests the URLRequestRewrite API properly handles adding a query with a
// question mark.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteAppendToQueryQuestionMark) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  const GURL page_url(embedded_test_server()->GetURL(kPage1Path));
  const GURL expected_url(
      embedded_test_server()->GetURL(std::string(kPage1Path) + "?qu?ery"));

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(cr_fuchsia::CreateRewriteAppendToQuery("qu?ery"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get to the URL with the query.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(expected_url);

  EXPECT_THAT(accumulated_requests_, Contains(Key(expected_url)));
}

// Tests the URLRequestRewrite API properly handles scheme and host filtering in
// rules.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteSchemeHostFilter) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites1;
  rewrites1.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test1", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule1;
  rule1.set_rewrites(std::move(rewrites1));
  rule1.set_hosts_filter({"127.0.0.1"});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites2;
  rewrites2.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test2", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule2;
  rule2.set_rewrites(std::move(rewrites2));
  rule2.set_hosts_filter({"test.xyz"});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites3;
  rewrites3.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test3", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule3;
  rule3.set_rewrites(std::move(rewrites3));
  rule3.set_schemes_filter({"http"});

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites4;
  rewrites4.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test4", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule4;
  rule4.set_rewrites(std::move(rewrites4));
  rule4.set_schemes_filter({"https"});

  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule1));
  rules.push_back(std::move(rule2));
  rules.push_back(std::move(rule3));
  rules.push_back(std::move(rule4));

  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  // Navigate, we should get the "Test1" and "Test3" headers, but not "Test2"
  // and "Test4".
  const GURL page_url(embedded_test_server()->GetURL("/default"));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), fuchsia::web::LoadUrlParams(),
      page_url.spec()));
  frame.navigation_listener().RunUntilUrlEquals(page_url);

  const auto iter = accumulated_requests_.find(page_url);
  ASSERT_NE(iter, accumulated_requests_.end());
  EXPECT_THAT(iter->second.headers, Contains(Key("Test1")));
  EXPECT_THAT(iter->second.headers, Contains(Key("Test3")));
  EXPECT_THAT(iter->second.headers, Not(Contains(Key("Test2"))));
  EXPECT_THAT(iter->second.headers, Not(Contains(Key("Test4"))));
}

// Tests the URLRequestRewrite API properly closes the Frame channel if the
// rules are invalid.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteInvalidRules) {
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});
  base::RunLoop run_loop;
  frame.ptr().set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_INVALID_ARGS);
    run_loop.Quit();
  });

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(cr_fuchsia::CreateRewriteAddHeaders("Te\nst1", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));

  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});
  run_loop.Run();
}

class TestPopupListener : public fuchsia::web::PopupFrameCreationListener {
 public:
  TestPopupListener() = default;
  ~TestPopupListener() override = default;
  TestPopupListener(const TestPopupListener&) = delete;
  TestPopupListener& operator=(const TestPopupListener&) = delete;

  void GetAndAckNextPopup(fuchsia::web::FramePtr* frame,
                          fuchsia::web::PopupFrameCreationInfo* creation_info) {
    if (!frame_) {
      base::RunLoop run_loop;
      received_popup_callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }

    *frame = frame_.Bind();
    *creation_info = std::move(creation_info_);

    popup_ack_callback_();
    popup_ack_callback_ = {};
  }

 private:
  void OnPopupFrameCreated(fidl::InterfaceHandle<fuchsia::web::Frame> frame,
                           fuchsia::web::PopupFrameCreationInfo creation_info,
                           OnPopupFrameCreatedCallback callback) override {
    creation_info_ = std::move(creation_info);
    frame_ = std::move(frame);

    popup_ack_callback_ = std::move(callback);

    if (received_popup_callback_)
      std::move(received_popup_callback_).Run();
  }

  fidl::InterfaceHandle<fuchsia::web::Frame> frame_;
  fuchsia::web::PopupFrameCreationInfo creation_info_;
  base::OnceClosure received_popup_callback_;
  OnPopupFrameCreatedCallback popup_ack_callback_;
};

// TODO(crbug.com/1155378): Move these tests to their own file in a follow-up
// CL.
class FrameImplPopupTest : public FrameImplTestBaseWithServer {
 public:
  FrameImplPopupTest()
      : popup_listener_binding_(&popup_listener_),
        popup_nav_listener_binding_(&popup_nav_listener_) {}

  ~FrameImplPopupTest() override = default;
  FrameImplPopupTest(const FrameImplPopupTest&) = delete;
  FrameImplPopupTest& operator=(const FrameImplPopupTest&) = delete;

 protected:
  // Builds a URL for the kPopupParentPath page to pop up a Frame with
  // |child_file_and_query|. |child_file_and_query| may optionally include a
  // query string.
  GURL GetParentPageTestServerUrl(const char* child) const;

  // Loads a page that autoplays video in a popup, populates the popup_*
  // members, and returns its URL.
  GURL LoadAutoPlayingPageInPopup(
      fuchsia::web::CreateFrameParams parent_frame_params);

  fuchsia::web::FramePtr popup_frame_;

  TestPopupListener popup_listener_;
  fidl::Binding<fuchsia::web::PopupFrameCreationListener>
      popup_listener_binding_;

  cr_fuchsia::TestNavigationListener popup_nav_listener_;
  fidl::Binding<fuchsia::web::NavigationEventListener>
      popup_nav_listener_binding_;
};

GURL FrameImplPopupTest::GetParentPageTestServerUrl(const char* child) const {
  const std::string url = base::StringPrintf("%s?%s=%s", kPopupParentPath,
                                             kChildQueryParamName, child);

  return embedded_test_server()->GetURL(url);
}

GURL FrameImplPopupTest::LoadAutoPlayingPageInPopup(
    fuchsia::web::CreateFrameParams parent_frame_params) {
  GURL popup_parent_url = GetParentPageTestServerUrl(kAutoplayFileAndQuery);
  GURL popup_child_url = embedded_test_server()->GetURL(
      base::StringPrintf("/%s", kAutoplayFileAndQuery));

  auto parent_frame = cr_fuchsia::FrameForTest::Create(
      context(), std::move(parent_frame_params));

  parent_frame->SetPopupFrameCreationListener(
      popup_listener_binding_.NewBinding());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      parent_frame.GetNavigationController(), {}, popup_parent_url.spec()));

  fuchsia::web::PopupFrameCreationInfo popup_info;
  popup_listener_.GetAndAckNextPopup(&popup_frame_, &popup_info);
  EXPECT_EQ(popup_info.initial_url(), popup_child_url);

  popup_frame_->SetNavigationEventListener(
      popup_nav_listener_binding_.NewBinding());

  return popup_child_url;
}

IN_PROC_BROWSER_TEST_F(FrameImplPopupTest, PopupWindowRedirect) {
  GURL popup_parent_url = GetParentPageTestServerUrl(kPopupChildFile);
  GURL popup_child_url(embedded_test_server()->GetURL(kPopupRedirectPath));
  GURL title1_url(embedded_test_server()->GetURL(kPage1Path));
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  frame->SetPopupFrameCreationListener(popup_listener_binding_.NewBinding());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), {}, popup_parent_url.spec()));

  // Verify the popup's initial URL, "popup_child.html".
  fuchsia::web::PopupFrameCreationInfo popup_info;
  popup_listener_.GetAndAckNextPopup(&popup_frame_, &popup_info);
  EXPECT_EQ(popup_info.initial_url(), popup_child_url);

  // Verify that the popup eventually redirects to "title1.html".
  popup_frame_->SetNavigationEventListener(
      popup_nav_listener_binding_.NewBinding());
  popup_nav_listener_.RunUntilUrlAndTitleEquals(title1_url, kPage1Title);
}

IN_PROC_BROWSER_TEST_F(FrameImplPopupTest, MultiplePopups) {
  GURL popup_parent_url(embedded_test_server()->GetURL(kPopupMultiplePath));
  GURL title1_url(embedded_test_server()->GetURL(kPage1Path));
  GURL title2_url(embedded_test_server()->GetURL(kPage2Path));
  auto frame = cr_fuchsia::FrameForTest::Create(context(), {});

  frame->SetPopupFrameCreationListener(popup_listener_binding_.NewBinding());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      frame.GetNavigationController(), {}, popup_parent_url.spec()));

  fuchsia::web::PopupFrameCreationInfo popup_info;
  popup_listener_.GetAndAckNextPopup(&popup_frame_, &popup_info);
  EXPECT_EQ(popup_info.initial_url(), title1_url);

  popup_listener_.GetAndAckNextPopup(&popup_frame_, &popup_info);
  EXPECT_EQ(popup_info.initial_url(), title2_url);
}

// Verifies that the child popup Frame has the same default CreateFrameParams as
// the parent Frame by verifying that autoplay is blocked in the child. This
// mostly verifies that AutoPlaySucceedsis actually modifies behavior.
IN_PROC_BROWSER_TEST_F(FrameImplPopupTest,
                       PopupFrameHasSameCreateFrameParams_AutoplayBlocked) {
  // The default autoplay_policy is REQUIRE_USER_ACTIVATION.
  fuchsia::web::CreateFrameParams parent_frame_params;

  // Load the page and wait for the popup Frame to be created.
  GURL popup_child_url =
      LoadAutoPlayingPageInPopup(std::move(parent_frame_params));

  // Verify that the child does not autoplay media.
  popup_nav_listener_.RunUntilUrlAndTitleEquals(popup_child_url,
                                                kAutoPlayBlockedTitle);
}

// Verifies that the child popup Frame has the same CreateFrameParams as the
// parent Frame by allowing autoplay in the parent's params and verifying that
// autoplay succeeds in the child.
IN_PROC_BROWSER_TEST_F(FrameImplPopupTest,
                       PopupFrameHasSameCreateFrameParams_AutoplaySucceeds) {
  // Set autoplay to always be allowed in the parent frame.
  fuchsia::web::CreateFrameParams parent_frame_params;
  parent_frame_params.set_autoplay_policy(fuchsia::web::AutoplayPolicy::ALLOW);

  // Load the page and wait for the popup Frame to be created.
  GURL popup_child_url =
      LoadAutoPlayingPageInPopup(std::move(parent_frame_params));

  // Verify that the child autoplays media.
  popup_nav_listener_.RunUntilUrlAndTitleEquals(popup_child_url,
                                                kAutoPlaySuccessTitle);
}
