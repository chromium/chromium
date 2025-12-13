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

// The NSFileCoordinator instance used for inter-process locking.
@property(nonatomic, strong, readonly) NSFileCoordinator* fileCoordinator;

@end

@implementation ArchivableCredentialStore

#pragma mark - Public

- (instancetype)initWithFileURL:(NSURL*)fileURL {
  self = [super init];
  if (self) {
    CHECK(fileURL.isFileURL) << "URL must be a file URL.";
    _fileURL = fileURL;
    _fileCoordinator = [[NSFileCoordinator alloc] initWithFilePresenter:nil];
  }
  return self;
}

#pragma mark - CredentialStore

- (void)saveDataWithCompletion:(void (^)(NSError* error))completion {
  __weak __typeof(self) weakSelf = self;
  dispatch_barrier_async(self.workingQueue, ^{
    if (weakSelf) {
      [weakSelf saveDataWithCompletionBlockBody:completion];
    } else if (completion) {
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

  __block NSMutableDictionary<NSString*, ArchivableCredential*>*
      resultDictionary = nil;
  NSError* error = nil;

  __weak __typeof(self) weakSelf = self;
  [self.fileCoordinator
      coordinateReadingItemAtURL:self.fileURL
                         options:NSFileCoordinatorReadingWithoutChanges
                           error:&error
                      byAccessor:^(NSURL* newURL) {
                        resultDictionary = [weakSelf loadStorageAtURL:newURL];
                      }];

  // File system errors do happen and are out of our control, so errors are
  // handled gracefully without crashing here. This is for debugging purposes
  // only.
  DCHECK(!error) << base::SysNSStringToUTF8(error.description);

  return resultDictionary;
}

#pragma mark - Private

// Loads the storage at the given file URL. This is a block protected by a
// `coordinateReadingItemAtURL` call.
- (NSMutableDictionary<NSString*, ArchivableCredential*>*)loadStorageAtURL:
    (NSURL*)fileURL {
  NSError* error = nil;
  NSData* data = [NSData dataWithContentsOfURL:fileURL options:0 error:&error];
  if (!data) {
    if (error && error.code == NSFileReadNoSuchFileError) {
      // File has not been created, return a fresh mutable set.
      return [[NSMutableDictionary alloc] init];
    }

    DUMP_WILL_BE_NOTREACHED() << error.localizedDescription;
    // There was an error accessing the file, so return nil to attempt to open
    // the file again next time MemoryCredentialStore::memoryStorage is called.
    return nil;
  }
  NSSet* classes =
      [NSSet setWithObjects:[ArchivableCredential class],
                            [NSMutableDictionary class], [NSString class], nil];
  NSMutableDictionary<NSString*, ArchivableCredential*>* dictionary =
      [NSKeyedUnarchiver unarchivedObjectOfClasses:classes
                                          fromData:data
                                             error:&error];
  // On error, return an empty dictionary. This is for debugging purposes only.
  DCHECK(!error) << base::SysNSStringToUTF8(error.description);
  // On error, the file may contain bad data, so return an empty dictionary to
  // avoid trying to unarchive the same bad data again.
  return dictionary ?: [[NSMutableDictionary alloc] init];
}

// Body of the `saveDataWithCompletion`'s block. Body was extracted so that the
// `self`/`weak self` management is easier. `saveDataWithCompletion` takes the
// responsibility of calling `saveDataWithCompletionBlockBody` on a weak
// version of `self`. There is therefore no need to use a weak reference
// everywhere in the method here.
- (void)saveDataWithCompletionBlockBody:(void (^)(NSError* error))completion {
  // There are odd crashes during shutdown which appear to only be possible if
  // the object no longer exists. Even though this should never happen, this
  // check adds extra safety to avoid accessing released memory.
  // See http://crbug.com/40914898 for details.
  if (!self) {
    return;
  }

  auto executeCompletionIfPresent = ^(NSError* error) {
    if (completion) {
      dispatch_async(dispatch_get_main_queue(), ^{
        completion(error);
      });
    }
  };

  __block NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:self.memoryStorage
                                       requiringSecureCoding:YES
                                                       error:&error];
  // On error, return early. This is for debugging purposes only.
  DCHECK(!error) << base::SysNSStringToUTF8(error.description);
  if (error) {
    executeCompletionIfPresent(error);
    return;
  }

  // Make sure the file URL's directory exists.
  // Note that even though we're creating a directory, not deleting it, the
  // NSFileCoordinatorWritingForDeleting option can be used to guard directory
  // structure changes.
  [self.fileCoordinator
      coordinateWritingItemAtURL:self.fileURL.URLByDeletingLastPathComponent
                         options:NSFileCoordinatorWritingForDeleting
                           error:&error
                      byAccessor:^(NSURL* newDirectoryURL) {
                        [ArchivableCredentialStore
                            createDirectoryIfNecessary:newDirectoryURL
                                                 error:&error];
                      }];

  // On error, return early. This is for debugging purposes only.
  DCHECK(!error) << base::SysNSStringToUTF8(error.description);
  if (error) {
    executeCompletionIfPresent(error);
    return;
  }

  // Write the archived data to the provided file URL. This may create a new
  // file or replace an existing file.
  [self.fileCoordinator
      coordinateWritingItemAtURL:self.fileURL
                         options:NSFileCoordinatorWritingForReplacing
                           error:&error
                      byAccessor:^(NSURL* newURL) {
                        [data writeToURL:newURL
                                 options:NSDataWritingAtomic
                                   error:&error];
                      }];

  // On error, still call completion block. This is for debugging purposes only.
  DCHECK(!error) << base::SysNSStringToUTF8(error.description);

  executeCompletionIfPresent(error);
}

// Creates a directory if it doesn't exist yet. Propagates unexpected errors.
+ (void)createDirectoryIfNecessary:(NSURL*)directoryURL
                             error:(NSError**)outError {
  NSError* error = nil;
  if (![[NSFileManager defaultManager] createDirectoryAtURL:directoryURL
                                withIntermediateDirectories:YES
                                                 attributes:nil
                                                      error:&error]) {
    *outError = error;
  }
}

@end
