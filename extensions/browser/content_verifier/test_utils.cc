// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/test_utils.h"

#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {

// TestContentVerifySingleJobObserver ------------------------------------------
TestContentVerifySingleJobObserver::TestContentVerifySingleJobObserver(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path)
    : extension_id_(extension_id), relative_path_(relative_path) {
  ContentVerifyJob::SetObserverForTests(this);
}

TestContentVerifySingleJobObserver::~TestContentVerifySingleJobObserver() {
  ContentVerifyJob::SetObserverForTests(nullptr);
}

void TestContentVerifySingleJobObserver::JobFinished(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path,
    ContentVerifyJob::FailureReason reason) {
  if (extension_id != extension_id_ || relative_path != relative_path_)
    return;
  EXPECT_FALSE(failure_reason_.has_value());
  failure_reason_ = reason;
  job_finished_run_loop_.Quit();
}

void TestContentVerifySingleJobObserver::OnHashesReady(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path,
    bool success) {
  if (extension_id != extension_id_ || relative_path != relative_path_)
    return;
  EXPECT_FALSE(seen_on_hashes_ready_);
  seen_on_hashes_ready_ = true;
  on_hashes_ready_run_loop_.Quit();
}

ContentVerifyJob::FailureReason
TestContentVerifySingleJobObserver::WaitForJobFinished() {
  // Run() returns immediately if Quit() has already been called.
  job_finished_run_loop_.Run();
  EXPECT_TRUE(failure_reason_.has_value());
  return failure_reason_.value_or(ContentVerifyJob::FAILURE_REASON_MAX);
}

void TestContentVerifySingleJobObserver::WaitForOnHashesReady() {
  // Run() returns immediately if Quit() has already been called.
  on_hashes_ready_run_loop_.Run();
}

// TestContentVerifyJobObserver ------------------------------------------------
void TestContentVerifyJobObserver::ExpectJobResult(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path,
    Result expected_result) {
  expectations_.push_back(
      ExpectedResult(extension_id, relative_path, expected_result));
}

TestContentVerifyJobObserver::TestContentVerifyJobObserver() {
  EXPECT_TRUE(
      content::BrowserThread::GetCurrentThreadIdentifier(&creation_thread_));
  ContentVerifyJob::SetObserverForTests(this);
}

TestContentVerifyJobObserver::~TestContentVerifyJobObserver() {
  ContentVerifyJob::SetObserverForTests(nullptr);
}

bool TestContentVerifyJobObserver::WaitForExpectedJobs() {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(creation_thread_));
  if (!expectations_.empty()) {
    base::RunLoop run_loop;
    job_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  return expectations_.empty();
}

void TestContentVerifyJobObserver::JobStarted(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path) {}

void TestContentVerifyJobObserver::JobFinished(
    const ExtensionId& extension_id,
    const base::FilePath& relative_path,
    ContentVerifyJob::FailureReason failure_reason) {
  if (!content::BrowserThread::CurrentlyOn(creation_thread_)) {
    base::PostTaskWithTraits(
        FROM_HERE, {creation_thread_},
        base::BindOnce(&TestContentVerifyJobObserver::JobFinished,
                       base::Unretained(this), extension_id, relative_path,
                       failure_reason));
    return;
  }
  Result result = failure_reason == ContentVerifyJob::NONE ? Result::SUCCESS
                                                           : Result::FAILURE;
  bool found = false;
  for (auto i = expectations_.begin(); i != expectations_.end(); ++i) {
    if (i->extension_id == extension_id && i->path == relative_path &&
        i->result == result) {
      found = true;
      expectations_.erase(i);
      break;
    }
  }
  if (found) {
    if (expectations_.empty() && job_quit_closure_)
      std::move(job_quit_closure_).Run();
  } else {
    LOG(WARNING) << "Ignoring unexpected JobFinished " << extension_id << "/"
                 << relative_path.value()
                 << " failure_reason:" << failure_reason;
  }
}

// MockContentVerifierDelegate ------------------------------------------------
MockContentVerifierDelegate::MockContentVerifierDelegate() = default;
MockContentVerifierDelegate::~MockContentVerifierDelegate() = default;

ContentVerifierDelegate::Mode MockContentVerifierDelegate::ShouldBeVerified(
    const Extension& extension) {
  return ContentVerifierDelegate::ENFORCE_STRICT;
}

ContentVerifierKey MockContentVerifierDelegate::GetPublicKey() {
  return ContentVerifierKey(kWebstoreSignaturesPublicKey,
                            kWebstoreSignaturesPublicKeySize);
}

GURL MockContentVerifierDelegate::GetSignatureFetchUrl(
    const ExtensionId& extension_id,
    const base::Version& version) {
  std::string url =
      base::StringPrintf("http://localhost/getsignature?id=%s&version=%s",
                         extension_id.c_str(), version.GetString().c_str());
  return GURL(url);
}

std::set<base::FilePath> MockContentVerifierDelegate::GetBrowserImagePaths(
    const extensions::Extension* extension) {
  ADD_FAILURE() << "Unexpected call for this test";
  return std::set<base::FilePath>();
}

void MockContentVerifierDelegate::VerifyFailed(
    const ExtensionId& extension_id,
    ContentVerifyJob::FailureReason reason) {
  ADD_FAILURE() << "Unexpected call for this test";
}

void MockContentVerifierDelegate::Shutdown() {}

// VerifierObserver -----------------------------------------------------------
VerifierObserver::VerifierObserver() {
  EXPECT_TRUE(
      content::BrowserThread::GetCurrentThreadIdentifier(&creation_thread_));
  ContentVerifier::SetObserverForTests(this);
}

VerifierObserver::~VerifierObserver() {
  ContentVerifier::SetObserverForTests(nullptr);
}

void VerifierObserver::WaitForFetchComplete(const ExtensionId& extension_id) {
  EXPECT_TRUE(content::BrowserThread::CurrentlyOn(creation_thread_));
  EXPECT_TRUE(id_to_wait_for_.empty());
  EXPECT_EQ(loop_runner_.get(), nullptr);
  id_to_wait_for_ = extension_id;
  loop_runner_ = new content::MessageLoopRunner();
  loop_runner_->Run();
  id_to_wait_for_.clear();
  loop_runner_ = nullptr;
}

void VerifierObserver::OnFetchComplete(const ExtensionId& extension_id,
                                       bool success) {
  if (!content::BrowserThread::CurrentlyOn(creation_thread_)) {
    base::PostTaskWithTraits(
        FROM_HERE, {creation_thread_},
        base::BindOnce(&VerifierObserver::OnFetchComplete,
                       base::Unretained(this), extension_id, success));
    return;
  }
  completed_fetches_.insert(extension_id);
  if (extension_id == id_to_wait_for_)
    loop_runner_->Quit();
}

namespace content_verifier_test_utils {

scoped_refptr<Extension> UnzipToDirAndLoadExtension(
    const base::FilePath& extension_zip,
    const base::FilePath& unzip_dir) {
  if (!zip::Unzip(extension_zip, unzip_dir)) {
    ADD_FAILURE() << "Failed to unzip path.";
    return nullptr;
  }
  std::string error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      unzip_dir, Manifest::INTERNAL, 0 /* flags */, &error);
  EXPECT_NE(nullptr, extension.get()) << " error:'" << error << "'";
  return extension;
}

}  // namespace content_verifier_test_utils

}  // namespace extensions
