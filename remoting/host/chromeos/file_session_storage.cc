// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/file_session_storage.h"

#include <optional>

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "remoting/base/async_file_util.h"

namespace remoting {

namespace {

constexpr char kStoredSessionFileName[] = "session";

template <class T>
std::optional<T> make_nullopt() {
  return std::nullopt;
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
                 base::BindOnce([](base::FileErrorOr<void> result) {
                   LOG_IF(ERROR, !result.has_value())
                       << "Failed to create CRD session information file: "
                       << base::File::ErrorToString(result.error());
                 }).Then(std::move(on_done)));
}

void FileSessionStorage::DeleteSession(base::OnceClosure on_done) {
  DeleteFileAsync(session_file(),
                  base::BindOnce([](base::FileErrorOr<void> result) {
                    LOG_IF(ERROR, !result.has_value())
                        << "Failed to remove CRD session information file: "
                        << base::File::ErrorToString(result.error());
                  }).Then(std::move(on_done)));
}

void FileSessionStorage::RetrieveSession(
    base::OnceCallback<void(std::optional<base::Value::Dict>)> on_done) {
  ReadFileAsync(session_file(),
                base::BindOnce([](base::FileErrorOr<std::string> content) {
                  if (!content.has_value()) {
                    LOG(ERROR)
                        << "Failed to read CRD session information file: "
                        << base::File::ErrorToString(content.error());
                    return make_nullopt<base::Value::Dict>();
                  }

                  auto dict_optional = base::JSONReader::ReadDict(
                      *content, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
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
