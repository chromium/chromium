// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <fstream>
#include <memory>

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "content/browser/webui/url_data_source_impl.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_ui_browsertest_util.h"
#include "content/shell/browser/shell.h"
#include "content/test/grit/web_ui_mojo_test_resources.h"
#include "content/test/grit/web_ui_mojo_test_resources_map.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/tests/validation_test_input_parser.h"
#include "mojo/public/interfaces/bindings/tests/validation_test_interfaces.mojom.h"
#include "ui/base/webui/resource_path.h"

namespace content {
namespace {

std::vector<base::FilePath> EnumerateFilesInDirectory(
    const base::FilePath& dir,
    const base::FilePath::StringType& pattern) {
  std::vector<base::FilePath> test_files;

  base::FileEnumerator e(dir, false, base::FileEnumerator::FILES, pattern);
  e.ForEach([&test_files](const base::FilePath& file_path) -> void {
    test_files.push_back(file_path);
  });

  return test_files;
}

// Reads the content of a file to a string. Crashes if reading failed.
std::string ReadFile(const base::FilePath& file_path) {
  std::string content;
  CHECK(base::ReadFileToString(file_path, &content));
  return std::string(
      base::TrimWhitespaceASCII(content, base::TrimPositions::TRIM_TRAILING));
}

std::vector<mojo::test::TestCasePtr> ParseTestCases() {
  base::FilePath base_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &base_dir);
  const auto test_dir_relative_path = base::FilePath::FromASCII(
      "mojo/public/interfaces/bindings/tests/data/"
      "validation/");
  base::FilePath test_cases_dir = base_dir.Append(test_dir_relative_path);
  std::vector<base::FilePath> test_cases = EnumerateFilesInDirectory(
      test_cases_dir, FILE_PATH_LITERAL("conformance_*.data"));

  std::vector<mojo::test::TestCasePtr> parsed_test_cases;
  for (const base::FilePath& test_file : test_cases) {
    std::string test_data_raw = ReadFile(test_file);

    std::vector<uint8_t> bytes;
    size_t num_handles;
    std::string error_msg;
    CHECK(mojo::test::ParseValidationTestInput(test_data_raw, &bytes,
                                               &num_handles, &error_msg));

    std::string expectation = ReadFile(
        test_file.RemoveFinalExtension().AddExtensionASCII("expected"));

    parsed_test_cases.push_back(mojo::test::TestCase::New(
        test_file.BaseName().RemoveFinalExtension().MaybeAsASCII(), bytes,
        expectation));
  }
  CHECK(parsed_test_cases.size() > 0)
      << "No test cases were found, there might be an issue with pathing, "
         "looked at: "
      << test_cases_dir;

  return parsed_test_cases;
}

// WebUIController that sets up mojo bindings.
class ConformanceTestWebUIController : public WebUIController,
                                       public mojo::test::PageHandlerFactory {
 public:
  explicit ConformanceTestWebUIController(WebUI* web_ui)
      : WebUIController(web_ui) {
    const base::span<const webui::ResourcePath> kMojoWebUiResources =
        base::make_span(kWebUiMojoTestResources);

    web_ui->SetBindings(
        {BindingsPolicyValue::kMojoWebUi, BindingsPolicyValue::kWebUi});
    WebUIDataSource* data_source = WebUIDataSource::CreateAndAdd(
        web_ui->GetWebContents()->GetBrowserContext(),
        "mojo-web-ui-conformance");
    data_source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ScriptSrc,
        "script-src chrome://resources 'self';");
    data_source->AddResourcePaths(kMojoWebUiResources);
    data_source->AddResourcePath("", IDR_WEB_UI_TS_TEST_CONFORMANCE_HTML);
  }

  ConformanceTestWebUIController(const ConformanceTestWebUIController&) =
      delete;
  ConformanceTestWebUIController& operator=(
      const ConformanceTestWebUIController&) = delete;

  void BindInterface(
      mojo::PendingReceiver<mojo::test::PageHandlerFactory> receiver) {
    page_factory_receiver_.reset();
    page_factory_receiver_.Bind(std::move(receiver));
  }

 private:
  void GetTestCases(GetTestCasesCallback cb) override {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            [](GetTestCasesCallback cb,
               const scoped_refptr<base::SequencedTaskRunner>& runner) {
              auto test_cases = ParseTestCases();
              runner->PostTask(
                  FROM_HERE,
                  base::BindOnce(std::move(cb), std::move(test_cases)));
            },
            std::move(cb), base::SequencedTaskRunner::GetCurrentDefault()));
  }

  mojo::Receiver<mojo::test::PageHandlerFactory> page_factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

WEB_UI_CONTROLLER_TYPE_IMPL(ConformanceTestWebUIController)

class ConformanceTestWebUIControllerFactory : public WebUIControllerFactory {
 public:
  ConformanceTestWebUIControllerFactory() = default;
  ConformanceTestWebUIControllerFactory(const TestWebUIControllerFactory&) =
      delete;
  ConformanceTestWebUIControllerFactory& operator=(
      const TestWebUIControllerFactory&) = delete;

  std::unique_ptr<WebUIController> CreateWebUIControllerForURL(
      WebUI* web_ui,
      const GURL& url) override {
    return std::make_unique<ConformanceTestWebUIController>(web_ui);
  }

  WebUI::TypeID GetWebUIType(BrowserContext* browser_context,
                             const GURL& url) override {
    if (!url.SchemeIs(kChromeUIScheme)) {
      return WebUI::kNoWebUI;
    }
    return reinterpret_cast<WebUI::TypeID>(1);
  }

  bool UseWebUIForURL(BrowserContext* browser_context,
                      const GURL& url) override {
    return true;
  }
};

class ConformanceTestWebUIClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  ConformanceTestWebUIClient() = default;
  ConformanceTestWebUIClient(const ConformanceTestWebUIClient&) = delete;
  ConformanceTestWebUIClient& operator=(const ConformanceTestWebUIClient&) =
      delete;

  void RegisterBrowserInterfaceBindersForFrame(
      RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* map) override {
    RegisterWebUIControllerInterfaceBinder<mojo::test::PageHandlerFactory,
                                           ConformanceTestWebUIController>(map);
  }
};

class WebUIMojoConformanceTest : public ContentBrowserTest {
 public:
  WebUIMojoConformanceTest() = default;
  WebUIMojoConformanceTest(const WebUIMojoConformanceTest&) = delete;
  WebUIMojoConformanceTest& operator=(const WebUIMojoConformanceTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    client_ = std::make_unique<ConformanceTestWebUIClient>();
  }
  void TearDownOnMainThread() override {}

  ConformanceTestWebUIControllerFactory factory_;
  ScopedWebUIControllerFactoryRegistration factory_registration_{&factory_};
  std::unique_ptr<ConformanceTestWebUIClient> client_;
};

IN_PROC_BROWSER_TEST_F(WebUIMojoConformanceTest, ConformanceTest) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("data:,foo")));

  GURL kTestUrl(GetWebUIURL("mojo-web-ui-conformance/"));
  const std::string kTestScript = "runTest();";
  EXPECT_TRUE(NavigateToURL(shell(), kTestUrl));
  EXPECT_EQ(true, EvalJs(shell()->web_contents(), kTestScript));
}

}  // namespace
}  // namespace content
