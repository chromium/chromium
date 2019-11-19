// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/sandbox_origin_database_interface.h"

namespace storage {

SandboxOriginDatabaseInterface::OriginRecord::OriginRecord() = default;

SandboxOriginDatabaseInterface::OriginRecord::OriginRecord(
    const std::string& origin_in,
    const base::FilePath& path_in)
    : origin(origin_in), path(path_in) {}

SandboxOriginDatabaseInterface::OriginRecord::~OriginRecord() = default;

}  // namespace storage
