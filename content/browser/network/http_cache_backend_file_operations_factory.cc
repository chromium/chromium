// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/http_cache_backend_file_operations_factory.h"

#include <string_view>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/simple/simple_file_enumerator.h"
#include "net/disk_cache/simple/simple_util.h"

namespace content {

namespace {

using OpenFileFlags = network::mojom::HttpCacheBackendOpenFileFlags;
static_assert(static_cast<uint32_t>(OpenFileFlags::kOpenAndRead) ==
                  (base::File::FLAG_OPEN | base::File::FLAG_READ),
              "kOpenAndRead");
static_assert(static_cast<uint32_t>(OpenFileFlags::kCreateAndWrite) ==
                  (base::File::FLAG_CREATE | base::File::FLAG_WRITE),
              "kCreateAndWrite");
static_assert(
    static_cast<uint32_t>(OpenFileFlags::kOpenReadWriteWinShareDelete) ==
        (base::File::FLAG_OPEN | base::File::FLAG_READ |
         base::File::FLAG_WRITE | base::File::FLAG_WIN_SHARE_DELETE),
    "kOpenReadWriteWinShareDelete");
static_assert(
    static_cast<uint32_t>(OpenFileFlags::kCreateReadWriteWinShareDelete) ==
        (base::File::FLAG_CREATE | base::File::FLAG_READ |
         base::File::FLAG_WRITE | base::File::FLAG_WIN_SHARE_DELETE),
    "kCreateReadWriteWinShareDelete");
static_assert(
    static_cast<uint32_t>(OpenFileFlags::kCreateAlwaysWriteWinShareDelete) ==
        (base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE |
         base::File::FLAG_WIN_SHARE_DELETE),
    "kCreateReadWriteWinShareDelete");
static_assert(static_cast<uint32_t>(
                  OpenFileFlags::kOpenReadWinShareDeleteWinSequentialScan) ==
                  (base::File::FLAG_OPEN | base::File::FLAG_READ |
                   base::File::FLAG_WIN_SHARE_DELETE |
                   base::File::FLAG_WIN_SEQUENTIAL_SCAN),
              "kOpenReadWinShareDeleteWinSequentialScan");

class FileEnumerator final : public network::mojom::FileEnumerator {
 public:
  explicit FileEnumerator(const base::FilePath& path) : enumerator_(path) {}
  ~FileEnumerator() override = default;

  void GetNext(uint32_t num_entries, GetNextCallback callback) override {
    std::vector<disk_cache::BackendFileOperations::FileEnumerationEntry>
        entries;
    bool end = false;
    for (uint32_t i = 0; i < num_entries; ++i) {
      if (auto entry = enumerator_.Next()) {
        entries.push_back(std::move(*entry));
      } else {
        end = true;
        break;
      }
    }
    std::move(callback).Run(entries, end, enumerator_.HasError());
  }

 private:
  disk_cache::SimpleFileEnumerator enumerator_;
};

class HttpCacheBackendFileOperations final
    : public network::mojom::HttpCacheBackendFileOperations {
 public:
  // All the operations must be performed under `path`.
  explicit HttpCacheBackendFileOperations(const base::FilePath& path)
      : path_(path) {
    DCHECK(path.IsAbsolute());
  }
  ~HttpCacheBackendFileOperations() override = default;

  void CreateDirectory(const base::FilePath& path,
                       CreateDirectoryCallback callback) override {
    if (!IsValid(path, "CreateDirectory")) {
      std::move(callback).Run(false);
      return;
    }

    bool result = base::CreateDirectory(path);
    DVLOG(1) << "CreateDirectory: path = " << path << " => " << result;
    std::move(callback).Run(result);
  }

  void PathExists(const base::FilePath& path,
                  PathExistsCallback callback) override {
    if (!IsValid(path, "PathExists")) {
      std::move(callback).Run(false);
      return;
    }

    bool result = base::PathExists(path);
    DVLOG(1) << "PathExists: path = " << path << " => " << result;
    std::move(callback).Run(result);
  }

  void DirectoryExists(const base::FilePath& path,
                       DirectoryExistsCallback callback) override {
    if (!IsValid(path, "DirectoryExists")) {
      std::move(callback).Run(false);
      return;
    }

    bool result = base::DirectoryExists(path);
    DVLOG(1) << "DirectoryExists: path = " << path << " => " << result;
    std::move(callback).Run(result);
  }

  void OpenFile(const base::FilePath& path,
                network::mojom::HttpCacheBackendOpenFileFlags flags,
                OpenFileCallback callback) override {
    // `flags` has already been checked in the deserializer.
    if (!IsValid(path, "OpenFile")) {
      std::move(callback).Run(base::File(),
                              base::File::FILE_ERROR_ACCESS_DENIED);
      return;
    }

    auto flags_to_pass = static_cast<uint32_t>(flags);
    base::File file(path, flags_to_pass);
    base::File::Error error = file.error_details();
    DVLOG(1) << "OpenFile: path = " << path << ", flags = " << flags_to_pass
             << " => file.IsValid() = " << file.IsValid();
    std::move(callback).Run(std::move(file), error);
  }

  void DeleteFile(const base::FilePath& path,
                  network::mojom::HttpCacheBackendDeleteFileMode mode,
                  DeleteFileCallback callback) override {
    using network::mojom::HttpCacheBackendDeleteFileMode;
    if (!IsValid(path, "DeleteFile")) {
      std::move(callback).Run(false);
      return;
    }

    bool result = false;
    switch (mode) {
      case HttpCacheBackendDeleteFileMode::kDefault:
        result = base::DeleteFile(path);
        break;
      case HttpCacheBackendDeleteFileMode::kEnsureImmediateAvailability:
        result = disk_cache::simple_util::SimpleCacheDeleteFile(path);
        break;
    }
    DVLOG(1) << "DeleteFile: path = " << path << " => " << result;
    std::move(callback).Run(result);
  }

  void RenameFile(const base::FilePath& from_path,
                  const base::FilePath& to_path,
                  RenameFileCallback callback) override {
    if (!IsValid(from_path, "RenameFile (from)")) {
      std::move(callback).Run(base::File::FILE_ERROR_ACCESS_DENIED);
      return;
    }
    if (!IsValid(to_path, "RenameFile (to)")) {
      std::move(callback).Run(base::File::FILE_ERROR_ACCESS_DENIED);
      return;
    }

    base::File::Error error = base::File::FILE_OK;
    base::ReplaceFile(from_path, to_path, &error);
    DVLOG(1) << "DeleteFile: from_path = " << from_path
             << ", to_path = " << to_path << " => " << error;
    std::move(callback).Run(error);
  }

  void GetFileInfo(const base::FilePath& path,
                   GetFileInfoCallback callback) override {
    if (!IsValid(path, "GetFileInfo")) {
      std::move(callback).Run(std::nullopt);
      return;
    }

    base::File::Info file_info;
    bool ok = base::GetFileInfo(path, &file_info);
    DVLOG(1) << "GetFileInfo: path = " << path << " => " << ok;

    std::move(callback).Run(ok ? std::make_optional(file_info) : std::nullopt);
  }

  void EnumerateFiles(
      const base::FilePath& path,
      mojo::PendingReceiver<network::mojom::FileEnumerator> receiver) override {
    if (!IsValid(path, "EnumerateFiles")) {
      return;
    }
    DVLOG(1) << "EnumerateFiles: path = " << path;
    mojo::MakeSelfOwnedReceiver(std::make_unique<FileEnumerator>(path),
                                std::move(receiver));
  }

  void CleanupDirectory(const base::FilePath& path,
                        CleanupDirectoryCallback callback) override {
    if (!IsValid(path, "CleanupDirectory")) {
      std::move(callback).Run(false);
      return;
    }
    disk_cache::CleanupDirectory(path, std::move(callback));
  }

 private:
  bool IsValid(const base::FilePath& path, std::string_view tag) const {
    if (!path.IsAbsolute()) {
      mojo::ReportBadMessage(static_cast<std::string>(tag) +
                             ": The path is not an absolute path.");
      return false;
    }
    if (path_ != path && !path_.IsParent(path)) {
      mojo::ReportBadMessage(static_cast<std::string>(tag) +
                             ": The path is not in the specified area.");
      return false;
    }
    if (path.ReferencesParent()) {
      mojo::ReportBadMessage(
          static_cast<std::string>(tag) +
          ": The path contains a parent directory traversal attempt.");
      return false;
    }
    return true;
  }

  const base::FilePath path_;
};

}  // namespace

HttpCacheBackendFileOperationsFactory::HttpCacheBackendFileOperationsFactory(
    const base::FilePath& path)
    : path_(path) {}
HttpCacheBackendFileOperationsFactory::
    ~HttpCacheBackendFileOperationsFactory() = default;

void HttpCacheBackendFileOperationsFactory::Create(
    mojo::PendingReceiver<network::mojom::HttpCacheBackendFileOperations>
        receiver) {
  auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](mojo::PendingReceiver<
                 network::mojom::HttpCacheBackendFileOperations> receiver,
             const base::FilePath& path) {
            mojo::MakeSelfOwnedReceiver(
                std::make_unique<HttpCacheBackendFileOperations>(path),
                std::move(receiver), nullptr);
          },
          std::move(receiver), path_));
}

}  // namespace content
