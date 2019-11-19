// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_isolated_origin_database.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "storage/browser/file_system/sandbox_origin_database.h"

namespace storage {

SandboxIsolatedOriginDatabase::SandboxIsolatedOriginDatabase(
    const std::string& origin,
    const base::FilePath& file_system_directory,
    const base::FilePath& origin_directory)
    : origin_(origin),
      file_system_directory_(file_system_directory),
      origin_directory_(origin_directory) {}

SandboxIsolatedOriginDatabase::~SandboxIsolatedOriginDatabase() = default;

bool SandboxIsolatedOriginDatabase::HasOriginPath(const std::string& origin) {
  return (origin_ == origin);
}

bool SandboxIsolatedOriginDatabase::GetPathForOrigin(
    const std::string& origin,
    base::FilePath* directory) {
  if (origin != origin_)
    return false;
  *directory = origin_directory_;
  return true;
}

bool SandboxIsolatedOriginDatabase::RemovePathForOrigin(
    const std::string& origin) {
  return true;
}

bool SandboxIsolatedOriginDatabase::ListAllOrigins(
    std::vector<OriginRecord>* origins) {
  origins->push_back(OriginRecord(origin_, origin_directory_));
  return true;
}

void SandboxIsolatedOriginDatabase::DropDatabase() {}

void SandboxIsolatedOriginDatabase::RewriteDatabase() {}

}  // namespace storage
