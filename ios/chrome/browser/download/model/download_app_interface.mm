// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_app_interface.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/task/thread_pool.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"

@implementation DownloadAppInterface

+ (void)deleteDownloadsDirectoryFileWithName:(NSString*)fileName {
  NSURL* fileURL = [NSURL fileURLWithPath:fileName];
  base::FilePath downloadsDirectory;
  GetDownloadsDirectory(&downloadsDirectory);
  base::FilePath filePath =
      downloadsDirectory.Append(base::apple::NSURLToFilePath(fileURL));
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), filePath));
}

@end
