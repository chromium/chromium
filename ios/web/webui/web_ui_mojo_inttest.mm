// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>
#import <string>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/web/grit/ios_web_resources.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/web_ui_ios_controller.h"
#import "ios/web/public/webui/web_ui_ios_controller_factory.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "ios/web/test/grit/test_resources.h"
#import "ios/web/test/mojo_test.mojom.h"
#import "ios/web/test/test_url_constants.h"
#import "ios/web/test/web_int_test.h"
#import "mojo/public/cpp/bindings/pending_remote.h"
#import "mojo/public/cpp/bindings/receiver_set.h"
#import "mojo/public/cpp/bindings/remote.h"
#import "url/gurl.h"
#import "url/scheme_host_port.h"

namespace web {

namespace {

// Hostname for test WebUI page.
const char kTestWebUIURLHost[] = "testwebui";

// Timeout to wait for a successful message exchange between native code and
// a web page using Mojo.
constexpr base::TimeDelta kMessageTimeout = base::Seconds(5);

// UI handler class which communicates with test WebUI page as follows:
// - page sends "syn" message to `TestUIHandler`
// - `TestUIHandler` replies with "ack" message
// - page replies back with "fin"
//
// Once "fin" is received `IsFinReceived()` call will return true, indicating
// that communication was successful. See test WebUI page code here:
// ios/web/test/data/mojo_test.js
class TestUIHandler : public mojom::TestUIHandlerMojo {
 public:
  TestUIHandler() {}
  ~TestUIHandler() override {}

  // Returns true if "fin" has been received.
  bool IsFinReceived() { return fin_received_; }

  // TestUIHandlerMojo overrides.
  void SetClientPage(mojo::PendingRemote<mojom::TestPage> page) override {
    page_.Bind(std::move(page));
  }

  void HandleJsMessageWithCallback(
      const std::string& message,
      HandleJsMessageWithCallbackCallback callback) override {
    auto result = mojom::NativeMessageResultMojo::New();
    result->message = "ack2";
    // Replay via PostTask to check it also works well.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
  }

  void HandleJsMessage(const std::string& message) override {
    if (message == "syn") {
      // Received "syn" message from WebUI page, send "ack" as reply.
      DCHECK(!syn_received_);
      DCHECK(!fin_received_);
      syn_received_ = true;
      mojom::NativeMessageResultMojoPtr result(
          mojom::NativeMessageResultMojo::New());
      result->message = "ack";
      page_->HandleNativeMessage(std::move(result));
    } else if (message == "fin") {
      // Received "fin" from the WebUI page in response to "ack".
      DCHECK(syn_received_);
      DCHECK(!fin_received_);
      fin_received_ = true;
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  void BindTestHandler(
      mojo::PendingReceiver<mojom::TestUIHandlerMojo> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  mojo::ReceiverSet<mojom::TestUIHandlerMojo> receivers_;
  mojo::Remote<mojom::TestPage> page_;
  // `true` if "syn" has been received.
  bool syn_received_ = false;
  // `true` if "fin" has been received.
  bool fin_received_ = false;
};

// Controller for test WebUI.
class TestUI : public WebUIIOSController {
 public:
  // Constructs controller from `web_ui` and `ui_handler` which will communicate
  // with test WebUI page.
  TestUI(WebUIIOS* web_ui, const std::string& host, TestUIHandler* ui_handler)
      : WebUIIOSController(web_ui, host) {
    web::WebUIIOSDataSource* source =
        web::WebUIIOSDataSource::Create(kTestWebUIURLHost);

    source->AddResourcePath("mojo_test.js", IDR_MOJO_TEST_JS);
    source->AddResourcePath("mojo_bindings.js", IDR_IOS_MOJO_BINDINGS_JS);
    source->AddResourcePath("mojo_test.mojom.js", IDR_MOJO_TEST_MOJO_JS);
    source->SetDefaultResource(IDR_MOJO_TEST_HTML);

    web::WebState* web_state = web_ui->GetWebState();
    web::WebUIIOSDataSource::Add(web_state->GetBrowserState(), source);

    web_state->GetInterfaceBinderForMainFrame()->AddInterface(
        base::BindRepeating(&TestUIHandler::BindTestHandler,
                            base::Unretained(ui_handler)));
  }

  ~TestUI() override = default;
};

// Factory that creates TestUI controller.
class TestWebUIControllerFactory : public WebUIIOSControllerFactory {
 public:
  // Constructs a controller factory which will eventually create `ui_handler`.
  explicit TestWebUIControllerFactory(TestUIHandler* ui_handler)
      : ui_handler_(ui_handler) {}

  // WebUIIOSControllerFactory overrides.
  std::unique_ptr<WebUIIOSController> CreateWebUIIOSControllerForURL(
      WebUIIOS* web_ui,
      const GURL& url) const override {
    if (!url.SchemeIs(kTestWebUIScheme))
      return nullptr;
    DCHECK_EQ(url.host(), kTestWebUIURLHost);
    return std::make_unique<TestUI>(web_ui, url.host(), ui_handler_);
  }

  NSInteger GetErrorCodeForWebUIURL(const GURL& url) const override {
    if (url.SchemeIs(kTestWebUIScheme))
      return 0;
    return NSURLErrorUnsupportedURL;
  }

 private:
  // UI handler class which communicates with test WebUI page.
  raw_ptr<TestUIHandler> ui_handler_;
};
}  // namespace

// A test fixture for verifying mojo communication for WebUI.
class WebUIMojoTest : public WebIntTest {
 protected:
  void SetUp() override {
    WebIntTest::SetUp();
    @autoreleasepool {
      ui_handler_ = std::make_unique<TestUIHandler>();
      WebState::CreateParams params(GetBrowserState());
      web_state_ = WebState::Create(params);
      factory_ =
          std::make_unique<TestWebUIControllerFactory>(ui_handler_.get());
      WebUIIOSControllerFactory::RegisterFactory(factory_.get());
    }
  }

  void TearDown() override {
    @autoreleasepool {
      // WebState owns CRWWebUIManager. When WebState is destroyed,
      // CRWWebUIManager is autoreleased and will be destroyed upon autorelease
      // pool purge. However in this test, WebTest destructor is called before
      // PlatformTest, thus CRWWebUIManager outlives the WebTaskenvironment.
      // However, CRWWebUIManager owns a URLFetcherImpl, which DCHECKs that its
      // destructor is called on UI web thread. Hence, URLFetcherImpl has to
      // outlive the WebTaskenvironment, since [NSThread mainThread] will not be
      // WebThread::UI once WebTaskenvironment is destroyed.
      web_state_.reset();
      ui_handler_.reset();
      WebUIIOSControllerFactory::DeregisterFactory(factory_.get());
    }

    WebIntTest::TearDown();
  }

  // Returns WebState which loads test WebUI page.
  WebState* web_state() { return web_state_.get(); }
  // Returns UI handler which communicates with WebUI page.
  TestUIHandler* test_ui_handler() { return ui_handler_.get(); }

 private:
  std::unique_ptr<WebState> web_state_;
  std::unique_ptr<TestUIHandler> ui_handler_;
  std::unique_ptr<TestWebUIControllerFactory> factory_;
};

// Tests that JS can send messages to the native code and vice versa.
// TestUIHandler is used for communication and test succeeds only when
// `TestUIHandler` successfully receives "ack" message from WebUI page.
TEST_F(WebUIMojoTest, MessageExchange) {
  @autoreleasepool {
    url::SchemeHostPort tuple(kTestWebUIScheme, kTestWebUIURLHost, 0);
    GURL url(tuple.Serialize());
    test::LoadUrl(web_state(), url);
    // LoadIfNecessary is needed because the view is not created (but needed)
    // when loading the page. TODO(crbug.com/41309809): Remove this call.
    web_state()->GetNavigationManager()->LoadIfNecessary();

    // Wait until `TestUIHandler` receives "fin" message from WebUI page.
    bool fin_received =
        base::test::ios::WaitUntilConditionOrTimeout(kMessageTimeout, ^{
          // Flush any pending tasks. Don't RunUntilIdle() because
          // RunUntilIdle() is incompatible with mojo::SimpleWatcher's
          // automatic arming behavior, which Mojo JS still depends upon.
          //
          // TODO(crbug.com/41307566): Introduce the full watcher API to JS and
          // get rid of this hack.
          base::RunLoop loop;
          base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
              FROM_HERE, loop.QuitClosure());
          loop.Run();
          return test_ui_handler()->IsFinReceived();
        });

    ASSERT_TRUE(fin_received);
    EXPECT_FALSE(web_state()->IsLoading());
    EXPECT_EQ(url, web_state()->GetLastCommittedURL());
  }
}

}  // namespace web
