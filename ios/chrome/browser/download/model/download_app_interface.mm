// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_app_interface.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/threading/thread_restrictions.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"

@implementation DownloadAppInterface

+ (void)deleteDownloadsDirectory {
  base::FilePath downloadsDirectory;
  GetDownloadsDirectory(&downloadsDirectory);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(base::IgnoreResult(&base::DeletePathRecursively),
                     downloadsDirectory));
}

+ (void)deleteDownloadsDirectoryFileWithName:(NSString*)fileName {
  NSURL* fileURL = [NSURL fileURLWithPath:fileName];
  base::FilePath downloadsDirectory;
  GetDownloadsDirectory(&downloadsDirectory);
  base::FilePath filePath =
      downloadsDirectory.Append(base::apple::NSURLToFilePath(fileURL));
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(base::IgnoreResult(&base::DeleteFile), filePath));
}

+ (void)createDownloadsDirectoryFileWithName:(NSString*)fileName
                                     content:(NSString*)content {
  base::FilePath downloadsDirectory;
  GetDownloadsDirectory(&downloadsDirectory);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(base::IgnoreResult(base::CreateDirectory),
                     downloadsDirectory)
          .Then(base::BindOnce(
              [](const base::FilePath& filePath, std::string fileContent) {
                base::WriteFile(filePath, fileContent);
              },
              downloadsDirectory.Append(base::SysNSStringToUTF8(fileName)),
              base::SysNSStringToUTF8(content))));
}

+ (int)fileCountInDownloadsDirectory {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath downloadsDirectory;
  GetDownloadsDirectory(&downloadsDirectory);
  int fileCount = 0;
  if (base::DirectoryExists(downloadsDirectory)) {
    base::FileEnumerator enumerator(downloadsDirectory, false,
                                    base::FileEnumerator::FILES);
    for (base::FilePath name = enumerator.Next(); !name.empty();
         name = enumerator.Next()) {
      fileCount++;
    }
  }
  return fileCount;
}

@end
