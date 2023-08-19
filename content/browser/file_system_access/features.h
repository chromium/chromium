// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FEATURES_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace content::features {

// All features in alphabetical order, grouped by buildflag. The features should
// be documented alongside the definition of their values in the .cc file.

// Alphabetical:
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFileSystemAccessDragAndDropCheckBlocklist);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFileSystemAccessDoNotOverwriteOnMove);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFileSystemAccessMoveLocalFiles);
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFileSystemAccessRemove);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kFileSystemAccessRenameWithoutParentAccessRequiresUserActivation);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kFileSystemAccessSkipAfterWriteChecksIfUnchangingExtension);
CONTENT_EXPORT BASE_DECLARE_FEATURE(
    kFileSystemAccessDirectoryIterationSymbolicLinkCheck);

#if BUILDFLAG(IS_MAC)
CONTENT_EXPORT BASE_DECLARE_FEATURE(kFileSystemAccessCowSwapFile);
#endif  // BUILDFLAG(IS_MAC)

}  // namespace content::features

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FEATURES_H_
