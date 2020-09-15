// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/archivable_credential_store.h"

#include "base/check.h"
#include "base/mac/foundation_util.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ArchivableCredentialStore ()

// Working queue used to sync the mutable set operations.
@property(nonatomic) dispatch_queue_t workingQueue;

// The fileURL to the disk file, can be nil.
@property(nonatomic, strong) NSURL* fileURL;

// The in-memory storage.
@property(nonatomic, strong)
    NSMutableDictionary<NSString*, ArchivableCredential*>* memoryStorage;

@end

@implementation ArchivableCredentialStore

#pragma mark - Public

- (instancetype)initWithFileURL:(NSURL*)fileURL {
  self = [super init];
  if (self) {
    if (fileURL) {
      DCHECK(fileURL.isFileURL) << "URL must be a file URL.";
    }
    _fileURL = fileURL;
    _workingQueue = dispatch_queue_create(nullptr, DISPATCH_QUEUE_CONCURRENT);
  }
  return self;
}

#pragma mark - CredentialStore

- (NSArray<id<Credential>>*)credentials {
  __block NSArray<id<Credential>>* credentials;
  dispatch_sync(self.workingQueue, ^{
    credentials = [self.memoryStorage allValues];
  });
  return credentials;
}

- (void)saveDataWithCompletion:(void (^)(NSError* error))completion {
  dispatch_barrier_async(self.workingQueue, ^{
    auto executeCompletionIfPresent = ^(NSError* error) {
      if (completion) {
        dispatch_async(dispatch_get_main_queue(), ^{
          completion(error);
        });
      }
    };

    if (!self.fileURL) {
      // There is no fileURL, store is being used as memory only.
      executeCompletionIfPresent(nil);
      return;
    }

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

- (void)removeAllCredentials {
  dispatch_barrier_async(self.workingQueue, ^{
    [self.memoryStorage removeAllObjects];
  });
}

- (void)addCredential:(id<Credential>)credential {
  DCHECK(credential.recordIdentifier)
      << "credential must have a record identifier";
  dispatch_barrier_async(self.workingQueue, ^{
    DCHECK(!self.memoryStorage[credential.recordIdentifier])
        << "Credential already exists in the storage";
    self.memoryStorage[credential.recordIdentifier] =
        base::mac::ObjCCastStrict<ArchivableCredential>(credential);
  });
}

- (void)updateCredential:(id<Credential>)credential {
  [self removeCredentialWithRecordIdentifier:credential.recordIdentifier];
  [self addCredential:credential];
}

- (void)removeCredentialWithRecordIdentifier:(NSString*)recordIdentifier {
  DCHECK(recordIdentifier.length) << "Invalid |recordIdentifier| was passed.";
  dispatch_barrier_async(self.workingQueue, ^{
    DCHECK(self.memoryStorage[recordIdentifier])
        << "Credential doesn't exist in the storage";
    self.memoryStorage[recordIdentifier] = nil;
  });
}

- (id<Credential>)credentialWithRecordIdentifier:(NSString*)recordIdentifier {
  DCHECK(recordIdentifier.length);
  __block id<Credential> credential;
  dispatch_sync(self.workingQueue, ^{
    credential = self.memoryStorage[recordIdentifier];
  });
  return credential;
}

#pragma mark - Getters

- (NSMutableDictionary<NSString*, ArchivableCredential*>*)memoryStorage {
#if !defined(NDEBUG)
  dispatch_assert_queue(self.workingQueue);
#endif  // !defined(NDEBUG)
  if (!_memoryStorage) {
    _memoryStorage = [self loadStorage];
  }
  return _memoryStorage;
}

#pragma mark - Private

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
  NSSet* classes = [NSSet setWithObjects:[ArchivableCredential class],
                                         [NSMutableDictionary class], nil];
  NSMutableDictionary<NSString*, ArchivableCredential*>* dictionary =
      [NSKeyedUnarchiver unarchivedObjectOfClasses:classes
                                          fromData:data
                                             error:&error];
  DCHECK(!error) << base::SysNSStringToUTF8(error.description);
  return dictionary;
}

@end
