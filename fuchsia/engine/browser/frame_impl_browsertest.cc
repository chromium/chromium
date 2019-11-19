// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>
#include <lib/ui/scenic/cpp/view_token_pair.h>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_timeouts.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "fuchsia/base/fit_adapter.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/result_receiver.h"
#include "fuchsia/base/string_util.h"
#include "fuchsia/base/test_navigation_listener.h"
#include "fuchsia/base/url_request_rewrite_test_util.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/test/test_data.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
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
using testing::Field;
using testing::InvokeWithoutArgs;
using testing::Mock;

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
const char kPopupPath[] = "/popup_parent.html";
const char kPopupRedirectPath[] = "/popup_child.html";
const char kPopupMultiplePath[] = "/popup_multiple.html";
const char kPage1Title[] = "title 1";
const char kPage2Title[] = "title 2";
const char kPage3Title[] = "websql not available";
const char kDataUrl[] =
    "data:text/html;base64,PGI+SGVsbG8sIHdvcmxkLi4uPC9iPg==";
const int64_t kOnLoadScriptId = 0;

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
  std::string output;
  CHECK(cr_fuchsia::StringFromMemBuffer(buffer, &output));
  return output;
}

}  // namespace

// Defines a suite of tests that exercise Frame-level functionality, such as
// navigation commands and page events.
class FrameImplTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  FrameImplTest()
      : run_timeout_(TestTimeouts::action_timeout(),
                     base::MakeExpectedNotRunClosure(FROM_HERE)) {
    set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
  }

  ~FrameImplTest() = default;

  MOCK_METHOD1(OnServeHttpRequest,
               void(const net::test_server::HttpRequest& request));

 protected:
  // Creates a Frame with |navigation_listener_| attached.
  fuchsia::web::FramePtr CreateFrame() {
    return WebEngineBrowserTest::CreateFrame(&navigation_listener_);
  }

  cr_fuchsia::TestNavigationListener navigation_listener_;

 private:
  const base::RunLoop::ScopedRunTimeoutForTest run_timeout_;

  DISALLOW_COPY_AND_ASSIGN(FrameImplTest);
};

void VerifyCanGoBackAndForward(fuchsia::web::NavigationController* controller,
                               bool can_go_back_expected,
                               bool can_go_forward_expected) {
  base::RunLoop run_loop;
  cr_fuchsia::ResultReceiver<fuchsia::web::NavigationState> visible_entry(
      run_loop.QuitClosure());
  controller->GetVisibleEntry(
      cr_fuchsia::CallbackToFitFunction(visible_entry.GetReceiveCallback()));
  run_loop.Run();
  EXPECT_TRUE(visible_entry->has_can_go_back());
  EXPECT_EQ(visible_entry->can_go_back(), can_go_back_expected);
  EXPECT_TRUE(visible_entry->has_can_go_forward());
  EXPECT_EQ(visible_entry->can_go_forward(), can_go_forward_expected);
}

// Verifies that the browser will navigate and generate a navigation listener
// event when LoadUrl() is called.
IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigateFrame) {
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url::kAboutBlankURL));
  navigation_listener_.RunUntilUrlAndTitleEquals(GURL(url::kAboutBlankURL),
                                                 url::kAboutBlankURL);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigateDataFrame) {
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kDataUrl));
  navigation_listener_.RunUntilUrlAndTitleEquals(GURL(kDataUrl), kDataUrl);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, FrameDeletedBeforeContext) {
  fuchsia::web::FramePtr frame = CreateFrame();

  // Process the frame creation message.
  base::RunLoop().RunUntilIdle();

  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame);
  MockWebContentsObserver deletion_observer(frame_impl->web_contents());
  base::RunLoop run_loop;
  EXPECT_CALL(deletion_observer, RenderViewDeleted(_))
      .WillOnce(InvokeWithoutArgs([&run_loop] { run_loop.Quit(); }));

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url::kAboutBlankURL));

  frame.Unbind();
  run_loop.Run();

  // Check that |context| remains bound after the frame is closed.
  EXPECT_TRUE(context());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ContextDeletedBeforeFrame) {
  fuchsia::web::FramePtr frame = CreateFrame();
  EXPECT_TRUE(frame);

  base::RunLoop run_loop;
  frame.set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  context().Unbind();
  run_loop.Run();
  EXPECT_FALSE(frame);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ContextDeletedBeforeFrameWithView) {
  fuchsia::web::FramePtr frame = CreateFrame();
  EXPECT_TRUE(frame);

  auto view_tokens = scenic::NewViewTokenPair();

  frame->CreateView(std::move(view_tokens.first));
  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  frame.set_error_handler([&run_loop](zx_status_t status) {
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
    run_loop.Quit();
  });
  context().Unbind();
  run_loop.Run();
  EXPECT_FALSE(frame);
}

// TODO(https://crbug.com/695592): Remove this test when WebSQL is removed from
// Chrome.
IN_PROC_BROWSER_TEST_F(FrameImplTest, EnsureWebSqlDisabled) {
  fuchsia::web::FramePtr frame = CreateFrame();
  EXPECT_TRUE(frame);
  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title3(embedded_test_server()->GetURL(kPage3Path));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), title3.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(title3, kPage3Title);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, GoBackAndForward) {
  fuchsia::web::FramePtr frame = CreateFrame();
  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), title1.spec()));
  navigation_listener_.RunUntilUrlTitleBackForwardEquals(title1, kPage1Title,
                                                         false, false);

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), title2.spec()));
  navigation_listener_.RunUntilUrlTitleBackForwardEquals(title2, kPage2Title,
                                                         true, false);

  VerifyCanGoBackAndForward(controller.get(), true, false);
  controller->GoBack();
  navigation_listener_.RunUntilUrlTitleBackForwardEquals(title1, kPage1Title,
                                                         false, true);

  // At the top of the navigation entry list; this should be a no-op.
  VerifyCanGoBackAndForward(controller.get(), false, true);
  controller->GoBack();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();

  VerifyCanGoBackAndForward(controller.get(), false, true);
  controller->GoForward();
  navigation_listener_.RunUntilUrlTitleBackForwardEquals(title2, kPage2Title,
                                                         true, false);

  // At the end of the navigation entry list; this should be a no-op.
  VerifyCanGoBackAndForward(controller.get(), true, false);
  controller->GoForward();

  // Process the navigation request message.
  base::RunLoop().RunUntilIdle();
}

// An HTTP response stream whose response payload can be sent as "chunks"
// with indeterminate-length pauses in between.
class ChunkedHttpTransaction {
 public:
  ChunkedHttpTransaction(const net::test_server::SendBytesCallback& send,
                         const net::test_server::SendCompleteCallback& done)
      : io_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        send_callback_(send),
        done_callback_(done) {
    DCHECK(!current_instance_);
    DCHECK(send_callback_);
    DCHECK(done_callback_);

    current_instance_ = this;
  }

  static ChunkedHttpTransaction* current() {
    DCHECK(current_instance_);
    return current_instance_;
  }

  void Close() {
    EnsureSendCompleted();
    io_task_runner_->PostTask(FROM_HERE, done_callback_);
    delete this;
  }

  void EnsureSendCompleted() {
    if (send_callback_)
      return;

    base::RunLoop run_loop;
    send_chunk_complete_callback_ = run_loop.QuitClosure();
    run_loop.Run();
    DCHECK(send_callback_);
  }

  void SendChunk(std::string chunk) {
    EnsureSendCompleted();

    // Temporarily nullify |send_callback_| while the operation is inflight, to
    // guard against concurrent sends. The callback will be restored by
    // SendChunkComplete().
    net::test_server::SendBytesCallback inflight_send_callback = send_callback_;
    send_callback_ = {};

    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(inflight_send_callback, chunk,
                       base::BindRepeating(
                           &ChunkedHttpTransaction::SendChunkCompleteOnIoThread,
                           base::Unretained(this), inflight_send_callback,
                           base::ThreadTaskRunnerHandle::Get())));
  }

 private:
  static ChunkedHttpTransaction* current_instance_;

  ~ChunkedHttpTransaction() { current_instance_ = nullptr; }

  void SendChunkCompleteOnIoThread(
      net::test_server::SendBytesCallback send_callback,
      scoped_refptr<base::TaskRunner> ui_thread_task_runner) {
    ui_thread_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&ChunkedHttpTransaction::SendChunkCompleteOnUiThread,
                       base::Unretained(this), send_callback));
  }

  void SendChunkCompleteOnUiThread(
      net::test_server::SendBytesCallback send_callback) {
    send_callback_ = send_callback;
    if (send_chunk_complete_callback_)
      std::move(send_chunk_complete_callback_).Run();
  }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Set by callers to SendChunk() waiting for the previous chunk to complete.
  base::OnceClosure send_chunk_complete_callback_;

  // Callbacks are affine with |io_task_runner_|.
  net::test_server::SendBytesCallback send_callback_;
  net::test_server::SendCompleteCallback done_callback_;

  DISALLOW_COPY_AND_ASSIGN(ChunkedHttpTransaction);
};

ChunkedHttpTransaction* ChunkedHttpTransaction::current_instance_ = nullptr;

class ChunkedHttpTransactionFactory : public net::test_server::HttpResponse {
 public:
  ChunkedHttpTransactionFactory() = default;
  ~ChunkedHttpTransactionFactory() override = default;

  void SetOnResponseCreatedCallback(base::OnceClosure on_response_created) {
    on_response_created_ = std::move(on_response_created);
  }

  // net::test_server::HttpResponse implementation.
  void SendResponse(
      const net::test_server::SendBytesCallback& send,
      const net::test_server::SendCompleteCallback& done) override {
    // The ChunkedHttpTransaction manages its own lifetime.
    new ChunkedHttpTransaction(send, done);

    if (on_response_created_)
      std::move(on_response_created_).Run();
  }

 private:
  base::OnceClosure on_response_created_;

  DISALLOW_COPY_AND_ASSIGN(ChunkedHttpTransactionFactory);
};

IN_PROC_BROWSER_TEST_F(FrameImplTest, NavigationEventDuringPendingLoad) {
  fuchsia::web::FramePtr frame = CreateFrame();
  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
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

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL hung_url(embedded_test_server()->GetURL("/pausable"));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), hung_url.spec()));
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
  navigation_listener_.RunUntilNavigationStateMatches(state_change);

  transaction->SendChunk(
      "<script>document.title='final load';</script><body></body>");
  transaction->Close();
  state_change.set_title("final load");
  state_change.set_is_main_document_loaded(true);
  navigation_listener_.RunUntilNavigationStateMatches(state_change);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ReloadFrame) {
  fuchsia::web::FramePtr frame = CreateFrame();
  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &FrameImplTest::OnServeHttpRequest, base::Unretained(this)));

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  EXPECT_CALL(*this, OnServeHttpRequest(_)).Times(testing::AtLeast(1));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(url, kPage1Title);

  // Reload with NO_CACHE.
  {
    MockWebContentsObserver web_contents_observer(
        context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());
    base::RunLoop run_loop;
    EXPECT_CALL(web_contents_observer,
                DidStartNavigation(NavigationHandleUrlEquals(url)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->Reload(fuchsia::web::ReloadType::NO_CACHE);
    run_loop.Run();
  }

  // Reload with PARTIAL_CACHE.
  {
    MockWebContentsObserver web_contents_observer(
        context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());
    base::RunLoop run_loop;
    EXPECT_CALL(web_contents_observer,
                DidStartNavigation(NavigationHandleUrlEquals(url)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->Reload(fuchsia::web::ReloadType::PARTIAL_CACHE);
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, GetVisibleEntry) {
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Verify that a Frame returns an empty NavigationState prior to receiving any
  // LoadUrl() calls.
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::NavigationState> result(
        run_loop.QuitClosure());
    controller->GetVisibleEntry(
        cr_fuchsia::CallbackToFitFunction(result.GetReceiveCallback()));
    run_loop.Run();
    EXPECT_FALSE(result->has_title());
    EXPECT_FALSE(result->has_url());
    EXPECT_FALSE(result->has_page_type());
  }

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  // Navigate to a page.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), title1.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(title1, kPage1Title);

  // Verify that GetVisibleEntry() reflects the new Frame navigation state.
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::NavigationState> result(
        run_loop.QuitClosure());
    controller->GetVisibleEntry(
        cr_fuchsia::CallbackToFitFunction(result.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_TRUE(result->has_url());
    EXPECT_EQ(result->url(), title1.spec());
    ASSERT_TRUE(result->has_title());
    EXPECT_EQ(result->title(), kPage1Title);
    ASSERT_TRUE(result->has_page_type());
    EXPECT_EQ(result->page_type(), fuchsia::web::PageType::NORMAL);
  }

  // Navigate to another page.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), title2.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(title2, kPage2Title);

  // Verify the navigation with GetVisibleEntry().
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::NavigationState> result(
        run_loop.QuitClosure());
    controller->GetVisibleEntry(
        cr_fuchsia::CallbackToFitFunction(result.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_TRUE(result->has_url());
    EXPECT_EQ(result->url(), title2.spec());
    ASSERT_TRUE(result->has_title());
    EXPECT_EQ(result->title(), kPage2Title);
    ASSERT_TRUE(result->has_page_type());
    EXPECT_EQ(result->page_type(), fuchsia::web::PageType::NORMAL);
  }

  // Navigate back to the first page.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), title1.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(title1, kPage1Title);

  // Verify the navigation with GetVisibleEntry().
  {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::NavigationState> result(
        run_loop.QuitClosure());
    controller->GetVisibleEntry(
        cr_fuchsia::CallbackToFitFunction(result.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_TRUE(result->has_url());
    EXPECT_EQ(result->url(), title1.spec());
    ASSERT_TRUE(result->has_title());
    EXPECT_EQ(result->title(), kPage1Title);
    ASSERT_TRUE(result->has_page_type());
    EXPECT_EQ(result->page_type(), fuchsia::web::PageType::NORMAL);
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, NoNavigationObserverAttached) {
  fuchsia::web::FramePtr frame = WebEngineBrowserTest::CreateFrame(nullptr);
  base::RunLoop().RunUntilIdle();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  MockWebContentsObserver observer(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, DidStartNavigation(NavigationHandleUrlEquals(title1)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        controller.get(), fuchsia::web::LoadUrlParams(), title1.spec()));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(observer, DidStartNavigation(NavigationHandleUrlEquals(title2)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        controller.get(), fuchsia::web::LoadUrlParams(), title2.spec()));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScript) {
  constexpr int64_t kBindingsId = 1234;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  fuchsia::web::FramePtr frame = CreateFrame();

  frame->AddBeforeLoadJavaScript(
      kBindingsId, {url.GetOrigin().spec()},
      cr_fuchsia::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener_.RunUntilUrlEquals(url);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScriptUpdated) {
  constexpr int64_t kBindingsId = 1234;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  fuchsia::web::FramePtr frame = CreateFrame();

  frame->AddBeforeLoadJavaScript(
      kBindingsId, {url.GetOrigin().spec()},
      cr_fuchsia::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Verify that this script clobbers the previous script, as opposed to being
  // injected alongside it. (The latter would result in the title being
  // "helloclobber").
  frame->AddBeforeLoadJavaScript(
      kBindingsId, {url.GetOrigin().spec()},
      cr_fuchsia::MemBufferFromString(
          "stashed_title = document.title + 'clobber';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(url, "clobber");
}

// Verifies that bindings are injected in order by producing a cumulative,
// non-commutative result.
IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScriptOrdered) {
  constexpr int64_t kBindingsId1 = 1234;
  constexpr int64_t kBindingsId2 = 5678;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  fuchsia::web::FramePtr frame = CreateFrame();

  frame->AddBeforeLoadJavaScript(
      kBindingsId1, {url.GetOrigin().spec()},
      cr_fuchsia::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });
  frame->AddBeforeLoadJavaScript(
      kBindingsId2, {url.GetOrigin().spec()},
      cr_fuchsia::MemBufferFromString("stashed_title += ' there';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(url, "hello there");
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScriptRemoved) {
  constexpr int64_t kBindingsId1 = 1234;
  constexpr int64_t kBindingsId2 = 5678;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  fuchsia::web::FramePtr frame = CreateFrame();

  frame->AddBeforeLoadJavaScript(
      kBindingsId1, {url.GetOrigin().spec()},
      cr_fuchsia::MemBufferFromString("stashed_title = 'foo';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Add a script which clobbers "foo".
  frame->AddBeforeLoadJavaScript(
      kBindingsId2, {url.GetOrigin().spec()},
      cr_fuchsia::MemBufferFromString("stashed_title = 'bar';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Deletes the clobbering script.
  frame->RemoveBeforeLoadJavaScript(kBindingsId2);

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(url, "foo");
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScriptRemoveInvalidId) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kPage1Path));
  fuchsia::web::FramePtr frame = CreateFrame();

  frame->RemoveBeforeLoadJavaScript(kOnLoadScriptId);

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(url, kPage1Title);
}

// Test JS injection using ExecuteJavaScriptNoResult() to set a value, and
// ExecuteJavaScript() to retrieve that value.
IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavaScript) {
  constexpr char kJsonStringLiteral[] = "\"I am a literal, literally\"";
  fuchsia::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL kUrl(embedded_test_server()->GetURL(kPage1Path));

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Navigate to a page and wait for it to finish loading.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), kUrl.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(kUrl, kPage1Title);

  // Execute with no result to set the variable.
  frame->ExecuteJavaScriptNoResult(
      {kUrl.GetOrigin().spec()},
      cr_fuchsia::MemBufferFromString(
          base::StringPrintf("my_variable = %s;", kJsonStringLiteral), "test"),
      [](fuchsia::web::Frame_ExecuteJavaScriptNoResult_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Execute a script snippet to return the variable's value.
  base::RunLoop loop;
  frame->ExecuteJavaScript(
      {kUrl.GetOrigin().spec()},
      cr_fuchsia::MemBufferFromString("my_variable;", "test"),
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
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  fuchsia::web::FramePtr frame = CreateFrame();

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId, {url.GetOrigin().spec()},
      cr_fuchsia::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(url, "hello");
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScriptWrongOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  fuchsia::web::FramePtr frame = CreateFrame();

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId, {"http://example.com"},
      cr_fuchsia::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Expect that the original HTML title is used, because we didn't inject a
  // script with a replacement title.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(
      url, "Welcome to Stan the Offline Dino's Homepage");
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, BeforeLoadScriptWildcardOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  fuchsia::web::FramePtr frame = CreateFrame();

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId, {"*"},
      cr_fuchsia::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Test script injection for the origin 127.0.0.1.
  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(url, "hello");

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url::kAboutBlankURL));
  navigation_listener_.RunUntilUrlEquals(GURL(url::kAboutBlankURL));

  // Test script injection using a different origin ("localhost"), which should
  // still be picked up by the wildcard.
  GURL alt_url = embedded_test_server()->GetURL("localhost", kDynamicTitlePath);
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), alt_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(alt_url, "hello");
}

// Test that we can inject scripts before and after RenderFrame creation.
IN_PROC_BROWSER_TEST_F(FrameImplTest,
                       BeforeLoadScriptEarlyAndLateRegistrations) {
  constexpr int64_t kOnLoadScriptId2 = kOnLoadScriptId + 1;

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kDynamicTitlePath));
  fuchsia::web::FramePtr frame = CreateFrame();

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId, {url.GetOrigin().spec()},
      cr_fuchsia::MemBufferFromString("stashed_title = 'hello';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(url, "hello");

  frame->AddBeforeLoadJavaScript(
      kOnLoadScriptId2, {url.GetOrigin().spec()},
      cr_fuchsia::MemBufferFromString("stashed_title += ' there';", "test"),
      [](fuchsia::web::Frame_AddBeforeLoadJavaScript_Result result) {
        EXPECT_TRUE(result.is_response());
      });

  // Navigate away to clean the slate.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url::kAboutBlankURL));
  navigation_listener_.RunUntilUrlEquals(GURL(url::kAboutBlankURL));

  // Navigate back and see if both scripts are working.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(url, "hello there");
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, ExecuteJavaScriptBadEncoding) {
  fuchsia::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kPage1Path));

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(url, kPage1Title);

  base::RunLoop run_loop;

  // 0xFE is an illegal UTF-8 byte; it should cause UTF-8 conversion to fail.
  frame->ExecuteJavaScriptNoResult(
      {url.GetOrigin().spec()},
      cr_fuchsia::MemBufferFromString("true;\xfe", "test"),
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
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  MockWebContentsObserver web_contents_observer(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());
  EXPECT_CALL(web_contents_observer,
              DidStartNavigation(NavigationHandleUrlEquals(title1)));

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), title1.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(title1, kPage1Title);

  // Disconnect the listener & spin the runloop to propagate the disconnection
  // event over IPC.
  navigation_listener_bindings().CloseAll();
  base::RunLoop().RunUntilIdle();

  base::RunLoop run_loop;
  EXPECT_CALL(web_contents_observer,
              DidStartNavigation(NavigationHandleUrlEquals(title2)))
      .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), title2.spec()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, DelayedNavigationEventAck) {
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL title1(embedded_test_server()->GetURL(kPage1Path));
  GURL title2(embedded_test_server()->GetURL(kPage2Path));

  // Expect an navigation event here, but deliberately postpone acknowledgement
  // until the end of the test.
  OnNavigationStateChangedCallback captured_ack_cb;
  navigation_listener_.SetBeforeAckHook(base::BindRepeating(
      [](OnNavigationStateChangedCallback* dest_cb,
         const fuchsia::web::NavigationState& state,
         OnNavigationStateChangedCallback cb) { *dest_cb = std::move(cb); },
      base::Unretained(&captured_ack_cb)));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), title1.spec()));
  fuchsia::web::NavigationState expected_state;
  expected_state.set_url(title1.spec());
  navigation_listener_.RunUntilNavigationStateMatches(expected_state);
  EXPECT_TRUE(captured_ack_cb);
  navigation_listener_.SetBeforeAckHook({});

  // Navigate to a second page.
  {
    // Since we have blocked NavigationEventObserver's flow, we must observe the
    // lower level browser navigation events directly from the WebContents.
    MockWebContentsObserver web_contents_observer(
        context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());

    base::RunLoop run_loop;
    EXPECT_CALL(web_contents_observer,
                DidStartNavigation(NavigationHandleUrlEquals(title2)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        controller.get(), fuchsia::web::LoadUrlParams(), title2.spec()));
    run_loop.Run();
  }

  // Navigate to the first page.
  {
    MockWebContentsObserver web_contents_observer(
        context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());
    base::RunLoop run_loop;
    EXPECT_CALL(web_contents_observer,
                DidStartNavigation(NavigationHandleUrlEquals(title1)))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        controller.get(), fuchsia::web::LoadUrlParams(), title1.spec()));
    run_loop.Run();
  }

  // Since there was no observable change in navigation state since the last
  // ack, there should be no more NavigationEvents generated.
  captured_ack_cb();
  navigation_listener_.RunUntilUrlAndTitleEquals(title1, kPage1Title);
}

// Observes events specific to the Stop() test case.
struct WebContentsObserverForStop : public content::WebContentsObserver {
  using content::WebContentsObserver::Observe;
  MOCK_METHOD1(DidStartNavigation, void(content::NavigationHandle*));
  MOCK_METHOD0(NavigationStopped, void());
};

IN_PROC_BROWSER_TEST_F(FrameImplTest, Stop) {
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  ASSERT_TRUE(embedded_test_server()->Start());

  // Use a request handler that will accept the connection and stall
  // indefinitely.
  GURL hung_url(embedded_test_server()->GetURL("/hung"));

  {
    base::RunLoop run_loop;
    WebContentsObserverForStop observer;
    observer.Observe(
        context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());
    EXPECT_CALL(observer, DidStartNavigation(_))
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->LoadUrl(
        hung_url.spec(), fuchsia::web::LoadUrlParams(),
        [](fuchsia::web::NavigationController_LoadUrl_Result) {});
    run_loop.Run();
  }

  EXPECT_TRUE(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_->IsLoading());

  {
    base::RunLoop run_loop;
    WebContentsObserverForStop observer;
    observer.Observe(
        context_impl()->GetFrameImplForTest(&frame)->web_contents_.get());
    EXPECT_CALL(observer, NavigationStopped())
        .WillOnce(InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));
    controller->Stop();
    run_loop.Run();
  }

  EXPECT_FALSE(
      context_impl()->GetFrameImplForTest(&frame)->web_contents_->IsLoading());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessage) {
  fuchsia::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(
      embedded_test_server()->GetURL("/window_post_message.html"));

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(),
      post_message_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(post_message_url,
                                                 "postmessage");

  fuchsia::web::WebMessage message;
  message.set_data(cr_fuchsia::MemBufferFromString(kPage1Path, "test"));
  cr_fuchsia::ResultReceiver<fuchsia::web::Frame_PostMessage_Result>
      post_result;
  frame->PostMessage(
      post_message_url.GetOrigin().spec(), std::move(message),
      cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));

  navigation_listener_.RunUntilUrlAndTitleEquals(
      embedded_test_server()->GetURL(kPage1Path), kPage1Title);

  EXPECT_TRUE(post_result->is_response());
}

// Send a MessagePort to the content, then perform bidirectional messaging
// through the port.
IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessagePassMessagePort) {
  fuchsia::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(),
      post_message_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(post_message_url,
                                                 "messageport");

  fuchsia::web::MessagePortPtr message_port;
  fuchsia::web::WebMessage msg;
  {
    fuchsia::web::OutgoingTransferable outgoing;
    outgoing.set_message_port(message_port.NewRequest());
    std::vector<fuchsia::web::OutgoingTransferable> outgoing_vector;
    outgoing_vector.push_back(std::move(outgoing));
    msg.set_outgoing_transfer(std::move(outgoing_vector));
    msg.set_data(cr_fuchsia::MemBufferFromString("hi", "test"));
    cr_fuchsia::ResultReceiver<fuchsia::web::Frame_PostMessage_Result>
        post_result;
    frame->PostMessage(
        post_message_url.GetOrigin().spec(), std::move(msg),
        cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));

    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> receiver(
        run_loop.QuitClosure());
    message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_TRUE(receiver->has_data());
    EXPECT_EQ("got_port", StringFromMemBufferOrDie(receiver->data()));
  }

  {
    msg.set_data(cr_fuchsia::MemBufferFromString("ping", "test"));
    cr_fuchsia::ResultReceiver<fuchsia::web::MessagePort_PostMessage_Result>
        post_result;
    message_port->PostMessage(
        std::move(msg),
        cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> receiver(
        run_loop.QuitClosure());
    message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_TRUE(receiver->has_data());
    EXPECT_EQ("ack ping", StringFromMemBufferOrDie(receiver->data()));
    EXPECT_TRUE(post_result->is_response());
  }
}

// Send a MessagePort to the content, then perform bidirectional messaging
// over its channel.
IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessageMessagePortDisconnected) {
  fuchsia::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(),
      post_message_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(post_message_url,
                                                 "messageport");

  fuchsia::web::MessagePortPtr message_port;
  fuchsia::web::WebMessage msg;
  {
    fuchsia::web::OutgoingTransferable outgoing;
    outgoing.set_message_port(message_port.NewRequest());
    std::vector<fuchsia::web::OutgoingTransferable> outgoing_vector;
    outgoing_vector.push_back(std::move(outgoing));
    msg.set_outgoing_transfer(std::move(outgoing_vector));
    msg.set_data(cr_fuchsia::MemBufferFromString("hi", "test"));
    cr_fuchsia::ResultReceiver<fuchsia::web::Frame_PostMessage_Result>
        post_result;
    frame->PostMessage(
        post_message_url.GetOrigin().spec(), std::move(msg),
        cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));

    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> receiver(
        run_loop.QuitClosure());
    message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_TRUE(receiver->has_data());
    EXPECT_EQ("got_port", StringFromMemBufferOrDie(receiver->data()));
    EXPECT_TRUE(post_result->is_response());
  }

  // Navigating off-page should tear down the Mojo channel, thereby causing the
  // MessagePortImpl to self-destruct and tear down its FIDL channel.
  {
    base::RunLoop run_loop;
    message_port.set_error_handler(
        [&run_loop](zx_status_t) { run_loop.Quit(); });
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        controller.get(), fuchsia::web::LoadUrlParams(), url::kAboutBlankURL));
    run_loop.Run();
  }
}

// Send a MessagePort to the content, and through that channel, receive a
// different MessagePort that was created by the content. Verify the second
// channel's liveness by sending a ping to it.
IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessageUseContentProvidedPort) {
  fuchsia::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(),
      post_message_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(post_message_url,
                                                 "messageport");

  fuchsia::web::MessagePortPtr incoming_message_port;
  fuchsia::web::WebMessage msg;
  {
    fuchsia::web::MessagePortPtr message_port;
    fuchsia::web::OutgoingTransferable outgoing;
    outgoing.set_message_port(message_port.NewRequest());
    std::vector<fuchsia::web::OutgoingTransferable> outgoing_vector;
    outgoing_vector.push_back(std::move(outgoing));
    msg.set_outgoing_transfer(std::move(outgoing_vector));
    msg.set_data(cr_fuchsia::MemBufferFromString("hi", "test"));
    cr_fuchsia::ResultReceiver<fuchsia::web::Frame_PostMessage_Result>
        post_result;
    frame->PostMessage(
        "*", std::move(msg),
        cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));

    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> receiver(
        run_loop.QuitClosure());
    message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_TRUE(receiver->has_data());
    EXPECT_EQ("got_port", StringFromMemBufferOrDie(receiver->data()));
    ASSERT_TRUE(receiver->has_incoming_transfer());
    ASSERT_EQ(receiver->incoming_transfer().size(), 1u);
    incoming_message_port =
        receiver->mutable_incoming_transfer()->at(0).message_port().Bind();
    EXPECT_TRUE(post_result->is_response());
  }

  // Get the content to send three 'ack ping' messages, which will accumulate in
  // the MessagePortImpl buffer.
  for (int i = 0; i < 3; ++i) {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::MessagePort_PostMessage_Result>
        post_result(run_loop.QuitClosure());
    msg.set_data(cr_fuchsia::MemBufferFromString("ping", "test"));
    incoming_message_port->PostMessage(
        std::move(msg),
        cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));
    run_loop.Run();
    EXPECT_TRUE(post_result->is_response());
  }

  // Receive another acknowledgement from content on a side channel to ensure
  // that all the "ack pings" are ready to be consumed.
  {
    fuchsia::web::MessagePortPtr ack_message_port;
    fuchsia::web::WebMessage msg;
    fuchsia::web::OutgoingTransferable outgoing;
    outgoing.set_message_port(ack_message_port.NewRequest());
    std::vector<fuchsia::web::OutgoingTransferable> outgoing_vector;
    outgoing_vector.push_back(std::move(outgoing));
    msg.set_outgoing_transfer(std::move(outgoing_vector));
    msg.set_data(cr_fuchsia::MemBufferFromString("hi", "test"));

    // Quit the runloop only after we've received a WebMessage AND a PostMessage
    // result.
    cr_fuchsia::ResultReceiver<fuchsia::web::Frame_PostMessage_Result>
        post_result;
    frame->PostMessage(
        "*", std::move(msg),
        cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> receiver(
        run_loop.QuitClosure());
    ack_message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_TRUE(receiver->has_data());
    EXPECT_EQ("got_port", StringFromMemBufferOrDie(receiver->data()));
    EXPECT_TRUE(post_result->is_response());
  }

  // Pull the three 'ack ping's from the buffer.
  for (int i = 0; i < 3; ++i) {
    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> receiver(
        run_loop.QuitClosure());
    incoming_message_port->ReceiveMessage(
        cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
    run_loop.Run();
    ASSERT_TRUE(receiver->has_data());
    EXPECT_EQ("ack ping", StringFromMemBufferOrDie(receiver->data()));
  }
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, PostMessageBadOriginDropped) {
  fuchsia::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL post_message_url(embedded_test_server()->GetURL("/message_port.html"));

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(),
      post_message_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(post_message_url,
                                                 "messageport");

  fuchsia::web::MessagePortPtr bad_origin_incoming_message_port;
  fuchsia::web::WebMessage msg;

  // PostMessage() to invalid origins should be ignored. We pass in a
  // MessagePort but nothing should happen to it.
  fuchsia::web::MessagePortPtr unused_message_port;
  fuchsia::web::OutgoingTransferable unused_outgoing;
  unused_outgoing.set_message_port(unused_message_port.NewRequest());
  std::vector<fuchsia::web::OutgoingTransferable> unused_outgoing_vector;
  unused_outgoing_vector.push_back(std::move(unused_outgoing));
  msg.set_outgoing_transfer(std::move(unused_outgoing_vector));
  msg.set_data(cr_fuchsia::MemBufferFromString("bad origin, bad!", "test"));

  cr_fuchsia::ResultReceiver<fuchsia::web::Frame_PostMessage_Result>
      unused_post_result;
  frame->PostMessage("https://example.com", std::move(msg),
                     cr_fuchsia::CallbackToFitFunction(
                         unused_post_result.GetReceiveCallback()));
  cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> unused_message_read;
  bad_origin_incoming_message_port->ReceiveMessage(
      cr_fuchsia::CallbackToFitFunction(
          unused_message_read.GetReceiveCallback()));

  // PostMessage() with a valid origin should succeed.
  // Verify it by looking for an ack message on the MessagePort we passed in.
  // Since message events are handled in order, observing the result of this
  // operation will verify whether the previous PostMessage() was received but
  // discarded.
  fuchsia::web::MessagePortPtr incoming_message_port;
  fuchsia::web::MessagePortPtr message_port;
  fuchsia::web::OutgoingTransferable outgoing;
  outgoing.set_message_port(message_port.NewRequest());
  std::vector<fuchsia::web::OutgoingTransferable> outgoing_vector;
  outgoing_vector.push_back(std::move(outgoing));
  msg.set_outgoing_transfer(std::move(outgoing_vector));
  msg.set_data(cr_fuchsia::MemBufferFromString("good origin", "test"));

  cr_fuchsia::ResultReceiver<fuchsia::web::Frame_PostMessage_Result>
      post_result;
  frame->PostMessage(
      "*", std::move(msg),
      cr_fuchsia::CallbackToFitFunction(post_result.GetReceiveCallback()));
  base::RunLoop run_loop;
  cr_fuchsia::ResultReceiver<fuchsia::web::WebMessage> receiver(
      run_loop.QuitClosure());
  message_port->ReceiveMessage(
      cr_fuchsia::CallbackToFitFunction(receiver.GetReceiveCallback()));
  run_loop.Run();
  ASSERT_TRUE(receiver->has_data());
  EXPECT_EQ("got_port", StringFromMemBufferOrDie(receiver->data()));
  ASSERT_TRUE(receiver->has_incoming_transfer());
  ASSERT_EQ(receiver->incoming_transfer().size(), 1u);
  incoming_message_port =
      receiver->mutable_incoming_transfer()->at(0).message_port().Bind();
  EXPECT_TRUE(post_result->is_response());

  // Verify that the first PostMessage() call wasn't handled.
  EXPECT_FALSE(unused_message_read.has_value());
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, RecreateView) {
  fuchsia::web::FramePtr frame = CreateFrame();

  ASSERT_TRUE(embedded_test_server()->Start());

  // Process the Frame creation request, and verify we can get the FrameImpl.
  base::RunLoop().RunUntilIdle();
  FrameImpl* frame_impl = context_impl()->GetFrameImplForTest(&frame);
  ASSERT_TRUE(frame_impl);
  EXPECT_FALSE(frame_impl->has_view_for_test());

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Verify that the Frame can navigate, prior to the View being created.
  const GURL page1_url(embedded_test_server()->GetURL(kPage1Path));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), page1_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(page1_url, kPage1Title);

  // Request a View from the Frame, and pump the loop to process the request.
  zx::eventpair owner_token, frame_token;
  ASSERT_EQ(zx::eventpair::create(0, &owner_token, &frame_token), ZX_OK);
  fuchsia::ui::views::ViewToken view_token;
  view_token.value = std::move(frame_token);
  frame->CreateView(std::move(view_token));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(frame_impl->has_view_for_test());

  // Verify that the Frame still works, by navigating to Page #2.
  const GURL page2_url(embedded_test_server()->GetURL(kPage2Path));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), page2_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(page2_url, kPage2Title);

  // Create new View tokens and request a new view.
  zx::eventpair owner_token2, frame_token2;
  ASSERT_EQ(zx::eventpair::create(0, &owner_token2, &frame_token2), ZX_OK);
  fuchsia::ui::views::ViewToken view_token2;
  view_token2.value = std::move(frame_token2);
  frame->CreateView(std::move(view_token2));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(frame_impl->has_view_for_test());

  // Verify that the Frame still works, by navigating back to Page #1.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), page1_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(page1_url, kPage1Title);
}

// Tests SetNavigationEventListener() immediately returns a NavigationEvent,
// even in the absence of a new navigation.
IN_PROC_BROWSER_TEST_F(FrameImplTest, ImmediateNavigationEvent) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL page_url(embedded_test_server()->GetURL(kPage1Path));

  // The first NavigationState received should be empty.
  base::RunLoop run_loop;
  navigation_listener_.SetBeforeAckHook(base::BindRepeating(
      [](base::RunLoop* run_loop, const fuchsia::web::NavigationState& change,
         OnNavigationStateChangedCallback callback) {
        EXPECT_TRUE(change.IsEmpty());
        run_loop->Quit();
        callback();
      },
      base::Unretained(&run_loop)));
  fuchsia::web::FramePtr frame = CreateFrame();
  run_loop.Run();
  navigation_listener_.SetBeforeAckHook({});

  // Navigate to a page and wait for the navigation to complete.
  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), page_url.spec()));
  navigation_listener_.RunUntilUrlEquals(page_url);

  // Attach a new navigation listener, we should get the new page state, even if
  // no new navigation occurred.
  cr_fuchsia::TestNavigationListener navigation_listener2;
  fidl::Binding<fuchsia::web::NavigationEventListener>
      navigation_listener_binding(&navigation_listener2);
  frame->SetNavigationEventListener(navigation_listener_binding.NewBinding());
  navigation_listener2.RunUntilUrlAndTitleEquals(page_url, kPage1Title);
}

// Check loading an invalid URL in NavigationController.LoadUrl() sets the right
// error.
IN_PROC_BROWSER_TEST_F(FrameImplTest, InvalidUrl) {
  fuchsia::web::FramePtr frame = CreateFrame();
  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  base::RunLoop run_loop;
  cr_fuchsia::ResultReceiver<fuchsia::web::NavigationController_LoadUrl_Result>
      result(run_loop.QuitClosure());
  controller->LoadUrl(
      "http:google.com:foo", fuchsia::web::LoadUrlParams(),
      cr_fuchsia::CallbackToFitFunction(result.GetReceiveCallback()));
  run_loop.Run();

  ASSERT_TRUE(result->is_err());
  EXPECT_EQ(result->err(),
            fuchsia::web::NavigationControllerError::INVALID_URL);
}

// Check setting invalid headers in NavigationController.LoadUrl() sets the
// right error.
IN_PROC_BROWSER_TEST_F(FrameImplTest, InvalidHeader) {
  fuchsia::web::FramePtr frame = CreateFrame();
  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  {
    // Set an invalid header name.
    fuchsia::web::LoadUrlParams load_url_params;
    fuchsia::net::http::Header header;
    header.name = cr_fuchsia::StringToBytes("Invalid:Header");
    header.value = cr_fuchsia::StringToBytes("1");
    load_url_params.set_headers({header});

    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<
        fuchsia::web::NavigationController_LoadUrl_Result>
        result(run_loop.QuitClosure());
    controller->LoadUrl(
        "http://site.ext/", std::move(load_url_params),
        cr_fuchsia::CallbackToFitFunction(result.GetReceiveCallback()));
    run_loop.Run();

    ASSERT_TRUE(result->is_err());
    EXPECT_EQ(result->err(),
              fuchsia::web::NavigationControllerError::INVALID_HEADER);
  }

  {
    // Set an invalid header value.
    fuchsia::web::LoadUrlParams load_url_params;
    fuchsia::net::http::Header header;
    header.name = cr_fuchsia::StringToBytes("Header");
    header.value = cr_fuchsia::StringToBytes("Invalid\rValue");
    load_url_params.set_headers({header});

    base::RunLoop run_loop;
    cr_fuchsia::ResultReceiver<
        fuchsia::web::NavigationController_LoadUrl_Result>
        result(run_loop.QuitClosure());
    controller->LoadUrl(
        "http://site.ext/", std::move(load_url_params),
        cr_fuchsia::CallbackToFitFunction(result.GetReceiveCallback()));
    run_loop.Run();

    ASSERT_TRUE(result->is_err());
    EXPECT_EQ(result->err(),
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

    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDown() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
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

  DISALLOW_COPY_AND_ASSIGN(RequestMonitoringFrameImplBrowserTest);
};

IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest, ExtraHeaders) {
  fuchsia::web::FramePtr frame = CreateFrame();

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

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
      controller.get(), std::move(load_url_params), page_url.spec()));
  navigation_listener_.RunUntilUrlAndTitleEquals(page_url, kPage1Title);

  // At this point, the page should be loaded, the server should have received
  // the request and the request should be in the map.
  const auto iter = accumulated_requests_.find(page_url);
  ASSERT_NE(iter, accumulated_requests_.end());
  EXPECT_THAT(iter->second.headers,
              testing::Contains(testing::Key("X-ExtraHeaders")));
  EXPECT_THAT(iter->second.headers,
              testing::Contains(testing::Key("X-2ExtraHeaders")));
}

// Tests the URLRequestRewrite API properly adds headers on every requests.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteAddHeaders) {
  fuchsia::web::FramePtr frame = CreateFrame();

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test", "Value"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Navigate, we should get the additional header on the main request and the
  // image request.
  const GURL page_url(embedded_test_server()->GetURL(kPage4Path));
  const GURL img_url(embedded_test_server()->GetURL(kPage4ImgPath));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), page_url.spec()));
  navigation_listener_.RunUntilUrlEquals(page_url);

  {
    const auto iter = accumulated_requests_.find(page_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
  }
  {
    const auto iter = accumulated_requests_.find(img_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test")));
  }
}

// Tests the URLRequestRewrite API properly removes headers on every requests.
// Also tests that rewrites are applied properly in succession.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteRemoveHeader) {
  fuchsia::web::FramePtr frame = CreateFrame();

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test", "Value"));
  rewrites.push_back(
      cr_fuchsia::CreateRewriteRemoveHeader(base::nullopt, "Test"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Navigate, we should get no "Test" header.
  const GURL page_url(embedded_test_server()->GetURL(kPage4Path));
  const GURL img_url(embedded_test_server()->GetURL(kPage4ImgPath));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), page_url.spec()));
  navigation_listener_.RunUntilUrlEquals(page_url);

  {
    const auto iter = accumulated_requests_.find(page_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers,
                testing::Not(testing::Contains(testing::Key("Test"))));
  }
  {
    const auto iter = accumulated_requests_.find(img_url);
    ASSERT_NE(iter, accumulated_requests_.end());
    EXPECT_THAT(iter->second.headers,
                testing::Not(testing::Contains(testing::Key("Test"))));
  }
}

// Tests the URLRequestRewrite API properly removes headers, based on the
// presence of a string in the query.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteRemoveHeaderWithQuery) {
  fuchsia::web::FramePtr frame = CreateFrame();

  const GURL page_url(embedded_test_server()->GetURL("/page?stuff=[pattern]"));

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(cr_fuchsia::CreateRewriteAddHeaders("Test", "Value"));
  rewrites.push_back(cr_fuchsia::CreateRewriteRemoveHeader(
      base::make_optional("[pattern]"), "Test"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Navigate, we should get no "Test" header.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), page_url.spec()));
  navigation_listener_.RunUntilUrlEquals(page_url);

  const auto iter = accumulated_requests_.find(page_url);
  ASSERT_NE(iter, accumulated_requests_.end());
  EXPECT_THAT(iter->second.headers,
              testing::Not(testing::Contains(testing::Key("Test"))));
}

// Tests the URLRequestRewrite API properly handles query substitution.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteSubstituteQueryPattern) {
  fuchsia::web::FramePtr frame = CreateFrame();

  std::vector<fuchsia::web::UrlRequestRewrite> rewrites;
  rewrites.push_back(cr_fuchsia::CreateRewriteSubstituteQueryPattern(
      "[pattern]", "substitution"));
  fuchsia::web::UrlRequestRewriteRule rule;
  rule.set_rewrites(std::move(rewrites));
  std::vector<fuchsia::web::UrlRequestRewriteRule> rules;
  rules.push_back(std::move(rule));
  frame->SetUrlRequestRewriteRules(std::move(rules), []() {});

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Navigate, we should get to the URL with the modified request.
  const GURL page_url(embedded_test_server()->GetURL("/page?[pattern]"));
  const GURL final_url(embedded_test_server()->GetURL("/page?substitution"));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), page_url.spec()));
  navigation_listener_.RunUntilUrlEquals(final_url);

  EXPECT_THAT(accumulated_requests_,
              testing::Contains(testing::Key(final_url)));
}

// Tests the URLRequestRewrite API properly handles URL replacement.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteReplaceUrl) {
  fuchsia::web::FramePtr frame = CreateFrame();

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

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Navigate, we should get to the replaced URL.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), page_url.spec()));
  navigation_listener_.RunUntilUrlEquals(final_url);

  EXPECT_THAT(accumulated_requests_,
              testing::Contains(testing::Key(final_url)));
}

// Tests the URLRequestRewrite API properly handles URL replacement when the
// original request URL contains a query and a fragment string.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteReplaceUrlQueryRef) {
  fuchsia::web::FramePtr frame = CreateFrame();

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

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Navigate, we should get to the replaced URL.
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), page_url.spec()));
  navigation_listener_.RunUntilUrlEquals(final_url_with_ref);

  EXPECT_THAT(accumulated_requests_,
              testing::Contains(testing::Key(final_url)));
}

// Tests the URLRequestRewrite API properly handles scheme and host filtering in
// rules.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteSchemeHostFilter) {
  fuchsia::web::FramePtr frame = CreateFrame();

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

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());

  // Navigate, we should get the "Test1" and "Test3" headers, but not "Test2"
  // and "Test4".
  const GURL page_url(embedded_test_server()->GetURL("/default"));
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
      controller.get(), fuchsia::web::LoadUrlParams(), page_url.spec()));
  navigation_listener_.RunUntilUrlEquals(page_url);

  const auto iter = accumulated_requests_.find(page_url);
  ASSERT_NE(iter, accumulated_requests_.end());
  EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test1")));
  EXPECT_THAT(iter->second.headers, testing::Contains(testing::Key("Test3")));
  EXPECT_THAT(iter->second.headers,
              testing::Not(testing::Contains(testing::Key("Test2"))));
  EXPECT_THAT(iter->second.headers,
              testing::Not(testing::Contains(testing::Key("Test4"))));
}

// Tests the URLRequestRewrite API properly closes the Frame channel if the
// rules are invalid.
IN_PROC_BROWSER_TEST_F(RequestMonitoringFrameImplBrowserTest,
                       UrlRequestRewriteInvalidRules) {
  fuchsia::web::FramePtr frame = CreateFrame();
  base::RunLoop run_loop;
  frame.set_error_handler([&run_loop](zx_status_t status) {
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

IN_PROC_BROWSER_TEST_F(FrameImplTest, PopupWindow) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL popup_url(embedded_test_server()->GetURL(kPopupPath));
  GURL popup_child_url(embedded_test_server()->GetURL(kPopupRedirectPath));
  GURL title1_url(embedded_test_server()->GetURL(kPage1Path));
  fuchsia::web::FramePtr frame = CreateFrame();

  TestPopupListener popup_listener;
  fidl::Binding<fuchsia::web::PopupFrameCreationListener>
      popup_listener_binding(&popup_listener);
  frame->SetPopupFrameCreationListener(popup_listener_binding.NewBinding());

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(controller.get(), {},
                                                   popup_url.spec()));

  // Verify the popup's initial URL, "popup_child.html".
  fuchsia::web::FramePtr popup_frame;
  fuchsia::web::PopupFrameCreationInfo popup_info;
  popup_listener.GetAndAckNextPopup(&popup_frame, &popup_info);
  EXPECT_EQ(popup_info.initial_url(), popup_child_url);

  // Verify that the popup eventually redirects to "title1.html".
  cr_fuchsia::TestNavigationListener popup_nav_listener;
  fidl::Binding<fuchsia::web::NavigationEventListener>
      popup_nav_listener_binding(&popup_nav_listener);
  popup_frame->SetNavigationEventListener(
      popup_nav_listener_binding.NewBinding());
  popup_nav_listener.RunUntilUrlAndTitleEquals(title1_url, kPage1Title);
}

IN_PROC_BROWSER_TEST_F(FrameImplTest, MultiplePopups) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL popup_url(embedded_test_server()->GetURL(kPopupMultiplePath));
  GURL title1_url(embedded_test_server()->GetURL(kPage1Path));
  GURL title2_url(embedded_test_server()->GetURL(kPage2Path));
  fuchsia::web::FramePtr frame = CreateFrame();

  TestPopupListener popup_listener;
  fidl::Binding<fuchsia::web::PopupFrameCreationListener>
      popup_listener_binding(&popup_listener);
  frame->SetPopupFrameCreationListener(popup_listener_binding.NewBinding());

  fuchsia::web::NavigationControllerPtr controller;
  frame->GetNavigationController(controller.NewRequest());
  EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(controller.get(), {},
                                                   popup_url.spec()));

  fuchsia::web::FramePtr popup_frame;
  fuchsia::web::PopupFrameCreationInfo popup_info;
  popup_listener.GetAndAckNextPopup(&popup_frame, &popup_info);
  EXPECT_EQ(popup_info.initial_url(), title1_url);

  popup_listener.GetAndAckNextPopup(&popup_frame, &popup_info);
  EXPECT_EQ(popup_info.initial_url(), title2_url);
}
