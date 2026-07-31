// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/credential_provider_migration_notifier.h"

#import "base/time/time.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

// The name of the file used to communicate the credential migration
// notification from the Credential Provider Extension to the browser. The file
// will contain a timestamp of the latest credential change/creation time.
// Note: The file name is kept as "credential_created_time" for compatibility.
static NSString* const kNotificationFileName = @"credential_created_time";

@interface CredentialProviderMigrationNotifier () <NSFilePresenter>

@end

@implementation CredentialProviderMigrationNotifier {
  // The agent which can trigger the passkey migration.
  ProceduralBlock _block;
}

- (instancetype)initWithBlock:(ProceduralBlock)block {
  self = [super init];

  if (self) {
    _block = block;
  }

  // Make sure this file presenter is properly set up with a presented item.
  NSURL* fileURL = [self presentedItemURL];
  if (!fileURL) {
    return nil;
  }

  // Ensure the presented file exists. Otherwise, the file presenter would not
  // be notified about the changes.
  if (![[NSFileManager defaultManager] fileExistsAtPath:fileURL.path]) {
    [[NSFileManager defaultManager] createFileAtPath:fileURL.path
                                            contents:nil
                                          attributes:nil];
  }

  [NSFileCoordinator addFilePresenter:self];

  return self;
}

- (void)dealloc {
  [NSFileCoordinator removeFilePresenter:self];
}

// Notify the browser that credentials changed or were created using file
// observers.
+ (void)notifyMigrationNeeded {
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
  [coordinator coordinateWritingItemAtURL:[CredentialProviderMigrationNotifier
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
  return [CredentialProviderMigrationNotifier notificationFile];
}

#pragma mark - Private

// Returns the URL to the credential migration notification file.
+ (NSURL*)notificationFile {
  NSFileManager* manager = [NSFileManager defaultManager];
  NSURL* containerURL =
      [manager containerURLForSecurityApplicationGroupIdentifier:
                   app_group::ApplicationGroup()];
  return [containerURL URLByAppendingPathComponent:kNotificationFileName];
}

@end
