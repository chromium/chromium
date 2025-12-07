// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/permissions_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/clipboard_observer.h"
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
        auto b64 = base::Base64Encode(base::as_byte_span(buf));
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
  SetPermissionControllerOverride(
      permission_controller, origin, origin,
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
    SetPermissionControllerOverride(permission_controller, origin, origin,
                                    blink::PermissionType::CLIPBOARD_READ_WRITE,
                                    status);
  }

  void SetPermissionOverrideForStrictlyProcessedWriteTests(
      blink::mojom::PermissionStatus status) {
    content::PermissionController* permission_controller =
        GetRenderFrameHost()->GetBrowserContext()->GetPermissionController();
    url::Origin origin = url::Origin::Create(
        embedded_https_test_server().GetURL("/title1.html"));
    SetPermissionControllerOverride(
        permission_controller, origin, origin,
        blink::PermissionType::CLIPBOARD_SANITIZED_WRITE, status);
  }

  void NavigateAndSetFocusToPage() {
    ASSERT_TRUE(NavigateToURL(
        shell(), embedded_https_test_server().GetURL("/title1.html")));
    shell()->web_contents()->Focus();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kClipboardChangeEvent};
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

namespace {
bool IsUint128(std::string_view data) {
  absl::uint128 deserialized;
  return absl::SimpleAtoi(data, &deserialized);
}
}  // namespace

IN_PROC_BROWSER_TEST_F(ClipboardBrowserTest, ClipboardChangeEvent) {
  NavigateAndSetFocusToPage();
  SetPermissionOverrideForStrictlyProcessedWriteTests(
      blink::mojom::PermissionStatus::GRANTED);

  // Set up a listener for the clipboardchange event.
  const char write_text_and_print_change_id[] = R"JS(
    (async () => {
      var p = new Promise((resolve, reject) => {
        navigator.clipboard.addEventListener('clipboardchange', (event) => {
          resolve(event.changeId.toString());
          resolve();
        }, {once: true});
      });
      await navigator.clipboard.writeText("Cthulhu");
      return await p;
    })();
  )JS";

  auto first_try_result =
      EvalJs(shell(), write_text_and_print_change_id).ExtractString();
  EXPECT_TRUE(IsUint128(first_try_result))
      << "Result is not Uint128, instead: " << first_try_result;

  auto second_try_result =
      EvalJs(shell(), write_text_and_print_change_id).ExtractString();
  EXPECT_TRUE(IsUint128(second_try_result))
      << "Result is not Uint128, instead: " << second_try_result;

  EXPECT_NE(first_try_result, second_try_result);
}

namespace {
class ClipboardEventsCounter : public ui::ClipboardObserver {
 public:
  explicit ClipboardEventsCounter(uint32_t wait_for_this_many_events)
      : countdown_(wait_for_this_many_events) {
    CHECK_GT(wait_for_this_many_events, 0);
  }

  void OnClipboardDataChanged() override {
    if (events_received_.IsReady()) {
      return;
    }
    if (!--countdown_) {
      events_received_.SetValue();
    }
  }

  bool WaitUntlReceived() { return events_received_.Wait(); }

 private:
  uint32_t countdown_ = 0;
  base::test::TestFuture<void> events_received_;
};
}  // namespace

IN_PROC_BROWSER_TEST_F(ClipboardBrowserTest,
                       ClipboardChangeEventNoDuplicateEvents) {
  NavigateAndSetFocusToPage();
  SetPermissionOverrideForStrictlyProcessedWriteTests(
      blink::mojom::PermissionStatus::GRANTED);

  // Stop updating sequence number so that implementation thinks that the
  // notifications have already been processed and discard them.
  auto* test_clipboard =
      static_cast<ui::TestClipboard*>(ui::Clipboard::GetForCurrentThread());
  test_clipboard->StopUpdatingSequenceNumberForTesting();
  auto* clipboard_monitor = ui::ClipboardMonitor::GetInstance();

  // Set up a listener for the clipboardchange event and write to trigger it.
  const char kWriteTextAndCollectChangeIds[] = R"JS(
    changeIds = [];
    listener = (event) => {
      changeIds.push(event.changeId.toString());
    }
    navigator.clipboard.addEventListener('clipboardchange', listener);

    const linesToWrite = [
      "And the Raven, never flitting, still is sitting, still is sitting     ",
      "On the pallid bust of Pallas just above my chamber door;              ",
      "And his eyes have all the seeming of a demon’s that is dreaming,      ",
      "And the lamp-light o’er him streaming throws his shadow on the floor; ",
      "And my soul from out that shadow that lies floating on the floor      ",
      "Shall be lifted—nevermore!                                            ",
      "                                                                      ",
      "// The Raven by Edgar Allan Poe (in public domain).                   "
    ];

    // Write a lot of lines, each should trigger a notification.
    for (const line of linesToWrite) {
      navigator.clipboard.writeText(line);
    }
  )JS";
  // This part is separate to ensure that browser side has received all the
  // notifications.
  const char kGetChangeIds[] = R"JS(
    navigator.clipboard.removeEventListener('clipboardchange', listener);
    changeIds;
  )JS";

  // Many notifications, but changeId stays the same. One event should be
  // dispatched.
  {
    ClipboardEventsCounter event_counter(/*wait_for_this_many_events=*/8);
    clipboard_monitor->AddObserver(&event_counter);

    ASSERT_TRUE(ExecJs(shell(), kWriteTextAndCollectChangeIds));

    // Notifications have been dispatched.
    ASSERT_TRUE(event_counter.WaitUntlReceived());

    auto result = EvalJs(shell(), kGetChangeIds);
    const auto& list = result.ExtractList();
    // Since the sequence number is artificially not being updated, only one
    // event should arrive (and rest discarded as already processed).
    EXPECT_EQ(list.size(), 1u) << list.DebugString();
    EXPECT_TRUE(IsUint128(list[0].GetString()));

    clipboard_monitor->RemoveObserver(&event_counter);
  }
  // Many notifications, but changeId the same as in already dispatched event.
  // No need to send any more events.
  {
    ClipboardEventsCounter event_counter(/*wait_for_this_many_events=*/8);
    clipboard_monitor->AddObserver(&event_counter);

    ASSERT_TRUE(ExecJs(shell(), kWriteTextAndCollectChangeIds));

    // Notifications have been dispatched.
    ASSERT_TRUE(event_counter.WaitUntlReceived());

    auto result = EvalJs(shell(), kGetChangeIds);
    const auto& list = result.ExtractList();
    // Since:
    // - The sequence number is artificially not being updated
    // - The last time already an event was sent
    // nothing should be dispatched this time.
    EXPECT_EQ(list.size(), 0u) << list.DebugString();

    clipboard_monitor->RemoveObserver(&event_counter);
  }

  test_clipboard->UpdateSequenceManuallyForTesting(
      ui::ClipboardBuffer::kCopyPaste);

  // Many notifications, and changeId changed again. Exactly one event should be
  // sent with the new one.
  {
    ClipboardEventsCounter event_counter(/*wait_for_this_many_events=*/8);
    clipboard_monitor->AddObserver(&event_counter);

    ASSERT_TRUE(ExecJs(shell(), kWriteTextAndCollectChangeIds));

    // Notifications have been dispatched.
    ASSERT_TRUE(event_counter.WaitUntlReceived());

    auto result = EvalJs(shell(), kGetChangeIds);
    const auto& list = result.ExtractList();
    // Sequence number changed, so again one event should be dispatched with the
    // new changeId.
    EXPECT_EQ(list.size(), 1u) << list.DebugString();
    EXPECT_TRUE(IsUint128(list[0].GetString()));

    clipboard_monitor->RemoveObserver(&event_counter);
  }
}

}  // namespace content
