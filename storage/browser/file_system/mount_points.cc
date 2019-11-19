// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/mount_points.h"

namespace storage {

MountPoints::MountPointInfo::MountPointInfo() = default;
MountPoints::MountPointInfo::MountPointInfo(const std::string& name,
                                            const base::FilePath& path)
    : name(name), path(path) {}

}  // namespace storage
