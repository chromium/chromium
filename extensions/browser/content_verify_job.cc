// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verify_job.h"

#include <algorithm>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/timer/elapsed_timer.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "extensions/browser/content_hash_reader.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/content_verifier/content_hash.h"

namespace extensions {

namespace {

bool g_ignore_verification_for_tests = false;
ContentVerifyJob::TestObserver* g_content_verify_job_test_observer = NULL;

class ScopedElapsedTimer {
 public:
  explicit ScopedElapsedTimer(base::TimeDelta* total) : total_(total) {
    DCHECK(total_);
  }

  ~ScopedElapsedTimer() { *total_ += timer.Elapsed(); }

 private:
  // Some total amount of time we should add our elapsed time to at
  // destruction.
  base::TimeDelta* total_;

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
                                   const base::Version& extension_version,
                                   const base::FilePath& extension_root,
                                   const base::FilePath& relative_path,
                                   FailureCallback failure_callback)
    : done_reading_(false),
      hashes_ready_(false),
      total_bytes_read_(0),
      current_block_(0),
      current_hash_byte_count_(0),
      extension_id_(extension_id),
      extension_version_(extension_version),
      extension_root_(extension_root),
      relative_path_(relative_path),
      failure_callback_(std::move(failure_callback)),
      failed_(false) {}

ContentVerifyJob::~ContentVerifyJob() {
  UMA_HISTOGRAM_COUNTS_1M("ExtensionContentVerifyJob.TimeSpentUS",
                          time_spent_.InMicroseconds());
}

void ContentVerifyJob::Start(ContentVerifier* verifier) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::AutoLock auto_lock(lock_);
  verifier->GetContentHash(
      extension_id_, extension_root_, extension_version_,
      true /* force_missing_computed_hashes_creation */,
      base::BindOnce(&ContentVerifyJob::DidGetContentHashOnIO, this));
}

void ContentVerifyJob::DidGetContentHashOnIO(
    scoped_refptr<const ContentHash> content_hash) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::AutoLock auto_lock(lock_);
  if (g_content_verify_job_test_observer)
    g_content_verify_job_test_observer->JobStarted(extension_id_,
                                                   relative_path_);
  // Build |hash_reader_|.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ContentHashReader::Create, relative_path_, content_hash),
      base::BindOnce(&ContentVerifyJob::OnHashesReady, this));
}

void ContentVerifyJob::Read(const char* data,
                            int count,
                            MojoResult read_result) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!done_reading_);
  ReadImpl(data, count, read_result);
}

void ContentVerifyJob::Done() {
  base::AutoLock auto_lock(lock_);
  ScopedElapsedTimer timer(&time_spent_);
  if (failed_)
    return;
  if (g_ignore_verification_for_tests)
    return;
  DCHECK(!done_reading_);
  done_reading_ = true;
  if (!hashes_ready_)
    return;  // Wait for OnHashesReady.

  const bool can_proceed = has_ignorable_read_error_ || FinishBlock();
  if (can_proceed) {
    if (g_content_verify_job_test_observer) {
      g_content_verify_job_test_observer->JobFinished(extension_id_,
                                                      relative_path_, NONE);
    }
  } else {
    DispatchFailureCallback(HASH_MISMATCH);
  }
}

void ContentVerifyJob::ReadImpl(const char* data,
                                int count,
                                MojoResult read_result) {
  ScopedElapsedTimer timer(&time_spent_);
  if (failed_)
    return;
  if (g_ignore_verification_for_tests)
    return;
  if (IsIgnorableReadError(read_result))
    has_ignorable_read_error_ = true;
  if (has_ignorable_read_error_)
    return;

  if (!hashes_ready_) {
    queue_.append(data, count);
    return;
  }
  DCHECK_GE(count, 0);
  int bytes_added = 0;

  while (bytes_added < count) {
    if (current_block_ >= hash_reader_->block_count())
      return DispatchFailureCallback(HASH_MISMATCH);

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
      DispatchFailureCallback(HASH_MISMATCH);
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
  current_hash_->Finish(base::data(final), final.size());
  current_hash_.reset();
  current_hash_byte_count_ = 0;

  int block = current_block_++;

  const std::string* expected_hash = NULL;
  if (!hash_reader_->GetHashForBlock(block, &expected_hash) ||
      *expected_hash != final) {
    return false;
  }

  return true;
}

void ContentVerifyJob::OnHashesReady(
    std::unique_ptr<const ContentHashReader> hash_reader) {
  base::AutoLock auto_lock(lock_);
  const bool success = hash_reader->succeeded();
  hash_reader_ = std::move(hash_reader);

  if (g_ignore_verification_for_tests)
    return;
  if (g_content_verify_job_test_observer) {
    g_content_verify_job_test_observer->OnHashesReady(extension_id_,
                                                      relative_path_, success);
  }
  if (!success) {
    if (!hash_reader_->has_content_hashes()) {
      DispatchFailureCallback(MISSING_ALL_HASHES);
      return;
    }

    if (hash_reader_->file_missing_from_verified_contents()) {
      // Ignore verification of non-existent resources.
      if (g_content_verify_job_test_observer) {
        g_content_verify_job_test_observer->JobFinished(extension_id_,
                                                        relative_path_, NONE);
      }
      return;
    }
    DispatchFailureCallback(NO_HASHES_FOR_FILE);
    return;
  }

  DCHECK(!failed_);

  hashes_ready_ = true;
  if (!queue_.empty()) {
    std::string tmp;
    queue_.swap(tmp);
    ReadImpl(base::data(tmp), tmp.size(), MOJO_RESULT_OK);
    if (failed_)
      return;
  }
  if (done_reading_) {
    ScopedElapsedTimer timer(&time_spent_);
    if (!has_ignorable_read_error_ && !FinishBlock()) {
      DispatchFailureCallback(HASH_MISMATCH);
    } else if (g_content_verify_job_test_observer) {
      g_content_verify_job_test_observer->JobFinished(extension_id_,
                                                      relative_path_, NONE);
    }
  }
}

// static
void ContentVerifyJob::SetIgnoreVerificationForTests(bool value) {
  DCHECK_NE(g_ignore_verification_for_tests, value);
  g_ignore_verification_for_tests = value;
}

// static
void ContentVerifyJob::SetObserverForTests(TestObserver* observer) {
  DCHECK(observer == nullptr || g_content_verify_job_test_observer == nullptr)
      << "SetObserverForTests does not support interleaving. Observers should "
      << "be set and then cleared one at a time.";
  g_content_verify_job_test_observer = observer;
}

void ContentVerifyJob::DispatchFailureCallback(FailureReason reason) {
  DCHECK(!failed_);
  failed_ = true;
  if (!failure_callback_.is_null()) {
    VLOG(1) << "job failed for " << extension_id_ << " "
            << relative_path_.MaybeAsASCII() << " reason:" << reason;
    std::move(failure_callback_).Run(reason);
  }
  if (g_content_verify_job_test_observer) {
    g_content_verify_job_test_observer->JobFinished(extension_id_,
                                                    relative_path_, reason);
  }
}

}  // namespace extensions
