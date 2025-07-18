// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/memory_credential_store.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"
#import "ios/chrome/common/credential_provider/credential_store_util.h"

@interface MemoryCredentialStore ()

// Working queue used to sync the mutable set and offload expensive get
// operations.
@property(nonatomic) dispatch_queue_t workingQueue;

// The in-memory storage.
@property(nonatomic, strong)
    NSMutableDictionary<NSString*, ArchivableCredential*>* memoryStorage;

@end

@implementation MemoryCredentialStore

#pragma mark - Public

- (instancetype)init {
  self = [super init];
  if (self) {
    _workingQueue = dispatch_queue_create(nullptr, DISPATCH_QUEUE_CONCURRENT);
  }
  return self;
}

#pragma mark - CredentialStore

- (NSArray<id<Credential>>*)credentials {
  __block NSArray<id<Credential>>* credentials;
  __weak __typeof(self) weakSelf = self;
  dispatch_sync(self.workingQueue, ^{
    __typeof(self) strongSelf = weakSelf;
    credentials = [strongSelf allMemoryStorageValues];
  });
  return credentials;
}

- (void)getCredentialsWithCompletion:
    (void (^)(NSArray<id<Credential>>*))completion {
  CHECK(completion);
  __weak __typeof(self) weakSelf = self;
  dispatch_async(self.workingQueue, ^{
    __typeof(self) strongSelf = weakSelf;
    completion([strongSelf allMemoryStorageValues]);
  });
}

- (void)saveDataWithCompletion:(void (^)(NSError* error))completion {
  // No-op.
  if (completion) {
    completion(nil);
  }
}

- (void)removeAllCredentials {
  __weak __typeof(self) weakSelf = self;
  dispatch_barrier_async(self.workingQueue, ^{
    __typeof(self) strongSelf = weakSelf;
    [strongSelf.memoryStorage removeAllObjects];
  });
}

- (void)addCredential:(id<Credential>)credential {
  DCHECK(credential.recordIdentifier)
      << "credential must have a record identifier";
  __weak __typeof(self) weakSelf = self;
  dispatch_barrier_async(self.workingQueue, ^{
    __typeof(self) strongSelf = weakSelf;
    strongSelf.memoryStorage[credential.recordIdentifier] =
        base::apple::ObjCCastStrict<ArchivableCredential>(credential);
  });
}

- (void)updateCredential:(id<Credential>)credential {
  [self removeCredentialWithRecordIdentifier:credential.recordIdentifier];
  [self addCredential:credential];
}

- (void)removeCredentialWithRecordIdentifier:(NSString*)recordIdentifier {
  DCHECK(recordIdentifier.length) << "Invalid `recordIdentifier` was passed.";
  __weak __typeof(self) weakSelf = self;
  dispatch_barrier_async(self.workingQueue, ^{
    __typeof(self) strongSelf = weakSelf;
    strongSelf.memoryStorage[recordIdentifier] = nil;
  });
}

- (id<Credential>)credentialWithRecordIdentifier:(NSString*)recordIdentifier {
  DCHECK(recordIdentifier.length);
  __block id<Credential> credential;
  __weak __typeof(self) weakSelf = self;
  dispatch_sync(self.workingQueue, ^{
    __typeof(self) strongSelf = weakSelf;
    credential = strongSelf.memoryStorage[recordIdentifier];
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

#pragma mark - Subclassing

// Loads the store from disk.
- (NSMutableDictionary<NSString*, ArchivableCredential*>*)loadStorage {
  return [[NSMutableDictionary alloc] init];
}

#pragma mark - Private

// Returns all values from the `memoryStorage` dictionary.
- (NSArray<ArchivableCredential*>*)allMemoryStorageValues {
  return [self.memoryStorage allValues];
}

@end
