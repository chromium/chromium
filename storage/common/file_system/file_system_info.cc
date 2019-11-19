// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/common/file_system/file_system_info.h"

namespace storage {

FileSystemInfo::FileSystemInfo()
    : mount_type(storage::kFileSystemTypeTemporary) {}

FileSystemInfo::FileSystemInfo(const std::string& name,
                               const GURL& root_url,
                               storage::FileSystemType mount_type)
    : name(name), root_url(root_url), mount_type(mount_type) {}

FileSystemInfo::~FileSystemInfo() = default;

}  // namespace storage
