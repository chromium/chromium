// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_CONTENT_VERIFY_JOB_H_
#define EXTENSIONS_BROWSER_CONTENT_VERIFY_JOB_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/version.h"
#include "extensions/browser/content_verifier/content_verifier_key.h"
#include "extensions/common/extension_id.h"
#include "mojo/public/c/system/types.h"

namespace base {
class FilePath;
}

namespace crypto {
class SecureHash;
}

namespace extensions {

class ContentHash;
class ContentHashReader;
class ContentVerifier;

// Objects of this class are responsible for verifying that the actual content
// read from an extension file matches an expected set of hashes. This class
// can be created and used on any thread.
class ContentVerifyJob : public base::RefCountedThreadSafe<ContentVerifyJob> {
 public:
  enum FailureReason {
    // No failure.
    NONE,

    // Failed because there were no expected hashes at all (eg they haven't
    // been fetched yet).
    MISSING_ALL_HASHES,

    // Failed because hashes files exist, but are unreadabale or damaged, and
    // content verifier was not able to compute new hashes.
    CORRUPTED_HASHES,

    // Failed because this file wasn't found in the list of expected hashes.
    NO_HASHES_FOR_FILE,

    // Some of the content read did not match the expected hash.
    HASH_MISMATCH,

    FAILURE_REASON_MAX
  };
  using FailureCallback = base::OnceCallback<void(FailureReason)>;

  // The |failure_callback| will be called at most once if there was a failure.
  ContentVerifyJob(const ExtensionId& extension_id,
                   const base::Version& extension_version,
                   const base::FilePath& extension_root,
                   const base::FilePath& relative_path,
                   FailureCallback failure_callback);

  // This begins the process of getting expected hashes, so it should be called
  // as early as possible.
  void Start(ContentVerifier* verifier);

  // Call this to add more bytes to verify. If at any point the read bytes
  // don't match the expected hashes, this will dispatch the failure callback.
  // The failure callback will only be run once even if more bytes are read.
  // Make sure to call Done so that any final bytes that were read that didn't
  // align exactly on a block size boundary get their hash checked as well.
  void Read(const char* data, int count, MojoResult read_result);

  // Call once when finished adding bytes via OnDone.
  // TODO(lazyboy): A more descriptive name of this method is warranted, "Done"
  // is not so appropriate.
  void Done();

  class TestObserver : public base::RefCountedThreadSafe<TestObserver> {
   public:
    virtual void JobStarted(const ExtensionId& extension_id,
                            const base::FilePath& relative_path) = 0;

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

  void DidGetContentHashOnIO(scoped_refptr<const ContentHash> hash);

  // Same as Read, but is run without acquiring lock.
  void ReadImpl(const char* data, int count, MojoResult read_result);

  // Called each time we're done adding bytes for the current block, and are
  // ready to finish the hash operation for those bytes and make sure it
  // matches what was expected for that block. Returns true if everything is
  // still ok so far, or false if a mismatch was detected.
  bool FinishBlock();

  // Dispatches the failure callback with the given reason.
  void DispatchFailureCallback(FailureReason reason);

  // Called when our ContentHashReader has finished initializing.
  void OnHashesReady(std::unique_ptr<const ContentHashReader> hash_reader);

  // True if BytesRead has seen some errors that can be ignored from content
  // verification's perspective.
  bool has_ignorable_read_error_ = false;

  // Indicates whether the caller has told us they are done calling BytesRead.
  bool done_reading_;

  // Set to true once hash_reader_ has read its expected hashes.
  bool hashes_ready_;

  // While we're waiting for the callback from the ContentHashReader, we need
  // to queue up bytes any bytes that are read.
  std::string queue_;

  // The total bytes we've read.
  int64_t total_bytes_read_;

  // The index of the block we're currently on.
  int current_block_;

  // The hash we're building up for the bytes of |current_block_|.
  std::unique_ptr<crypto::SecureHash> current_hash_;

  // The number of bytes we've already input into |current_hash_|.
  int current_hash_byte_count_;

  // Valid and set after |hashes_ready_| is set to true.
  std::unique_ptr<const ContentHashReader> hash_reader_;

  // Resource info for this verify job.
  const ExtensionId extension_id_;
  const base::Version extension_version_;
  const base::FilePath extension_root_;
  const base::FilePath relative_path_;

  base::TimeDelta time_spent_;

  // Called once if verification fails.
  FailureCallback failure_callback_;

  // Set to true if we detected a mismatch and called the failure callback.
  bool failed_;

  // Used to synchronize all public methods.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(ContentVerifyJob);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_CONTENT_VERIFY_JOB_H_
