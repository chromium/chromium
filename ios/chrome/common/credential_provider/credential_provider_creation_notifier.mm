// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/credential_provider_creation_notifier.h"

#import "base/time/time.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

// The name of the file used to communicate the new credential notification from
// the Credential Provider Extension to Chrome. The file will contain a
// timestamp of the latest credential's creation time.
static NSString* const kNotificationFileName = @"credential_created_time";

@interface CredentialProviderCreationNotifier () <NSFilePresenter>

@end

@implementation CredentialProviderCreationNotifier {
  // The agent which can trigger the passkey migration.
  ProceduralBlock _block;
}

- (instancetype)initWithBlock:(ProceduralBlock)block {
  self = [super init];

  if (self) {
    _block = block;
  }

  // Make sure this file presenter is properly set up with a presented item.
  if (![self presentedItemURL]) {
    return nil;
  }

  [NSFileCoordinator addFilePresenter:self];

  return self;
}

- (void)dealloc {
  [NSFileCoordinator removeFilePresenter:self];
}

// Notify Chrome that a new credential was created using file observers.
+ (void)notifyCredentialCreated {
  void (^mergingAccessor)(NSURL*) = ^(NSURL* url) {
    NSString* creationTime =
        [NSString stringWithFormat:@"%lld", base::Time::Now()
                                                .ToDeltaSinceWindowsEpoch()
                                                .InMicroseconds()];
    [[NSFileManager defaultManager]
        createFileAtPath:[url path]
                contents:[creationTime dataUsingEncoding:NSUTF8StringEncoding]
              attributes:nil];
  };

  NSFileCoordinator* coordinator =
      [[NSFileCoordinator alloc] initWithFilePresenter:nil];
  NSError* error = nil;
  [coordinator coordinateWritingItemAtURL:[CredentialProviderCreationNotifier
                                              notificationFile]
                                  options:NSFileCoordinatorWritingForMerging
                                    error:&error
                               byAccessor:mergingAccessor];
}

#pragma mark - NSFilePresenter methods

- (void)presentedItemDidChange {
  _block();
}

- (NSOperationQueue*)presentedItemOperationQueue {
  return [NSOperationQueue mainQueue];
}

- (NSURL*)presentedItemURL {
  return [CredentialProviderCreationNotifier notificationFile];
}

#pragma mark - Private

// Returns the URL to the credential creation notification file.
+ (NSURL*)notificationFile {
  NSFileManager* manager = [NSFileManager defaultManager];
  NSURL* containerURL =
      [manager containerURLForSecurityApplicationGroupIdentifier:
                   app_group::ApplicationGroup()];
  return [containerURL URLByAppendingPathComponent:kNotificationFileName];
}

@end
