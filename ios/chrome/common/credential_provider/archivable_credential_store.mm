// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/archivable_credential_store.h"

#import <ostream>

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ArchivableCredentialStore ()

// The fileURL to the disk file, can be nil.
@property(nonatomic, strong) NSURL* fileURL;

@end

@implementation ArchivableCredentialStore

#pragma mark - Public

- (instancetype)initWithFileURL:(NSURL*)fileURL {
  self = [super init];
  if (self) {
    DCHECK(fileURL.isFileURL) << "URL must be a file URL.";
    _fileURL = fileURL;
  }
  return self;
}

#pragma mark - CredentialStore

- (void)saveDataWithCompletion:(void (^)(NSError* error))completion {
  dispatch_barrier_async(self.workingQueue, ^{
    auto executeCompletionIfPresent = ^(NSError* error) {
      if (completion) {
        dispatch_async(dispatch_get_main_queue(), ^{
          completion(error);
        });
      }
    };

    NSError* error = nil;
    NSData* data =
        [NSKeyedArchiver archivedDataWithRootObject:self.memoryStorage
                              requiringSecureCoding:YES
                                              error:&error];
    DCHECK(!error) << base::SysNSStringToUTF8(error.description);
    if (error) {
      executeCompletionIfPresent(error);
      return;
    }

    [[NSFileManager defaultManager]
               createDirectoryAtURL:self.fileURL.URLByDeletingLastPathComponent
        withIntermediateDirectories:YES
                         attributes:nil
                              error:&error];

    if (error) {
      executeCompletionIfPresent(error);
      return;
    }

    [data writeToURL:self.fileURL options:NSDataWritingAtomic error:&error];
    DCHECK(!error) << base::SysNSStringToUTF8(error.description);
    executeCompletionIfPresent(error);
  });
}

#pragma mark - Subclassing

// Loads the store from disk.
- (NSMutableDictionary<NSString*, ArchivableCredential*>*)loadStorage {
#if !defined(NDEBUG)
  dispatch_assert_queue(self.workingQueue);
#endif  // !defined(NDEBUG)
  if (!self.fileURL) {
    return [[NSMutableDictionary alloc] init];
  }
  NSError* error = nil;
  [self.fileURL checkResourceIsReachableAndReturnError:&error];
  if (error) {
    if (error.code == NSFileReadNoSuchFileError) {
      // File has not been created, return a fresh mutable set.
      return [[NSMutableDictionary alloc] init];
    }
    NOTREACHED();
  }
  NSData* data = [NSData dataWithContentsOfURL:self.fileURL
                                       options:0
                                         error:&error];
  DCHECK(!error) << base::SysNSStringToUTF8(error.description);
  NSSet* classes =
      [NSSet setWithObjects:[ArchivableCredential class],
                            [NSMutableDictionary class], [NSString class], nil];
  NSMutableDictionary<NSString*, ArchivableCredential*>* dictionary =
      [NSKeyedUnarchiver unarchivedObjectOfClasses:classes
                                          fromData:data
                                             error:&error];
  DCHECK(!error) << base::SysNSStringToUTF8(error.description);
  return dictionary;
}

@end
