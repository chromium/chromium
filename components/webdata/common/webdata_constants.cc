// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webdata/common/webdata_constants.h"

const base::FilePath::CharType kWebDataFilename[] =
    FILE_PATH_LITERAL("Web Data");

#if defined(OS_ANDROID) || defined(OS_IOS)
const base::FilePath::CharType kAccountWebDataFilename[] =
    FILE_PATH_LITERAL("Account Web Data");
#endif  // defined(OS_ANDROID) || defined(OS_IOS)
