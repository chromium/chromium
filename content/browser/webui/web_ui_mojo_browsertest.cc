// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/data/web_ui_test_mojo_bindings.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/binder_map.h"

namespace content {
namespace {

bool g_got_message = false;

base::FilePath GetFilePathForJSResource(const std::string& path) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::string binding_path = "gen/" + path;
#if defined(OS_WIN)
  base::ReplaceChars(binding_path, "//", "\\", &binding_path);
#endif
  base::FilePath exe_dir;
  base::PathService::Get(base::DIR_EXE, &exe_dir);
  return exe_dir.AppendASCII(binding_path);
}

// The bindings for the page are generated from a .mojom file. This code looks
// up the generated file from disk and returns it.
void GetResource(const std::string& id,
                 const WebUIDataSource::GotDataCallback& callback) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  std::string contents;
  if (base::EndsWith(id, ".mojom-lite.js", base::CompareCase::SENSITIVE)) {
    CHECK(base::ReadFileToString(GetFilePathForJSResource(id), &contents))
        << id;
  } else {
    base::FilePath path;
    CHECK(base::PathService::Get(content::DIR_TEST_DATA, &path));
    path = path.AppendASCII(id.substr(0, id.find("?")));
    CHECK(base::ReadFileToString(path, &contents)) << path.value();
  }

  base::RefCountedString* ref_contents = new base::RefCountedString;
  ref_contents->data() = contents;
  callback.Run(ref_contents);
}

class BrowserTargetImpl : public mojom::BrowserTarget {
 public:
  BrowserTargetImpl(base::RunLoop* run_loop,
                    mojo::PendingReceiver<mojom::BrowserTarget> receiver)
      : run_loop_(run_loop), receiver_(this, std::move(receiver)) {}

  ~BrowserTargetImpl() override {}

  // mojom::BrowserTarget overrides:
  void Start(StartCallback closure) override { std::move(closure).Run(); }
  void Stop() override {
    g_got_message = true;
    run_loop_->Quit();
  }

 protected:
  base::RunLoop* const run_loop_;

 private:
  mojo::Receiver<mojom::BrowserTarget> receiver_;
  DISALLOW_COPY_AND_ASSIGN(BrowserTargetImpl);
};

// WebUIController that sets up mojo bindings.
class TestWebUIController : public WebUIController {
 public:
  TestWebUIController(WebUI* web_ui,
                      base::RunLoop* run_loop,
                      int bindings = BINDINGS_POLICY_MOJO_WEB_UI)
      : WebUIController(web_ui), run_loop_(run_loop) {
    web_ui->SetBindings(bindings);
    {
      WebUIDataSource* data_source = WebUIDataSource::Create("mojo-web-ui");
      data_source->SetRequestFilter(
          base::BindRepeating([](const std::string& path) { return true; }),
          base::BindRepeating(&GetResource));
      WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                           data_source);
    }
    {
      WebUIDataSource* data_source = WebUIDataSource::Create("dummy-web-ui");
      data_source->SetRequestFilter(
          base::BindRepeating([](const std::string& path) { return true; }),
          base::BindRepeating(
              [](const std::string& id,
                 const WebUIDataSource::GotDataCallback& callback) {
                callback.Run(new base::RefCountedString);
              }));
      WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                           data_source);
    }
  }

 protected:
  base::RunLoop* const run_loop_;
  std::unique_ptr<BrowserTargetImpl> browser_target_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWebUIController);
};

// TestWebUIController that additionally creates the ping test BrowserTarget
// implementation at the right time.
class PingTestWebUIController : public TestWebUIController {
 public:
  PingTestWebUIController(WebUI* web_ui, base::RunLoop* run_loop)
      : TestWebUIController(web_ui, run_loop) {}

  ~PingTestWebUIController() override {}

  void CreateHandler(mojo::PendingReceiver<mojom::BrowserTarget> receiver) {
    browser_target_ =
        std::make_unique<BrowserTargetImpl>(run_loop_, std::move(receiver));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PingTestWebUIController);
};

// WebUIControllerFactory that creates TestWebUIController.
class TestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  TestWebUIControllerFactory()
      : run_loop_(nullptr),
        registered_controllers_(
            {{"ping", base::BindRepeating(
                          &TestWebUIControllerFactory::CreatePingController,
                          base::Unretained(this))},
             {"hybrid", base::BindRepeating(
                            &TestWebUIControllerFactory::CreateHybridController,
                            base::Unretained(this))},
             {"webui_bindings",
              base::BindRepeating(
                  &TestWebUIControllerFactory::CreateWebUIController,
                  base::Unretained(this))}}) {}

  void set_run_loop(base::RunLoop* run_loop) { run_loop_ = run_loop; }

  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override {
    if (!web_ui_enabled_ || !url.SchemeIs(kChromeUIScheme))
      return nullptr;

    auto it = registered_controllers_.find(url.query());
    if (it != registered_controllers_.end())
      return it->second.Run(web_ui);

    return std::make_unique<TestWebUIController>(web_ui, run_loop_);
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
  bool UseWebUIBindingsForURL(BrowserContext* browser_context,
                              const GURL& url) override {
    return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
  }

  void set_web_ui_enabled(bool enabled) { web_ui_enabled_ = enabled; }

 private:
  std::unique_ptr<WebUIController> CreatePingController(WebUI* web_ui) {
    return std::make_unique<PingTestWebUIController>(web_ui, run_loop_);
  }

  std::unique_ptr<WebUIController> CreateHybridController(WebUI* web_ui) {
    return std::make_unique<TestWebUIController>(
        web_ui, run_loop_,
        BINDINGS_POLICY_WEB_UI | BINDINGS_POLICY_MOJO_WEB_UI);
  }

  std::unique_ptr<WebUIController> CreateWebUIController(WebUI* web_ui) {
    return std::make_unique<TestWebUIController>(web_ui, run_loop_,
                                                 BINDINGS_POLICY_WEB_UI);
  }

  base::RunLoop* run_loop_;
  bool web_ui_enabled_ = true;
  const base::flat_map<
      std::string,
      base::RepeatingCallback<std::unique_ptr<WebUIController>(WebUI*)>>
      registered_controllers_;

  DISALLOW_COPY_AND_ASSIGN(TestWebUIControllerFactory);
};

// Base for unit tests that need a ContentBrowserClient.
class TestWebUIContentBrowserClient : public ContentBrowserClient {
 public:
  TestWebUIContentBrowserClient() {}
  TestWebUIContentBrowserClient(const TestWebUIContentBrowserClient&) = delete;
  TestWebUIContentBrowserClient& operator=(
      const TestWebUIContentBrowserClient&) = delete;
  ~TestWebUIContentBrowserClient() override {}

  void RegisterBrowserInterfaceBindersForFrame(
      service_manager::BinderMapWithContext<content::RenderFrameHost*>* map)
      override {
    map->Add<mojom::BrowserTarget>(
        base::BindRepeating(&TestWebUIContentBrowserClient::BindBrowserTarget,
                            base::Unretained(this)));
  }
  void BindBrowserTarget(content::RenderFrameHost* render_frame_host,
                         mojo::PendingReceiver<mojom::BrowserTarget> receiver) {
    auto* contents = WebContents::FromRenderFrameHost(render_frame_host);
    static_cast<PingTestWebUIController*>(contents->GetWebUI()->GetController())
        ->CreateHandler(std::move(receiver));
  }
};

class WebUIMojoTest : public ContentBrowserTest {
 public:
  WebUIMojoTest() {
    WebUIControllerFactory::RegisterFactory(&factory_);
  }

  ~WebUIMojoTest() override {
    WebUIControllerFactory::UnregisterFactoryForTesting(&factory_);
  }

  TestWebUIControllerFactory* factory() { return &factory_; }

  void NavigateWithNewWebUI(const std::string& path) {
    // Load a dummy WebUI URL first so that a new WebUI is set up when we load
    // the URL we're actually interested in.
    EXPECT_TRUE(NavigateToURL(shell(), GetWebUIURL("dummy-web-ui")));
    EXPECT_TRUE(NavigateToURL(shell(), GetWebUIURL("mojo-web-ui/" + path)));
  }

  // Run |script| and return a boolean result.
  bool RunBoolFunction(const std::string& script) {
    bool result = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell()->web_contents(), "domAutomationController.send(" + script + ")",
        &result));
    return result;
  }

 protected:
  void SetUpOnMainThread() override {
    original_client_ = SetBrowserClientForTesting(&client_);
  }

  void TearDownOnMainThread() override {
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

 private:
  TestWebUIControllerFactory factory_;
  ContentBrowserClient* original_client_ = nullptr;
  TestWebUIContentBrowserClient client_;

  DISALLOW_COPY_AND_ASSIGN(WebUIMojoTest);
};

bool IsGeneratedResourceAvailable(const std::string& resource_path) {
  // Currently there is no way to have a generated file included in the isolate
  // files. If the bindings file doesn't exist assume we're on such a bot and
  // pass.
  // TODO(sky): remove this conditional when isolates support copying from gen.
  base::ScopedAllowBlockingForTesting allow_blocking;
  const base::FilePath test_file_path(GetFilePathForJSResource(resource_path));
  if (base::PathExists(test_file_path))
    return true;
  LOG(WARNING) << " mojom binding file doesn't exist, assuming on isolate";
  return false;
}

// Loads a webui page that contains mojo bindings and verifies a message makes
// it from the browser to the page and back.
IN_PROC_BROWSER_TEST_F(WebUIMojoTest, EndToEndPing) {
  if (!IsGeneratedResourceAvailable(
          "content/test/data/web_ui_test_mojo_bindings.mojom-lite.js"))
    return;
  GURL test_url(GetWebUIURL("mojo-web-ui/web_ui_mojo.html?ping"));

  {
    g_got_message = false;
    base::RunLoop run_loop;
    factory()->set_run_loop(&run_loop);
    EXPECT_TRUE(NavigateToURL(shell(), test_url));
    // RunLoop is quit when message received from page.
    run_loop.Run();
    EXPECT_TRUE(g_got_message);
  }

  {
    // Check that a second shell works correctly.
    Shell* other_shell = CreateBrowser();
    g_got_message = false;
    base::RunLoop other_run_loop;
    factory()->set_run_loop(&other_run_loop);
    EXPECT_TRUE(NavigateToURL(other_shell, test_url));
    // RunLoop is quit when message received from page.
    other_run_loop.Run();
    EXPECT_TRUE(g_got_message);

    // We expect two independent chrome://foo tabs/shells to use a separate
    // process.
    EXPECT_NE(shell()->web_contents()->GetMainFrame()->GetProcess(),
              other_shell->web_contents()->GetMainFrame()->GetProcess());

    // Close the second shell and wait until its process exits.
    RenderProcessHostWatcher process_watcher(
        other_shell->web_contents()->GetMainFrame()->GetProcess(),
        RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    other_shell->Close();
    process_watcher.Wait();
  }

  {
    // Check that a third shell works correctly, even if we force it to share a
    // process with the first shell, by forcing an artificially low process
    // limit.
    RenderProcessHost::SetMaxRendererProcessCount(1);

    Shell* other_shell = CreateBrowser();
    g_got_message = false;
    base::RunLoop other_run_loop;
    factory()->set_run_loop(&other_run_loop);
    EXPECT_TRUE(NavigateToURL(other_shell, test_url));
    // RunLoop is quit when message received from page.
    other_run_loop.Run();
    EXPECT_TRUE(g_got_message);
    EXPECT_EQ(shell()->web_contents()->GetMainFrame()->GetProcess(),
              other_shell->web_contents()->GetMainFrame()->GetProcess());
  }
}

// Disabled due to flakiness: crbug.com/860385.
#if defined(OS_ANDROID)
#define MAYBE_NativeMojoAvailable DISABLED_NativeMojoAvailable
#else
#define MAYBE_NativeMojoAvailable NativeMojoAvailable
#endif
IN_PROC_BROWSER_TEST_F(WebUIMojoTest, MAYBE_NativeMojoAvailable) {
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
#if defined(OS_ANDROID)
#define MAYBE_ChromeSendAvailable DISABLED_ChromeSendAvailable
#else
#define MAYBE_ChromeSendAvailable ChromeSendAvailable
#endif
IN_PROC_BROWSER_TEST_F(WebUIMojoTest, MAYBE_ChromeSendAvailable) {
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

}  // namespace
}  // namespace content
