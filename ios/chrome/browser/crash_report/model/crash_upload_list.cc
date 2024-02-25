// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/crash_report/model/crash_upload_list.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "components/crash/core/browser/crash_upload_list_crashpad.h"
#include "components/crash/core/common/reporter_running_ios.h"
#include "components/upload_list/crash_upload_list.h"
#include "components/upload_list/text_log_upload_list.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"

namespace ios {

scoped_refptr<UploadList> CreateCrashUploadList() {
  if (crash_reporter::IsCrashpadRunning()) {
    return new CrashUploadListCrashpad();
  }

  base::FilePath crash_dir_path;
  base::PathService::Get(ios::DIR_CRASH_DUMPS, &crash_dir_path);
  base::FilePath upload_log_path =
      crash_dir_path.AppendASCII(CrashUploadList::kReporterLogFilename);
  return new TextLogUploadList(upload_log_path);
}

}  // namespace ios
