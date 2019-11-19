// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_prioritized_origin_database.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "storage/browser/file_system/sandbox_isolated_origin_database.h"
#include "storage/browser/file_system/sandbox_origin_database.h"
#include "third_party/leveldatabase/leveldb_chrome.h"

namespace storage {

const base::FilePath::CharType* const
    SandboxPrioritizedOriginDatabase::kPrimaryDirectory =
        FILE_PATH_LITERAL("primary");

const base::FilePath::CharType* const
    SandboxPrioritizedOriginDatabase::kPrimaryOriginFile =
        FILE_PATH_LITERAL("primary.origin");

namespace {

bool WritePrimaryOriginFile(const base::FilePath& path,
                            const std::string& origin) {
  base::File file(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  if (!file.IsValid())
    return false;
  if (!file.created())
    file.SetLength(0);
  base::Pickle pickle;
  pickle.WriteString(origin);
  file.Write(0, static_cast<const char*>(pickle.data()), pickle.size());
  file.Flush();
  return true;
}

bool ReadPrimaryOriginFile(const base::FilePath& path, std::string* origin) {
  std::string buffer;
  if (!base::ReadFileToString(path, &buffer))
    return false;
  base::Pickle pickle(buffer.data(), buffer.size());
  base::PickleIterator iter(pickle);
  return iter.ReadString(origin) && !origin->empty();
}

}  // namespace

SandboxPrioritizedOriginDatabase::SandboxPrioritizedOriginDatabase(
    const base::FilePath& file_system_directory,
    leveldb::Env* env_override)
    : file_system_directory_(file_system_directory),
      env_override_(env_override),
      primary_origin_file_(file_system_directory_.Append(kPrimaryOriginFile)) {}

SandboxPrioritizedOriginDatabase::~SandboxPrioritizedOriginDatabase() = default;

bool SandboxPrioritizedOriginDatabase::InitializePrimaryOrigin(
    const std::string& origin) {
  const bool is_in_memory =
      env_override_ && leveldb_chrome::IsMemEnv(env_override_);
  if (!primary_origin_database_ && !is_in_memory) {
    if (!MaybeLoadPrimaryOrigin() && ResetPrimaryOrigin(origin)) {
      MaybeMigrateDatabase(origin);
      primary_origin_database_.reset(new SandboxIsolatedOriginDatabase(
          origin, file_system_directory_, base::FilePath(kPrimaryDirectory)));
      return true;
    }
  }

  if (primary_origin_database_)
    return primary_origin_database_->HasOriginPath(origin);

  return false;
}

std::string SandboxPrioritizedOriginDatabase::GetPrimaryOrigin() {
  MaybeLoadPrimaryOrigin();
  if (primary_origin_database_)
    return primary_origin_database_->origin();
  return std::string();
}

bool SandboxPrioritizedOriginDatabase::HasOriginPath(
    const std::string& origin) {
  MaybeInitializeDatabases(false);
  if (primary_origin_database_ &&
      primary_origin_database_->HasOriginPath(origin))
    return true;
  if (origin_database_)
    return origin_database_->HasOriginPath(origin);
  return false;
}

bool SandboxPrioritizedOriginDatabase::GetPathForOrigin(
    const std::string& origin,
    base::FilePath* directory) {
  MaybeInitializeDatabases(true);
  if (primary_origin_database_ &&
      primary_origin_database_->GetPathForOrigin(origin, directory))
    return true;
  DCHECK(origin_database_);
  return origin_database_->GetPathForOrigin(origin, directory);
}

bool SandboxPrioritizedOriginDatabase::RemovePathForOrigin(
    const std::string& origin) {
  MaybeInitializeDatabases(false);
  if (primary_origin_database_ &&
      primary_origin_database_->HasOriginPath(origin)) {
    primary_origin_database_.reset();
    base::DeleteFile(file_system_directory_.Append(kPrimaryOriginFile),
                     true /* recursive */);
    return true;
  }
  if (origin_database_)
    return origin_database_->RemovePathForOrigin(origin);
  return true;
}

bool SandboxPrioritizedOriginDatabase::ListAllOrigins(
    std::vector<OriginRecord>* origins) {
  // SandboxOriginDatabase may clear the |origins|, so call this before
  // primary_origin_database_.
  MaybeInitializeDatabases(false);
  if (origin_database_ && !origin_database_->ListAllOrigins(origins))
    return false;
  if (primary_origin_database_)
    return primary_origin_database_->ListAllOrigins(origins);
  return true;
}

void SandboxPrioritizedOriginDatabase::DropDatabase() {
  primary_origin_database_.reset();
  origin_database_.reset();
}

void SandboxPrioritizedOriginDatabase::RewriteDatabase() {
  if (primary_origin_database_)
    primary_origin_database_->RewriteDatabase();
  if (origin_database_)
    origin_database_->RewriteDatabase();
}

bool SandboxPrioritizedOriginDatabase::MaybeLoadPrimaryOrigin() {
  if (primary_origin_database_)
    return true;
  std::string saved_origin;
  if (!ReadPrimaryOriginFile(primary_origin_file_, &saved_origin))
    return false;
  primary_origin_database_.reset(new SandboxIsolatedOriginDatabase(
      saved_origin, file_system_directory_, base::FilePath(kPrimaryDirectory)));
  return true;
}

bool SandboxPrioritizedOriginDatabase::ResetPrimaryOrigin(
    const std::string& origin) {
  DCHECK(!primary_origin_database_);
  if (!WritePrimaryOriginFile(primary_origin_file_, origin))
    return false;
  // We reset the primary origin directory too.
  // (This means the origin file corruption causes data loss
  // We could keep the directory there as the same origin will likely
  // become the primary origin, but let's play conservatively.)
  base::DeleteFile(file_system_directory_.Append(kPrimaryDirectory),
                   true /* recursive */);
  return true;
}

void SandboxPrioritizedOriginDatabase::MaybeMigrateDatabase(
    const std::string& origin) {
  MaybeInitializeNonPrimaryDatabase(false);
  if (!origin_database_)
    return;
  if (origin_database_->HasOriginPath(origin)) {
    base::FilePath directory_name;
    if (origin_database_->GetPathForOrigin(origin, &directory_name) &&
        directory_name != base::FilePath(kPrimaryOriginFile)) {
      base::FilePath from_path = file_system_directory_.Append(directory_name);
      base::FilePath to_path = file_system_directory_.Append(kPrimaryDirectory);

      if (base::PathExists(to_path))
        base::DeleteFile(to_path, true /* recursive */);
      base::Move(from_path, to_path);
    }

    origin_database_->RemovePathForOrigin(origin);
  }

  std::vector<OriginRecord> origins;
  origin_database_->ListAllOrigins(&origins);
  if (origins.empty()) {
    origin_database_->RemoveDatabase();
    origin_database_.reset();
  }
}

void SandboxPrioritizedOriginDatabase::MaybeInitializeDatabases(bool create) {
  MaybeLoadPrimaryOrigin();
  MaybeInitializeNonPrimaryDatabase(create);
}

void SandboxPrioritizedOriginDatabase::MaybeInitializeNonPrimaryDatabase(
    bool create) {
  if (origin_database_)
    return;

  origin_database_.reset(
      new SandboxOriginDatabase(file_system_directory_, env_override_));
  if (!create && !base::DirectoryExists(origin_database_->GetDatabasePath())) {
    origin_database_.reset();
    return;
  }
}

SandboxOriginDatabase*
SandboxPrioritizedOriginDatabase::GetSandboxOriginDatabase() {
  MaybeInitializeNonPrimaryDatabase(true);
  return origin_database_.get();
}

}  // namespace storage
