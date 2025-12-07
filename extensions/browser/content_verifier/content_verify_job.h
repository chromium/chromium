// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFY_JOB_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFY_JOB_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/version.h"
#include "crypto/hash.h"
#include "extensions/browser/content_verifier/content_verifier_key.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/c/system/types.h"

namespace base {
class FilePath;
}

namespace extensions {

class ContentHash;
class ContentHashReader;
class ContentVerifier;

// Objects of this class are responsible for verifying that the actual content
// read from an extension file matches an expected set of hashes. This class
// has complex threading behavior: it can be created on any thread, and some of
// its methods can be called on any thread, but most of them can only be called
// on the IO thread.
//
// The basic usage flow is like this:
// 1. Create a ContentVerifyJob on any thread
// 2. Post a task to the IO thread to call Start() on it
// 3. If you already have bytes of the data to hash available, pass them in via
//    BytesRead() (which can be called on any thread)
// 4. Once verification is done, the failure callback will be run *on an
//    unspecified thread* - it can be either the IO thread or the thread you
//    called DoneReading() from. If the failure callback is never run, then
//    verification succeeded. Your callback must tolerate being run on any
//    thread and it must tolerate never being run at all.
//
// You may want to use BindPostTask() with this class to deal with the
// unpredictable callback thread.
class ContentVerifyJob : public base::RefCountedThreadSafe<ContentVerifyJob> {
 public:
  // Used in UMA metrics. Ensure this stays in sync with
  // CorruptExtensionDisabledReason in
  // //tools/metrics/histograms/metadata/extensions/enums.xml.
  enum FailureReason {
    // No failure.
    NONE,

    // Failed because there were no expected hashes at all (eg they haven't
    // been fetched yet).
    MISSING_ALL_HASHES,

    // Failed because hashes files exist, but are unreadable or damaged, and
    // content verifier was not able to compute new hashes.
    CORRUPTED_HASHES,

    // Failed because this file wasn't found in the list of expected hashes.
    NO_HASHES_FOR_FILE,

    // Some of the content read did not match the expected hash.
    HASH_MISMATCH,

    FAILURE_REASON_MAX
  };
  using FailureCallback = base::OnceCallback<void(FailureReason)>;

  ContentVerifyJob(const ExtensionId& extension_id,
                   const base::Version& extension_version,
                   const base::FilePath& extension_root,
                   const base::FilePath& relative_path);

  ContentVerifyJob(const ContentVerifyJob&) = delete;
  ContentVerifyJob& operator=(const ContentVerifyJob&) = delete;

  // This begins the process of getting expected hashes, so it should be called
  // as early as possible.
  // The `failure_callback` will be called at most once if there was a failure.
  void Start(ContentVerifier* verifier,
             const base::Version& current_extension_version,
             int manifest_version,
             FailureCallback failure_callback);

  // Call this to add more bytes to verify. If at any point the read bytes
  // don't match the expected hashes, this will dispatch the failure callback.
  // The failure callback will only be run once even if more bytes are read.
  // Make sure to call DoneReading so that any final bytes that were read that
  // didn't align exactly on a block size boundary get their hash checked as
  // well.
  void BytesRead(base::span<const char> data, MojoResult read_result);

  // Call once when finished adding bytes via OnDone.
  void DoneReading();

  const ExtensionId& extension_id() const { return extension_id_; }
  const base::Version& extension_version() const { return extension_version_; }
  const base::FilePath& relative_path() const { return relative_path_; }

  class TestObserver : public base::RefCountedThreadSafe<TestObserver> {
   public:
    virtual void JobFinished(const ExtensionId& extension_id,
                             const base::FilePath& relative_path,
                             FailureReason failure_reason) = 0;

    virtual void OnHashesReady(const ExtensionId& extension_id,
                               const base::FilePath& relative_path,
                               const ContentHashReader& hash_reader) = 0;

   protected:
    virtual ~TestObserver() = default;
    friend class base::RefCountedThreadSafe<TestObserver>;
  };

  static void SetIgnoreVerificationForTests(bool value);

  // Note: having interleaved observer is not supported.
  static void SetObserverForTests(scoped_refptr<TestObserver> observer);

 private:
  virtual ~ContentVerifyJob();
  friend class base::RefCountedThreadSafe<ContentVerifyJob>;

  // Called when the content verification hashes are created.
  void DidCreateContentHashOnIO(scoped_refptr<const ContentHash> hash);

  // Starts the verification process with the content verification hashes.
  void StartWithContentHash(scoped_refptr<const ContentHash> hash);

  // Called once both the content has been read and the relevant hashes have
  // been fetched.
  void OnDoneReadingAndHashesReady();

  // Called once a hash mismatch is found.
  void OnHashMismatch();

  // Same as BytesRead, but is run without acquiring lock.
  void BytesReadImpl(base::span<const char> data, MojoResult read_result);

  // Called each time we're done adding bytes for the current block, and are
  // ready to finish the hash operation for those bytes and make sure it
  // matches what was expected for that block. Returns true if everything is
  // still ok so far, or false if a mismatch was detected.
  bool FinishBlock();

  // Dispatches the failure callback with the given reason.
  void DispatchFailureCallback(FailureReason reason);

  // Reports the job has completed (successfully or with a failure).
  void ReportJobFinished(FailureReason reason);

  // Indicates the read error, if any, of the verified file.
  MojoResult read_error_ = MOJO_RESULT_OK;

  // Indicates whether the caller has told us they are done calling BytesRead.
  bool done_reading_ = false;

  // Set to true once hash_reader_ has read its expected hashes.
  bool hashes_ready_ = false;

  // In between when the job is created and when it is started, the caller can
  // call BytesRead() to start pushing in bytes to verify. In that case, we
  // queue those bytes here.
  std::string queue_;

  // The total bytes we've read.
  int64_t total_bytes_read_ = 0;

  // The index of the block we're currently on.
  int current_block_ = 0;

  // The hash we're building up for the bytes of `current_block_`.
  std::optional<crypto::hash::Hasher> current_hash_;

  // The number of bytes we've already input into `current_hash_`.
  int current_hash_byte_count_ = 0;

  // Valid and set after `hashes_ready_` is set to true.
  std::unique_ptr<const ContentHashReader> hash_reader_;

  // Resource info for this verify job.
  const ExtensionId extension_id_;
  const base::FilePath extension_root_;
  const base::FilePath relative_path_;

  // The manifest version of the extension associated with the verify job.
  // Used only for metrics purposes.
  int manifest_version_ = 0;

  // The version of the extension associated with the verify job.
  // Used for comparing against the version from `ContentHash`.
  const base::Version extension_version_;

  // Called once if verification fails.
  FailureCallback failure_callback_;

  // Set to true if we detected a mismatch and called the failure callback.
  bool failed_ = false;

  // Used to synchronize all public methods.
  base::Lock lock_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFIER_CONTENT_VERIFY_JOB_H_
