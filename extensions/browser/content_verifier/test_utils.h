// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_TEST_UTILS_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_TEST_UTILS_H_

#include <list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "crypto/rsa_private_key.h"
#include "extensions/browser/content_hash_reader.h"
#include "extensions/browser/content_verifier/content_hash.h"
#include "extensions/browser/content_verifier/content_verifier.h"
#include "extensions/browser/content_verifier/content_verifier_delegate.h"
#include "extensions/browser/content_verifier/content_verify_job.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

class Extension;

// Test class to observe *a particular* extension resource's ContentVerifyJob
// lifetime.  Provides a way to wait for a job to finish and return
// the job's result.
class TestContentVerifySingleJobObserver {
 public:
  TestContentVerifySingleJobObserver(const ExtensionId& extension_id,
                                     const base::FilePath& relative_path);
  ~TestContentVerifySingleJobObserver();

  TestContentVerifySingleJobObserver(
      const TestContentVerifySingleJobObserver&) = delete;
  TestContentVerifySingleJobObserver& operator=(
      const TestContentVerifySingleJobObserver&) = delete;

  // Waits for a ContentVerifyJob to finish and returns job's status.
  [[nodiscard]] ContentVerifyJob::FailureReason WaitForJobFinished();

  // Waits for ContentVerifyJob to finish the attempt to read content hashes.
  ContentHashReader::InitStatus WaitForOnHashesReady();

 private:
  class ObserverClient : public ContentVerifyJob::TestObserver {
   public:
    ObserverClient(const ExtensionId& extension_id,
                   const base::FilePath& relative_path);

    ObserverClient(const ObserverClient&) = delete;
    ObserverClient& operator=(const ObserverClient&) = delete;

    // ContentVerifyJob::TestObserver:
    void JobStarted(const ExtensionId& extension_id,
                    const base::FilePath& relative_path) override {}
    void JobFinished(const ExtensionId& extension_id,
                     const base::FilePath& relative_path,
                     ContentVerifyJob::FailureReason reason) override;
    void OnHashesReady(const ExtensionId& extension_id,
                       const base::FilePath& relative_path,
                       const ContentHashReader& hash_reader) override;

    // Passed methods from ContentVerifySingleJobObserver:
    [[nodiscard]] ContentVerifyJob::FailureReason WaitForJobFinished();
    ContentHashReader::InitStatus WaitForOnHashesReady();

   private:
    ~ObserverClient() override;

    void OnHashesReadyOnCreationThread(
        const ExtensionId& extension_id,
        const base::FilePath& relative_path,
        ContentHashReader::InitStatus content_hash_status);

    content::BrowserThread::ID creation_thread_;

    base::RunLoop job_finished_run_loop_;
    base::RunLoop on_hashes_ready_run_loop_;

    ExtensionId extension_id_;
    base::FilePath relative_path_;
    std::optional<ContentVerifyJob::FailureReason> failure_reason_;
    bool seen_on_hashes_ready_ = false;
    ContentHashReader::InitStatus hashes_status_;
  };

  scoped_refptr<ObserverClient> client_;
};

// Test class to observe expected set of ContentVerifyJobs.
class TestContentVerifyJobObserver {
 public:
  TestContentVerifyJobObserver();
  ~TestContentVerifyJobObserver();

  TestContentVerifyJobObserver(const TestContentVerifyJobObserver&) = delete;
  TestContentVerifyJobObserver& operator=(const TestContentVerifyJobObserver&) =
      delete;

  enum class Result { SUCCESS, FAILURE };

  // Call this to add an expected job result.
  void ExpectJobResult(const ExtensionId& extension_id,
                       const base::FilePath& relative_path,
                       Result expected_result);

  // Wait to see expected jobs. Returns true when we've seen all expected jobs
  // finish, or false if there was an error or timeout.
  bool WaitForExpectedJobs();

 private:
  class ObserverClient : public ContentVerifyJob::TestObserver {
   public:
    ObserverClient();

    ObserverClient(const ObserverClient&) = delete;
    ObserverClient& operator=(const ObserverClient&) = delete;

    // ContentVerifyJob::TestObserver:
    void JobStarted(const ExtensionId& extension_id,
                    const base::FilePath& relative_path) override {}
    void JobFinished(const ExtensionId& extension_id,
                     const base::FilePath& relative_path,
                     ContentVerifyJob::FailureReason failure_reason) override;
    void OnHashesReady(const ExtensionId& extension_id,
                       const base::FilePath& relative_path,
                       const ContentHashReader& hash_reader) override {}

    // Passed methods from TestContentVerifyJobObserver:
    void ExpectJobResult(const ExtensionId& extension_id,
                         const base::FilePath& relative_path,
                         Result expected_result);
    bool WaitForExpectedJobs();

   private:
    struct ExpectedResult {
     public:
      ExtensionId extension_id;
      base::FilePath path;
      Result result;

      ExpectedResult(const ExtensionId& extension_id,
                     const base::FilePath& path,
                     Result result)
          : extension_id(extension_id), path(path), result(result) {}
    };

    ~ObserverClient() override;

    std::list<ExpectedResult> expectations_;
    content::BrowserThread::ID creation_thread_;
    // Accessed on |creation_thread_|.
    base::OnceClosure job_quit_closure_;
  };

  scoped_refptr<ObserverClient> client_;
};

// An extensions/ implementation of ContentVerifierDelegate for using in tests.
// Provides mock versions of content verification mode, keys and fetch url.
class MockContentVerifierDelegate : public ContentVerifierDelegate {
 public:
  MockContentVerifierDelegate();

  MockContentVerifierDelegate(const MockContentVerifierDelegate&) = delete;
  MockContentVerifierDelegate& operator=(const MockContentVerifierDelegate&) =
      delete;

  ~MockContentVerifierDelegate() override;

  // ContentVerifierDelegate:
  VerifierSourceType GetVerifierSourceType(const Extension& extension) override;
  ContentVerifierKey GetPublicKey() override;
  GURL GetSignatureFetchUrl(const ExtensionId& extension_id,
                            const base::Version& version) override;
  std::set<base::FilePath> GetBrowserImagePaths(
      const extensions::Extension* extension) override;
  void VerifyFailed(const ExtensionId& extension_id,
                    ContentVerifyJob::FailureReason reason) override;
  void Shutdown() override;

  // Modifier.
  void SetVerifierSourceType(VerifierSourceType type);
  void SetVerifierKey(std::vector<uint8_t> key);

 private:
  VerifierSourceType verifier_source_type_ = VerifierSourceType::SIGNED_HASHES;
  std::vector<uint8_t> verifier_key_;
};

// Observes ContentVerifier::OnFetchComplete of a particular extension.
class VerifierObserver : public ContentVerifier::TestObserver {
 public:
  VerifierObserver();

  VerifierObserver(const VerifierObserver&) = delete;
  VerifierObserver& operator=(const VerifierObserver&) = delete;

  virtual ~VerifierObserver();

  const std::set<base::FilePath>& hash_mismatch_unix_paths() {
    DCHECK(content_hash_);
    return content_hash_->hash_mismatch_unix_paths();
  }
  bool did_hash_mismatch() const { return did_hash_mismatch_; }

  // Ensures that |extension_id| has seen OnFetchComplete, waits for it to
  // complete if it hasn't already.
  void EnsureFetchCompleted(const ExtensionId& extension_id);

  // ContentVerifier::TestObserver
  void OnFetchComplete(const scoped_refptr<const ContentHash>& content_hash,
                       bool did_hash_mismatch) override;

 private:
  std::set<ExtensionId> completed_fetches_;
  ExtensionId id_to_wait_for_;
  scoped_refptr<const ContentHash> content_hash_;
  bool did_hash_mismatch_ = true;

  // Created and accessed on |creation_thread_|.
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  content::BrowserThread::ID creation_thread_;

  base::WeakPtrFactory<VerifierObserver> weak_ptr_factory_{this};
};

// Used to hold the result of a callback from the ContentHash creation.
struct ContentHashResult {
  ContentHashResult(const ExtensionId& extension_id,
                    bool success,
                    bool was_cancelled,
                    const std::set<base::FilePath> mismatch_paths);
  ~ContentHashResult();

  ExtensionId extension_id;
  bool success;
  bool was_cancelled;
  std::set<base::FilePath> mismatch_paths;
};

// Allows waiting for the callback from a ContentHash, returning the
// data that was passed to that callback.
class ContentHashWaiter {
 public:
  ContentHashWaiter();

  ContentHashWaiter(const ContentHashWaiter&) = delete;
  ContentHashWaiter& operator=(const ContentHashWaiter&) = delete;

  ~ContentHashWaiter();

  std::unique_ptr<ContentHashResult> CreateAndWaitForCallback(
      ContentHash::FetchKey key,
      ContentVerifierDelegate::VerifierSourceType source_type);

 private:
  void CreatedCallback(scoped_refptr<ContentHash> content_hash,
                       bool was_cancelled);

  void CreateContentHash(
      ContentHash::FetchKey key,
      ContentVerifierDelegate::VerifierSourceType source_type);

  scoped_refptr<base::SequencedTaskRunner> reply_task_runner_;
  base::RunLoop run_loop_;
  std::unique_ptr<ContentHashResult> result_;
};

namespace content_verifier_test_utils {

// Helper class to create directory with extension files, including signed
// hashes for content verification.
class TestExtensionBuilder {
 public:
  TestExtensionBuilder();
  explicit TestExtensionBuilder(const ExtensionId& extension_id);
  ~TestExtensionBuilder();

  TestExtensionBuilder(const TestExtensionBuilder&) = delete;
  TestExtensionBuilder& operator=(const TestExtensionBuilder&) = delete;

  // Accept parameters by values since we'll store them.
  void AddResource(base::FilePath::StringType relative_path,
                   std::string contents);

  void WriteManifest();

  // Accept parameters by values since we'll store them.
  void WriteResource(base::FilePath::StringType relative_path,
                     std::string contents);

  void WriteComputedHashes();

  std::string CreateVerifiedContents() const;

  void WriteVerifiedContents();

  std::vector<uint8_t> GetTestContentVerifierPublicKey() const;

  base::FilePath extension_path() const {
    return extension_dir_.UnpackedPath();
  }
  const ExtensionId& extension_id() const { return extension_id_; }

 private:
  struct ExtensionResource {
    ExtensionResource(base::FilePath relative_path, std::string contents)
        : relative_path(std::move(relative_path)),
          contents(std::move(contents)) {}

    base::FilePath relative_path;
    std::string contents;
  };

  std::unique_ptr<base::Value> CreateVerifiedContentsPayload() const;

  std::unique_ptr<crypto::RSAPrivateKey> test_content_verifier_key_;
  ExtensionId extension_id_;
  std::vector<ExtensionResource> extension_resources_;

  TestExtensionDir extension_dir_;
};

// Unzips the extension source from |extension_zip| into |unzip_dir|
// directory and loads it. Returns the resulting Extension object.
// |destination| points to the path where the extension was extracted.
//
// TODO(lazyboy): Move this function to a generic file.
scoped_refptr<Extension> UnzipToDirAndLoadExtension(
    const base::FilePath& extension_zip,
    const base::FilePath& unzip_dir);

}  // namespace content_verifier_test_utils

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_TEST_UTILS_H_
