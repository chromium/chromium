// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/user_defaults_credential_store.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/credential_provider/archivable_credential.h"

@interface UserDefaultsCredentialStore ()

@property(nonatomic, strong) NSUserDefaults* userDefaults;
@property(nonatomic, copy) NSString* key;
@end

@implementation UserDefaultsCredentialStore

- (instancetype)initWithUserDefaults:(NSUserDefaults*)userDefaults
                                 key:(NSString*)key {
  self = [super init];
  if (self) {
    _userDefaults = userDefaults;
    _key = key;
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

    [self.userDefaults setObject:data forKey:self.key];
    executeCompletionIfPresent(nil);
  });
}

#pragma mark - Subclassing

- (NSMutableDictionary<NSString*, ArchivableCredential*>*)loadStorage {
#if !defined(NDEBUG)
  dispatch_assert_queue(self.workingQueue);
#endif  // !defined(NDEBUG)
  NSData* data = [self.userDefaults dataForKey:self.key];
  if (!data) {
    return [[NSMutableDictionary alloc] init];
  }
  NSError* error = nil;
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
