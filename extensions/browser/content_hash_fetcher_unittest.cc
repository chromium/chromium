// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/version.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/content_hash_fetcher.h"
#include "extensions/browser/content_verifier/test_utils.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_paths.h"
#include "extensions/common/file_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {

// Installs and tests various functionality of an extension loaded without
// verified_contents.json file.
class ContentHashFetcherTest : public ExtensionsTest {
 public:
  ContentHashFetcherTest()
      // We need a real IO thread to be able to entercept the network request
      // for the missing verified_contents.json file.
      : ExtensionsTest(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  ContentHashFetcherTest(const ContentHashFetcherTest&) = delete;
  ContentHashFetcherTest& operator=(const ContentHashFetcherTest&) = delete;

  ~ContentHashFetcherTest() override {}

  bool LoadTestExtension() {
    test_dir_base_ = GetTestPath(
        base::FilePath(FILE_PATH_LITERAL("missing_verified_contents")));

    // We unzip the extension source to a temp directory to simulate it being
    // installed there, because the ContentHashFetcher will create the
    // _metadata/ directory within the extension install dir and write the
    // fetched verified_contents.json file there.
    extension_ =
        UnzipToTempDirAndLoad(test_dir_base_.AppendASCII("source.zip"));
    if (!extension_.get()) {
      return false;
    }

    // Make sure there isn't already a verified_contents.json file there.
    EXPECT_FALSE(VerifiedContentsFileExists());
    delegate_ = std::make_unique<MockContentVerifierDelegate>();
    fetch_url_ = delegate_->GetSignatureFetchUrl(extension_->id(),
                                                 extension_->version());
    return true;
  }

  std::unique_ptr<ContentHashResult> DoHashFetch() {
    if (!extension_.get() || !delegate_.get()) {
      ADD_FAILURE() << "No valid extension_ or delegate_, "
                       "did you forget to call LoadTestExtension()?";
      return nullptr;
    }

    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        url_loader_factory_remote;
    test_url_loader_factory_.Clone(
        url_loader_factory_remote.InitWithNewPipeAndPassReceiver());

    std::unique_ptr<ContentHashResult> result =
        ContentHashWaiter().CreateAndWaitForCallback(
            ContentHash::FetchKey(extension_->id(), extension_->path(),
                                  extension_->version(),
                                  std::move(url_loader_factory_remote),
                                  fetch_url_, delegate_->GetPublicKey()),
            ContentVerifierDelegate::VerifierSourceType::SIGNED_HASHES);

    delegate_.reset();

    return result;
  }

  const GURL& fetch_url() { return fetch_url_; }

  const base::FilePath& extension_root() { return extension_->path(); }

  bool VerifiedContentsFileExists() const {
    return base::PathExists(
        file_util::GetVerifiedContentsPath(extension_->path()));
  }

  base::FilePath GetResourcePath(const std::string& resource_filename) const {
    return test_dir_base_.AppendASCII(resource_filename);
  }

  // Registers interception of requests for |url| to respond with the contents
  // of the file at |response_path|.
  void RegisterInterception(const GURL& url,
                            const base::FilePath& response_path) {
    ASSERT_TRUE(base::PathExists(response_path));
    std::string data;
    EXPECT_TRUE(ReadFileToString(response_path, &data));
    constexpr size_t kMaxFileSize = 1024 * 2;  // Using 2k file size for safety.
    ASSERT_LE(data.length(), kMaxFileSize);
    test_url_loader_factory_.AddResponse(url.spec(), data);
  }

  void RegisterInterceptionWithFailure(const GURL& url, int net_error) {
    test_url_loader_factory_.AddResponse(
        GURL(url), network::mojom::URLResponseHead::New(), std::string(),
        network::URLLoaderCompletionStatus(net_error));
  }

 private:
  // Helper to get files from our subdirectory in the general extensions test
  // data dir.
  base::FilePath GetTestPath(const base::FilePath& relative_path) {
    base::FilePath base_path;
    EXPECT_TRUE(base::PathService::Get(extensions::DIR_TEST_DATA, &base_path));
    base_path = base_path.AppendASCII("content_hash_fetcher");
    return base_path.Append(relative_path);
  }

  // Unzips the extension source from |extension_zip| into a temporary
  // directory and loads it, returning the resuling Extension object.
  scoped_refptr<Extension> UnzipToTempDirAndLoad(
      const base::FilePath& extension_zip) {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath destination = temp_dir_.GetPath();
    EXPECT_TRUE(zip::Unzip(extension_zip, destination));

    std::string error;
    static constexpr char kTestExtensionId[] =
        "jmllhlobpjcnnomjlipadejplhmheiif";
    scoped_refptr<Extension> extension = file_util::LoadExtension(
        destination, kTestExtensionId, mojom::ManifestLocation::kInternal,
        0 /* flags */, &error);
    EXPECT_NE(nullptr, extension.get()) << " error:'" << error << "'";
    return extension;
  }

  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;

  base::ScopedTempDir temp_dir_;

  GURL fetch_url_;
  base::FilePath test_dir_base_;
  std::unique_ptr<MockContentVerifierDelegate> delegate_;
  scoped_refptr<Extension> extension_;
};

// This tests our ability to successfully fetch, parse, and validate a missing
// verified_contents.json file for an extension.
TEST_F(ContentHashFetcherTest, MissingVerifiedContents) {
  ASSERT_TRUE(LoadTestExtension());

  RegisterInterception(fetch_url(), GetResourcePath("verified_contents.json"));

  // Make sure the fetch was successful.
  std::unique_ptr<ContentHashResult> result = DoHashFetch();
  ASSERT_TRUE(result.get());
  EXPECT_TRUE(result->success);
  EXPECT_FALSE(result->was_cancelled);
  EXPECT_TRUE(result->mismatch_paths.empty());

  // Make sure the verified_contents.json file was written into the extension's
  // install dir.
  EXPECT_TRUE(VerifiedContentsFileExists());
}

// Tests that if the network fetches invalid verified_contents.json, failure
// happens correctly.
TEST_F(ContentHashFetcherTest, FetchInvalidVerifiedContents) {
  ASSERT_TRUE(LoadTestExtension());

  // Simulate invalid verified_contents.json fetch by providing a modified and
  // incorrect json file.
  // invalid_verified_contents.json is a modified version of
  // verified_contents.json, with one hash character garbled.
  RegisterInterception(fetch_url(),
                       GetResourcePath("invalid_verified_contents.json"));

  std::unique_ptr<ContentHashResult> result = DoHashFetch();
  ASSERT_TRUE(result.get());
  EXPECT_FALSE(result->success);
  EXPECT_FALSE(result->was_cancelled);
  EXPECT_TRUE(result->mismatch_paths.empty());

  // TODO(lazyboy): This should be EXPECT_FALSE, we shouldn't be writing
  // verified_contents.json file if it didn't succeed.
  //// Make sure the verified_contents.json file was *not* written into the
  //// extension's install dir.
  // EXPECT_FALSE(VerifiedContentsFileExists());
  EXPECT_TRUE(VerifiedContentsFileExists());
}

// Tests that if the verified_contents.json network request 404s, failure
// happens as expected.
TEST_F(ContentHashFetcherTest, Fetch404VerifiedContents) {
  ASSERT_TRUE(LoadTestExtension());

  RegisterInterceptionWithFailure(fetch_url(), net::HTTP_NOT_FOUND);

  // Make sure the fetch was *not* successful.
  std::unique_ptr<ContentHashResult> result = DoHashFetch();
  ASSERT_TRUE(result.get());
  EXPECT_FALSE(result->success);
  EXPECT_FALSE(result->was_cancelled);
  EXPECT_TRUE(result->mismatch_paths.empty());

  // Make sure the verified_contents.json file was *not* written into the
  // extension's install dir.
  EXPECT_FALSE(VerifiedContentsFileExists());
}

// Similar to MissingVerifiedContents, but tests the case where the extension
// actually has corruption.
TEST_F(ContentHashFetcherTest, MissingVerifiedContentsAndCorrupt) {
  ASSERT_TRUE(LoadTestExtension());

  // Tamper with a file in the extension.
  base::FilePath script_path = extension_root().AppendASCII("script.js");
  std::string addition = "//hello world";
  ASSERT_TRUE(base::AppendToFile(script_path, addition));

  RegisterInterception(fetch_url(), GetResourcePath("verified_contents.json"));

  // Make sure the fetch was *not* successful.
  std::unique_ptr<ContentHashResult> result = DoHashFetch();
  ASSERT_NE(nullptr, result.get());
  EXPECT_TRUE(result->success);
  EXPECT_FALSE(result->was_cancelled);
  EXPECT_TRUE(base::Contains(result->mismatch_paths, script_path.BaseName()));

  // Make sure the verified_contents.json file was written into the extension's
  // install dir.
  EXPECT_TRUE(VerifiedContentsFileExists());
}

}  // namespace extensions
