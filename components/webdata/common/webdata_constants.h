// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEBDATA_CONSTANTS_H_
#define COMPONENTS_WEBDATA_COMMON_WEBDATA_CONSTANTS_H_

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "components/webdata/common/webdata_export.h"

WEBDATA_EXPORT extern const base::FilePath::CharType kWebDataFilename[];

// Note: On desktop, the account-scoped web data store is only stored in memory,
// so doesn't have a file path. So this constant only exists on mobile.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
WEBDATA_EXPORT extern const base::FilePath::CharType kAccountWebDataFilename[];
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

#endif  // COMPONENTS_WEBDATA_COMMON_WEBDATA_CONSTANTS_H_
