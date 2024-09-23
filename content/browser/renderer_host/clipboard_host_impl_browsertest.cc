// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/permissions_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "url/origin.h"

namespace content {

// End-to-end tests for clipboard file access.

class ClipboardHostImplBrowserTest : public ContentBrowserTest {
 public:
  struct File {
    std::string name;
    std::string type;
  };

  void SetUp() override {
    ASSERT_TRUE(embedded_https_test_server().Start());
    ui::TestClipboard::CreateForCurrentThread();
    ContentBrowserTest::SetUp();
  }

  void TearDown() override { ContentBrowserTest::TearDown(); }

  RenderFrameHost* GetRenderFrameHost() {
    return ToRenderFrameHost(shell()->web_contents()).render_frame_host();
  }

  void CopyPasteFiles(std::vector<File> files) {
    ASSERT_TRUE(NavigateToURL(
        shell(), embedded_https_test_server().GetURL("/title1.html")));
    // Create a promise that will resolve on paste with comma-separated
    // '<name>:<type>:<b64-content>' of each file on the clipboard.
    ASSERT_TRUE(
        ExecJs(shell(),
               "var p = new Promise((resolve, reject) => {"
               "  window.document.onpaste = async (event) => {"
               "    const data = event.clipboardData;"
               "    const files = [];"
               "    for (let i = 0; i < data.items.length; i++) {"
               "      if (data.items[i].kind != 'file') {"
               "        reject('The clipboard item[' + i +'] was of kind: ' +"
               "               data.items[i].kind + '. Expected file.');"
               "      }"
               "      files.push(data.files[i]);"
               "    }"
               "    const result = [];"
               "    for (let i = 0; i < files.length; i++) {"
               "      const file = files[i];"
               "      const buf = await file.arrayBuffer();"
               "      const buf8 = new Uint8Array(buf);"
               "      const b64 = btoa(String.fromCharCode(...buf8));"
               "      result.push(file.name + ':' + file.type + ':' + b64);"
               "    }"
               "    resolve(result.join(','));"
               "  };"
               "});"));

    // Put files on clipboard.
    base::FilePath source_root;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));
    std::vector<std::string> expected;
    std::vector<ui::FileInfo> file_infos;
    std::vector<std::u16string> file_paths;
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      for (const auto& f : files) {
        base::FilePath file =
            source_root.AppendASCII("content/test/data/clipboard")
                .AppendASCII(f.name);
        std::string buf;
        EXPECT_TRUE(base::ReadFileToString(file, &buf));
        auto b64 = base::Base64Encode(base::as_bytes(base::make_span(buf)));
        expected.push_back(base::JoinString({f.name, f.type, b64}, ":"));
        file_infos.push_back(ui::FileInfo(file, base::FilePath()));
        file_paths.push_back(file.AsUTF16Unsafe());
      }
      ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
      // Write both filenames (text/uri-list) and the full file paths
      // (text/plain), and validate in the test that only the Files are exposed
      // in the renderer (item.kind == 'file') and the String full paths are not
      // included (http://crbug.com/1214108).
      writer.WriteFilenames(ui::FileInfosToURIList(file_infos));
      writer.WriteText(base::JoinString(file_paths, u"\n"));
    }

    // Send paste event and wait for JS promise to resolve with file data.
    shell()->web_contents()->Paste();
    EXPECT_EQ(base::JoinString(expected, ","), EvalJs(shell(), "p"));
  }
};

IN_PROC_BROWSER_TEST_F(ClipboardHostImplBrowserTest, TextFile) {
  CopyPasteFiles({File{"hello.txt", "text/plain"}});
}

IN_PROC_BROWSER_TEST_F(ClipboardHostImplBrowserTest, ImageFile) {
  CopyPasteFiles({File{"small.jpg", "image/jpeg"}});
}

IN_PROC_BROWSER_TEST_F(ClipboardHostImplBrowserTest, Empty) {
  CopyPasteFiles({});
}

IN_PROC_BROWSER_TEST_F(ClipboardHostImplBrowserTest, Multiple) {
  CopyPasteFiles({
      File{"hello.txt", "text/plain"},
      File{"small.jpg", "image/jpeg"},
  });
}

class ClipboardDocUrlBrowserTestP : public ClipboardHostImplBrowserTest,
                                    public testing::WithParamInterface<bool> {
 public:
  ClipboardDocUrlBrowserTestP() = default;
};

INSTANTIATE_TEST_SUITE_P(ClipboardDocUrlBrowserTests,
                         ClipboardDocUrlBrowserTestP,
                         testing::Values(true, false));

IN_PROC_BROWSER_TEST_P(ClipboardDocUrlBrowserTestP, HtmlUrl) {
  GURL main_url(embedded_https_test_server().GetURL("/title1.html"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));
  PermissionController* permission_controller =
      GetRenderFrameHost()->GetBrowserContext()->GetPermissionController();
  url::Origin origin = url::Origin::Create(main_url);
  SetPermissionControllerOverrideForDevTools(
      permission_controller, origin,
      blink::PermissionType::CLIPBOARD_SANITIZED_WRITE,
      blink::mojom::PermissionStatus::GRANTED);
  base::RunLoop loop;
  ASSERT_TRUE(ExecJs(
      shell(),
      " const format1 = 'text/html';"
      " const textInput = '<p>Hello</p>';"
      " const blobInput1 = new Blob([textInput], {type: format1});"
      " const clipboardItemInput = new ClipboardItem({[format1]: blobInput1});"
      " navigator.clipboard.write([clipboardItemInput]);"));
  loop.RunUntilIdle();
  // Read HTML format to check that the URL is populated correctly during
  // write().
  std::u16string html;
  std::string src_url;
  uint32_t fragment_start;
  uint32_t fragment_end;
  ui::Clipboard::GetForCurrentThread()->ReadHTML(
      ui::ClipboardBuffer::kCopyPaste, nullptr, &html, &src_url,
      &fragment_start, &fragment_end);
  EXPECT_EQ(src_url, main_url.spec());
}

class ClipboardBrowserTest : public ClipboardHostImplBrowserTest {
 public:
  ClipboardBrowserTest() = default;

  void SetPermissionOverrideForAsyncClipboardTests(
      blink::mojom::PermissionStatus status) {
    content::PermissionController* permission_controller =
        GetRenderFrameHost()->GetBrowserContext()->GetPermissionController();
    url::Origin origin = url::Origin::Create(
        embedded_https_test_server().GetURL("/title1.html"));
    SetPermissionControllerOverrideForDevTools(
        permission_controller, origin,
        blink::PermissionType::CLIPBOARD_READ_WRITE, status);
  }

  void SetPermissionOverrideForStrictlyProcessedWriteTests(
      blink::mojom::PermissionStatus status) {
    content::PermissionController* permission_controller =
        GetRenderFrameHost()->GetBrowserContext()->GetPermissionController();
    url::Origin origin = url::Origin::Create(
        embedded_https_test_server().GetURL("/title1.html"));
    SetPermissionControllerOverrideForDevTools(
        permission_controller, origin,
        blink::PermissionType::CLIPBOARD_SANITIZED_WRITE, status);
  }

  void NavigateAndSetFocusToPage() {
    ASSERT_TRUE(NavigateToURL(
        shell(), embedded_https_test_server().GetURL("/title1.html")));
    shell()->web_contents()->Focus();
  }
};

IN_PROC_BROWSER_TEST_F(ClipboardBrowserTest, EmptyClipboard) {
  base::HistogramTester histogram_tester;
  NavigateAndSetFocusToPage();
  SetPermissionOverrideForAsyncClipboardTests(
      blink::mojom::PermissionStatus::GRANTED);
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  ASSERT_TRUE(ExecJs(shell(), " navigator.clipboard.read()"));
  content::FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectBucketCount("Blink.Clipboard.Read.NumberOfFormats", 0,
                                     1);
}

IN_PROC_BROWSER_TEST_F(ClipboardBrowserTest, NumberOfFormatsOnRead) {
  base::HistogramTester histogram_tester;
  NavigateAndSetFocusToPage();
  SetPermissionOverrideForAsyncClipboardTests(
      blink::mojom::PermissionStatus::GRANTED);
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
  ASSERT_TRUE(ExecJs(shell(), " navigator.clipboard.read()"));
  SetPermissionOverrideForStrictlyProcessedWriteTests(
      blink::mojom::PermissionStatus::GRANTED);
  ASSERT_TRUE(ExecJs(
      shell(),
      " const format1 = 'text/html';"
      " const textInput = '<p>Hello</p>';"
      " const blobInput1 = new Blob([textInput], {type: format1});"
      " const clipboardItemInput = new ClipboardItem({[format1]: blobInput1});"
      " navigator.clipboard.write([clipboardItemInput]);"));
  ASSERT_TRUE(ExecJs(shell(), " navigator.clipboard.read()"));
  content::FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectBucketCount("Blink.Clipboard.Read.NumberOfFormats", 0,
                                     1);
  histogram_tester.ExpectBucketCount("Blink.Clipboard.Read.NumberOfFormats", 1,
                                     1);
}

}  // namespace content
