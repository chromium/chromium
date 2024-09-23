// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "content/browser/webui/web_ui_managed_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/data/web_ui_managed_interface_test.test-mojom.h"
#include "content/test/grit/web_ui_mojo_test_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/untrusted_web_ui_browsertest_util.h"
#include "url/gurl.h"

namespace content {

namespace {

static constexpr char kFooURL[] = "chrome://foo/";
static constexpr char kFooInIframeURL[] = "chrome-untrusted://foo/";

static constexpr char kBindFooJs[] =
    "(async () => {"
    "  let jsBridgeRemote = window.TestWebUIJsBridge.getRemote();"
    "  let fooRemote = new FooRemote();"
    "  jsBridgeRemote.bindFoo(fooRemote.$.bindNewPipeAndPassReceiver());"
    "  return (await fooRemote.getFoo()).value;"
    "})()";

// A helper class for counting alive instances of a specific class.
template <typename Type>
class InstanceCounter {
 public:
  static void Increment() { count_++; }
  static void Decrement() { count_--; }
  static int count() { return count_; }

 private:
  static int count_;
};

template <typename Type>
int InstanceCounter<Type>::count_ = 0;

// FooImpl implements Foo.
class FooImpl : public mojom::Foo,
                public WebUIManagedInterface<FooImpl, mojom::Foo> {
 public:
  FooImpl() { InstanceCounter<FooImpl>::Increment(); }

  ~FooImpl() override { InstanceCounter<FooImpl>::Decrement(); }

  // mojom::Foo:
  void GetFoo(GetFooCallback callback) override {
    std::move(callback).Run("foo-success");
  }
};

// FooImpl implements Foo and talks to a Bar remote.
class FooBarImpl
    : public mojom::Foo,
      public WebUIManagedInterface<FooBarImpl, mojom::Foo, mojom::Bar> {
 public:
  FooBarImpl() { InstanceCounter<FooBarImpl>::Increment(); }

  ~FooBarImpl() override { InstanceCounter<FooBarImpl>::Decrement(); }

  // WebUIManagedInterface:
  void OnReady() override {
    remote()->GetBar(base::BindRepeating(
        [](const std::string& value) { EXPECT_EQ("bar-success", value); }));
  }

  // mojom::Foo:
  void GetFoo(GetFooCallback callback) override {
    std::move(callback).Run("foo-success");
  }
};

// Baz talks to a mojom::Baz remote. It does not implement any interfaces.
class Baz : public WebUIManagedInterface<Baz,
                                         WebUIManagedInterfaceNoPageHandler,
                                         mojom::Baz> {
 public:
  Baz() { InstanceCounter<Baz>::Increment(); }

  ~Baz() override { InstanceCounter<Baz>::Decrement(); }

  // WebUIManagedInterface:
  void OnReady() override {
    remote()->GetBaz(base::BindRepeating(
        [](const std::string& value) { EXPECT_EQ("baz-success", value); }));
  }
};

class WebUIManagedInterfaceTestUI : public WebUIController,
                                    public mojom::TestWebUIJsBridge {
 public:
  explicit WebUIManagedInterfaceTestUI(WebUI* web_ui)
      : WebUIController(web_ui) {
    // Allow resources to be loaded for both top-level and embedded WebUIs.
    // Due to the behavior of URLDataManagerBackend::GetDataSourceFromURL(),
    // WebUIDataSource::CreateAndAdd() expects "host" as the `source_name` arg
    // for trusted hosts and "chrome-untrusted://host" for untrusted hosts.
    for (const auto& host :
         {GURL(kFooURL).host(), std::string(kFooInIframeURL)}) {
      WebUIDataSource* data_source = WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(), host);
      data_source->SetDefaultResource(IDR_WEB_UI_MANAGED_INTERFACE_TEST_HTML);
      data_source->AddResourcePath(
          "web_ui_managed_interface_test.test-mojom-webui.js",
          IDR_WEB_UI_MANAGED_INTERFACE_TEST_TEST_MOJOM_WEBUI_JS);
      data_source->AddResourcePath("web_ui_managed_interface_test.js",
                                   IDR_WEB_UI_MANAGED_INTERFACE_TEST_JS);

      // Allow WebUI to be embedded in an iframe under
      // chrome-untrusted://test-host.
      if (host == kFooInIframeURL) {
        data_source->AddFrameAncestor(GURL("chrome-untrusted://test-host"));
      }
    }
  }

  void BindInterface(mojo::PendingReceiver<mojom::TestWebUIJsBridge> receiver) {
    js_bridge_receiver_.reset();
    js_bridge_receiver_.Bind(std::move(receiver));
  }

  // mojom::TestWebUIJsBridge:
  void BindFoo(mojo::PendingReceiver<mojom::Foo> foo_receiver) override {
    FooImpl::Create(this, std::move(foo_receiver));
  }

  void BindFooBar(mojo::PendingReceiver<mojom::Foo> foo_receiver,
                  mojo::PendingRemote<mojom::Bar> bar_remote) override {
    FooBarImpl::Create(this, std::move(foo_receiver), std::move(bar_remote));
  }

  void BindBaz(mojo::PendingRemote<mojom::Baz> baz_remote) override {
    Baz::Create(this, std::move(baz_remote));
  }

  WEB_UI_CONTROLLER_TYPE_DECL();

 private:
  mojo::Receiver<mojom::TestWebUIJsBridge> js_bridge_receiver_{this};
};

WEB_UI_CONTROLLER_TYPE_IMPL(WebUIManagedInterfaceTestUI)

// WebUIControllerFactory that serves our TestWebUIController.
class TestFooWebUIControllerFactory : public WebUIControllerFactory {
 public:
  TestFooWebUIControllerFactory() = default;

  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override {
    if (url::IsSameOriginWith(url, GURL(kFooURL)) ||
        url::IsSameOriginWith(url, GURL(kFooInIframeURL))) {
      return std::make_unique<WebUIManagedInterfaceTestUI>(web_ui);
    }

    return nullptr;
  }

  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override {
    if (url::IsSameOriginWith(url, GURL(kFooURL))) {
      return &kFooURL;
    }

    if (url::IsSameOriginWith(url, GURL(kFooInIframeURL))) {
      return &kFooInIframeURL;
    }

    return WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override {
    return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
  }
};

}  // namespace

class WebUIManagedInterfaceBrowserTest : public ContentBrowserTest {
 public:
  WebUIManagedInterfaceBrowserTest() {
    factory_ = std::make_unique<TestFooWebUIControllerFactory>();
    WebUIControllerFactory::RegisterFactory(factory_.get());
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    test_content_browser_client_ = std::make_unique<TestContentBrowserClient>();
  }

  // Evaluate `statement` in frame (defaults to main frame), and returns its
  // result. For convenience, `script` should evaluate to a string, or a promise
  // that resolves to a string.
  std::string EvalStatement(const std::string& statement,
                            content::RenderFrameHost* frame = nullptr) {
    RenderFrameHost* eval_frame =
        frame ? frame : ConvertToRenderFrameHost(shell());

    // Use EvalJs to execute the statement
    auto result =
        EvalJs(eval_frame, statement, content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
               content::ISOLATED_WORLD_ID_GLOBAL);

    EXPECT_TRUE(result.error.empty());
    return result.value.GetString();
  }

  void Reload(RenderFrameHost* frame = nullptr) {
    RenderFrameHost* eval_frame =
        frame ? frame : ConvertToRenderFrameHost(shell());
    TestNavigationObserver observer(shell()->web_contents(), 1);
    EXPECT_TRUE(ExecJs(eval_frame, "location.reload()"));
    observer.Wait();
  }

 private:
  class TestContentBrowserClient
      : public ContentBrowserTestContentBrowserClient {
   public:
    TestContentBrowserClient() = default;
    TestContentBrowserClient(const TestContentBrowserClient&) = delete;
    TestContentBrowserClient& operator=(const TestContentBrowserClient&) =
        delete;
    ~TestContentBrowserClient() override = default;

    void RegisterWebUIInterfaceBrokers(
        WebUIBrowserInterfaceBrokerRegistry& registry) override {
      registry.ForWebUI<WebUIManagedInterfaceTestUI>()
          .Add<mojom::TestWebUIJsBridge>();
    }
  };

  std::unique_ptr<TestFooWebUIControllerFactory> factory_;
  std::unique_ptr<TestContentBrowserClient> test_content_browser_client_;
};

// Test FooImpl that implements Foo is constructed properly and destroyed on
// navigation.
IN_PROC_BROWSER_TEST_F(WebUIManagedInterfaceBrowserTest, Foo) {
  WebContents* web_contents = shell()->web_contents();

  ASSERT_TRUE(NavigateToURL(web_contents, GURL(kFooURL)));

  EXPECT_EQ("foo-success", EvalStatement(kBindFooJs));
  EXPECT_EQ(1, InstanceCounter<FooImpl>::count());

  // Navigation will destroy interface impls.
  Reload();
  EXPECT_EQ(0, InstanceCounter<FooImpl>::count());
}

// Test FooBarImpl that implements Foo and talks to the Bar remote is
// constructed properly and destroyed on navigation.
IN_PROC_BROWSER_TEST_F(WebUIManagedInterfaceBrowserTest, FooBar) {
  WebContents* web_contents = shell()->web_contents();

  ASSERT_TRUE(NavigateToURL(web_contents, GURL(kFooURL)));

  EXPECT_EQ("foo-success",
            EvalStatement(
                "(async () => {"
                "  let jsBridgeRemote = window.TestWebUIJsBridge.getRemote();"
                "  let fooRemote = new FooRemote();"
                "  let barCallbackRouter = new BarCallbackRouter();"
                "  let listenerPromise = new Promise(resolve => {"
                "    barCallbackRouter.getBar.addListener(() => {"
                "      /* Resolve the promise after the response is sent. */"
                "      setTimeout(resolve, 0);"
                "      return { value: 'bar-success' };"
                "    });"
                "  });"
                ""
                "  jsBridgeRemote.bindFooBar("
                "    fooRemote.$.bindNewPipeAndPassReceiver(),"
                "    barCallbackRouter.$.bindNewPipeAndPassRemote());"
                ""
                "  /* Wait for the listener to be called. */"
                "  await listenerPromise;"
                "  /* Wait for the response to get to the browser. */"
                "  await barCallbackRouter.$.flush();"
                ""
                "  return (await fooRemote.getFoo()).value;"
                "})()"));
  EXPECT_EQ(1, InstanceCounter<FooBarImpl>::count());

  // Navigation will destroy interface impls.
  Reload();
  EXPECT_EQ(0, InstanceCounter<FooBarImpl>::count());
}

// Test Baz that talks to the Baz remote is constructed properly and
// destroyed on navigation.
IN_PROC_BROWSER_TEST_F(WebUIManagedInterfaceBrowserTest, Baz) {
  WebContents* web_contents = shell()->web_contents();

  ASSERT_TRUE(NavigateToURL(web_contents, GURL(kFooURL)));

  EXPECT_EQ("success",
            EvalStatement(
                "(async () => {"
                "  let jsBridgeRemote = window.TestWebUIJsBridge.getRemote();"
                "  let bazCallbackRouter = new BazCallbackRouter();"
                "  let listenerPromise = new Promise(resolve => {"
                "    bazCallbackRouter.getBaz.addListener(() => {"
                "      /* Resolve the promise after the response is sent. */"
                "      setTimeout(resolve, 0);"
                "      return { value: 'baz-success' };"
                "    });"
                "  });"
                ""
                "  jsBridgeRemote.bindBaz("
                "    bazCallbackRouter.$.bindNewPipeAndPassRemote());"
                ""
                "  /* Wait for the listener to be called. */"
                "  await listenerPromise;"
                "  /* Wait for the response to get to the browser. */"
                "  await bazCallbackRouter.$.flush();"
                ""
                "  return 'success';"
                "})()"));
  EXPECT_EQ(1, InstanceCounter<Baz>::count());

  // Navigation will destroy interface impls.
  Reload();
  EXPECT_EQ(0, InstanceCounter<Baz>::count());
}

// Test that interface impls of an iframe WebUI are destroyed on iframe reload.
IN_PROC_BROWSER_TEST_F(WebUIManagedInterfaceBrowserTest, WebUIInIframe) {
  WebContents* web_contents = shell()->web_contents();
  // Allow adding chrome-untrusted://foo in an iframe.
  TestUntrustedDataSourceHeaders headers;
  headers.child_src = "child-src *;";
  // Use a test host WebUI. We intentionally don't use chrome://foo as the host
  // WebUI, otherwise there will be two instances of WebUIManagedInterfaceTestUI
  // while we only care about the embedded one.
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-host", headers));

  // Load host page.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            GetChromeUntrustedUIURL("test-host/title1.html")));

  // Append an iframe that opens chrome-untrusted://foo.
  EXPECT_EQ(true,
            EvalJs(web_contents->GetPrimaryMainFrame(),
                   JsReplace("let frame = document.createElement('iframe');"
                             "frame.src=$1;"
                             "!!document.body.appendChild(frame);",
                             GURL(kFooInIframeURL).spec()),
                   EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  RenderFrameHost* foo_frame =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_EQ(GURL(kFooInIframeURL), foo_frame->GetLastCommittedURL());

  EXPECT_EQ("foo-success", EvalStatement(kBindFooJs, foo_frame));
  EXPECT_EQ(1, InstanceCounter<FooImpl>::count());

  // Iframe navigation will destroy interface impls.
  Reload(foo_frame);
  foo_frame = ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  EXPECT_EQ(0, InstanceCounter<FooImpl>::count());

  // Interface impls can be created after reload.
  EXPECT_EQ("foo-success", EvalStatement(kBindFooJs, foo_frame));
  EXPECT_EQ(1, InstanceCounter<FooImpl>::count());
}

// Test that interface impls of an iframe WebUI are destroyed on iframe removal.
IN_PROC_BROWSER_TEST_F(WebUIManagedInterfaceBrowserTest, RemoveIframe) {
  WebContents* web_contents = shell()->web_contents();
  // Allow adding chrome-untrusted://foo in an iframe.
  TestUntrustedDataSourceHeaders headers;
  headers.child_src = "child-src *;";
  // Use a test host WebUI. We intentionally don't use chrome://foo as the host
  // WebUI, otherwise there will be two instances of WebUIManagedInterfaceTestUI
  // while we only care about the embedded one.
  content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
      std::make_unique<ui::TestUntrustedWebUIConfig>("test-host", headers));

  // Load host page.
  ASSERT_TRUE(NavigateToURL(web_contents,
                            GetChromeUntrustedUIURL("test-host/title1.html")));

  // Append an iframe that opens chrome-untrusted://foo.
  EXPECT_EQ(true,
            EvalJs(web_contents->GetPrimaryMainFrame(),
                   JsReplace("let frame = document.createElement('iframe');"
                             "frame.src=$1;frame.id='untrusted-webui';"
                             "!!document.body.appendChild(frame);",
                             GURL(kFooInIframeURL).spec()),
                   EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1 /* world_id */));
  EXPECT_TRUE(WaitForLoadStop(web_contents));

  RenderFrameHost* foo_frame =
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_EQ(GURL(kFooInIframeURL), foo_frame->GetLastCommittedURL());

  EXPECT_EQ("foo-success", EvalStatement(kBindFooJs, foo_frame));
  EXPECT_EQ(1, InstanceCounter<FooImpl>::count());

  // Iframe removal will destroy interface impls.
  RenderFrameDeletedObserver frame_deleted_observer(foo_frame);
  EXPECT_TRUE(
      ExecJs(shell(), "document.getElementById('untrusted-webui').remove()"));
  frame_deleted_observer.WaitUntilDeleted();
  EXPECT_EQ(0, InstanceCounter<FooImpl>::count());
}

}  // namespace content
