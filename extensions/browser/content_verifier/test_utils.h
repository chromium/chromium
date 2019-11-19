// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_TEST_UTILS_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_TEST_UTILS_H_

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/content_verifier/content_hash.h"
#include "extensions/browser/content_verifier_delegate.h"
#include "extensions/browser/content_verify_job.h"
#include "extensions/common/extension_id.h"

namespace extensions {

class Extension;

// Test class to observe *a particular* extension resource's ContentVerifyJob
// lifetime.  Provides a way to wait for a job to finish and return
// the job's result.
class TestContentVerifySingleJobObserver : ContentVerifyJob::TestObserver {
 public:
  TestContentVerifySingleJobObserver(const ExtensionId& extension_id,
                                     const base::FilePath& relative_path);
  ~TestContentVerifySingleJobObserver();

  // ContentVerifyJob::TestObserver:
  void JobStarted(const ExtensionId& extension_id,
                  const base::FilePath& relative_path) override {}
  void JobFinished(const ExtensionId& extension_id,
                   const base::FilePath& relative_path,
                   ContentVerifyJob::FailureReason reason) override;
  void OnHashesReady(const ExtensionId& extension_id,
                     const base::FilePath& relative_path,
                     bool success) override;

  // Waits for a ContentVerifyJob to finish and returns job's status.
  ContentVerifyJob::FailureReason WaitForJobFinished() WARN_UNUSED_RESULT;

  // Waits for ContentVerifyJob to finish the attempt to read content hashes.
  void WaitForOnHashesReady();

 private:
  base::RunLoop job_finished_run_loop_;
  base::RunLoop on_hashes_ready_run_loop_;

  ExtensionId extension_id_;
  base::FilePath relative_path_;
  base::Optional<ContentVerifyJob::FailureReason> failure_reason_;
  bool seen_on_hashes_ready_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestContentVerifySingleJobObserver);
};

// Test class to observe expected set of ContentVerifyJobs.
class TestContentVerifyJobObserver : public ContentVerifyJob::TestObserver {
 public:
  TestContentVerifyJobObserver();
  virtual ~TestContentVerifyJobObserver();

  enum class Result { SUCCESS, FAILURE };

  // Call this to add an expected job result.
  void ExpectJobResult(const ExtensionId& extension_id,
                       const base::FilePath& relative_path,
                       Result expected_result);

  // Wait to see expected jobs. Returns true when we've seen all expected jobs
  // finish, or false if there was an error or timeout.
  bool WaitForExpectedJobs();

  // ContentVerifyJob::TestObserver interface
  void JobStarted(const ExtensionId& extension_id,
                  const base::FilePath& relative_path) override;
  void JobFinished(const ExtensionId& extension_id,
                   const base::FilePath& relative_path,
                   ContentVerifyJob::FailureReason failure_reason) override;
  void OnHashesReady(const ExtensionId& extension_id,
                     const base::FilePath& relative_path,
                     bool success) override {}

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
  std::list<ExpectedResult> expectations_;
  content::BrowserThread::ID creation_thread_;
  // Accessed on |creation_thread_|.
  base::OnceClosure job_quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(TestContentVerifyJobObserver);
};

// An extensions/ implementation of ContentVerifierDelegate for using in tests.
// Provides mock versions of content verification mode, keys and fetch url.
class MockContentVerifierDelegate : public ContentVerifierDelegate {
 public:
  MockContentVerifierDelegate();
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

 private:
  VerifierSourceType verifier_source_type_ = VerifierSourceType::SIGNED_HASHES;

  DISALLOW_COPY_AND_ASSIGN(MockContentVerifierDelegate);
};

// Observes ContentVerifier::OnFetchComplete of a particular extension.
class VerifierObserver : public ContentVerifier::TestObserver {
 public:
  VerifierObserver();
  virtual ~VerifierObserver();

  const std::set<ExtensionId>& completed_fetches() {
    return completed_fetches_;
  }

  // Returns when we've seen OnFetchComplete for |extension_id|.
  void WaitForFetchComplete(const ExtensionId& extension_id);

  // ContentVerifier::TestObserver
  void OnFetchComplete(const ExtensionId& extension_id, bool success) override;

 private:
  std::set<ExtensionId> completed_fetches_;
  ExtensionId id_to_wait_for_;

  // Created and accessed on |creation_thread_|.
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  content::BrowserThread::ID creation_thread_;

  DISALLOW_COPY_AND_ASSIGN(VerifierObserver);
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

  DISALLOW_COPY_AND_ASSIGN(ContentHashWaiter);
};

namespace content_verifier_test_utils {

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
