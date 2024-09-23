// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/open_extension/open_view_controller.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/app_group/app_group_command.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/extension_open_url.h"

// Type for completion handler to fetch the components of the share items.
// `idResponse` type depends on the element beeing fetched.
using ItemBlock = void (^)(id idResponse, NSError* error);

namespace {

// Logs the new outcome by incrementing the outcome dictionary's values.
void LogOutcome(app_group::OpenExtensionOutcome outcome_type) {
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();

  NSMutableDictionary<NSString*, NSNumber*>*
      open_extension_outcome_dictionnary = [[shared_defaults
          dictionaryForKey:app_group::kOpenExtensionOutcomes] mutableCopy];

  if (!open_extension_outcome_dictionnary) {
    open_extension_outcome_dictionnary = [NSMutableDictionary dictionary];
  }

  NSString* key_for_outcome_type = KeyForOpenExtensionOutcomeType(outcome_type);

  NSInteger old_value_for_open_in_outcome =
      open_extension_outcome_dictionnary[key_for_outcome_type].integerValue;

  [open_extension_outcome_dictionnary
      setValue:@(old_value_for_open_in_outcome + 1)
        forKey:key_for_outcome_type];

  [shared_defaults setObject:open_extension_outcome_dictionnary
                      forKey:app_group::kOpenExtensionOutcomes];
  [shared_defaults synchronize];
}

// Convert outcome_type to an error type.
NSError* ErrorForOutcome(app_group::OpenExtensionOutcome outcome_type) {
  NSInteger error_code = NSURLErrorUnknown;
  switch (outcome_type) {
    case app_group::OpenExtensionOutcome::kFailureInvalidURL:
      error_code = NSURLErrorBadURL;
      break;
    case app_group::OpenExtensionOutcome::kFailureURLNotFound:
      error_code = NSURLErrorBadURL;
      break;
    case app_group::OpenExtensionOutcome::kFailureOpenInNotFound:
      error_code = NSURLErrorUnknown;
      break;
    case app_group::OpenExtensionOutcome::kFailureUnsupportedScheme:
      error_code = NSURLErrorUnsupportedURL;
      break;
    default:
      NOTREACHED();
  }
  return [NSError errorWithDomain:NSURLErrorDomain
                             code:error_code
                         userInfo:nil];
}
}  // namespace

@implementation OpenViewController {
  NSURL* _openInURL;
  NSExtensionItem* _openInItem;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadElementsFromContext];
}

- (void)loadElementsFromContext {
  NSString* typeURL = UTTypeURL.identifier;
  BOOL foundMatch = false;
  for (NSExtensionItem* item in self.extensionContext.inputItems) {
    for (NSItemProvider* itemProvider in item.attachments) {
      if ([itemProvider hasItemConformingToTypeIdentifier:typeURL]) {
        foundMatch = true;
        __weak __typeof(self) weakSelf = self;
        ItemBlock URLCompletion = ^(id idURL, NSError* error) {
          NSURL* URL = base::apple::ObjCCast<NSURL>(idURL);
          if (!URL) {
            // Display the error view when the URL is invalid.
            [self displayErrorViewForOutcome:app_group::OpenExtensionOutcome::
                                                 kFailureInvalidURL];
            return;
          }
          dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf shareItem:item url:URL];
          });
        };
        [itemProvider loadItemForTypeIdentifier:typeURL
                                        options:nil
                              completionHandler:URLCompletion];
      }
    }
  }
  // Display the error view when no URL has been found.
  if (!foundMatch) {
    [self displayErrorViewForOutcome:app_group::OpenExtensionOutcome::
                                         kFailureURLNotFound];
  }
}

- (void)shareItem:(NSExtensionItem*)item url:(NSURL*)URL {
  _openInItem = [item copy];
  _openInURL = [URL copy];
  if ([[_openInURL scheme] isEqualToString:@"http"] ||
      [[_openInURL scheme] isEqualToString:@"https"]) {
    [self openInChrome];
  } else {
    [self displayErrorViewForOutcome:app_group::OpenExtensionOutcome::
                                         kFailureUnsupportedScheme];
  }
}

- (void)performOpenURL:(NSURL*)openURL {
  bool result = ExtensionOpenURL(openURL, self, ^(BOOL success) {
    if (success) {
      LogOutcome(app_group::OpenExtensionOutcome::kSuccess);
    }
  });
  if (result) {
    [self.extensionContext completeRequestReturningItems:@[ _openInItem ]
                                       completionHandler:nil];
    return;
  }
  // Display the error view when Open in is not found
  [self displayErrorViewForOutcome:app_group::OpenExtensionOutcome::
                                       kFailureOpenInNotFound];
}

- (void)openInChrome {
  __weak OpenViewController* weakSelf = self;
  AppGroupCommand* command = [[AppGroupCommand alloc]
      initWithSourceApp:app_group::kOpenCommandSourceOpenExtension
         URLOpenerBlock:^(NSURL* openURL) {
           [weakSelf performOpenURL:openURL];
         }];
  [command prepareToOpenURL:_openInURL];
  [command executeInApp];
}

- (void)displayErrorViewForOutcome:(app_group::OpenExtensionOutcome)outcome {
  __weak OpenViewController* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf displayErrorViewMainThreadForOutcome:outcome];
  });
}

- (void)displayErrorViewMainThreadForOutcome:
    (app_group::OpenExtensionOutcome)outcome {
  LogOutcome(outcome);
  NSString* errorMessage =
      NSLocalizedString(@"IDS_IOS_ERROR_MESSAGE_OPEN_IN_EXTENSION",
                        @"The error message to display to the user.");
  NSString* okButton =
      NSLocalizedString(@"IDS_IOS_OK_BUTTON_OPEN_IN_EXTENSION",
                        @"The label of the OK button in open in extension.");
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:errorMessage
                                          message:[_openInURL absoluteString]
                                   preferredStyle:UIAlertControllerStyleAlert];

  __weak __typeof(self) weakSelf = self;
  UIAlertAction* defaultAction = [UIAlertAction
      actionWithTitle:okButton
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                NSError* outcomeError = ErrorForOutcome(outcome);
                [weakSelf.extensionContext cancelRequestWithError:outcomeError];
              }];
  [alert addAction:defaultAction];
  [self presentViewController:alert animated:YES completion:nil];
}
@end
