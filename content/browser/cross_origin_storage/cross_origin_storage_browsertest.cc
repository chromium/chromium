// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browsing_data/browsing_data_browsertest_utils.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_file_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_writer.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

// Cross-Origin Storage is exposed only to secure contexts, so these tests run
// against an HTTPS server. CERT_TEST_NAMES covers a.test, b.test and c.test,
// which stand in for unrelated sites sharing the cache.
constexpr char kOriginA[] = "a.test";
constexpr char kOriginB[] = "b.test";
constexpr char kOriginC[] = "c.test";

// Digests below were produced with:
//   printf '%s' '<content>' | sha256sum
constexpr char kContent[] = "Hello, Cross-Origin Storage!";
constexpr char kContentDigest[] =
    "45f1abd8700c5686f77e47b184502dd40cf9752ef7c575d045ba87cd9a508a26";

constexpr char kSharedContent[] = "shared with any origin";
constexpr char kSharedContentDigest[] =
    "fb216316dfb9233f64423eabb1ace6134b77c044766789b365f7b2355454755a";

// Digest for empty content (""), computed with:
//   printf '' | sha256sum
constexpr char kEmptyContentDigest[] =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

// Intentionally does not hash to `kContentDigest`.
constexpr char kMismatchedContent[] = "mismatched bytes";

// Returns script that creates a writable file handle for `digest`, writes
// `content`, and closes the stream, resolving to "ok" or to the name of the
// rejection.
std::string WriteContentScript(const std::string& content,
                               const std::string& digest,
                               const std::string& origins_literal) {
  return base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                       digest, R"('},
            {create: true, origins: )",
                       origins_literal, R"(});
        const writable = await handle.createWritable();
        await writable.write(')",
                       content, R"(');
        await writable.close();
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"});
}

// Returns script that requests a read handle for `digest` and resolves to
// "ok" or to the name of the rejection.
std::string RequestReadHandleScript(const std::string& digest) {
  return base::StrCat({R"(
    (async () => {
      try {
        await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                       digest, R"('});
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"});
}

// Returns script that reads `digest` back via `getFileHandle` + `getFile()`
// and resolves to its text, or to the name of the rejection.
std::string RequestFileTextScript(const std::string& digest) {
  return base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                       digest, R"('});
        const file = await handle.getFile();
        return await file.text();
      } catch (e) {
        return e.name;
      }
    })()
  )"});
}

}  // namespace

class CrossOriginStorageBrowserTest : public ContentBrowserTest {
 public:
  CrossOriginStorageBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kCrossOriginStorage);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    browsing_data_browsertest_utils::SetIgnoreCertificateErrors(command_line);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    if (IsOutOfProcessNetworkService()) {
      browsing_data_browsertest_utils::SetUpMockCertVerifier(net::OK);
    }

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->RegisterRequestHandler(base::BindRepeating(
        &CrossOriginStorageBrowserTest::HandleRequest, base::Unretained(this)));
    https_server_->AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(https_server_->Start());
  }

 protected:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/cos_service_worker.js") {
      return nullptr;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/javascript");
    response->set_content(base::StrCat({R"(
      self.onmessage = async (e) => {
        const port = e.ports[0];
        // 1. Valid write request rejects with NotAllowedError.
        try {
          await navigator.crossOriginStorage.getFileHandle(
              {algorithm: 'SHA-256', value: ')",
                                        kSharedContentDigest, R"('},
              {create: true, origins: '*'});
          port.postMessage('unexpected_write_success');
          return;
        } catch (err) {
          if (err.name !== 'NotAllowedError') {
            port.postMessage(`unexpected_write_error:${err.name}`);
            return;
          }
        }
        // 2. Valid read request rejects with NotAllowedError.
        try {
          await navigator.crossOriginStorage.getFileHandle(
              {algorithm: 'SHA-256', value: ')",
                                        kSharedContentDigest, R"('});
          port.postMessage('unexpected_read_success');
          return;
        } catch (err) {
          if (err.name !== 'NotAllowedError') {
            port.postMessage(`unexpected_read_error:${err.name}`);
            return;
          }
        }
        // 3. A request with invalid hash/origins still rejects with
        // NotAllowedError rather than TypeError because we are in a service
        // worker.
        try {
          await navigator.crossOriginStorage.getFileHandle(
              {algorithm: 'SHA-1', value: 'invalid'},
              {create: true, origins: 'http://insecure.test'});
          port.postMessage('unexpected_invalid_success');
        } catch (err) {
          port.postMessage(err.name);
        }
      };
    )"}));
    return response;
  }

  net::EmbeddedTestServer* https_server() { return https_server_.get(); }

  // Navigates to an empty page on `host` so that script can run there.
  void NavigateToHost(const std::string& host) {
    ASSERT_TRUE(
        NavigateToURL(shell(), https_server()->GetURL(host, "/empty.html")));
  }

  base::FilePath RootDirectory() {
    return shell()->web_contents()->GetBrowserContext()->GetPath().Append(
        FILE_PATH_LITERAL("CrossOriginStorage"));
  }

  base::FilePath BlobPath(const std::string& digest) {
    return RootDirectory()
        .Append(FILE_PATH_LITERAL("blobs"))
        .AppendASCII("sha256")
        .AppendASCII(digest);
  }

  bool BlobExistsOnDisk(const std::string& digest) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::PathExists(BlobPath(digest));
  }

  // Nothing may be left behind in staging once an operation has finished;
  // staged files are unverified and must never become readable. Uses
  // `base::test::RunUntil` because `FileSystemUnderlyingSink::abort()` resets
  // its Mojo remote and resolves the JS promise before the browser receives the
  // pipe disconnect and finishes deleting the staging file on the background
  // sequence.
  bool StagingIsEmpty() {
    return base::test::RunUntil([this]() {
      base::ScopedAllowBlockingForTesting allow_blocking;
      return base::IsDirectoryEmpty(
          RootDirectory().Append(FILE_PATH_LITERAL("staging")));
    });
  }

  base::WeakPtr<FileSystemAccessManagerImpl> file_system_access_manager() {
    auto* storage_partition =
        static_cast<StoragePartitionImpl*>(shell()
                                               ->web_contents()
                                               ->GetBrowserContext()
                                               ->GetDefaultStoragePartition());
    return storage_partition->GetFileSystemAccessManager()->AsWeakPtr();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
};

// A written resource whose bytes match the declared digest commits to `blobs/`,
// leaves `staging/` empty, and is readable back in full via `handle.getFile()`.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       WriteAndRequestFileHandle) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("ok", EvalJs(shell(),
                         WriteContentScript(kContent, kContentDigest, "'*'")));
  EXPECT_TRUE(BlobExistsOnDisk(kContentDigest));
  EXPECT_TRUE(StagingIsEmpty());

  EXPECT_EQ(kContent, EvalJs(shell(), RequestFileTextScript(kContentDigest)));
}

// The returned FileSystemFileHandle and File are named after the digest,
// report the stored size, provide "application/octet-stream" MIME type, and
// sanitize lastModified to Unix epoch 0.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       RequestFileHandleAttributes) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  ASSERT_EQ("ok", EvalJs(shell(),
                         WriteContentScript(kContent, kContentDigest, "'*'")));

  auto result = EvalJs(shell(), base::StrCat({R"(
    (async () => {
      const handle = await navigator.crossOriginStorage.getFileHandle(
          {algorithm: 'SHA-256', value: ')",
                                              kContentDigest, R"('});
      const file = await handle.getFile();
      return [handle.name, file.name, file.size, file.type, file.lastModified];
    })()
  )"}));
  const auto& list = result.ExtractList();
  ASSERT_EQ(5u, list.size());
  EXPECT_EQ(kContentDigest, list[0].GetString());
  EXPECT_EQ(kContentDigest, list[1].GetString());
  EXPECT_EQ(static_cast<int>(std::string_view(kContent).size()),
            list[2].GetInt());
  EXPECT_EQ("application/octet-stream", list[3].GetString());
  EXPECT_EQ(0, list[4].GetInt());
}

// Content that does not hash to the declared digest must be rejected on
// `writable.close()` with DataError, and must not leave anything behind in
// either `blobs/` or `staging/`.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       WriteDigestMismatchRejectedOnClose) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("DataError",
            EvalJs(shell(), WriteContentScript(kMismatchedContent,
                                               kContentDigest, "'*'")));
  EXPECT_FALSE(BlobExistsOnDisk(kContentDigest));
  EXPECT_TRUE(StagingIsEmpty());
}

// Reading a digest that was never stored reports NotFoundError.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest, RequestUnknownHash) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("NotFoundError",
            EvalJs(shell(), RequestReadHandleScript(kContentDigest)));
}

// A malformed hash is rejected in the renderer, before any IPC is sent.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       RequestFileHandleInvalidHash) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("TypeError", EvalJs(shell(), R"(
    (async () => {
      try {
        await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: 'too-short'});
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"));
}

// An uppercase digest is rejected rather than normalized: the spec requires
// /^[0-9a-f]{64}$/ and compares hash values with an exact string comparison.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       RequestFileHandleUppercaseHash) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  std::string uppercase_digest = base::ToUpperASCII(kContentDigest);
  EXPECT_EQ("TypeError", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                       uppercase_digest, R"('});
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));
}

// The algorithm name, unlike the value, is matched case-insensitively.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       RequestFileHandleAlgorithmIsCaseInsensitive) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  // Reaching the browser at all means validation passed; the entry itself was
  // never stored.
  EXPECT_EQ("NotFoundError",
            EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'sha-256', value: ')",
                                          kContentDigest, R"('});
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));
}

// The cache is unpartitioned: content stored by one site with '*' sharing is
// readable by an unrelated site.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest, CrossOriginRead) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));
  ASSERT_EQ("ok",
            EvalJs(shell(), WriteContentScript(kSharedContent,
                                               kSharedContentDigest, "'*'")));

  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginB));
  EXPECT_EQ(kSharedContent,
            EvalJs(shell(), RequestFileTextScript(kSharedContentDigest)));
}

// Content shared with a specific origin is not disclosable to others and
// rejects with NotFoundError per the WICG spec (§3.1 & §8.4).
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       CrossOriginReadNotAllowed) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));
  const std::string allowed_origin =
      https_server()->GetOrigin(kOriginC).Serialize();
  ASSERT_EQ("ok",
            EvalJs(shell(), WriteContentScript(
                                kContent, kContentDigest,
                                base::StrCat({"['", allowed_origin, "']"}))));

  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginB));
  EXPECT_EQ("NotFoundError",
            EvalJs(shell(), RequestFileTextScript(kContentDigest)));

  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginC));
  EXPECT_EQ(kContent, EvalJs(shell(), RequestFileTextScript(kContentDigest)));
}

// The storing origin can always read back what it stored, even when the
// sharing scope names only other origins.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest, StoringOriginCanRead) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));
  const std::string allowed_origin =
      https_server()->GetOrigin(kOriginC).Serialize();
  ASSERT_EQ("ok",
            EvalJs(shell(), WriteContentScript(
                                kContent, kContentDigest,
                                base::StrCat({"['", allowed_origin, "']"}))));

  EXPECT_EQ(kContent, EvalJs(shell(), RequestFileTextScript(kContentDigest)));
}

// Read-only handles returned by `getFileHandle(hash)` (without
// `create: true`) must reject `createWritable()` and `remove()` so a renderer
// can never modify or delete a shared resource on disk.
// Calling `getFile()` on a `{create: true}` handle before writing must also
// reject with NotAllowedError so `{create: true}` is not an existence oracle.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       ReadHandleIsReadOnlyAndCreateHandleIsNotOracle) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));
  ASSERT_EQ("ok", EvalJs(shell(),
                         WriteContentScript(kContent, kContentDigest, "'*'")));

  // 1. A `{create: true}` handle on another origin cannot read existing bytes
  // before writing and verifying them itself.
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginB));
  EXPECT_EQ("NotAllowedError",
            EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const createHandle =
            await navigator.crossOriginStorage.getFileHandle(
                {algorithm: 'SHA-256', value: ')",
                                          kContentDigest, R"('},
                {create: true});
        await createHandle.getFile();
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));

  // 2. A `{create: false}` read handle cannot call `createWritable()`.
  EXPECT_EQ("NotAllowedError",
            EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const readHandle =
            await navigator.crossOriginStorage.getFileHandle(
                {algorithm: 'SHA-256', value: ')",
                                          kContentDigest, R"('});
        await readHandle.createWritable();
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));

  // 3. A `{create: false}` read handle cannot call `remove()`.
  EXPECT_EQ("NotAllowedError",
            EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const readHandle =
            await navigator.crossOriginStorage.getFileHandle(
                {algorithm: 'SHA-256', value: ')",
                                          kContentDigest, R"('});
        await readHandle.remove();
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));
}

// Storing the same content twice is deduplicated rather than duplicated on
// disk, leaves staging empty, and both storing origins retain access.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest, WriteDeduplicates) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));
  ASSERT_EQ("ok",
            EvalJs(shell(), WriteContentScript(kSharedContent,
                                               kSharedContentDigest, "'*'")));

  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginB));
  EXPECT_EQ("ok",
            EvalJs(shell(), WriteContentScript(kSharedContent,
                                               kSharedContentDigest, "'*'")));

  EXPECT_TRUE(BlobExistsOnDisk(kSharedContentDigest));
  EXPECT_TRUE(StagingIsEmpty());
  EXPECT_EQ(kSharedContent,
            EvalJs(shell(), RequestFileTextScript(kSharedContentDigest)));

  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));
  EXPECT_EQ(kSharedContent,
            EvalJs(shell(), RequestFileTextScript(kSharedContentDigest)));
}

// A same-site origin (`sub.a.test`) can read an entry stored by `a.test` even
// when no `origins` sharing list was specified.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest, SameSiteCanRead) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));
  ASSERT_EQ("ok", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                kContentDigest, R"('},
            {create: true});
        const writable = await handle.createWritable();
        await writable.write(')",
                                                kContent, R"(');
        await writable.close();
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));

  ASSERT_NO_FATAL_FAILURE(NavigateToHost("sub.a.test"));
  EXPECT_EQ(kContent, EvalJs(shell(), RequestFileTextScript(kContentDigest)));

  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginB));
  EXPECT_EQ("NotFoundError",
            EvalJs(shell(), RequestFileTextScript(kContentDigest)));
}

// Visibility grants are additive and never downgrade earlier grants when the
// same content is stored again with a different `origins` list.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       VisibilityUpgradePreservesAllowedOrigins) {
  const std::string origin_b = https_server()->GetOrigin(kOriginB).Serialize();
  const std::string origin_c = https_server()->GetOrigin(kOriginC).Serialize();

  // Origin A stores with `origins: [origin_b]`.
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));
  ASSERT_EQ("ok", EvalJs(shell(), WriteContentScript(
                                      kSharedContent, kSharedContentDigest,
                                      base::StrCat({"['", origin_b, "']"}))));

  // Origin B stores the same hash with `origins: [origin_c]`.
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginB));
  ASSERT_EQ("ok", EvalJs(shell(), WriteContentScript(
                                      kSharedContent, kSharedContentDigest,
                                      base::StrCat({"['", origin_c, "']"}))));

  // Both Origin B and Origin C have read access, while an unlisted origin
  // (`d.test`) is still denied.
  EXPECT_EQ(kSharedContent,
            EvalJs(shell(), RequestFileTextScript(kSharedContentDigest)));

  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginC));
  EXPECT_EQ(kSharedContent,
            EvalJs(shell(), RequestFileTextScript(kSharedContentDigest)));

  ASSERT_NO_FATAL_FAILURE(NavigateToHost("d.test"));
  EXPECT_EQ("NotFoundError",
            EvalJs(shell(), RequestFileTextScript(kSharedContentDigest)));

  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));
  EXPECT_EQ(kSharedContent,
            EvalJs(shell(), RequestFileTextScript(kSharedContentDigest)));
}

// Random-access writes (`seek()` and `truncate()`) trigger on-disk rehashing on
// `close()`: a stream whose final bytes match the declared SHA-256 digest
// commits, while a seeked stream with mismatched final bytes rejects with
// DataError and leaves no staging files behind.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       WriteWithSeekAndTruncate) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  // 1. Seek back without truncating trailing bytes -> digest mismatch ->
  // DataError.
  EXPECT_EQ("DataError", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                       kContentDigest, R"('},
            {create: true, origins: '*'});
        const writable = await handle.createWritable();
        await writable.write('Hello, placeholder_extra_bytes');
        await writable.seek(7);
        await writable.write('Cross-Origin Storage!');
        await writable.close();
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));
  EXPECT_FALSE(BlobExistsOnDisk(kContentDigest));
  EXPECT_TRUE(StagingIsEmpty());

  // 2. Write, seek back to offset 7, overwrite, and truncate to 28 bytes so the
  // final on-disk content equals `kContent` ("Hello, Cross-Origin Storage!").
  EXPECT_EQ("ok", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                kContentDigest, R"('},
            {create: true, origins: '*'});
        const writable = await handle.createWritable();
        await writable.write('Hello, placeholder_extra_bytes');
        await writable.seek(7);
        await writable.write('Cross-Origin Storage!');
        await writable.truncate(28);
        await writable.close();
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));
  EXPECT_TRUE(BlobExistsOnDisk(kContentDigest));
  EXPECT_TRUE(StagingIsEmpty());

  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginB));
  EXPECT_EQ(kContent, EvalJs(shell(), RequestFileTextScript(kContentDigest)));
}

// Aborting an in-progress write (`writable.abort()`) deletes the staging file
// and does not commit a stale or partial entry to the registry.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       AbortedWriteLeavesNoStaleEntries) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("aborted", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      const handle = await navigator.crossOriginStorage.getFileHandle(
          {algorithm: 'SHA-256', value: ')",
                                                     kContentDigest, R"('},
          {create: true, origins: '*'});
      const writable = await handle.createWritable();
      await writable.write(')",
                                                     kContent, R"(');
      await writable.abort();
      return 'aborted';
    })()
  )"})));

  EXPECT_FALSE(BlobExistsOnDisk(kContentDigest));
  EXPECT_TRUE(StagingIsEmpty());
  EXPECT_EQ("NotFoundError",
            EvalJs(shell(), RequestReadHandleScript(kContentDigest)));
}

// Multiple concurrent writers targeting the same hash isolate their staging
// files so an aborted writer does not interfere with a committing writer, and
// concurrent commits deduplicate cleanly leaving `staging/` empty.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       ConcurrentWritersToSameHash) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  // 1. Two concurrent writers on `kSharedContentDigest`: writer 1 aborts while
  // writer 2 commits.
  EXPECT_EQ("ok", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      const handle1 = await navigator.crossOriginStorage.getFileHandle(
          {algorithm: 'SHA-256', value: ')",
                                                kSharedContentDigest, R"('},
          {create: true, origins: '*'});
      const handle2 = await navigator.crossOriginStorage.getFileHandle(
          {algorithm: 'SHA-256', value: ')",
                                                kSharedContentDigest, R"('},
          {create: true, origins: '*'});
      const writable1 = await handle1.createWritable();
      const writable2 = await handle2.createWritable();
      await Promise.all([
        writable1.write('partial content'),
        writable2.write(')",
                                                kSharedContent, R"(')
      ]);
      await Promise.all([
        writable1.abort(),
        writable2.close()
      ]);
      return 'ok';
    })()
  )"})));
  EXPECT_TRUE(BlobExistsOnDisk(kSharedContentDigest));
  EXPECT_TRUE(StagingIsEmpty());

  // 2. Two concurrent writers on `kContentDigest` both writing and closing at
  // the same time.
  EXPECT_EQ("ok", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      const handle1 = await navigator.crossOriginStorage.getFileHandle(
          {algorithm: 'SHA-256', value: ')",
                                                kContentDigest, R"('},
          {create: true, origins: '*'});
      const handle2 = await navigator.crossOriginStorage.getFileHandle(
          {algorithm: 'SHA-256', value: ')",
                                                kContentDigest, R"('},
          {create: true, origins: '*'});
      const writable1 = await handle1.createWritable();
      const writable2 = await handle2.createWritable();
      await Promise.all([
        writable1.write(')",
                                                kContent, R"('),
        writable2.write(')",
                                                kContent, R"(')
      ]);
      await Promise.all([
        writable1.close(),
        writable2.close()
      ]);
      return 'ok';
    })()
  )"})));
  EXPECT_TRUE(BlobExistsOnDisk(kContentDigest));
  EXPECT_TRUE(StagingIsEmpty());

  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginB));
  EXPECT_EQ(kSharedContent,
            EvalJs(shell(), RequestFileTextScript(kSharedContentDigest)));
  EXPECT_EQ(kContent, EvalJs(shell(), RequestFileTextScript(kContentDigest)));
}

// Per WICG § 4.3, `createWritable({keepExistingData: true})` must produce a
// stream whose contents start empty regardless of `keepExistingData`.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       CreateWritableKeepExistingDataStartsEmpty) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("ok", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                kContentDigest, R"('},
            {create: true, origins: '*'});
        const writable = await handle.createWritable({keepExistingData: true});
        await writable.write(')",
                                                kContent, R"(');
        await writable.close();
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));
  EXPECT_TRUE(BlobExistsOnDisk(kContentDigest));
  EXPECT_TRUE(StagingIsEmpty());
}

// `createSyncAccessHandle()` rejects with InvalidStateError whenever the handle
// is not in a bucket file system (WICG § 4.1).
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       CreateSyncAccessHandleRejectsWithInvalidStateError) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("InvalidStateError", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      const workerScript = `
        self.onmessage = async () => {
          try {
            const handle = await navigator.crossOriginStorage.getFileHandle(
                {algorithm: 'SHA-256', value: ')",
                                                               kContentDigest,
                                                               R"('},
                {create: true, origins: '*'});
            if (!handle.createSyncAccessHandle) {
              self.postMessage('not_exposed');
              return;
            }
            await handle.createSyncAccessHandle();
            self.postMessage('unexpected_success');
          } catch (e) {
            self.postMessage(e.name);
          }
        };
      `;
      const blob = new Blob([workerScript], {type: 'text/javascript'});
      const worker = new Worker(URL.createObjectURL(blob));
      return new Promise((resolve) => {
        worker.onmessage = (e) => resolve(e.data);
        worker.postMessage('start');
      });
    })()
  )"})));
}

// `queryPermission()` and `requestPermission()` report "granted" for read mode,
// report "granted" for write mode on writable handles, and resolve cleanly
// without rejecting (WICG § 4.1).
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       PermissionQueryAndRequest) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("ok", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const writableHandle =
            await navigator.crossOriginStorage.getFileHandle(
                {algorithm: 'SHA-256', value: ')",
                                                kContentDigest, R"('},
                {create: true, origins: '*'});

        if (await writableHandle.queryPermission({mode: 'read'}) !==
            'granted') {
          return 'writable query read failed';
        }
        if (await writableHandle.queryPermission({mode: 'readwrite'}) !==
            'granted') {
          return 'writable query write failed';
        }
        if (await writableHandle.requestPermission({mode: 'read'}) !==
            'granted') {
          return 'writable request read failed';
        }
        if (await writableHandle.requestPermission({mode: 'readwrite'}) !==
            'granted') {
          return 'writable request write failed';
        }

        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));
}

// Verifies that a read-only handle reports "denied" for write mode on both
// GetPermissionStatus and RequestPermission, while reporting "granted" for read
// mode, and resolves cleanly with kOk status (WICG § 4.1).
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       ReadOnlyHandlePermissionStatus) {
  auto hash = CrossOriginStorageHash::Create(
      blink::mojom::CrossOriginStorageAlgorithm::kSha256, kContentDigest);
  ASSERT_TRUE(hash.has_value());
  auto origin = url::Origin::Create(GURL("https://a.test"));
  auto access_spec = CrossOriginStorageAccessSpec::AllowedOrigins({});

  CrossOriginStorageFileHandleImpl handle(
      /*registry=*/nullptr, /*blob_storage_context=*/nullptr,
      /*file_system_access_manager=*/nullptr, *hash, origin, access_spec,
      /*is_writable=*/false, /*may_read=*/false);

  // 1. GetPermissionStatus: read mode -> GRANTED, write mode -> DENIED.
  base::test::TestFuture<blink::mojom::PermissionStatus> read_status_future;
  handle.GetPermissionStatus(
      blink::mojom::FileSystemAccessPermissionMode::kRead,
      read_status_future.GetCallback());
  EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED, read_status_future.Get());

  base::test::TestFuture<blink::mojom::PermissionStatus> write_status_future;
  handle.GetPermissionStatus(
      blink::mojom::FileSystemAccessPermissionMode::kWrite,
      write_status_future.GetCallback());
  EXPECT_EQ(blink::mojom::PermissionStatus::DENIED, write_status_future.Get());

  // 2. RequestPermission: read mode -> (kOk, GRANTED), write mode -> (kOk,
  // DENIED).
  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         blink::mojom::PermissionStatus>
      req_read_future;
  handle.RequestPermission(blink::mojom::FileSystemAccessPermissionMode::kRead,
                           req_read_future.GetCallback());
  auto [read_err, read_perm] = req_read_future.Take();
  EXPECT_EQ(blink::mojom::FileSystemAccessStatus::kOk, read_err->status);
  EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED, read_perm);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         blink::mojom::PermissionStatus>
      req_write_future;
  handle.RequestPermission(blink::mojom::FileSystemAccessPermissionMode::kWrite,
                           req_write_future.GetCallback());
  auto [write_err, write_perm] = req_write_future.Take();
  EXPECT_EQ(blink::mojom::FileSystemAccessStatus::kOk, write_err->status);
  EXPECT_EQ(blink::mojom::PermissionStatus::DENIED, write_perm);
}

// Calling `getFile()` on an unwritten create handle must reject with
// NotAllowedError so that `{create: true}` cannot act as an existence oracle.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       GetFileOnUnwrittenHandleRejectsWithNotAllowedError) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("NotAllowedError", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                             kContentDigest,
                                                             R"('},
            {create: true, origins: '*'});
        await handle.getFile();
        return 'unexpected_success';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));
}

// `isSameEntry()` returns true for handles addressing the same COS hash, false
// for different hashes, and rejects with NotAllowedError when compared with an
// OPFS handle (WICG § 4.1).
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest, IsSameEntryBehavior) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("ok", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const handle1 = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                kContentDigest, R"('},
            {create: true, origins: '*'});
        const handle1Copy = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                kContentDigest, R"('},
            {create: true, origins: '*'});
        const handle2 = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                kSharedContentDigest, R"('},
            {create: true, origins: '*'});

        const same = await handle1.isSameEntry(handle1Copy);
        if (!same) return 'expected same to be true';

        const different = await handle1.isSameEntry(handle2);
        if (different) return 'expected different to be false';

        const root = await navigator.storage.getDirectory();
        const opfsFile = await root.getFileHandle('test.txt', {create: true});
        try {
          await handle1.isSameEntry(opfsFile);
          return 'expected OPFS comparison to reject';
        } catch (e) {
          if (e.name !== 'NotAllowedError') {
            return `wrong error for OPFS: ${e.name}`;
          }
        }

        // Spec § 5: Calling `isSameEntry(cosHandle)` on an OPFS handle keeps
        // standard File System behavior and resolves to `false`.
        const opfsComparedToCos = await opfsFile.isSameEntry(handle1);
        if (opfsComparedToCos) {
          return 'expected opfsFile.isSameEntry(handle1) to be false';
        }

        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));
}

// `structuredClone()` and `postMessage()` preserve Cross-Origin Storage file
// handles (hash, origin, writability, and `may_read` state) across transfers.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       StructuredCloneAndPostMessage) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("ok", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                kContentDigest, R"('},
            {create: true, origins: '*'});

        const cloned = structuredClone(handle);
        if (!(await handle.isSameEntry(cloned))) {
          return 'cloned handle did not match original';
        }

        // Unwritten create handle stays unreadable after structuredClone.
        try {
          await cloned.getFile();
          return 'expected cloned getFile before write to reject';
        } catch (e) {
          if (e.name !== 'NotAllowedError') {
            return `unexpected error on cloned getFile: ${e.name}`;
          }
        }

        // Cloned create handle remains writable via postMessage.
        const posted = await new Promise((resolve) => {
          const channel = new MessageChannel();
          channel.port1.onmessage = (e) => resolve(e.data);
          channel.port2.postMessage(cloned);
        });
        if (!(await handle.isSameEntry(posted))) {
          return 'posted handle did not match original';
        }

        const writable = await posted.createWritable();
        await writable.write(')",
                                                kContent, R"(');
        await writable.close();

        // The handle that was written through becomes readable.
        const postedFile = await posted.getFile();
        if ((await postedFile.text()) !== ')",
                                                kContent, R"(') {
          return 'posted getFile content did not match';
        }

        // The pre-clone handle was not written through and remains unreadable.
        try {
          await handle.getFile();
          return 'expected pre-clone handle to remain unreadable';
        } catch (e) {
          if (e.name !== 'NotAllowedError') {
            return `unexpected error on pre-clone handle getFile: ${e.name}`;
          }
        }

        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));
  EXPECT_TRUE(BlobExistsOnDisk(kContentDigest));
  EXPECT_TRUE(StagingIsEmpty());
}

// `isSameEntry()` with handles obtained by different origins must reject with
// NotAllowedError (`kPermissionDenied`) per WICG § 4.1.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       IsSameEntryCrossOriginRejected) {
  auto hash = CrossOriginStorageHash::Create(
      blink::mojom::CrossOriginStorageAlgorithm::kSha256, kContentDigest);
  ASSERT_TRUE(hash.has_value());
  auto origin_a = url::Origin::Create(GURL("https://a.test"));
  auto origin_b = url::Origin::Create(GURL("https://b.test"));
  auto access_spec = CrossOriginStorageAccessSpec::AllowedOrigins({});

  CrossOriginStorageFileHandleImpl handle_a(
      /*registry=*/nullptr, /*blob_storage_context=*/nullptr,
      file_system_access_manager(), *hash, origin_a, access_spec,
      /*is_writable=*/true, /*may_read=*/false);

  CrossOriginStorageFileHandleImpl handle_b(
      /*registry=*/nullptr, /*blob_storage_context=*/nullptr,
      file_system_access_manager(), *hash, origin_b, access_spec,
      /*is_writable=*/true, /*may_read=*/false);

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_b;
  handle_b.Transfer(token_b.InitWithNewPipeAndPassReceiver());

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr, bool>
      same_entry_future;
  handle_a.IsSameEntry(std::move(token_b), same_entry_future.GetCallback());
  auto [err, is_same] = same_entry_future.Take();
  EXPECT_EQ(blink::mojom::FileSystemAccessStatus::kPermissionDenied,
            err->status);
  EXPECT_FALSE(is_same);
}

// Verifies `GetUniqueId()` rejects with `kNotSupportedError` on Cross-Origin
// Storage handles.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       GetUniqueIdReturnsNotSupportedError) {
  auto hash = CrossOriginStorageHash::Create(
      blink::mojom::CrossOriginStorageAlgorithm::kSha256, kContentDigest);
  ASSERT_TRUE(hash.has_value());
  auto origin = url::Origin::Create(GURL("https://a.test"));
  auto access_spec = CrossOriginStorageAccessSpec::AllowedOrigins({});

  CrossOriginStorageFileHandleImpl handle(
      /*registry=*/nullptr, /*blob_storage_context=*/nullptr,
      /*file_system_access_manager=*/nullptr, *hash, origin, access_spec,
      /*is_writable=*/true, /*may_read=*/false);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr,
                         const std::string&>
      id_future;
  handle.GetUniqueId(id_future.GetCallback());
  auto [err, id] = id_future.Take();
  EXPECT_EQ(blink::mojom::FileSystemAccessStatus::kNotSupportedError,
            err->status);
  EXPECT_TRUE(id.empty());
}

// Writing an empty (0-byte) file with matching SHA-256 digest commits to
// `blobs/`, leaves `staging/` empty, and is readable back via `getFile()`.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest, WriteEmptyFileAndCommit) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("ok", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                kEmptyContentDigest, R"('},
            {create: true, origins: '*'});
        const writable = await handle.createWritable();
        await writable.close();
        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));
  EXPECT_TRUE(BlobExistsOnDisk(kEmptyContentDigest));
  EXPECT_TRUE(StagingIsEmpty());

  EXPECT_EQ("", EvalJs(shell(), RequestFileTextScript(kEmptyContentDigest)));

  auto result = EvalJs(shell(), base::StrCat({R"(
    (async () => {
      const handle = await navigator.crossOriginStorage.getFileHandle(
          {algorithm: 'SHA-256', value: ')",
                                              kEmptyContentDigest, R"('});
      const file = await handle.getFile();
      return [file.size, await file.text()];
    })()
  )"}));
  const auto& list = result.ExtractList();
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(0, list[0].GetInt());
  EXPECT_EQ("", list[1].GetString());
}

// Calling `createWritable()` on a read-only handle (`is_writable_ == false`)
// must reject with `kPermissionDenied` (WICG § 4.1).
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       CreateWritableOnReadOnlyHandleRejects) {
  auto hash = CrossOriginStorageHash::Create(
      blink::mojom::CrossOriginStorageAlgorithm::kSha256, kContentDigest);
  ASSERT_TRUE(hash.has_value());
  auto origin = url::Origin::Create(GURL("https://a.test"));
  auto access_spec = CrossOriginStorageAccessSpec::AllowedOrigins({});

  CrossOriginStorageFileHandleImpl handle(
      /*registry=*/nullptr, /*blob_storage_context=*/nullptr,
      /*file_system_access_manager=*/nullptr, *hash, origin, access_spec,
      /*is_writable=*/false, /*may_read=*/false);

  base::test::TestFuture<
      blink::mojom::FileSystemAccessErrorPtr,
      mojo::PendingRemote<blink::mojom::FileSystemAccessFileWriter>>
      writer_future;
  handle.CreateFileWriter(
      /*keep_existing_data=*/false, /*auto_close=*/false,
      blink::mojom::FileSystemAccessWritableFileStreamLockMode::kExclusive,
      writer_future.GetCallback());
  auto [err, writer_remote] = writer_future.Take();
  EXPECT_EQ(blink::mojom::FileSystemAccessStatus::kPermissionDenied,
            err->status);
  EXPECT_FALSE(writer_remote.is_valid());
}

// Calling `getFile()` on a newly created handle before writing rejects with
// NotAllowedError (existence oracle guard). Once the write succeeds and
// commits, the handle transitions to `may_read_ == true`, allowing subsequent
// reads.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest, GetFileAfterWriteCommit) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("ok", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                kContentDigest, R"('},
            {create: true, origins: '*'});

        try {
          await handle.getFile();
          return 'expected getFile before commit to reject';
        } catch (e) {
          if (e.name !== 'NotAllowedError') {
            return `unexpected error before commit: ${e.name}`;
          }
        }

        const writable = await handle.createWritable();
        await writable.write(')",
                                                kContent, R"(');
        await writable.close();

        const file = await handle.getFile();
        const text = await file.text();
        if (text !== ')",
                                                kContent, R"(') {
          return `unexpected content after commit: ${text}`;
        }

        return 'ok';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));
  EXPECT_TRUE(BlobExistsOnDisk(kContentDigest));
  EXPECT_TRUE(StagingIsEmpty());
}

// Spec § 3.1: Invalid `origins` options (insecure origins, malformed URL
// strings, or sequences exceeding 100 entries) reject with TypeError.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       GetFileHandleInvalidOrigins) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  // 1. Insecure HTTP origin string.
  EXPECT_EQ("TypeError", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                       kContentDigest, R"('},
            {create: true, origins: 'http://insecure.test'});
        return 'unexpected_success';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));

  // 2. Malformed origin string in sequence.
  EXPECT_EQ("TypeError", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                       kContentDigest, R"('},
            {create: true, origins: ['invalid-origin-string']});
        return 'unexpected_success';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));

  // 3. Insecure HTTP origin in sequence.
  EXPECT_EQ("TypeError", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                       kContentDigest, R"('},
            {create: true, origins: ['http://insecure.test']});
        return 'unexpected_success';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));

  // 4. Origins sequence exceeding maximum length of 100.
  EXPECT_EQ("TypeError", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const largeList = Array(101).fill('https://a.test');
        await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                       kContentDigest, R"('},
            {create: true, origins: largeList});
        return 'unexpected_success';
      } catch (e) {
        return e.name;
      }
    })()
  )"})));
}

// Calling stream operations on a writer that has already been closed or aborted
// rejects with an error.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       OperationsOnClosedOrAbortedWriterReject) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  // 1. Write after close rejects with TypeError.
  EXPECT_EQ("TypeError", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                       kContentDigest, R"('},
            {create: true, origins: '*'});
        const writable = await handle.createWritable();
        await writable.write(')",
                                                       kContent, R"(');
        await writable.close();
        await writable.write('additional bytes');
        return 'unexpected_success';
      } catch (e) {
        return e ? e.name : 'rejected_undefined';
      }
    })()
  )"})));

  // 2. Close after close rejects with TypeError.
  EXPECT_EQ("TypeError", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                       kContentDigest, R"('},
            {create: true, origins: '*'});
        const writable = await handle.createWritable();
        await writable.write(')",
                                                       kContent, R"(');
        await writable.close();
        await writable.close();
        return 'unexpected_success';
      } catch (e) {
        return e ? e.name : 'rejected_undefined';
      }
    })()
  )"})));

  // 3. Write after abort rejects with the abort reason.
  EXPECT_EQ("AbortError", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                        kContentDigest, R"('},
            {create: true, origins: '*'});
        const writable = await handle.createWritable();
        await writable.abort(new DOMException('aborted', 'AbortError'));
        await writable.write('additional bytes');
        return 'unexpected_success';
      } catch (e) {
        return e ? e.name : 'rejected_undefined';
      }
    })()
  )"})));

  // 4. Close after abort rejects with TypeError per WHATWG Streams
  // WritableStreamClose step 2 ("If state is 'closed' or 'errored', return a
  // promise rejected with a TypeError exception").
  EXPECT_EQ("TypeError", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      try {
        const handle = await navigator.crossOriginStorage.getFileHandle(
            {algorithm: 'SHA-256', value: ')",
                                                       kContentDigest, R"('},
            {create: true, origins: '*'});
        const writable = await handle.createWritable();
        await writable.abort(new DOMException('aborted', 'AbortError'));
        await writable.close();
        return 'unexpected_success';
      } catch (e) {
        return e ? e.name : 'rejected_undefined';
      }
    })()
  )"})));
}

// Verifies that a transfer token remains valid in the registry and can be
// queried via `IsSameEntry()` even after the originating
// `CrossOriginStorageFileHandleImpl` has been destroyed.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       TransferTokenSurvivesHandleDestruction) {
  auto hash = CrossOriginStorageHash::Create(
      blink::mojom::CrossOriginStorageAlgorithm::kSha256, kContentDigest);
  ASSERT_TRUE(hash.has_value());
  auto origin = url::Origin::Create(GURL("https://a.test"));
  auto access_spec = CrossOriginStorageAccessSpec::AllowedOrigins({});

  mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token_b;
  {
    CrossOriginStorageFileHandleImpl handle_b(
        /*registry=*/nullptr, /*blob_storage_context=*/nullptr,
        file_system_access_manager(), *hash, origin, access_spec,
        /*is_writable=*/true, /*may_read=*/false);
    handle_b.Transfer(token_b.InitWithNewPipeAndPassReceiver());
  }

  CrossOriginStorageFileHandleImpl handle_a(
      /*registry=*/nullptr, /*blob_storage_context=*/nullptr,
      file_system_access_manager(), *hash, origin, access_spec,
      /*is_writable=*/true, /*may_read=*/false);

  base::test::TestFuture<blink::mojom::FileSystemAccessErrorPtr, bool>
      same_entry_future;
  handle_a.IsSameEntry(std::move(token_b), same_entry_future.GetCallback());
  auto [err, is_same] = same_entry_future.Take();
  EXPECT_EQ(blink::mojom::FileSystemAccessStatus::kOk, err->status);
  EXPECT_TRUE(is_same);
}

// Verifies that Cross-Origin Storage is accessible and functional inside a
// DedicatedWorker execution context, and that invalid hash/origins arguments
// reject with TypeError.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       DedicatedWorkerExecutionContext) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("ok", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      const workerScript = `
        self.onmessage = async () => {
          try {
            const handle = await navigator.crossOriginStorage.getFileHandle(
                {algorithm: 'SHA-256', value: ')",
                                                kSharedContentDigest, R"('},
                {create: true, origins: '*'});
            const writable = await handle.createWritable();
            await writable.write(')",
                                                kSharedContent, R"(');
            await writable.close();
          } catch (err) {
            self.postMessage('unexpected_valid_error:' + err.name);
            return;
          }
          try {
            await navigator.crossOriginStorage.getFileHandle(
                {algorithm: 'SHA-1', value: 'invalid'},
                {create: true, origins: 'http://insecure.test'});
            self.postMessage('unexpected_invalid_success');
            return;
          } catch (err) {
            if (err.name !== 'TypeError') {
              self.postMessage('unexpected_invalid_error:' + err.name);
              return;
            }
          }
          self.postMessage('ok');
        };
      `;
      const blob = new Blob([workerScript], {type: 'text/javascript'});
      const worker = new Worker(URL.createObjectURL(blob));
      return new Promise((resolve) => {
        worker.onmessage = (e) => resolve(e.data);
        worker.onerror = () => resolve('worker_error');
        worker.postMessage('start');
      });
    })()
  )"})));

  EXPECT_TRUE(BlobExistsOnDisk(kSharedContentDigest));
  EXPECT_TRUE(StagingIsEmpty());
}

// Verifies that Cross-Origin Storage is accessible and functional inside a
// SharedWorker execution context, and that invalid hash/origins arguments
// reject with TypeError.
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       SharedWorkerExecutionContext) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("ok", EvalJs(shell(), base::StrCat({R"(
    (async () => {
      const workerScript = `
        self.onconnect = (e) => {
          const port = e.ports[0];
          port.onmessage = async () => {
            try {
              const handle = await navigator.crossOriginStorage.getFileHandle(
                  {algorithm: 'SHA-256', value: ')",
                                                kSharedContentDigest, R"('},
                  {create: true, origins: '*'});
              const writable = await handle.createWritable();
              await writable.write(')",
                                                kSharedContent, R"(');
              await writable.close();
            } catch (err) {
              port.postMessage('unexpected_valid_error:' + err.name);
              return;
            }
            try {
              await navigator.crossOriginStorage.getFileHandle(
                  {algorithm: 'SHA-1', value: 'invalid'},
                  {create: true, origins: 'http://insecure.test'});
              port.postMessage('unexpected_invalid_success');
              return;
            } catch (err) {
              if (err.name !== 'TypeError') {
                port.postMessage('unexpected_invalid_error:' + err.name);
                return;
              }
            }
            port.postMessage('ok');
          };
        };
      `;
      const blob = new Blob([workerScript], {type: 'text/javascript'});
      const worker = new SharedWorker(URL.createObjectURL(blob));
      worker.port.start();
      return new Promise((resolve) => {
        worker.port.onmessage = (e) => resolve(e.data);
        worker.onerror = (e) => resolve('worker_error');
        worker.port.postMessage('start');
      });
    })()
  )"})));

  EXPECT_TRUE(BlobExistsOnDisk(kSharedContentDigest));
  EXPECT_TRUE(StagingIsEmpty());
}

// Verifies that Cross-Origin Storage is exposed in a ServiceWorker execution
// context (`Exposed=(Window,Worker)`), and that `getFileHandle()` rejects with
// NotAllowedError before hash/options validation (WICG § 3.1 & § 6).
IN_PROC_BROWSER_TEST_F(CrossOriginStorageBrowserTest,
                       ServiceWorkerExecutionContext) {
  ASSERT_NO_FATAL_FAILURE(NavigateToHost(kOriginA));

  EXPECT_EQ("NotAllowedError", EvalJs(shell(), R"(
    (async () => {
      await navigator.serviceWorker.register('/cos_service_worker.js');
      const readyReg = await navigator.serviceWorker.ready;
      const channel = new MessageChannel();
      return new Promise((resolve) => {
        channel.port1.onmessage = (e) => resolve(e.data);
        readyReg.active.postMessage('start', [channel.port2]);
      });
    })()
  )"));

  EXPECT_FALSE(BlobExistsOnDisk(kSharedContentDigest));
  EXPECT_TRUE(StagingIsEmpty());
}

}  // namespace content
