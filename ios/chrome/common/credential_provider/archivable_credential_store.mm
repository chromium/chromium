// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/archivable_credential_store.h"

#import <ostream>

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"

@interface ArchivableCredentialStore ()

// The fileURL to the disk file.
@property(nonatomic, strong) NSURL* fileURL;

@end

@implementation ArchivableCredentialStore

#pragma mark - Public

- (instancetype)initWithFileURL:(NSURL*)fileURL {
  self = [super init];
  if (self) {
    CHECK(fileURL.isFileURL) << "URL must be a file URL.";
    _fileURL = fileURL;
  }
  return self;
}

#pragma mark - CredentialStore

- (void)saveDataWithCompletion:(void (^)(NSError* error))completion {
  __weak __typeof(self) weakSelf = self;
  dispatch_barrier_async(self.workingQueue, ^{
    if (weakSelf) {
      [weakSelf saveDataWithCompletionBlockBody:completion];
    } else {
      NSError* error =
          [[NSError alloc] initWithDomain:@""
                                     code:0
                                 userInfo:@{
                                   NSLocalizedDescriptionKey :
                                       @"ArchivableCredentialStore is nil."
                                 }];
      completion(error);
    }
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
    DUMP_WILL_BE_NOTREACHED();
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

#pragma mark - Private

// Body of the `saveDataWithCompletion`'s block. Body was extracted so that the
// `self`/`weak self` management is easier. `saveDataWithCompletion` takes the
// responsability of calling `saveDataWithCompletionBlockBody` on a weak
// version of `self`. There is therefore no need to use a weak reference
// everywhere in the method here.
- (void)saveDataWithCompletionBlockBody:(void (^)(NSError* error))completion {
  auto executeCompletionIfPresent = ^(NSError* error) {
    if (completion) {
      dispatch_async(dispatch_get_main_queue(), ^{
        completion(error);
      });
    }
  };

  NSError* error = nil;

  if (self) {
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
  }

  executeCompletionIfPresent(error);
}

@end
