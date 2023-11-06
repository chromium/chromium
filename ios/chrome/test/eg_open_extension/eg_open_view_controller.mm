// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/eg_open_extension/eg_open_view_controller.h"

#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "ios/chrome/common/extension_open_url.h"

// Type for completion handler to fetch the components of the share items.
// `idResponse` type depends on the element beeing fetched.
using ItemBlock = void (^)(id idResponse, NSError* error);

@implementation EGOpenViewController

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
            [weakSelf cancel];
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
  if (!foundMatch) {
    [self cancel];
  }
}

- (void)cancel {
  [self.extensionContext
      cancelRequestWithError:[NSError errorWithDomain:NSURLErrorDomain
                                                 code:NSURLErrorUnknown
                                             userInfo:nil]];
}

- (void)shareItem:(NSExtensionItem*)item url:(NSURL*)URL {
  NSString* scheme =
      base::apple::ObjCCast<NSString>([base::apple::FrameworkBundle()
          objectForInfoDictionaryKey:@"KSChannelChromeScheme"]);
  // KSChannelChromeScheme opens the URLs in HTTPS by default, but EG tests only
  // support HTTP. Embed the URL in x-callback-url to force HTTP.
  NSString* encodedURL =
      [[URL absoluteString] stringByAddingPercentEncodingWithAllowedCharacters:
                                [NSCharacterSet URLQueryAllowedCharacterSet]];
  NSURL* urlToOpen = [NSURL
      URLWithString:[NSString
                        stringWithFormat:@"%@://x-callback-url/open?url=%@",
                                         scheme, encodedURL]];
  if (!scheme) {
    [self cancel];
    return;
  }
  bool result = ExtensionOpenURL(urlToOpen, self, nil);
  if (!result) {
    [self cancel];
    return;
  }
  [self.extensionContext completeRequestReturningItems:@[ item ]
                                     completionHandler:nil];
}

@end
