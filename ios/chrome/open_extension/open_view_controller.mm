// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/open_extension/open_view_controller.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/app_group/app_group_command.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

// Type for completion handler to fetch the components of the share items.
// `idResponse` type depends on the element beeing fetched.
using ItemBlock = void (^)(id idResponse, NSError* error);

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
            [self displayErrorView];
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
  // Display the error view when no match have been found.
  if (!foundMatch) {
    [self displayErrorView];
  }
}

- (void)shareItem:(NSExtensionItem*)item url:(NSURL*)URL {
  _openInItem = [item copy];
  _openInURL = [URL copy];
  if ([[_openInURL scheme] isEqualToString:@"http"] ||
      [[_openInURL scheme] isEqualToString:@"https"]) {
    [self openInChrome];
  } else {
    [self displayErrorView];
  }
}

- (void)openInChrome {
  UIResponder* responder = self;

  while ((responder = responder.nextResponder)) {
    if ([responder respondsToSelector:@selector(openURL:)]) {
      [self prepareCommand:responder];
      [self.extensionContext completeRequestReturningItems:@[ _openInItem ]
                                         completionHandler:nil];
      return;
    }
  }
  [self.extensionContext
      cancelRequestWithError:[NSError errorWithDomain:NSURLErrorDomain
                                                 code:NSURLErrorUnknown
                                             userInfo:nil]];
}

- (void)prepareCommand:(UIResponder*)responder {
  AppGroupCommand* command = [[AppGroupCommand alloc]
      initWithSourceApp:app_group::kOpenCommandSourceOpenExtension
         URLOpenerBlock:^(NSURL* openURL) {
           [responder performSelector:@selector(openURL:) withObject:openURL];
         }];
  [command prepareToOpenURL:_openInURL];
  [command executeInApp];
}

- (void)displayErrorView {
  __weak OpenViewController* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf displayErrorViewMainThread];
  });
}

- (void)displayErrorViewMainThread {
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
  UIAlertAction* defaultAction =
      [UIAlertAction actionWithTitle:okButton
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action) {
                               NSError* unsupportedURLError = [NSError
                                   errorWithDomain:NSURLErrorDomain
                                              code:NSURLErrorUnsupportedURL
                                          userInfo:nil];
                               [weakSelf.extensionContext
                                   cancelRequestWithError:unsupportedURLError];
                             }];
  [alert addAction:defaultAction];
  [self presentViewController:alert animated:YES completion:nil];
}
@end
