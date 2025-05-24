// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/file_session_storage.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"

namespace remoting {

namespace {

constexpr char kStoredSessionFileName[] = "session";

template <class T>
std::optional<T> make_nullopt() {
  return std::nullopt;
}

base::TaskTraits GetFileTaskTraits() {
  return {base::MayBlock(), base::TaskPriority::BEST_EFFORT};
}

void WriteFileAsync(const base::FilePath& file,
                    std::string content,
                    base::OnceCallback<void(bool)> on_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, GetFileTaskTraits(),
      base::BindOnce(
          [](base::FilePath file, std::string content) {
            return base::WriteFile(file, content);
          },
          file, std::move(content)),
      std::move(on_done));
}

void DeleteFileAsync(const base::FilePath& file,
                     base::OnceCallback<void(bool)> on_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, GetFileTaskTraits(), base::BindOnce(&base::DeleteFile, file),
      std::move(on_done));
}

void FileExistsAsync(const base::FilePath& file,
                     base::OnceCallback<void(bool)> on_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, GetFileTaskTraits(), base::BindOnce(&base::PathExists, file),
      std::move(on_done));
}

// Wrapper around base::ReadFileToString that returns the result as an optional
// string.
std::optional<std::string> ReadFileToString(const base::FilePath& file) {
  std::string result;
  bool success = base::ReadFileToString(file, &result);
  if (success) {
    return result;
  } else {
    return std::nullopt;
  }
}

void ReadFileAsync(
    const base::FilePath& file,
    base::OnceCallback<void(std::optional<std::string>)> on_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, GetFileTaskTraits(), base::BindOnce(&ReadFileToString, file),
      std::move(on_done));
}

base::FilePath GetDefaultDirectory() {
  base::FilePath result;
  base::PathService::Get(chrome::DIR_CHROMEOS_CRD_DATA, &result);
  return result;
}

}  // namespace

FileSessionStorage::FileSessionStorage()
    : FileSessionStorage(GetDefaultDirectory()) {}

FileSessionStorage::FileSessionStorage(const base::FilePath& storage_directory)
    : storage_directory_(storage_directory) {}

void FileSessionStorage::StoreSession(const base::Value::Dict& information,
                                      base::OnceClosure on_done) {
  WriteFileAsync(session_file(), *base::WriteJson(information),
                 base::BindOnce([](bool success) {
                   LOG_IF(ERROR, !success)
                       << "Failed to create CRD session information file";
                 }).Then(std::move(on_done)));
}

void FileSessionStorage::DeleteSession(base::OnceClosure on_done) {
  DeleteFileAsync(session_file(),
                  base::BindOnce([](bool success) {
                    LOG_IF(ERROR, !success)
                        << "Failed to remove CRD session information file";
                  }).Then(std::move(on_done)));
}

void FileSessionStorage::RetrieveSession(
    base::OnceCallback<void(std::optional<base::Value::Dict>)> on_done) {
  ReadFileAsync(session_file(),
                base::BindOnce([](std::optional<std::string> content) {
                  if (!content.has_value()) {
                    LOG(ERROR) << "Failed to read CRD session information file";
                    return make_nullopt<base::Value::Dict>();
                  }

                  auto dict_optional = base::JSONReader::ReadDict(*content);
                  LOG_IF(ERROR, !dict_optional.has_value())
                      << "Failed to parse stored CRD session information";
                  return dict_optional;
                }).Then(std::move(on_done)));
}

void FileSessionStorage::HasSession(
    base::OnceCallback<void(bool)> on_done) const {
  FileExistsAsync(session_file(), std::move(on_done));
}

void FileSessionStorage::SetStorageDirectoryForTesting(
    const base::FilePath& dir) {
  storage_directory_ = dir;
}

base::FilePath FileSessionStorage::session_file() const {
  return storage_directory_.AppendASCII(kStoredSessionFileName);
}

}  // namespace remoting
