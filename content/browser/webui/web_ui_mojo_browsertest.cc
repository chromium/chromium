// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <limits>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/referrer.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/data/web_ui_ts_test.test-mojom.h"
#include "content/test/data/web_ui_ts_test_types.test-mojom.h"
#include "content/test/grit/web_ui_mojo_test_resources.h"
#include "content/test/grit/web_ui_mojo_test_resources_map.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "content/test/data/web_ui_test.test-mojom.h"
#include "content/test/data/web_ui_test_types.test-mojom.h"
#endif

namespace content {
namespace {

const char kMojoWebUiHost[] = "mojo-web-ui";
const char kMojoWebUiTsHost[] = "mojo-web-ui-ts";
const char kDummyWebUiHost[] = "dummy-web-ui";

#if BUILDFLAG(IS_CHROMEOS_ASH)
class WebUIMojoTestCacheImpl : public mojom::WebUIMojoTestCache {
 public:
  explicit WebUIMojoTestCacheImpl(
      mojo::PendingReceiver<mojom::WebUIMojoTestCache> receiver)
      : receiver_(this, std::move(receiver)) {}

  ~WebUIMojoTestCacheImpl() override = default;

  // mojom::WebUIMojoTestCache overrides:
  void Put(const GURL& url, const std::string& contents) override {
    cache_[url] = contents;
  }

  void GetAll(GetAllCallback callback) override {
    std::vector<mojom::CacheItemPtr> items;
    for (const auto& entry : cache_)
      items.push_back(mojom::CacheItem::New(entry.first, entry.second));
    std::move(callback).Run(std::move(items));
  }

 private:
  mojo::Receiver<mojom::WebUIMojoTestCache> receiver_;
  std::map<GURL, std::string> cache_;
};
#endif

// Duplicate for the TypeScript version of the test. We can't re-use because
// the TS interface has to be named differently to avoid conflicting symbols.
class WebUITsMojoTestCacheImpl : public mojom::WebUITsMojoTestCache {
 public:
  explicit WebUITsMojoTestCacheImpl(
      mojo::PendingReceiver<mojom::WebUITsMojoTestCache> receiver)
      : receiver_(this, std::move(receiver)) {}

  ~WebUITsMojoTestCacheImpl() override = default;

  // mojom::WebUITsMojoTestCache overrides:
  void Put(const GURL& url, const std::string& contents) override {
    cache_[url] = contents;
  }

  void GetAll(GetAllCallback callback) override {
    std::vector<mojom::TsCacheItemPtr> items;
    for (const auto& entry : cache_)
      items.push_back(mojom::TsCacheItem::New(entry.first, entry.second));
    std::move(callback).Run(std::move(items));
  }

  void Echo(
      std::optional<bool> optional_bool,
      std::optional<uint8_t> optional_uint8,
      std::optional<mojom::TestEnum> optional_enum,
      mojom::OptionalNumericsStructPtr optional_numerics,
      const std::vector<std::optional<bool>>& optional_bools,
      const std::vector<std::optional<uint32_t>>& optional_ints,
      const std::vector<std::optional<mojom::TestEnum>>& optional_enums,
      const base::flat_map<int32_t, std::optional<bool>>& bool_map,
      const base::flat_map<int32_t, std::optional<int32_t>>& int_map,
      const base::flat_map<int32_t, std::optional<mojom::TestEnum>>& enum_map,
      mojom::SimpleMappedTypePtr simple_mapped,
      mojom::NestedMappedTypePtr nested_mapped,
      mojom::StringDictPtr dict_ptr,
      EchoCallback callback) override {
    std::move(callback).Run(
        optional_bool.has_value() ? std::make_optional(!optional_bool.value())
                                  : std::nullopt,
        optional_uint8.has_value() ? std::make_optional(~optional_uint8.value())
                                   : std::nullopt,
        optional_enum.has_value() ? std::make_optional(mojom::TestEnum::kTwo)
                                  : std::nullopt,
        mojom::OptionalNumericsStruct::New(
            optional_numerics->optional_bool.has_value()
                ? std::make_optional(!optional_numerics->optional_bool.value())
                : std::nullopt,
            optional_numerics->optional_uint8.has_value()
                ? std::make_optional(~optional_numerics->optional_uint8.value())
                : std::nullopt,
            optional_numerics->optional_enum.has_value()
                ? std::make_optional(mojom::TestEnum::kTwo)
                : std::nullopt),
        optional_bools, optional_ints, optional_enums, bool_map, int_map,
        enum_map, simple_mapped->Clone(), nested_mapped->Clone(),
        dict_ptr ? dict_ptr->Clone() : nullptr);
  }

 private:
  mojo::Receiver<mojom::WebUITsMojoTestCache> receiver_;
  std::map<GURL, std::string> cache_;
};

// WebUIController that sets up mojo bindings.
class TestWebUIController : public WebUIController {
 public:
  explicit TestWebUIController(WebUI* web_ui,
                               BindingsPolicySet bindings = BindingsPolicySet(
                                   {BindingsPolicyValue::kMojoWebUi}))
      : WebUIController(web_ui) {
    const base::span<const webui::ResourcePath> kMojoWebUiResources =
        base::make_span(kWebUiMojoTestResources, kWebUiMojoTestResourcesSize);

    web_ui->SetBindings(bindings);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {
      WebUIDataSource* data_source = WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(), kMojoWebUiHost);
      data_source->OverrideContentSecurityPolicy(
          network::mojom::CSPDirectiveName::ScriptSrc,
          "script-src chrome://resources 'self' 'unsafe-eval';");
      data_source->DisableTrustedTypesCSP();
      data_source->AddResourcePaths(kMojoWebUiResources);
      data_source->AddResourcePath("", IDR_WEB_UI_MOJO_HTML);
    }
#endif
    {
      WebUIDataSource* data_source = WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(), kMojoWebUiTsHost);
      data_source->OverrideContentSecurityPolicy(
          network::mojom::CSPDirectiveName::ScriptSrc,
          "script-src chrome://resources 'self' 'unsafe-eval';");
      data_source->DisableTrustedTypesCSP();
      data_source->AddResourcePaths(kMojoWebUiResources);
      data_source->AddResourcePath("", IDR_WEB_UI_MOJO_TS_HTML);
    }
    {
      WebUIDataSource* data_source = WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(), kDummyWebUiHost);
      data_source->SetRequestFilter(
          base::BindRepeating([](const std::string& path) { return true; }),
          base::BindRepeating([](const std::string& id,
                                 WebUIDataSource::GotDataCallback callback) {
            std::move(callback).Run(new base::RefCountedString);
          }));
    }
  }

  TestWebUIController(const TestWebUIController&) = delete;
  TestWebUIController& operator=(const TestWebUIController&) = delete;

 protected:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<WebUIMojoTestCacheImpl> cache_;
#endif
  std::unique_ptr<WebUITsMojoTestCacheImpl> ts_cache_;
};

// TestWebUIController that can bind a WebUIMojoTestCache or
// WebUITsMojoTestCache interface when requested by the page. Uses asserts to
// ensure only one of the two is created for each test.
class CacheTestWebUIController : public TestWebUIController {
 public:
  explicit CacheTestWebUIController(WebUI* web_ui)
      : TestWebUIController(web_ui) {}
  ~CacheTestWebUIController() override = default;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void BindInterface(
      mojo::PendingReceiver<mojom::WebUIMojoTestCache> receiver) {
    cache_ = std::make_unique<WebUIMojoTestCacheImpl>(std::move(receiver));
    ASSERT_FALSE(ts_cache_);
  }
#endif

  void BindInterface(
      mojo::PendingReceiver<mojom::WebUITsMojoTestCache> receiver) {
    ts_cache_ = std::make_unique<WebUITsMojoTestCacheImpl>(std::move(receiver));
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ASSERT_FALSE(cache_);
#endif
  }

  WEB_UI_CONTROLLER_TYPE_DECL();
};

WEB_UI_CONTROLLER_TYPE_IMPL(CacheTestWebUIController)

// WebUIControllerFactory that creates TestWebUIController.
class TestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  TestWebUIControllerFactory()
      : registered_controllers_(
            {{"cache", base::BindRepeating(
                           &TestWebUIControllerFactory::CreateCacheController,
                           base::Unretained(this))},
             {"hybrid", base::BindRepeating(
                            &TestWebUIControllerFactory::CreateHybridController,
                            base::Unretained(this))},
             {"webui_bindings",
              base::BindRepeating(
                  &TestWebUIControllerFactory::CreateWebUIController,
                  base::Unretained(this))}}) {}

  TestWebUIControllerFactory(const TestWebUIControllerFactory&) = delete;
  TestWebUIControllerFactory& operator=(const TestWebUIControllerFactory&) =
      delete;

  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override {
    if (!web_ui_enabled_ || !url.SchemeIs(kChromeUIScheme))
      return nullptr;

    auto it = registered_controllers_.find(url.query());
    if (it != registered_controllers_.end())
      return it->second.Run(web_ui);

    return std::make_unique<TestWebUIController>(web_ui);
  }

  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override {
    if (!web_ui_enabled_ || !url.SchemeIs(kChromeUIScheme))
      return WebUI::kNoWebUI;

    return reinterpret_cast<WebUI::TypeID>(1);
  }

  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override {
    return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
  }

  void set_web_ui_enabled(bool enabled) { web_ui_enabled_ = enabled; }

 private:
  std::unique_ptr<WebUIController> CreateCacheController(WebUI* web_ui) {
    return std::make_unique<CacheTestWebUIController>(web_ui);
  }

  std::unique_ptr<WebUIController> CreateHybridController(WebUI* web_ui) {
    return std::make_unique<TestWebUIController>(web_ui,
                                                 kWebUIBindingsPolicySet);
  }

  std::unique_ptr<WebUIController> CreateWebUIController(WebUI* web_ui) {
    return std::make_unique<TestWebUIController>(
        web_ui, BindingsPolicySet({BindingsPolicyValue::kWebUi}));
  }

  bool web_ui_enabled_ = true;
  const base::flat_map<
      std::string,
      base::RepeatingCallback<std::unique_ptr<WebUIController>(WebUI*)>>
      registered_controllers_;
};

class TestWebUIContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  TestWebUIContentBrowserClient() {}
  TestWebUIContentBrowserClient(const TestWebUIContentBrowserClient&) = delete;
  TestWebUIContentBrowserClient& operator=(
      const TestWebUIContentBrowserClient&) = delete;
  ~TestWebUIContentBrowserClient() override {}

  void RegisterBrowserInterfaceBindersForFrame(
      RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    RegisterWebUIControllerInterfaceBinder<mojom::WebUIMojoTestCache,
                                           CacheTestWebUIController>(map);
#endif
    RegisterWebUIControllerInterfaceBinder<mojom::WebUITsMojoTestCache,
                                           CacheTestWebUIController>(map);
  }
};

class WebUIMojoTest : public ContentBrowserTest,
                      public testing::WithParamInterface<bool> {
 public:
  WebUIMojoTest() = default;

  WebUIMojoTest(const WebUIMojoTest&) = delete;
  WebUIMojoTest& operator=(const WebUIMojoTest&) = delete;

  TestWebUIControllerFactory* factory() { return &factory_; }

  void NavigateWithNewWebUI(const std::string& path) {
    // Load a dummy WebUI URL first so that a new WebUI is set up when we load
    // the URL we're actually interested in.
    EXPECT_TRUE(NavigateToURL(shell(), GetWebUIURL(kDummyWebUiHost)));
    EXPECT_TRUE(NavigateToURL(
        shell(), GetWebUIURL(GetMojoWebUiHost() + std::string("/") + path)));
  }

  // Run |script| and return a boolean result.
  bool RunBoolFunction(const std::string& script) {
    return EvalJs(shell()->web_contents(), script).ExtractBool();
  }

 protected:
  std::string GetMojoWebUiHost() {
    return std::string(GetParam() ? kMojoWebUiTsHost : kMojoWebUiHost);
  }

  void SetUpOnMainThread() override {
    client_ = std::make_unique<TestWebUIContentBrowserClient>();
  }

  void TearDownOnMainThread() override { client_.reset(); }

 private:
  TestWebUIControllerFactory factory_;
  content::ScopedWebUIControllerFactoryRegistration factory_registration_{
      &factory_};
  std::unique_ptr<TestWebUIContentBrowserClient> client_;
};

// Test both JS and TS on Ash, since Ash widely uses both types of WebUI
// bindings. Test TS only on other platforms.
#if BUILDFLAG(IS_CHROMEOS_ASH)
INSTANTIATE_TEST_SUITE_P(All, WebUIMojoTest, testing::Bool());
#else
INSTANTIATE_TEST_SUITE_P(All, WebUIMojoTest, testing::Values(true));
#endif

#if BUILDFLAG(IS_LINUX)
// TODO(crbug.com/353502934): This test became flaky on Linux TSan builds since
// 2024-07-16.
#define MAYBE_EndToEndCommunication DISABLED_EndToEndCommunication
#else
#define MAYBE_EndToEndCommunication EndToEndCommunication
#endif
// Loads a WebUI page that contains Mojo JS bindings and verifies a message
// round-trip between the page and the browser.
IN_PROC_BROWSER_TEST_P(WebUIMojoTest, MAYBE_EndToEndCommunication) {
  // Load a dummy page in the initial RenderFrameHost.  The initial
  // RenderFrameHost is created by the test harness prior to installing
  // TestWebUIContentBrowserClient in WebUIMojoTest::SetUpOnMainThread().  If we
  // were to navigate that initial RFH to WebUI directly, it would get reused,
  // but it wouldn't have the test's browser interface binders (registered via
  // TestWebUIContentBrowserClient::RegisterBrowserInterfaceBindersForFrame() at
  // RFH creation time).  Navigating the initial RFH to some other page forces
  // the subsequent WebUI navigation to create a new RenderFrameHost, and by
  // this time, TestWebUIContentBrowserClient will take effect on that new RFH.
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:,foo")));

  GURL kTestUrl(GetWebUIURL(GetMojoWebUiHost() + "/?cache"));
  const std::string kTestScript = "runTest();";
  EXPECT_TRUE(NavigateToURL(shell(), kTestUrl));
  EXPECT_EQ(true, EvalJs(shell()->web_contents(), kTestScript));

  // Check that a second shell works correctly.
  Shell* other_shell = CreateBrowser();
  EXPECT_TRUE(WaitForLoadStop(other_shell->web_contents()));
  EXPECT_TRUE(NavigateToURL(other_shell, kTestUrl));
  EXPECT_EQ(true, EvalJs(other_shell->web_contents(), kTestScript));

  // Close the second shell and wait until the second shell exits.
  RenderFrameHostWrapper wrapper(
      other_shell->web_contents()->GetPrimaryMainFrame());
  other_shell->Close();
  EXPECT_TRUE(wrapper.WaitUntilRenderFrameDeleted());

  // Check that a third shell works correctly, even if we force it to share a
  // process with the first shell, by forcing an artificially low process
  // limit.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  // Subtle: provide an explicit initial SiteInstance, since otherwise the WebUI
  // will stay in the initial RFH's process and avoid process reuse needed for
  // this test.
  other_shell = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), GURL(),
      SiteInstance::CreateForURL(shell()->web_contents()->GetBrowserContext(),
                                 kTestUrl),
      gfx::Size());
  EXPECT_TRUE(NavigateToURL(other_shell, kTestUrl));
  EXPECT_EQ(shell()->web_contents()->GetPrimaryMainFrame()->GetProcess(),
            other_shell->web_contents()->GetPrimaryMainFrame()->GetProcess());
  EXPECT_EQ(true, EvalJs(other_shell->web_contents(), kTestScript));
}

// Disabled due to flakiness: crbug.com/860385.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_NativeMojoAvailable DISABLED_NativeMojoAvailable
#else
#define MAYBE_NativeMojoAvailable NativeMojoAvailable
#endif
IN_PROC_BROWSER_TEST_P(WebUIMojoTest, MAYBE_NativeMojoAvailable) {
  // Mojo bindings should be enabled.
  NavigateWithNewWebUI("web_ui_mojo_native.html");
  EXPECT_TRUE(RunBoolFunction("isNativeMojoAvailable()"));

  // Now navigate again with normal WebUI bindings and ensure chrome.send is
  // available.
  NavigateWithNewWebUI("web_ui_mojo_native.html?webui_bindings");
  EXPECT_FALSE(RunBoolFunction("isNativeMojoAvailable()"));

  // Now navigate again both WebUI and Mojo bindings and ensure chrome.send is
  // available.
  NavigateWithNewWebUI("web_ui_mojo_native.html?hybrid");
  EXPECT_TRUE(RunBoolFunction("isNativeMojoAvailable()"));

  // Now navigate again with WebUI disabled and ensure the native bindings are
  // not available.
  factory()->set_web_ui_enabled(false);
  NavigateWithNewWebUI("web_ui_mojo_native.html?hybrid");
  EXPECT_FALSE(RunBoolFunction("isNativeMojoAvailable()"));
}

// Disabled due to flakiness: crbug.com/860385.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ChromeSendAvailable DISABLED_ChromeSendAvailable
#else
#define MAYBE_ChromeSendAvailable ChromeSendAvailable
#endif
IN_PROC_BROWSER_TEST_P(WebUIMojoTest, MAYBE_ChromeSendAvailable) {
  // chrome.send is not available on mojo-only WebUIs.
  NavigateWithNewWebUI("web_ui_mojo_native.html");
  EXPECT_FALSE(RunBoolFunction("isChromeSendAvailable()"));

  // Now navigate again with normal WebUI bindings and ensure chrome.send is
  // available.
  NavigateWithNewWebUI("web_ui_mojo_native.html?webui_bindings");
  EXPECT_TRUE(RunBoolFunction("isChromeSendAvailable()"));

  // Now navigate again both WebUI and Mojo bindings and ensure chrome.send is
  // available.
  NavigateWithNewWebUI("web_ui_mojo_native.html?hybrid");
  EXPECT_TRUE(RunBoolFunction("isChromeSendAvailable()"));

  // Now navigate again with WebUI disabled and ensure that chrome.send is
  // not available.
  factory()->set_web_ui_enabled(false);
  NavigateWithNewWebUI("web_ui_mojo_native.html?hybrid");
  EXPECT_FALSE(RunBoolFunction("isChromeSendAvailable()"));
}

IN_PROC_BROWSER_TEST_P(WebUIMojoTest, ChromeSendAvailable_AfterCrash) {
  GURL test_url(GetWebUIURL(GetMojoWebUiHost() +
                            "/web_ui_mojo_native.html?webui_bindings"));

  // Navigate with normal WebUI bindings and ensure chrome.send is available.
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_TRUE(EvalJs(shell(), "isChromeSendAvailable()").ExtractBool());

  WebUIImpl* web_ui = static_cast<WebUIImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetWebUI());

  // Simulate a crash on the page.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(shell());
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  shell()->web_contents()->GetController().LoadURL(
      GURL(blink::kChromeUICrashURL), content::Referrer(),
      ui::PAGE_TRANSITION_TYPED, std::string());
  crash_observer.Wait();
  EXPECT_FALSE(web_ui->GetRemoteForTest().is_bound());

  // Now navigate again both WebUI and Mojo bindings and ensure chrome.send is
  // available.
  EXPECT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_TRUE(EvalJs(shell(), "isChromeSendAvailable()").ExtractBool());
  // The RenderFrameHost has been replaced after the crash, so get web_ui again.
  web_ui = static_cast<WebUIImpl*>(
      shell()->web_contents()->GetPrimaryMainFrame()->GetWebUI());
  EXPECT_TRUE(web_ui->GetRemoteForTest().is_bound());
}

}  // namespace
}  // namespace content
