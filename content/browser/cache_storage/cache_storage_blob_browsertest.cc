// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/browser/quota/quota_settings.h"

namespace content {

namespace {

void SetQuotaSettingsOnIOThread(
    scoped_refptr<storage::QuotaManager> quota_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  storage::QuotaSettings settings;
  settings.pool_size = 10LL * 1024 * 1024 * 1024;             // 10 GB
  settings.per_storage_key_quota = 5LL * 1024 * 1024 * 1024;  // 5 GB
  quota_manager->SetQuotaSettings(settings);
}

}  // namespace

class CacheStorageBlobBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    // Set a large quota for the test.
    base::RunLoop run_loop;
    storage::QuotaManager* quota_manager = shell()
                                               ->web_contents()
                                               ->GetBrowserContext()
                                               ->GetDefaultStoragePartition()
                                               ->GetQuotaManager();
    GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&SetQuotaSettingsOnIOThread,
                       base::RetainedRef(quota_manager)),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  void RunAndMatchFileTest(const std::string& script,
                           uint64_t file_size,
                           bool check_boundary) {
    base::test::ScopedRunLoopTimeout timeout(
        FROM_HERE, TestTimeouts::action_max_timeout() * 5);

    auto response =
        std::make_unique<net::test_server::ControllableHttpResponse>(
            embedded_test_server(), "/test");
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    ASSERT_TRUE(NavigateToURL(
        shell(), embedded_test_server()->GetURL("/cache_storage/test.html")));

    // Open a cache and fetch a resource. The fetch will hang until we send a
    // response from the server.
    ASSERT_TRUE(ExecJs(shell(), script));

    SendFile(std::move(response), file_size);

    auto result = EvalJs(shell(), "window.p");
    ASSERT_TRUE(result.is_ok());
    ASSERT_TRUE(result.is_dict());

    const base::Value::Dict& result_dict = result.ExtractDict();
    const std::string* text = result_dict.FindString("text");
    ASSERT_TRUE(text);
    std::string expected_prefix = CreateChunk(80);
    EXPECT_EQ(expected_prefix, *text);

    if (check_boundary) {
      const std::string* boundary_text = result_dict.FindString("boundaryText");
      ASSERT_TRUE(boundary_text);
      EXPECT_EQ(expected_prefix, *boundary_text);
    }

    std::optional<double> length = result_dict.FindDouble("length");
    ASSERT_TRUE(length);
    EXPECT_EQ(file_size, static_cast<uint64_t>(*length));
  }

  void SendFile(
      std::unique_ptr<net::test_server::ControllableHttpResponse> response,
      uint64_t file_size) {
    // Wait for the request and then send the response.
    response->WaitForRequest();
    response->Send(
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n");

    constexpr uint64_t kChunkSize = 1024 * 1024;
    std::string chunk = CreateChunk(kChunkSize);

    std::string chunk_header =
        base::StringPrintf("%" PRIx64 "\r\n", kChunkSize);
    for (uint64_t i = 0; i < file_size / kChunkSize; ++i) {
      response->Send(chunk_header);
      response->Send(chunk);
      response->Send("\r\n");
    }

    response->Send("0\r\n\r\n");
    response->Done();
  }

  std::string CreateChunk(uint64_t size) {
    constexpr std::string kChunkPattern = "abcdefgh";

    std::string chunk;
    chunk.reserve(size);
    for (uint64_t i = 0; i < size / kChunkPattern.length(); ++i) {
      chunk += kChunkPattern;
    }
    return chunk;
  }
};

IN_PROC_BROWSER_TEST_F(CacheStorageBlobBrowserTest, PutAndMatch) {
  constexpr uint64_t kFileSize = 100LL * 1024 * 1024;
  RunAndMatchFileTest(
      R"((async () => {
        window.p = new Promise(async (resolve, reject) => {
          const cache = await caches.open('test-cache');
          const response = await fetch('/test');
          await cache.put('test-request', response);

          const cachedResponse = await cache.match('test-request');
          if (typeof cachedResponse === 'undefined') {
            reject('undefined');
            return;
          }

          const blob = await cachedResponse.blob();
          const prefixBlob = blob.slice(0, 80);
          const prefixText = await prefixBlob.text();

          resolve({text: prefixText, length: blob.size});
        });
      })())",
      kFileSize, /*check_boundary=*/false);
}

IN_PROC_BROWSER_TEST_F(CacheStorageBlobBrowserTest, AddAndMatch) {
  constexpr uint64_t kFileSize = 100LL * 1024 * 1024;
  RunAndMatchFileTest(
      R"((async () => {
        window.p = new Promise(async (resolve, reject) => {
          const cache = await caches.open('test-cache');
          await cache.add('/test');

          const cachedResponse = await cache.match('/test');
          if (typeof cachedResponse === 'undefined') {
            reject('undefined');
            return;
          }

          const blob = await cachedResponse.blob();
          const prefixBlob = blob.slice(0, 80);
          const prefixText = await prefixBlob.text();

          resolve({text: prefixText, length: blob.size});
        });
      })())",
      kFileSize, /*check_boundary=*/false);
}

// Tests for large files (2GB+ file size) are set to manual due to the execution
// time and the disk space limitation.
IN_PROC_BROWSER_TEST_F(CacheStorageBlobBrowserTest, MANUAL_PutAndMatchLarge) {
  constexpr uint64_t kLargeFileSize = (2LL * 1024 + 1) * 1024 * 1024;
  RunAndMatchFileTest(
      R"((async () => {
        window.p = new Promise(async (resolve, reject) => {
          const cache = await caches.open('test-cache');
          const response = await fetch('/test');
          await cache.put('test-request', response);

          const cachedResponse = await cache.match('test-request');
          if (typeof cachedResponse === 'undefined') {
            reject('undefined');
            return;
          }

          const blob = await cachedResponse.blob();
          const prefixBlob = blob.slice(0, 80);
          const prefixText = await prefixBlob.text();

          const kInt32Max = 2147483647;
          const boundaryBlob = blob.slice(kInt32Max - 47, kInt32Max + 33);
          const boundaryText = await boundaryBlob.text();

          resolve({text: prefixText, boundaryText: boundaryText,
                   length: blob.size});
        });
      })())",
      kLargeFileSize, /*check_boundary=*/true);
}

IN_PROC_BROWSER_TEST_F(CacheStorageBlobBrowserTest, MANUAL_AddAndMatchLarge) {
  constexpr uint64_t kLargeFileSize = (2LL * 1024 + 1) * 1024 * 1024;
  RunAndMatchFileTest(
      R"((async () => {
        window.p = new Promise(async (resolve, reject) => {
          const cache = await caches.open('test-cache');
          await cache.add('/test');

          const cachedResponse = await cache.match('/test');
          if (typeof cachedResponse === 'undefined') {
            reject('undefined');
            return;
          }

          const blob = await cachedResponse.blob();
          const prefixBlob = blob.slice(0, 80);
          const prefixText = await prefixBlob.text();

          const kInt32Max = 2147483647;
          const boundaryBlob = blob.slice(kInt32Max - 47, kInt32Max + 33);
          const boundaryText = await boundaryBlob.text();

          resolve({text: prefixText, boundaryText: boundaryText,
                   length: blob.size});
        });
      })())",
      kLargeFileSize, /*check_boundary=*/true);
}

}  // namespace content
