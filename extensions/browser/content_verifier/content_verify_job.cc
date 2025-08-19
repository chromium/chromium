// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verifier/content_verify_job.h"

#include <algorithm>

#include "base/containers/span.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/typed_macros.h"
#include "base/version_info/channel.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/hash.h"
#include "extensions/browser/content_hash_reader.h"
#include "extensions/browser/content_verifier/content_hash.h"
#include "extensions/browser/content_verifier/content_verifier.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"

namespace extensions {

namespace {

bool g_ignore_verification_for_tests = false;

scoped_refptr<ContentVerifyJob::TestObserver>& GetTestObserver() {
  static base::NoDestructor<scoped_refptr<ContentVerifyJob::TestObserver>>
      instance;
  return *instance;
}

bool IsIgnorableReadError(MojoResult read_result) {
  // Extension reload, for example, can cause benign MOJO_RESULT_ABORTED error.
  // Do not incorrectly fail content verification in that case.
  // See https://crbug.com/977805 for details.
  return read_result == MOJO_RESULT_ABORTED;
}

base::debug::CrashKeyString* GetContentHashExtensionVersionFromRootCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ext_content_hash_root_version", base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString*
GetContentVerifyJobExtensionVersionFromRootCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ext_verify_job_root_version", base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetContentHashExtensionIdCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ext_content_hash_id", base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetContentVerifyJobExtensionIdCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ext_verify_job_id", base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetContentHashExtensionVersionCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ext_content_hash_ext_version", base::debug::CrashKeySize::Size256);
  return crash_key;
}

base::debug::CrashKeyString* GetContentVerifyJobExtensionVersionCrashKey() {
  static auto* crash_key = base::debug::AllocateCrashKeyString(
      "ext_verify_job_ext_version", base::debug::CrashKeySize::Size256);
  return crash_key;
}

// Returns the last path component of the extension root filepath, which should
// be the extension version.
std::string GetExtensionVersionFromExtensionRoot(
    const base::FilePath& extension_root) {
  return extension_root.BaseName().MaybeAsASCII();
}

}  // namespace

namespace debug {

// Helper for adding a crash keys when extension roots don't match during
// content verification.
//
// It is only created at the start of the verification process when the process
// is provided content verification hashes *and* the extension roots for the
// content verification hash and the verification job don't match.
//
// All keys are logged every time this class is instantiated.
class ScopedContentVerifyJobCrashKey {
 public:
  explicit ScopedContentVerifyJobCrashKey(
      const base::FilePath& content_hash_extension_root,
      const base::FilePath& verify_job_extension_root,
      const ExtensionId& content_hash_extension_id,
      const ExtensionId& verify_job_extension_id,
      const base::Version& content_hash_extension_version,
      const base::Version& verify_job_extension_version)
      : content_hash_ext_root_version_crash_key_(
            GetContentHashExtensionVersionFromRootCrashKey(),
            GetExtensionVersionFromExtensionRoot(content_hash_extension_root)),
        verify_job_ext_root_version_crash_key_(
            GetContentVerifyJobExtensionVersionFromRootCrashKey(),
            GetExtensionVersionFromExtensionRoot(verify_job_extension_root)),
        content_hash_ext_id_crash_key_(GetContentHashExtensionIdCrashKey(),
                                       content_hash_extension_id),
        verify_job_ext_id_crash_key_(GetContentVerifyJobExtensionIdCrashKey(),
                                     verify_job_extension_id),
        content_hash_ext_version_crash_key_(
            GetContentHashExtensionVersionCrashKey(),
            content_hash_extension_version.GetString()),
        verify_job_ext_version_crash_key_(
            GetContentVerifyJobExtensionVersionCrashKey(),
            verify_job_extension_version.GetString())

  {}
  ~ScopedContentVerifyJobCrashKey() = default;

 private:
  // These record the extension's version from the extension root of ContentHash
  // and ContentVerify Job. E.g. from:
  //   "/path/to/chromium/<profile_name>/Extensions/<ext_id>/<ext_version>/""
  //
  // We record <ext_version>.
  base::debug::ScopedCrashKeyString content_hash_ext_root_version_crash_key_;
  base::debug::ScopedCrashKeyString verify_job_ext_root_version_crash_key_;

  // The ExtensionId for ContentHash and ContentVerify Job.
  base::debug::ScopedCrashKeyString content_hash_ext_id_crash_key_;
  base::debug::ScopedCrashKeyString verify_job_ext_id_crash_key_;

  // These extension version recorded when the content hash was created and when
  // the verification job was created.
  base::debug::ScopedCrashKeyString content_hash_ext_version_crash_key_;
  base::debug::ScopedCrashKeyString verify_job_ext_version_crash_key_;
};

}  // namespace debug

ContentVerifyJob::ContentVerifyJob(const ExtensionId& extension_id,
                                   const base::Version& extension_version,
                                   const base::FilePath& extension_root,
                                   const base::FilePath& relative_path)
    : extension_id_(extension_id),
      extension_root_(extension_root),
      relative_path_(relative_path),
      extension_version_(extension_version) {}

ContentVerifyJob::~ContentVerifyJob() = default;

void ContentVerifyJob::Start(ContentVerifier* verifier,
                             const base::Version& current_extension_version,
                             int manifest_version,
                             FailureCallback failure_callback) {
  TRACE_EVENT("extensions.content_verifier.debug", "ContentVerifyJob::Start",
              "job_extension_version", extension_version_.GetString(),
              "current_extension_version",
              current_extension_version.GetString(), "job_root",
              extension_root_);
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (base::FeatureList::IsEnabled(
          extensions_features::kContentVerifyJobUseJobVersionForHashing) &&
      current_extension_version != extension_version_) {
    // The version should have been checked in ContentVerifier::StartJob(), so
    // we should never reach here.
    NOTREACHED() << "Content verification job was started for an extension "
                    "version other than the currently loaded extension. "
                    "Hashing errors could occur if the job continued.";
  }

  base::AutoLock auto_lock(lock_);
  manifest_version_ = manifest_version;
  failure_callback_ = std::move(failure_callback);

  // The content verification hashes are most likely already cached.
  auto content_hash = verifier->GetCachedContentHash(
      extension_id_, extension_version_,
      /*force_missing_computed_hashes_creation=*/true);
  if (content_hash) {
    StartWithContentHash(std::move(content_hash));
    return;
  }

  verifier->CreateContentHash(
      extension_id_, extension_root_, extension_version_,
      /*force_missing_computed_hashes_creation=*/true,
      base::BindOnce(&ContentVerifyJob::DidCreateContentHashOnIO, this));
}

void ContentVerifyJob::DidCreateContentHashOnIO(
    scoped_refptr<const ContentHash> content_hash) {
  TRACE_EVENT("extensions.content_verifier.debug",
              "ContentVerifyJob::DidCreateContentHashOnIO", "hash_extension_id",
              content_hash->extension_id(), "hash_extension_root",
              content_hash->extension_root());
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  base::AutoLock auto_lock(lock_);
  StartWithContentHash(std::move(content_hash));
}

void ContentVerifyJob::StartWithContentHash(
    scoped_refptr<const ContentHash> content_hash) {
  TRACE_EVENT("extensions.content_verifier.debug",
              "ContentVerifyJob::StartWithContentHash", "job_root",
              extension_root_, "hash_root", content_hash->extension_root());
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (base::FeatureList::IsEnabled(
          extensions_features::kContentVerifyJobUseJobVersionForHashing) &&
      content_hash->extension_version() != extension_version_) {
    // TODO(crbug.com/416484593): Remove crash keys once we're confident the
    // issue is fixed.
    debug::ScopedContentVerifyJobCrashKey crash_keys(
        content_hash->extension_root(), extension_root_,
        content_hash->extension_id(), extension_id_,
        content_hash->extension_version(), extension_version_);
    // The version should have been checked in ContentVerifierJob::Start(), so
    // we should never reach here.
    NOTREACHED() << "Content verification job was started with a hash for a "
                    "different extension version. Hashing errors could occur "
                    "if the job continued.";
  }

  // If the hash and the verify jobs' roots don't match then the hash comparison
  // done later will match against the wrong files.
  if ((GetCurrentChannel() == version_info::Channel::CANARY ||
       GetCurrentChannel() == version_info::Channel::DEV) &&
      content_hash->extension_root() != extension_root_) {
    debug::ScopedContentVerifyJobCrashKey crash_keys(
        content_hash->extension_root(), extension_root_,
        content_hash->extension_id(), extension_id_,
        content_hash->extension_version(), extension_version_);
    base::debug::DumpWithoutCrashing();
  }

  // Build |hash_reader_|.
  hash_reader_ = ContentHashReader::Create(relative_path_, content_hash);

  if (g_ignore_verification_for_tests) {
    return;
  }

  scoped_refptr<TestObserver> test_observer = GetTestObserver();
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
      // Proceed and dispatch failure only if the file exists.
      break;
    }
    case ContentHashReader::InitStatus::SUCCESS: {
      // Just proceed with hashes in case of success.
      break;
    }
  }

  // Verification can't actually happen until hashes_ready_, so this object
  // can't enter a failed state before that point, and the only way for
  // hashes_ready_ to become true is right below this.
  DCHECK(!failed_);

  hashes_ready_ = true;
  if (!queue_.empty()) {
    DCHECK_EQ(read_error_, MOJO_RESULT_OK);
    std::string tmp;
    queue_.swap(tmp);
    BytesReadImpl(tmp, MOJO_RESULT_OK);
    if (failed_) {
      return;
    }
  }
  if (done_reading_) {
    OnDoneReadingAndHashesReady();
  }
}

void ContentVerifyJob::BytesRead(base::span<const char> data,
                                 MojoResult read_result) {
  base::AutoLock auto_lock(lock_);
  DCHECK(!done_reading_);
  BytesReadImpl(data, read_result);
}

void ContentVerifyJob::DoneReading() {
  base::AutoLock auto_lock(lock_);
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

void ContentVerifyJob::BytesReadImpl(base::span<const char> data,
                                     MojoResult read_result) {
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
    queue_.append(data.begin(), data.end());
    return;
  }
  if (hash_reader_->status() != ContentHashReader::InitStatus::SUCCESS) {
    return;
  }
  const int count = data.size();
  int bytes_added = 0;

  while (bytes_added < count) {
    if (current_block_ >= hash_reader_->block_count())
      return OnHashMismatch();

    if (!current_hash_) {
      current_hash_byte_count_ = 0;
      current_hash_ = crypto::hash::Hasher(crypto::hash::kSha256);
    }
    // Compute how many bytes we should hash, and add them to the current hash.
    int bytes_to_hash =
        std::min(hash_reader_->block_size() - current_hash_byte_count_,
                 count - bytes_added);
    DCHECK_GT(bytes_to_hash, 0);
    auto bytes_span = base::as_byte_span(data).subspan(
        // TODO(https://crbug.com/434977723): get rid of these checked casts
        // when this code uses size_t throughout.
        base::checked_cast<size_t>(bytes_added),
        base::checked_cast<size_t>(bytes_to_hash));
    current_hash_->Update(bytes_span);
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
    current_hash_ = crypto::hash::Hasher(crypto::hash::kSha256);
  }
  std::string final(crypto::hash::kSha256Size, 0);
  current_hash_->Finish(base::as_writable_byte_span(final));
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
  DCHECK(observer == nullptr || GetTestObserver() == nullptr)
      << "SetObserverForTests does not support interleaving. Observers should "
      << "be set and then cleared one at a time.";
  GetTestObserver() = std::move(observer);
}

void ContentVerifyJob::DispatchFailureCallback(FailureReason reason) {
  DCHECK(!failed_);
  failed_ = true;
  if (!failure_callback_.is_null()) {
    // TODO(crbug.com/416484593): Reduce back to VLOG once the cause and fix has
    // been determined.
    LOG(ERROR) << "Content verify job failed for extension: " << extension_id_
               << " at path: " << relative_path_.MaybeAsASCII()
               << " and for reason:" << reason;
    std::move(failure_callback_).Run(reason);
  }

  ReportJobFinished(reason);
}

void ContentVerifyJob::ReportJobFinished(FailureReason reason) {
  auto record_job_finished = [this, &reason](const char* mv2_histogram,
                                             const char* mv3_histogram) {
    if (mv2_histogram && manifest_version_ == 2) {
      base::UmaHistogramEnumeration(mv2_histogram, reason, FAILURE_REASON_MAX);
    } else if (manifest_version_ == 3) {
      base::UmaHistogramEnumeration(mv3_histogram, reason, FAILURE_REASON_MAX);
    }
  };

  record_job_finished("Extensions.ContentVerification.VerifyJobResultMV2",
                      "Extensions.ContentVerification.VerifyJobResultMV3");

  scoped_refptr<TestObserver> test_observer = GetTestObserver();
  if (test_observer) {
    test_observer->JobFinished(extension_id_, relative_path_, reason);
  }
}

}  // namespace extensions
