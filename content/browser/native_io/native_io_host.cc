// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_host.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/pass_key.h"
#include "build/build_config.h"
#include "content/browser/native_io/native_io_file_host.h"
#include "content/browser/native_io/native_io_manager.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/native_io/native_io_utils.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"

using blink::mojom::NativeIOError;
using blink::mojom::NativeIOErrorPtr;
using blink::mojom::NativeIOErrorType;

namespace content {

namespace {

bool IsValidNativeIONameCharacter(char name_char) {
  return base::IsAsciiLower(name_char) || base::IsAsciiDigit(name_char) ||
         name_char == '_';
}

// Maximum allowed filename length, inclusive.
const int kMaximumFilenameLength = 100;

bool IsValidNativeIOName(const std::string& name) {
  if (name.empty())
    return false;

  if (name.length() > kMaximumFilenameLength)
    return false;

  return base::ranges::all_of(name, &IsValidNativeIONameCharacter);
}

base::FilePath GetNativeIOFilePath(const base::FilePath& root_path,
                                   const std::string& name) {
  DCHECK(IsValidNativeIOName(name));
  DCHECK(!root_path.empty());

  // This simple implementation assumes that the name doesn't have any special
  // meaning to the host operating system.
  base::FilePath file_path = root_path.AppendASCII(name);
  DCHECK(root_path.IsParent(file_path));
  return file_path;
}

// Creates a task runner suitable for running file I/O tasks.
scoped_refptr<base::TaskRunner> CreateFileTaskRunner() {
  // We use a SequencedTaskRunner so that there is a global ordering to a
  // storage key's directory operations.
  return base::ThreadPool::CreateSequencedTaskRunner({
      // Needed for file I/O.
      base::MayBlock(),

      // Reasonable compromise, given that a few database operations are
      // blocking, while most operations are not. We should be able to do better
      // when we get scheduling APIs on the Web Platform.
      base::TaskPriority::USER_VISIBLE,

      // BLOCK_SHUTDOWN is definitely not appropriate. We might be able to move
      // to CONTINUE_ON_SHUTDOWN after very careful analysis.
      base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
  });
}

// Performs the file I/O work in OpenFile().
std::pair<base::File, int64_t> DoOpenFile(const base::FilePath& root_path,
                                          const std::string& name) {
  DCHECK(IsValidNativeIOName(name));
  DCHECK(!root_path.empty());

  // Lazily create the storage key's directory.
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(root_path, &error))
    return {base::File(), /*file_length=*/0};

  // SHARE_DELETE allows the browser to delete files even if a compromised
  // renderer refuses to close its file handles.
  int open_flags = base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                   base::File::FLAG_WRITE | base::File::FLAG_WIN_SHARE_DELETE;
  base::File file(GetNativeIOFilePath(root_path, name), open_flags);

  int64_t file_length = file.IsValid() ? file.GetLength() : 0;

  return {std::move(file), file_length};
}

// Performs the file I/O work in DeleteFile().
std::pair<blink::mojom::NativeIOErrorPtr, int64_t> DoDeleteFile(
    const base::FilePath& root_path,
    const std::string& name) {
  DCHECK(IsValidNativeIOName(name));
  DCHECK(!root_path.empty());

  // If the storage key's directory wasn't created yet, there's nothing to
  // delete.
  if (!base::PathExists(root_path))
    return {NativeIOError::New(NativeIOErrorType::kSuccess, ""),
            /*deleted_file_length=*/0};

  int64_t deleted_file_length;
  base::FilePath file_path = GetNativeIOFilePath(root_path, name);
  // If the file wasn't created yet, there's nothing to delete.
  if (!base::PathExists(file_path))
    return {NativeIOError::New(NativeIOErrorType::kSuccess, ""),
            /*deleted_file_length=*/0};
  if (!base::GetFileSize(file_path, &deleted_file_length))
    return {NativeIOManager::FileErrorToNativeIOError(
                base::File::GetLastFileError()),
            /*deleted_file_length=*/0};

  if (!base::DeleteFile(file_path))
    return {NativeIOManager::FileErrorToNativeIOError(
                base::File::GetLastFileError()),
            /*deleted_file_length=*/0};

  return {NativeIOError::New(NativeIOErrorType::kSuccess, ""),
          deleted_file_length};
}

using GetAllFileNamesResult =
    std::pair<base::File::Error, std::vector<std::string>>;

// Performs the file I/O work in GetAllFileNames().
GetAllFileNamesResult DoGetAllFileNames(const base::FilePath& root_path) {
  DCHECK(!root_path.empty());

  std::vector<std::string> result;

  // If the storage key's directory wasn't created yet, there's no file to
  // report.
  if (!base::PathExists(root_path))
    return {base::File::FILE_OK, std::move(result)};

  base::FileEnumerator file_enumerator(
      root_path, /*recursive=*/false, base::FileEnumerator::FILES,
      /*pattern=*/base::FilePath::StringType(),
      base::FileEnumerator::FolderSearchPolicy::ALL,
      base::FileEnumerator::ErrorPolicy::STOP_ENUMERATION);

  // TODO(pwnall): The result vector can grow to an unbounded size. Add a limit
  //               parameter with a reasonable upper bound.

  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    // If the file name has a non-ASCII character, |file_name| will be the empty
    // string. This will correctly be flagged as corruption by the check below.
    std::string file_name = file_path.BaseName().MaybeAsASCII();

    // Chrome's NativeIO implementation only creates files that have valid
    // NativeIO names. Any other file names imply directory corruption.
    if (!IsValidNativeIOName(file_name)) {
      // TODO(pwnall): Figure out the corruption handling strategy. We could
      //               silently ignore the corrupted file, delete it, or stop
      //               and report an error.
      continue;
    }
    result.push_back(std::move(file_name));
  }

  // Don't return a partial list of files if an error occurred. The partial list
  // isn't meaningful, and may be useful information for a compromised renderer.
  //
  // TODO(pwnall): Reconsider this if we end up making NativeIO unusually
  //               friendly to corruption recovery.
  base::File::Error enumeration_error = file_enumerator.GetError();
  if (enumeration_error != base::File::FILE_OK)
    result.clear();

  return {enumeration_error, std::move(result)};
}

// Performs the file I/O work in RenameFile().
NativeIOErrorPtr DoRenameFile(const base::FilePath& root_path,
                              const std::string& old_name,
                              const std::string& new_name) {
  DCHECK(!root_path.empty());
  DCHECK(IsValidNativeIOName(old_name));
  DCHECK(IsValidNativeIOName(new_name));

  base::File::Error error = base::File::FILE_OK;
  // If the storage key's directory wasn't created yet, there's nothing to
  // rename. This error cannot be used to determine the existence of files
  // outside of the storage key's directory, as |old_name| is a valid NativeIO
  // name.
  if (!base::PathExists(root_path) ||
      !base::PathExists(GetNativeIOFilePath(root_path, old_name))) {
    return NativeIOError::New(NativeIOErrorType::kNotFound,
                              "Source file does not exist");
  }

  // Do not overwrite an existing file. This error cannot be used to determine
  // the existence of files outside of the storage key's directory, as
  // `new_name` is a valid NativeIO name.
  if (base::PathExists(GetNativeIOFilePath(root_path, new_name)))
    return NativeIOError::New(NativeIOErrorType::kNoModificationAllowed,
                              "Target file exists");

  base::ReplaceFile(GetNativeIOFilePath(root_path, old_name),
                    GetNativeIOFilePath(root_path, new_name), &error);
  return NativeIOManager::FileErrorToNativeIOError(error);
}

// Performs the file I/O work in DeleteAllData().
base::File::Error DoDeleteAllData(const base::FilePath& storage_key_dir) {
  DCHECK(!storage_key_dir.empty());
  CHECK(!storage_key_dir.ReferencesParent())
      << "Removing a parent directory is disallowed.";
  bool delete_success = base::DeletePathRecursively(storage_key_dir);
  if (!delete_success) {
    return base::File::GetLastFileError();
  }
  return base::File::FILE_OK;
}

}  // namespace

NativeIOHost::NativeIOHost(const blink::StorageKey& storage_key,
                           base::FilePath root_path,
#if BUILDFLAG(IS_MAC)
                           bool allow_set_length_ipc,
#endif  // BUILDFLAG(IS_MAC)
                           NativeIOManager* manager)
    : storage_key_(storage_key),
      root_path_(std::move(root_path)),
#if BUILDFLAG(IS_MAC)
      allow_set_length_ipc_(allow_set_length_ipc),
#endif  // BUILDFLAG(IS_MAC)
      manager_(manager),
      file_task_runner_(CreateFileTaskRunner()) {
  DCHECK(manager != nullptr);

  // base::Unretained is safe here because this NativeIOHost owns `receivers_`.
  // So, the unretained NativeIOHost is guaranteed to outlive `receivers_` and
  // the closure that it uses.
  receivers_.set_disconnect_handler(base::BindRepeating(
      &NativeIOHost::OnReceiverDisconnect, base::Unretained(this)));
}

NativeIOHost::~NativeIOHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NativeIOHost::BindReceiver(
    mojo::PendingReceiver<blink::mojom::NativeIOHost> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(receiver));
}

void NativeIOHost::OpenFile(
    const std::string& name,
    mojo::PendingReceiver<blink::mojom::NativeIOFileHost> file_host_receiver,
    OpenFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (is_incognito_mode()) {
    std::move(callback).Run(
        base::File(), /*file_length=*/0,
        NativeIOError::New(NativeIOErrorType::kInvalidState,
                           "StorageFoundation unavailable for this host"));
    return;
  }

  if (delete_all_data_in_progress()) {
    std::move(callback).Run(
        base::File(), /*file_length=*/0,
        NativeIOError::New(NativeIOErrorType::kInvalidState,
                           "Data removal pending on storage key"));
    return;
  }

  if (!IsValidNativeIOName(name)) {
    mojo::ReportBadMessage("Invalid file name");
    std::move(callback).Run(
        base::File(), /*file_length=*/0,
        NativeIOError::New(NativeIOErrorType::kUnknown, "Invalid file name"));
    return;
  }

  if (open_file_hosts_.find(name) != open_file_hosts_.end()) {
    std::move(callback).Run(
        base::File(), /*file_length=*/0,
        NativeIOError::New(NativeIOErrorType::kNoModificationAllowed,
                           "File is open"));
    return;
  }

  auto insert_result = io_pending_files_.insert(name);
  bool insert_success = insert_result.second;
  if (!insert_success) {
    std::move(callback).Run(
        base::File(), /*file_length=*/0,
        NativeIOError::New(NativeIOErrorType::kNoModificationAllowed,
                           "Operation pending on file"));
    return;
  }

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoOpenFile, root_path_, name),
      base::BindOnce(&NativeIOHost::DidOpenFile, weak_factory_.GetWeakPtr(),
                     name, std::move(file_host_receiver), std::move(callback)));
}

void NativeIOHost::DeleteFile(const std::string& name,
                              DeleteFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (is_incognito_mode()) {
    std::move(callback).Run(
        NativeIOError::New(NativeIOErrorType::kInvalidState,
                           "StorageFoundation unavailable for this host"),
        /*granted_capacity_delta=*/0);
    return;
  }

  if (delete_all_data_in_progress()) {
    std::move(callback).Run(
        NativeIOError::New(NativeIOErrorType::kInvalidState,
                           "Data removal pending on storage key"),
        /*granted_capacity_delta=*/0);
    return;
  }

  if (!IsValidNativeIOName(name)) {
    mojo::ReportBadMessage("Invalid file name");
    std::move(callback).Run(
        NativeIOError::New(NativeIOErrorType::kUnknown, "Invalid file name"),
        /*granted_capacity_delta=*/0);
    return;
  }

  if (open_file_hosts_.find(name) != open_file_hosts_.end()) {
    std::move(callback).Run(
        NativeIOError::New(NativeIOErrorType::kNoModificationAllowed,
                           "File is open"),
        /*granted_capacity_delta=*/0);
    return;
  }

  auto insert_result = io_pending_files_.insert(name);
  bool insert_success = insert_result.second;
  if (!insert_success) {
    std::move(callback).Run(
        NativeIOError::New(NativeIOErrorType::kNoModificationAllowed,
                           "Operation pending on file"),
        /*granted_capacity_delta=*/0);
    return;
  }

  manager_->quota_manager_proxy()->NotifyBucketAccessed(
      storage::BucketLocator::ForDefaultBucket(storage_key()),
      base::Time::Now());

  // The deletion task runs on the file_task_runner and is skipped on shutdown,
  // as is ok for storage key data deletion.
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoDeleteFile, root_path_, name),
      base::BindOnce(&NativeIOHost::DidDeleteFile, weak_factory_.GetWeakPtr(),
                     name, std::move(callback)));
}

void NativeIOHost::GetAllFileNames(GetAllFileNamesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (is_incognito_mode()) {
    std::move(callback).Run(false, {});
    return;
  }

  if (delete_all_data_in_progress()) {
    std::move(callback).Run(false, {});
    return;
  }

  manager_->quota_manager_proxy()->NotifyBucketAccessed(
      storage::BucketLocator::ForDefaultBucket(storage_key()),
      base::Time::Now());

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoGetAllFileNames, root_path_),
      base::BindOnce(
          [](blink::mojom::NativeIOHost::GetAllFileNamesCallback callback,
             GetAllFileNamesResult result) {
            std::move(callback).Run(result.first == base::File::FILE_OK,
                                    std::move(result.second));
          },
          std::move(callback)));
}

void NativeIOHost::RenameFile(const std::string& old_name,
                              const std::string& new_name,
                              RenameFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (is_incognito_mode()) {
    std::move(callback).Run(
        NativeIOError::New(NativeIOErrorType::kInvalidState,
                           "StorageFoundation unavailable for this host"));
    return;
  }

  if (delete_all_data_in_progress()) {
    std::move(callback).Run(
        NativeIOError::New(NativeIOErrorType::kInvalidState,
                           "Data removal pending on storage key"));
    return;
  }

  if (!IsValidNativeIOName(old_name) || !IsValidNativeIOName(new_name)) {
    mojo::ReportBadMessage("Invalid file name");
    std::move(callback).Run(
        NativeIOError::New(NativeIOErrorType::kUnknown, "Invalid file name"));
    return;
  }

  if (open_file_hosts_.find(old_name) != open_file_hosts_.end() ||
      open_file_hosts_.find(new_name) != open_file_hosts_.end()) {
    std::move(callback).Run(NativeIOError::New(
        NativeIOErrorType::kNoModificationAllowed, "Source file is open"));
    return;
  }

  if (open_file_hosts_.find(old_name) != open_file_hosts_.end()) {
    std::move(callback).Run(NativeIOError::New(
        NativeIOErrorType::kNoModificationAllowed, "Target file is open"));
    return;
  }

  auto old_iterator_and_success = io_pending_files_.insert(old_name);
  if (!old_iterator_and_success.second) {
    std::move(callback).Run(
        NativeIOError::New(NativeIOErrorType::kNoModificationAllowed,
                           "Operation pending on source file"));
    return;
  }
  auto new_iterator_and_success = io_pending_files_.insert(new_name);
  if (!new_iterator_and_success.second) {
    io_pending_files_.erase(old_iterator_and_success.first);
    std::move(callback).Run(
        NativeIOError::New(NativeIOErrorType::kNoModificationAllowed,
                           "Operation pending on target file"));
    return;
  }

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoRenameFile, root_path_, old_name, new_name),
      base::BindOnce(&NativeIOHost::DidRenameFile, weak_factory_.GetWeakPtr(),
                     old_name, new_name, std::move(callback)));
}

void NativeIOHost::RequestCapacityChange(
    int64_t capacity_delta,
    RequestCapacityChangeCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  if (is_incognito_mode()) {
    std::move(callback).Run(0);
    return;
  }

  // TODO(rstz): Implement quota limits.
  constexpr int64_t kMaximumAllocation = int64_t{8} * 1024 * 1024 * 1024;
  if (capacity_delta > kMaximumAllocation) {
    std::move(callback).Run(0);
    return;
  }
  std::move(callback).Run(capacity_delta);
}

void NativeIOHost::OnFileClose(NativeIOFileHost* file_host,
                               base::PassKey<NativeIOFileHost>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(open_file_hosts_.count(file_host->file_name()) > 0);
  DCHECK_EQ(open_file_hosts_[file_host->file_name()].get(), file_host);

  open_file_hosts_.erase(file_host->file_name());
}

void NativeIOHost::DeleteAllData(DeleteAllDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  delete_all_data_callbacks_.push_back(std::move(callback));
  if (delete_all_data_callbacks_.size() > 1) {
    return;
  }

  // Clearing open file hosts informs the renderer that the file handles should
  // not be used any longer.
  open_file_hosts_.clear();

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoDeleteAllData, root_path_),
      base::BindOnce(&NativeIOHost::DidDeleteAllData,
                     weak_factory_.GetWeakPtr()));
}

void NativeIOHost::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // May delete `this`.
  manager_->OnHostReceiverDisconnect(this, base::PassKey<NativeIOHost>());
}

void NativeIOHost::DidOpenFile(
    const std::string& name,
    mojo::PendingReceiver<blink::mojom::NativeIOFileHost> file_host_receiver,
    OpenFileCallback callback,
    std::pair<base::File, int64_t> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_pending_files_.count(name));
  DCHECK(!open_file_hosts_.count(name));

  base::File file = std::move(result.first);
  int64_t length = result.second;
  io_pending_files_.erase(name);

  base::File::Error open_error = file.error_details();

  if (!file.IsValid()) {
    // Make sure an error is reported whenever the file is not valid.
    open_error = open_error != base::File::FILE_OK
                     ? open_error
                     : base::File::FILE_ERROR_FAILED;
    std::move(callback).Run(
        std::move(file), length,
        NativeIOManager::FileErrorToNativeIOError(open_error));
    return;
  }

  // DoOpenFile may create a file if none exists, which justifies
  // NotifyBucketModified.
  manager_->quota_manager_proxy()->NotifyBucketModified(
      storage::QuotaClientType::kNativeIO,
      storage::BucketLocator::ForDefaultBucket(storage_key()), 0,
      base::Time::Now(), base::SequencedTaskRunner::GetCurrentDefault(),
      base::DoNothing());

  open_file_hosts_.insert({
    name, std::make_unique<NativeIOFileHost>(this, name,
#if BUILDFLAG(IS_MAC)
                                             allow_set_length_ipc_,
#endif  // BUILDFLAG(IS_MAC)
                                             std::move(file_host_receiver))
  });

  std::move(callback).Run(
      std::move(file), length,
      NativeIOManager::FileErrorToNativeIOError(open_error));
  return;
}

void NativeIOHost::DidDeleteFile(
    const std::string& name,
    DeleteFileCallback callback,
    std::pair<blink::mojom::NativeIOErrorPtr, int64_t> delete_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_pending_files_.count(name));
  DCHECK(!open_file_hosts_.count(name));

  io_pending_files_.erase(name);

  manager_->quota_manager_proxy()->NotifyBucketModified(
      storage::QuotaClientType::kNativeIO,
      storage::BucketLocator::ForDefaultBucket(storage_key()), 0,
      base::Time::Now(), base::SequencedTaskRunner::GetCurrentDefault(),
      base::DoNothing());

  std::move(callback).Run(std::move(delete_result.first), delete_result.second);
  return;
}

void NativeIOHost::DidRenameFile(const std::string& old_name,
                                 const std::string& new_name,
                                 RenameFileCallback callback,
                                 NativeIOErrorPtr rename_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_pending_files_.count(old_name));
  DCHECK(!open_file_hosts_.count(old_name));
  DCHECK(io_pending_files_.count(new_name));
  DCHECK(!open_file_hosts_.count(new_name));

  io_pending_files_.erase(old_name);
  io_pending_files_.erase(new_name);

  manager_->quota_manager_proxy()->NotifyBucketModified(
      storage::QuotaClientType::kNativeIO,
      storage::BucketLocator::ForDefaultBucket(storage_key()), 0,
      base::Time::Now(), base::SequencedTaskRunner::GetCurrentDefault(),
      base::DoNothing());

  std::move(callback).Run(std::move(rename_error));
  return;
}

void NativeIOHost::DidDeleteAllData(base::File::Error error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Moving callbacks to a local variable to avoid race conditions if the vector
  // is accessed during callback execution.
  std::vector<DeleteAllDataCallback> callbacks =
      std::move(delete_all_data_callbacks_);
  delete_all_data_callbacks_.clear();
  for (DeleteAllDataCallback& callback : callbacks) {
    std::move(callback).Run(error);
  }

  // May delete `this`.
  manager_->DidDeleteHostData(this, base::PassKey<NativeIOHost>());
}

}  // namespace content
