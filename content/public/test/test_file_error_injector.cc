// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_file_error_injector.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/download/public/common/download_file_factory.h"
#include "components/download/public/common/download_file_impl.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_task_runner.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace net {
class NetLogWithSource;
}

namespace content {
class ByteStreamReader;

namespace {

// A class that performs file operations and injects errors.
class DownloadFileWithError : public download::DownloadFileImpl {
 public:
  DownloadFileWithError(
      std::unique_ptr<download::DownloadSaveInfo> save_info,
      const base::FilePath& default_download_directory,
      std::unique_ptr<download::InputStream> stream,
      uint32_t download_id,
      base::WeakPtr<download::DownloadDestinationObserver> observer,
      const TestFileErrorInjector::FileErrorInfo& error_info,
      base::OnceClosure ctor_callback,
      base::OnceClosure dtor_callback);

  ~DownloadFileWithError() override;

  void Initialize(
      InitializeCallback initialize_callback,
      CancelRequestCallback cancel_request_callback,
      const download::DownloadItem::ReceivedSlices& received_slices) override;

  // DownloadFile interface.
  download::DownloadInterruptReason ValidateAndWriteDataToFile(
      int64_t offset,
      const char* data,
      size_t bytes_to_validate,
      size_t bytes_to_write) override;

  download::DownloadInterruptReason HandleStreamCompletionStatus(
      SourceStream* source_stream) override;

  void RenameAndUniquify(const base::FilePath& full_path,
                         RenameCompletionCallback callback) override;
  void RenameAndAnnotate(
      const base::FilePath& full_path,
      const std::string& client_guid,
      const GURL& source_url,
      const GURL& referrer_url,
      const std::optional<url::Origin>& request_initiator,
      mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
      RenameCompletionCallback callback) override;

 private:
  // Error generating helper.
  download::DownloadInterruptReason ShouldReturnError(
      TestFileErrorInjector::FileOperationCode code,
      download::DownloadInterruptReason original_error);

  // Determine whether to overwrite an operation with the given code
  // with a substitute error; if returns true, |*original_error| is
  // written with the error to use for overwriting.
  // NOTE: This routine changes state; specifically, it increases the
  // operations counts for the specified code.  It should only be called
  // once per operation.
  bool OverwriteError(TestFileErrorInjector::FileOperationCode code,
                      download::DownloadInterruptReason* output_error);

  // Our injected error.  Only one per file.
  TestFileErrorInjector::FileErrorInfo error_info_;

  // Count per operation.  0-based.
  std::map<TestFileErrorInjector::FileOperationCode, int> operation_counter_;

  // Callback for destruction.
  base::OnceClosure destruction_callback_;
};

static void InitializeErrorCallback(
    download::DownloadFile::InitializeCallback original_callback,
    download::DownloadInterruptReason overwrite_error,
    download::DownloadInterruptReason original_error,
    int64_t bytes_wasted) {
  std::move(original_callback).Run(overwrite_error, bytes_wasted);
}

static void RenameErrorCallback(
    download::DownloadFile::RenameCompletionCallback original_callback,
    download::DownloadInterruptReason overwrite_error,
    download::DownloadInterruptReason original_error,
    const base::FilePath& path_result) {
  std::move(original_callback)
      .Run(overwrite_error,
           overwrite_error == download::DOWNLOAD_INTERRUPT_REASON_NONE
               ? path_result
               : base::FilePath());
}

DownloadFileWithError::DownloadFileWithError(
    std::unique_ptr<download::DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    std::unique_ptr<download::InputStream> stream,
    uint32_t download_id,
    base::WeakPtr<download::DownloadDestinationObserver> observer,
    const TestFileErrorInjector::FileErrorInfo& error_info,
    base::OnceClosure ctor_callback,
    base::OnceClosure dtor_callback)
    : download::DownloadFileImpl(std::move(save_info),
                                 default_download_directory,
                                 std::move(stream),
                                 download_id,
                                 observer),
      error_info_(error_info),
      destruction_callback_(std::move(dtor_callback)) {
  // DownloadFiles are created on the UI thread and are destroyed on the
  // download task runner. Schedule the ConstructionCallback on the
  // download task runner, so that if a download::DownloadItem schedules a
  // DownloadFile to be destroyed and creates another one (as happens during
  // download resumption), then the DestructionCallback for the old DownloadFile
  // is run before the ConstructionCallback for the next DownloadFile.
  download::GetDownloadTaskRunner()->PostTask(FROM_HERE,
                                              std::move(ctor_callback));
}

DownloadFileWithError::~DownloadFileWithError() {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  std::move(destruction_callback_).Run();
}

void DownloadFileWithError::Initialize(
    InitializeCallback initialize_callback,
    CancelRequestCallback cancel_request_callback,
    const download::DownloadItem::ReceivedSlices& received_slices) {
  download::DownloadInterruptReason error_to_return =
      download::DOWNLOAD_INTERRUPT_REASON_NONE;
  InitializeCallback callback_to_use = std::move(initialize_callback);

  // Replace callback if the error needs to be overwritten.
  if (OverwriteError(
          TestFileErrorInjector::FILE_OPERATION_INITIALIZE,
          &error_to_return)) {
    if (download::DOWNLOAD_INTERRUPT_REASON_NONE != error_to_return) {
      // Don't execute a, probably successful, Initialize; just
      // return the error.
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback_to_use), error_to_return, 0));
      return;
    }

    // Otherwise, just wrap the return.
    callback_to_use = base::BindRepeating(
        &InitializeErrorCallback, std::move(callback_to_use), error_to_return);
  }

  download::DownloadFileImpl::Initialize(std::move(callback_to_use),
                                         std::move(cancel_request_callback),
                                         received_slices);
}

download::DownloadInterruptReason
DownloadFileWithError::ValidateAndWriteDataToFile(int64_t offset,
                                                  const char* data,
                                                  size_t bytes_to_validate,
                                                  size_t bytes_to_write) {
  download::DownloadInterruptReason origin_error =
      download::DownloadFileImpl::ValidateAndWriteDataToFile(
          offset, data, bytes_to_validate, bytes_to_write);
  if (error_info_.data_write_offset == -1 ||
      ((offset <= error_info_.data_write_offset) &&
       (offset + bytes_to_write >=
        static_cast<size_t>(error_info_.data_write_offset)))) {
    return ShouldReturnError(TestFileErrorInjector::FILE_OPERATION_WRITE,
                             origin_error);
  }
  return origin_error;
}

download::DownloadInterruptReason
DownloadFileWithError::HandleStreamCompletionStatus(
    SourceStream* source_stream) {
  download::DownloadInterruptReason origin_error =
      download::DownloadFileImpl::HandleStreamCompletionStatus(source_stream);

  if (error_info_.code ==
          TestFileErrorInjector::FILE_OPERATION_STREAM_COMPLETE &&
      source_stream->offset() == error_info_.stream_offset &&
      source_stream->bytes_written() >= error_info_.stream_bytes_written) {
    return error_info_.error;
  }

  return origin_error;
}

void DownloadFileWithError::RenameAndUniquify(
    const base::FilePath& full_path,
    RenameCompletionCallback callback) {
  download::DownloadInterruptReason error_to_return =
      download::DOWNLOAD_INTERRUPT_REASON_NONE;
  RenameCompletionCallback callback_to_use;

  // Replace callback if the error needs to be overwritten.
  if (OverwriteError(
          TestFileErrorInjector::FILE_OPERATION_RENAME_UNIQUIFY,
          &error_to_return)) {
    if (download::DOWNLOAD_INTERRUPT_REASON_NONE != error_to_return) {
      // Don't execute a, probably successful, RenameAndUniquify; just
      // return the error.
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), error_to_return,
                                    base::FilePath()));
      return;
    }

    // Otherwise, just wrap the return.
    callback_to_use = base::BindOnce(&RenameErrorCallback, std::move(callback),
                                     error_to_return);
  }

  if (!callback_to_use)
    callback_to_use = std::move(callback);

  download::DownloadFileImpl::RenameAndUniquify(full_path,
                                                std::move(callback_to_use));
}

void DownloadFileWithError::RenameAndAnnotate(
    const base::FilePath& full_path,
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url,
    const std::optional<url::Origin>& request_initiator,
    mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
    RenameCompletionCallback callback) {
  download::DownloadInterruptReason error_to_return =
      download::DOWNLOAD_INTERRUPT_REASON_NONE;
  RenameCompletionCallback callback_to_use;

  // Replace callback if the error needs to be overwritten.
  if (OverwriteError(
          TestFileErrorInjector::FILE_OPERATION_RENAME_ANNOTATE,
          &error_to_return)) {
    if (download::DOWNLOAD_INTERRUPT_REASON_NONE != error_to_return) {
      // Don't execute a, probably successful, RenameAndAnnotate; just
      // return the error.
      GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), error_to_return,
                                    base::FilePath()));
      return;
    }

    // Otherwise, just wrap the return.
    callback_to_use = base::BindOnce(&RenameErrorCallback, std::move(callback),
                                     error_to_return);
  }

  if (!callback_to_use)
    callback_to_use = std::move(callback);

  download::DownloadFileImpl::RenameAndAnnotate(
      full_path, client_guid, source_url, referrer_url,
      /*request_initiator=*/std::nullopt, mojo::NullRemote(),
      std::move(callback_to_use));
}

bool DownloadFileWithError::OverwriteError(
    TestFileErrorInjector::FileOperationCode code,
    download::DownloadInterruptReason* output_error) {
  int counter = operation_counter_[code]++;

  if (code != error_info_.code)
    return false;

  if (counter != error_info_.operation_instance)
    return false;

  *output_error = error_info_.error;
  return true;
}

download::DownloadInterruptReason DownloadFileWithError::ShouldReturnError(
    TestFileErrorInjector::FileOperationCode code,
    download::DownloadInterruptReason original_error) {
  download::DownloadInterruptReason output_error = original_error;
  OverwriteError(code, &output_error);
  return output_error;
}

}  // namespace

// A factory for constructing DownloadFiles that inject errors.
class DownloadFileWithErrorFactory : public download::DownloadFileFactory {
 public:
  DownloadFileWithErrorFactory(base::RepeatingClosure ctor_callback,
                               base::RepeatingClosure dtor_callback);
  ~DownloadFileWithErrorFactory() override;

  // DownloadFileFactory interface.
  download::DownloadFile* CreateFile(
      std::unique_ptr<download::DownloadSaveInfo> save_info,
      const base::FilePath& default_download_directory,
      std::unique_ptr<download::InputStream> stream,
      uint32_t download_id,
      const base::FilePath& duplicate_download_file_path,
      base::WeakPtr<download::DownloadDestinationObserver> observer) override;

  bool SetError(TestFileErrorInjector::FileErrorInfo error);

 private:
  // Our injected error.
  TestFileErrorInjector::FileErrorInfo injected_error_;

  // Callback for creation and destruction of a DownloadFile.
  base::RepeatingClosure construction_callback_;
  base::RepeatingClosure destruction_callback_;
};

DownloadFileWithErrorFactory::DownloadFileWithErrorFactory(
    base::RepeatingClosure ctor_callback,
    base::RepeatingClosure dtor_callback)
    : construction_callback_(std::move(ctor_callback)),
      destruction_callback_(std::move(dtor_callback)) {}

DownloadFileWithErrorFactory::~DownloadFileWithErrorFactory() {}

download::DownloadFile* DownloadFileWithErrorFactory::CreateFile(
    std::unique_ptr<download::DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    std::unique_ptr<download::InputStream> stream,
    uint32_t download_id,
    const base::FilePath& duplicate_download_file_path,
    base::WeakPtr<download::DownloadDestinationObserver> observer) {
  return new DownloadFileWithError(
      std::move(save_info), default_download_directory, std::move(stream),
      download_id, observer, injected_error_, construction_callback_,
      destruction_callback_);
}

bool DownloadFileWithErrorFactory::SetError(
    TestFileErrorInjector::FileErrorInfo error) {
  injected_error_ = std::move(error);
  return true;
}

TestFileErrorInjector::FileErrorInfo::FileErrorInfo()
    : FileErrorInfo(FILE_OPERATION_INITIALIZE,
                    -1,
                    download::DOWNLOAD_INTERRUPT_REASON_NONE) {}

TestFileErrorInjector::FileErrorInfo::FileErrorInfo(
    FileOperationCode code,
    int operation_instance,
    download::DownloadInterruptReason error)
    : code(code), operation_instance(operation_instance), error(error) {}

TestFileErrorInjector::TestFileErrorInjector(DownloadManager* download_manager)
    :  // This code is only used for browser_tests, so a
      // DownloadManager is always a DownloadManagerImpl.
      download_manager_(static_cast<DownloadManagerImpl*>(download_manager)) {
}

TestFileErrorInjector::~TestFileErrorInjector() {
}

void TestFileErrorInjector::ClearError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // An error with an index of -1, which will never be reached.
  static const TestFileErrorInjector::FileErrorInfo kNoOpErrorInfo = {
      TestFileErrorInjector::FILE_OPERATION_INITIALIZE, -1,
      download::DOWNLOAD_INTERRUPT_REASON_NONE};
  InjectError(kNoOpErrorInfo);
}

bool TestFileErrorInjector::InjectError(const FileErrorInfo& error_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ClearTotalFileCount();
  DCHECK_EQ(static_cast<download::DownloadFileFactory*>(created_factory_),
            download_manager_->GetDownloadFileFactoryForTesting());
  created_factory_->SetError(error_info);
  return true;
}

size_t TestFileErrorInjector::CurrentFileCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return active_file_count_;
}

size_t TestFileErrorInjector::TotalFileCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return total_file_count_;
}

void TestFileErrorInjector::ClearTotalFileCount() {
  total_file_count_ = 0;
}

void TestFileErrorInjector::DownloadFileCreated() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ++active_file_count_;
  ++total_file_count_;
}

void TestFileErrorInjector::DestroyingDownloadFile() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  --active_file_count_;
}

void TestFileErrorInjector::RecordDownloadFileConstruction() {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&TestFileErrorInjector::DownloadFileCreated, this));
}

void TestFileErrorInjector::RecordDownloadFileDestruction() {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&TestFileErrorInjector::DestroyingDownloadFile, this));
}

// static
scoped_refptr<TestFileErrorInjector> TestFileErrorInjector::Create(
    DownloadManager* download_manager) {
  static bool visited = false;
  DCHECK(!visited);  // Only allowed to be called once.
  visited = true;

  scoped_refptr<TestFileErrorInjector> single_injector(
      new TestFileErrorInjector(download_manager));
  // Record the value of the pointer, for later validation.
  single_injector->created_factory_ = new DownloadFileWithErrorFactory(
      base::BindRepeating(
          &TestFileErrorInjector::RecordDownloadFileConstruction,
          single_injector),
      base::BindRepeating(&TestFileErrorInjector::RecordDownloadFileDestruction,
                          single_injector));

  // We will transfer ownership of the factory to the download manager.
  std::unique_ptr<download::DownloadFileFactory> download_file_factory(
      single_injector->created_factory_);

  single_injector->download_manager_->SetDownloadFileFactoryForTesting(
      std::move(download_file_factory));

  return single_injector;
}

// static
std::string TestFileErrorInjector::DebugString(FileOperationCode code) {
  switch (code) {
    case FILE_OPERATION_INITIALIZE:
      return "INITIALIZE";
    case FILE_OPERATION_WRITE:
      return "WRITE";
    case FILE_OPERATION_RENAME_UNIQUIFY:
      return "RENAME_UNIQUIFY";
    case FILE_OPERATION_RENAME_ANNOTATE:
      return "RENAME_ANNOTATE";
    default:
      break;
  }

  return "Unknown";
}

}  // namespace content
