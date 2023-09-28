// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_UPLOAD_LIST_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_UPLOAD_LIST_H_

#include "base/memory/ref_counted.h"
#include "components/upload_list/upload_list.h"

namespace ios {

// Factory that creates the platform-specific implementation of the crash
// upload list with the given callback delegate.
scoped_refptr<UploadList> CreateCrashUploadList();

}  // namespace ios

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_UPLOAD_LIST_H_
