// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_FILE_ERROR_INJECTOR_H_
#define CONTENT_PUBLIC_TEST_TEST_FILE_ERROR_INJECTOR_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "url/gurl.h"

namespace content {

class DownloadFileWithErrorFactory;
class DownloadManager;
class DownloadManagerImpl;

// Test helper for injecting errors into download file operations.  All errors
// for a download must be injected before it starts.  This class needs to be
// |RefCountedThreadSafe| because the implementation is referenced by other
// classes that live past the time when the user is nominally done with it.
//
// Once created, an error injected via InjectError() will cause any
// DownloadFiles created to fail with that error. Call ClearError() to stop
// injecting errors.
//
// Example:
//
// FileErrorInfo a = { ... };
//
// scoped_refptr<TestFileErrorInjector> injector =
//     TestFileErrorInjector::Create(download_manager);
//
// injector->InjectError(a);
//
// download_manager->DownloadUrl(url1, ...); // Will be interrupted due to |a|.
// download_manager->DownloadUrl(url2, ...); // Will be interrupted due to |a|.
//
// injector->ClearError();
//
// download_manager->DownloadUrl(url3, ...); // Won't be interrupted due to |a|.
class TestFileErrorInjector
    : public base::RefCountedThreadSafe<TestFileErrorInjector> {
 public:
  enum FileOperationCode {
    FILE_OPERATION_INITIALIZE,
    FILE_OPERATION_WRITE,
    FILE_OPERATION_STREAM_COMPLETE,
    FILE_OPERATION_RENAME_UNIQUIFY,
    FILE_OPERATION_RENAME_ANNOTATE,
  };

  // Structure that encapsulates the information needed to inject a file error.
  struct FileErrorInfo {
    FileErrorInfo();
    FileErrorInfo(FileOperationCode code,
                  int operation_instance,
                  download::DownloadInterruptReason error);
    FileOperationCode code;  // Operation to affect.
    int operation_instance;  // 0-based count of operation calls, for each code.
    download::DownloadInterruptReason error;  // Error to inject.
    int64_t stream_offset = -1;     // Offset of the error stream.
    int64_t stream_bytes_written = -1;  // Bytes written to the error stream.
    // If > 0, only write operations covering this offset will generate errors.
    // Otherwise, all file writes will generate errors.
    int64_t data_write_offset = -1;
  };

  // Creates an instance.  May only be called once.
  // Lives until all callbacks (in the implementation) are complete and the
  // creator goes out of scope.
  // TODO(rdsmith): Allow multiple calls for different download managers.
  static scoped_refptr<TestFileErrorInjector> Create(
      DownloadManager* download_manager);

  TestFileErrorInjector(const TestFileErrorInjector&) = delete;
  TestFileErrorInjector& operator=(const TestFileErrorInjector&) = delete;

  // Injects the errors such that new download files will be affected.
  // The download system must already be initialized before calling this.
  // Multiple calls are allowed, but only useful if the errors have changed.
  // Replaces the injected error list.
  bool InjectError(const FileErrorInfo& error_to_inject);

  // Clears all errors.
  // Only affects files created after the next call to InjectErrors().
  void ClearError();

  // Tells how many files are currently open.
  size_t CurrentFileCount() const;

  // Tells how many files have ever been open (since construction or the
  // last call to |ClearTotalFileCount()|).
  size_t TotalFileCount() const;

  // Resets the total file count. Doesn't affect what's returned by
  // CurrentFileCount().
  void ClearTotalFileCount();

  static std::string DebugString(FileOperationCode code);

 private:
  friend class base::RefCountedThreadSafe<TestFileErrorInjector>;

  explicit TestFileErrorInjector(DownloadManager* download_manager);

  virtual ~TestFileErrorInjector();

  // Callbacks from the download file, to record lifetimes.
  // These may be called on any thread.
  void RecordDownloadFileConstruction();
  void RecordDownloadFileDestruction();

  // These run on the UI thread.
  void DownloadFileCreated();
  void DestroyingDownloadFile();

  // All the data is used on the UI thread.
  // Keep track of active DownloadFiles.
  size_t active_file_count_ = 0;

  // Keep track of found DownloadFiles.
  size_t total_file_count_ = 0;

  // The factory we created. May outlive this class.
  // This dangling raw_ptr occurred in:
  // browser_tests: DownloadTest.DownloadHistoryCheck
  // https://ci.chromium.org/ui/p/chromium/builders/try/linux-chromeos-rel/1540091/test-results?q=ExactID%3Aninja%3A%2F%2Fchrome%2Ftest%3Abrowser_tests%2FDownloadTest.DownloadHistoryCheck+VHash%3A282db19e8ac0a6be
  raw_ptr<DownloadFileWithErrorFactory, FlakyDanglingUntriaged>
      created_factory_ = nullptr;

  // The download manager we set the factory on.
  raw_ptr<DownloadManagerImpl, FlakyDanglingUntriaged> download_manager_ =
      nullptr;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_FILE_ERROR_INJECTOR_H_
