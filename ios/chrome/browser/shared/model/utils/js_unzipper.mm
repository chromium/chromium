// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/utils/js_unzipper.h"

#import <WebKit/WebKit.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"

const NSErrorDomain kJSUnzipperErrorDomain = @"js_unzipper";

@implementation JSUnzipper {
  WKWebView* _webView;
}

- (void)unzipData:(NSData*)data
    completionCallback:(void (^)(NSArray<NSData*>*, NSError*))callback {
  CHECK(callback);
  NSString* base64Data = [data base64EncodedStringWithOptions:0];

  WKWebViewConfiguration* configuration = [[WKWebViewConfiguration alloc] init];
  _webView = [[WKWebView alloc] initWithFrame:CGRectZero
                                configuration:configuration];

  NSString* path = [NSBundle.mainBundle pathForResource:@"jszip" ofType:@"js"];
  NSError* libraryError = nil;
  NSString* library = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&libraryError];

  if (libraryError) {
    callback(nil, libraryError);
  }

  NSString* script = [NSString
      stringWithFormat:@"var p = JSZip.loadAsync(\"%@\","
                       @"{base64: true}).then(function(zip) {"
                       @"  results = [];"
                       @"  zip.forEach(function (relativePath, zipEntry) {"
                       @"    results.push(zipEntry.async(\"base64\"));"
                       @"  });"
                       @"  return Promise.all(results);"
                       @"});await p;return p;",
                       base64Data];

  [_webView
      callAsyncJavaScript:[library stringByAppendingString:script]
                arguments:nil
                  inFrame:nil
           inContentWorld:WKContentWorld.pageWorld
        completionHandler:^(id result, NSError* error) {
          if (error) {
            callback(nil, error);
            return;
          }
          if (![result isKindOfClass:NSArray.class]) {
            callback(nil, [NSError errorWithDomain:kJSUnzipperErrorDomain
                                              code:-1
                                          userInfo:nil]);
            return;
          }
          NSArray* resultArray = base::apple::ObjCCast<NSArray>(result);
          NSMutableArray* decodedData = [NSMutableArray array];
          for (NSString* base64Result : resultArray) {
            if (![result isKindOfClass:NSArray.class]) {
              callback(nil, [NSError errorWithDomain:kJSUnzipperErrorDomain
                                                code:-1
                                            userInfo:nil]);
              return;
            }
            [decodedData addObject:[[NSData alloc]
                                       initWithBase64EncodedString:base64Result
                                                           options:0]];
          }

          callback(decodedData, error);
        }];
}

@end
