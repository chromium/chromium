// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/content_verifier/content_verify_job.h"

#include <algorithm>

#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "build/buildflag.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "extensions/browser/content_hash_reader.h"
#include "extensions/browser/content_verifier/content_hash.h"
#include "extensions/browser/content_verifier/content_verifier.h"
#include "extensions/common/constants.h"

#if BUILDFLAG(IS_MAC)
#include "extensions/common/extension_features.h"
#endif

namespace extensions {

namespace {

bool g_ignore_verification_for_tests = false;

base::LazyInstance<scoped_refptr<ContentVerifyJob::TestObserver>>::Leaky
    g_content_verify_job_test_observer = LAZY_INSTANCE_INITIALIZER;

scoped_refptr<ContentVerifyJob::TestObserver> GetTestObserver() {
  if (!g_content_verify_job_test_observer.IsCreated())
    return nullptr;
  return g_content_verify_job_test_observer.Get();
}

class ScopedElapsedTimer {
 public:
  explicit ScopedElapsedTimer(base::TimeDelta* total) : total_(total) {
    DCHECK(total_);
  }

  ~ScopedElapsedTimer() { *total_ += timer.Elapsed(); }

 private:
  // Some total amount of time we should add our elapsed time to at
  // destruction.
  raw_ptr<base::TimeDelta> total_;

  // A timer for how long this object has been alive.
  base::ElapsedTimer timer;
};

bool IsIgnorableReadError(MojoResult read_result) {
  // Extension reload, for example, can cause benign MOJO_RESULT_ABORTED error.
  // Do not incorrectly fail content verification in that case.
  // See https://crbug.com/977805 for details.
  return read_result == MOJO_RESULT_ABORTED;
}

}  // namespace

ContentVerifyJob::ContentVerifyJob(const ExtensionId& extension_id,
                                   const base::FilePath& extension_root,
                                   const base::FilePath& relative_path)
    : extension_id_(extension_id),
      extension_root_(extension_root),
      relative_path_(relative_path) {}

ContentVerifyJob::~ContentVerifyJob() = default;

void ContentVerifyJob::Start(ContentVerifier* verifier,
                             const base::Version& extension_version,
                             int manifest_version,
                             FailureCallback failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::AutoLock auto_lock(lock_);
  manifest_version_ = manifest_version;
  failure_callback_ = std::move(failure_callback);

  // The content verification hashes are most likely already cached.
  auto content_hash = verifier->GetCachedContentHash(
      extension_id_, extension_version,
      /*force_missing_computed_hashes_creation=*/true);
  if (content_hash) {
    StartWithContentHash(std::move(content_hash));
    return;
  }

  verifier->CreateContentHash(
      extension_id_, extension_root_, extension_version,
      /*force_missing_computed_hashes_creation=*/true,
      base::BindOnce(&ContentVerifyJob::DidCreateContentHashOnIO, this));
}

void ContentVerifyJob::DidCreateContentHashOnIO(
    scoped_refptr<const ContentHash> content_hash) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::AutoLock auto_lock(lock_);
  StartWithContentHash(std::move(content_hash));
}

void ContentVerifyJob::StartWithContentHash(
    scoped_refptr<const ContentHash> content_hash) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  scoped_refptr<TestObserver> test_observer = GetTestObserver();
  if (test_observer)
    test_observer->JobStarted(extension_id_, relative_path_);
  // Build |hash_reader_|.
  hash_reader_ = ContentHashReader::Create(relative_path_, content_hash);

  if (g_ignore_verification_for_tests) {
    return;
  }
  if (test_observer) {
    test_observer->OnHashesReady(extension_id_, relative_path_, *hash_reader_);
  }

  switch (hash_reader_->status()) {
    case ContentHashReader::InitStatus::HASHES_MISSING: {
      DispatchFailureCallback(MISSING_ALL_HASHES);
      return;
    }
    case ContentHashReader::InitStatus::HASHES_DAMAGED: {
      DispatchFailureCallback(CORRUPTED_HASHES);
      return;
    }
    case ContentHashReader::InitStatus::NO_HASHES_FOR_RESOURCE: {
#if BUILDFLAG(IS_MAC)
      // Skip verification for file paths ending with a separator. Unlike other
      // platforms, macOS strips the trailing separator when `realpath` is used,
      // which causes inconsistencies. See https://crbug.com/356878412.
      // TODO(crbug.com/357636604): Remove after the flag is removed in M132.
      if (!base::FeatureList::IsEnabled(
              extensions_features::kMacRejectFilePathsEndingWithSeparator) &&
          relative_path_.EndsWithSeparator()) {
        ReportJobFinished(NONE);
        return;
      }
#endif

      // Proceed and dispatch failure only if the file exists.
      break;
    }
    case ContentHashReader::InitStatus::SUCCESS: {
      // Just proceed with hashes in case of success.
      break;
    }
  }

  DCHECK(!failed_);

  hashes_ready_ = true;
  if (!queue_.empty()) {
    DCHECK_EQ(read_error_, MOJO_RESULT_OK);
    std::string tmp;
    queue_.swap(tmp);
    BytesReadImpl(std::data(tmp), tmp.size(), MOJO_RESULT_OK);
    if (failed_) {
      return;
    }
  }
  if (done_reading_) {
    ScopedElapsedTimer timer(&time_spent_);
    OnDoneReadingAndHashesReady();
  }
}

void ContentVerifyJob::BytesRead(const char* data,
                                 int count,
                                 MojoResult read_result) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!done_reading_);
  BytesReadImpl(data, count, read_result);
}

void ContentVerifyJob::DoneReading() {
  base::AutoLock auto_lock(lock_);
  ScopedElapsedTimer timer(&time_spent_);
  if (failed_)
    return;
  if (g_ignore_verification_for_tests)
    return;
  DCHECK(!done_reading_);
  done_reading_ = true;
  if (hashes_ready_) {
    OnDoneReadingAndHashesReady();
  }
}

void ContentVerifyJob::OnDoneReadingAndHashesReady() {
  // Some errors, such as the read being aborted, shouldn't cause a verification
  // failure.
  if (read_error_ != MOJO_RESULT_OK && IsIgnorableReadError(read_error_)) {
    ReportJobFinished(NONE);
    return;
  }

  if (hash_reader_->status() ==
      ContentHashReader::InitStatus::NO_HASHES_FOR_RESOURCE) {
    // Making a request to a non-existent file or to a directory should not
    // result in content verification failure.
    if (read_error_ == MOJO_RESULT_NOT_FOUND) {
      ReportJobFinished(NONE);
    } else {
      DispatchFailureCallback(NO_HASHES_FOR_FILE);
    }
    return;
  }

  // Other statuses are handled in `DidGetContentHashOnIO`.
  DCHECK_EQ(hash_reader_->status(), ContentHashReader::InitStatus::SUCCESS);

  // Any error that wasn't handled above should result in a verification
  // failure.
  if (read_error_ != MOJO_RESULT_OK) {
    DispatchFailureCallback(HASH_MISMATCH);
    return;
  }

  // Finish computing the hash and make sure the expected hash matches.
  if (!FinishBlock()) {
    DispatchFailureCallback(HASH_MISMATCH);
    return;
  }

  ReportJobFinished(NONE);
}

void ContentVerifyJob::OnHashMismatch() {
  if (hash_reader_->status() ==
      ContentHashReader::InitStatus::NO_HASHES_FOR_RESOURCE) {
    DispatchFailureCallback(NO_HASHES_FOR_FILE);
  } else {
    DCHECK_EQ(hash_reader_->status(), ContentHashReader::InitStatus::SUCCESS);
    DispatchFailureCallback(HASH_MISMATCH);
  }
}

void ContentVerifyJob::BytesReadImpl(const char* data,
                                     int count,
                                     MojoResult read_result) {
  ScopedElapsedTimer timer(&time_spent_);
  if (failed_)
    return;
  if (g_ignore_verification_for_tests)
    return;
  if (read_error_ != MOJO_RESULT_OK) {
    // If we have already seen an error, we should not continue verifying.
    return;
  }
  if (read_result != MOJO_RESULT_OK) {
    read_error_ = read_result;
    queue_.clear();
    return;
  }

  if (!hashes_ready_) {
    queue_.append(data, count);
    return;
  }
  if (hash_reader_->status() != ContentHashReader::InitStatus::SUCCESS) {
    return;
  }
  DCHECK_GE(count, 0);
  int bytes_added = 0;

  while (bytes_added < count) {
    if (current_block_ >= hash_reader_->block_count())
      return OnHashMismatch();

    if (!current_hash_) {
      current_hash_byte_count_ = 0;
      current_hash_ = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
    }
    // Compute how many bytes we should hash, and add them to the current hash.
    int bytes_to_hash =
        std::min(hash_reader_->block_size() - current_hash_byte_count_,
                 count - bytes_added);
    DCHECK_GT(bytes_to_hash, 0);
    current_hash_->Update(data + bytes_added, bytes_to_hash);
    bytes_added += bytes_to_hash;
    current_hash_byte_count_ += bytes_to_hash;
    total_bytes_read_ += bytes_to_hash;

    // If we finished reading a block worth of data, finish computing the hash
    // for it and make sure the expected hash matches.
    if (current_hash_byte_count_ == hash_reader_->block_size() &&
        !FinishBlock()) {
      OnHashMismatch();
      return;
    }
  }
}

bool ContentVerifyJob::FinishBlock() {
  DCHECK(!failed_);
  if (current_hash_byte_count_ == 0) {
    if (!done_reading_ ||
        // If we have checked all blocks already, then nothing else to do here.
        current_block_ == hash_reader_->block_count()) {
      return true;
    }
  }
  if (!current_hash_) {
    // This happens when we fail to read the resource. Compute empty content's
    // hash in this case.
    current_hash_ = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  }
  std::string final(crypto::kSHA256Length, 0);
  current_hash_->Finish(std::data(final), final.size());
  current_hash_.reset();
  current_hash_byte_count_ = 0;

  int block = current_block_++;

  const std::string* expected_hash = nullptr;
  if (!hash_reader_->GetHashForBlock(block, &expected_hash) ||
      *expected_hash != final) {
    return false;
  }

  return true;
}

// static
void ContentVerifyJob::SetIgnoreVerificationForTests(bool value) {
  DCHECK_NE(g_ignore_verification_for_tests, value);
  g_ignore_verification_for_tests = value;
}

// static
void ContentVerifyJob::SetObserverForTests(
    scoped_refptr<TestObserver> observer) {
  DCHECK(observer == nullptr ||
         g_content_verify_job_test_observer.Get() == nullptr)
      << "SetObserverForTests does not support interleaving. Observers should "
      << "be set and then cleared one at a time.";
  g_content_verify_job_test_observer.Get() = std::move(observer);
}

void ContentVerifyJob::DispatchFailureCallback(FailureReason reason) {
  DCHECK(!failed_);
  failed_ = true;
  if (!failure_callback_.is_null()) {
    VLOG(1) << "job failed for " << extension_id_ << " "
            << relative_path_.MaybeAsASCII() << " reason:" << reason;
    std::move(failure_callback_).Run(reason);
  }

  ReportJobFinished(reason);
}

void ContentVerifyJob::ReportJobFinished(FailureReason reason) {
  auto record_job_finished = [this, &reason](const char* mv2_histogram,
                                             const char* mv3_histogram) {
    if (manifest_version_ == 2) {
      base::UmaHistogramEnumeration(mv2_histogram, reason, FAILURE_REASON_MAX);
    } else if (manifest_version_ == 3) {
      base::UmaHistogramEnumeration(mv3_histogram, reason, FAILURE_REASON_MAX);
    }
  };

  record_job_finished("Extensions.ContentVerification.VerifyJobResultMV2",
                      "Extensions.ContentVerification.VerifyJobResultMV3");

  // TODO(crbug.com/325613709): Remove docs offline specific logging after a few
  // milestones.
  if (extension_id_ == extension_misc::kDocsOfflineExtensionId) {
    record_job_finished(
        "Extensions.ContentVerification.VerifyJobResultMV2.GoogleDocsOffline",
        "Extensions.ContentVerification.VerifyJobResultMV3.GoogleDocsOffline");
  }

  scoped_refptr<TestObserver> test_observer = GetTestObserver();
  if (test_observer) {
    test_observer->JobFinished(extension_id_, relative_path_, reason);
  }
}

}  // namespace extensions
