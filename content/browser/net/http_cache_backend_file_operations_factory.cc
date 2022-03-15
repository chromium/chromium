// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/http_cache_backend_file_operations_factory.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

using OpenFileFlags = network::mojom::HttpCacheBackendOpenFileFlags;
static_assert(static_cast<uint32_t>(OpenFileFlags::kOpenAndRead) ==
                  (base::File::FLAG_OPEN | base::File::FLAG_READ),
              "kOpenAndRead");
static_assert(static_cast<uint32_t>(OpenFileFlags::kCreateAndWrite) ==
                  (base::File::FLAG_CREATE | base::File::FLAG_WRITE),
              "kCreateAndWrite");

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
                  DeleteFileCallback callback) override {
    if (!IsValid(path, "DeleteFile")) {
      std::move(callback).Run(false);
      return;
    }

    bool result = base::DeleteFile(path);
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

    base::File::Error error;
    base::ReplaceFile(from_path, to_path, &error);
    DVLOG(1) << "DeleteFile: from_path = " << from_path
             << ", to_path = " << to_path << " => " << error;
    std::move(callback).Run(error);
  }

  void GetFileInfo(const base::FilePath& path,
                   GetFileInfoCallback callback) override {
    if (!IsValid(path, "GetFileInfo")) {
      std::move(callback).Run(absl::nullopt);
      return;
    }

    base::File::Info file_info;
    bool ok = base::GetFileInfo(path, &file_info);
    DVLOG(1) << "GetFileInfo: path = " << path << " => " << ok;

    std::move(callback).Run(ok ? absl::make_optional(file_info)
                               : absl::nullopt);
  }

 private:
  bool IsValid(const base::FilePath& path, base::StringPiece tag) const {
    if (!path.IsAbsolute()) {
      mojo::ReportBadMessage(static_cast<std::string>(tag) +
                             ": The path is not an absolute path.");
      return false;
    }
    if (!path_.IsParent(path)) {
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
    mojo::PendingReceiver<network::mojom::HttpCacheBackendFileOperationsFactory>
        receiver,
    const base::FilePath& path)
    : receiver_(this, std::move(receiver)), path_(path) {}
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
