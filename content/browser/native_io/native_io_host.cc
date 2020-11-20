// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_io/native_io_host.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "content/browser/native_io/native_io_context.h"
#include "content/browser/native_io/native_io_file_host.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"

namespace content {

namespace {

bool IsValidNativeIONameCharacter(char name_char) {
  return base::IsAsciiLower(name_char) || base::IsAsciiDigit(name_char) ||
         name_char == '_';
}

bool IsValidNativeIOName(const std::string& name) {
  if (name.empty())
    return false;

  return std::all_of(name.begin(), name.end(), &IsValidNativeIONameCharacter);
}

base::FilePath GetNativeIOFilePath(const base::FilePath& root_path,
                                   const std::string& name) {
  DCHECK(IsValidNativeIOName(name));

  // This simple implementation assumes that the name doesn't have any special
  // meaning to the host operating system.
  base::FilePath file_path = root_path.AppendASCII(name);
  DCHECK(root_path.IsParent(file_path));
  return file_path;
}

// Creates a task runner suitable for running file I/O tasks.
scoped_refptr<base::TaskRunner> CreateFileTaskRunner() {
  // We use a SequencedTaskRunner so that there is a global ordering to an
  // origin's directory operations.
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
base::File DoOpenFile(const base::FilePath& root_path,
                      const std::string& name) {
  DCHECK(IsValidNativeIOName(name));

  // Lazily create the origin's directory.
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(root_path, &error))
    return base::File();

  return base::File(GetNativeIOFilePath(root_path, name),
                    base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_READ |
                        base::File::FLAG_WRITE | base::File::FLAG_SHARE_DELETE);
}

// Performs the file I/O work in DeleteFile().
bool DoDeleteFile(const base::FilePath& root_path, const std::string& name) {
  DCHECK(IsValidNativeIOName(name));

  // If the origin's directory wasn't created yet, there's nothing to delete.
  if (!base::PathExists(root_path))
    return true;

  return base::DeleteFile(GetNativeIOFilePath(root_path, name));
}

using GetAllFileNamesResult =
    std::pair<base::File::Error, std::vector<std::string>>;

// Performs the file I/O work in GetAllFileNames().
GetAllFileNamesResult DoGetAllFileNames(const base::FilePath& root_path) {
  std::vector<std::string> result;

  // If the origin's directory wasn't created yet, there's no file to report.
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

// Reports the result of the file I/O work in GetAllFileNames().
void DidGetAllFileNames(
    blink::mojom::NativeIOHost::GetAllFileNamesCallback callback,
    GetAllFileNamesResult result) {
  std::move(callback).Run(result.first == base::File::FILE_OK,
                          std::move(result.second));
}

// Performs the file I/O work in RenameFile().
bool DoRenameFile(const base::FilePath& root_path,
                  const std::string& old_name,
                  const std::string& new_name) {
  DCHECK(IsValidNativeIOName(old_name));
  DCHECK(IsValidNativeIOName(new_name));

  // If the origin's directory wasn't created yet, there's nothing to rename.
  if (!base::PathExists(root_path))
    return false;

  // Do not overwrite an existing file.
  if (base::PathExists(GetNativeIOFilePath(root_path, new_name)))
    return false;

  // TODO(rstz): Report error.
  base::File::Error error;
  return base::ReplaceFile(GetNativeIOFilePath(root_path, old_name),
                           GetNativeIOFilePath(root_path, new_name), &error);
}

}  // namespace

NativeIOHost::NativeIOHost(NativeIOContext* context,
                           const url::Origin& origin,
                           base::FilePath root_path)
    : root_path_(std::move(root_path)),
      context_(context),
      origin_(origin),
      file_task_runner_(CreateFileTaskRunner()) {
  DCHECK(!root_path_.empty());
  DCHECK(context != nullptr);

  // base::Unretained is safe here because this NativeIOHost owns |receivers_|.
  // So, the unretained NativeIOHost is guaranteed to outlive |receivers_| and
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

  if (!IsValidNativeIOName(name)) {
    mojo::ReportBadMessage("Invalid file name");
    std::move(callback).Run(base::File());
    return;
  }

  if (open_file_hosts_.find(name) != open_file_hosts_.end()) {
    // TODO(pwnall): Report that the file is locked.
    std::move(callback).Run(base::File());
    return;
  }

  auto insert_result = io_pending_files_.insert(name);
  bool insert_success = insert_result.second;
  if (!insert_success) {
    // TODO(pwnall): Report that the file is locked.
    std::move(callback).Run(base::File());
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

  if (!IsValidNativeIOName(name)) {
    mojo::ReportBadMessage("Invalid file name");
    std::move(callback).Run(false);
    return;
  }

  if (open_file_hosts_.find(name) != open_file_hosts_.end()) {
    // TODO(pwnall): Report that the file is locked.
    std::move(callback).Run(false);
    return;
  }

  auto insert_result = io_pending_files_.insert(name);
  bool insert_success = insert_result.second;
  if (!insert_success) {
    // TODO(pwnall): Report that the file is locked.
    std::move(callback).Run(false);
    return;
  }

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoDeleteFile, root_path_, name),
      base::BindOnce(&NativeIOHost::DidDeleteFile, weak_factory_.GetWeakPtr(),
                     name, std::move(callback)));
}

void NativeIOHost::GetAllFileNames(GetAllFileNamesCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoGetAllFileNames, root_path_),
      base::BindOnce(&DidGetAllFileNames, std::move(callback)));
}

void NativeIOHost::RenameFile(const std::string& old_name,
                              const std::string& new_name,
                              RenameFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!IsValidNativeIOName(old_name) || !IsValidNativeIOName(new_name)) {
    mojo::ReportBadMessage("Invalid file name");
    std::move(callback).Run(false);
    return;
  }

  if (open_file_hosts_.find(old_name) != open_file_hosts_.end() ||
      open_file_hosts_.find(new_name) != open_file_hosts_.end()) {
    // TODO(rstz): Report that the file is locked.
    std::move(callback).Run(false);
    return;
  }

  auto old_iterator_and_success = io_pending_files_.insert(old_name);
  if (!old_iterator_and_success.second) {
    std::move(callback).Run(false);
    return;
  }
  auto new_iterator_and_success = io_pending_files_.insert(new_name);
  if (!new_iterator_and_success.second) {
    io_pending_files_.erase(old_iterator_and_success.first);
    std::move(callback).Run(false);
    return;
  }

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&DoRenameFile, root_path_, old_name, new_name),
      base::BindOnce(&NativeIOHost::DidRenameFile, weak_factory_.GetWeakPtr(),
                     old_name, new_name, std::move(callback)));
}

void NativeIOHost::OnFileClose(NativeIOFileHost* file_host) {
  DCHECK(open_file_hosts_.count(file_host->file_name()) > 0);
  DCHECK_EQ(open_file_hosts_[file_host->file_name()].get(), file_host);

  open_file_hosts_.erase(file_host->file_name());
}

void NativeIOHost::OnReceiverDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  context_->OnHostReceiverDisconnect(this);
}

void NativeIOHost::DidOpenFile(
    const std::string& name,
    mojo::PendingReceiver<blink::mojom::NativeIOFileHost> file_host_receiver,
    OpenFileCallback callback,
    base::File file) {
  DCHECK(io_pending_files_.count(name));
  DCHECK(!open_file_hosts_.count(name));
  io_pending_files_.erase(name);

  if (!file.IsValid()) {
    std::move(callback).Run(std::move(file));
    return;
  }

  open_file_hosts_.insert(
      {name, std::make_unique<NativeIOFileHost>(std::move(file_host_receiver),
                                                this, name)});

  std::move(callback).Run(std::move(file));
  return;
}

void NativeIOHost::DidDeleteFile(const std::string& name,
                                 DeleteFileCallback callback,
                                 bool success) {
  DCHECK(io_pending_files_.count(name));
  DCHECK(!open_file_hosts_.count(name));
  io_pending_files_.erase(name);

  std::move(callback).Run(success);
  return;
}

void NativeIOHost::DidRenameFile(const std::string& old_name,
                                 const std::string& new_name,
                                 RenameFileCallback callback,
                                 bool success) {
  DCHECK(io_pending_files_.count(old_name));
  DCHECK(!open_file_hosts_.count(old_name));
  DCHECK(io_pending_files_.count(new_name));
  DCHECK(!open_file_hosts_.count(new_name));
  io_pending_files_.erase(old_name);
  io_pending_files_.erase(new_name);

  std::move(callback).Run(success);
  return;
}

}  // namespace content
