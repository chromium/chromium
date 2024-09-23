// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_error_page_helper.h"

#import <ostream>

#import "base/apple/bundle_locations.h"
#import "base/check.h"
#import "base/strings/escape.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

namespace {

const char kOriginalUrlKey[] = "url";

// Escapes HTML characters in `text`.
NSString* EscapeHTMLCharacters(NSString* text) {
  return base::SysUTF8ToNSString(
      base::EscapeForHTML(base::SysNSStringToUTF8(text)));
}

// Resturns the path for the error page to be loaded.
NSString* LoadedErrorPageFilePath() {
  NSString* path =
      [base::apple::FrameworkBundle() pathForResource:@"error_page_loaded"
                                               ofType:@"html"];
  DCHECK(path) << "Loaded error page should exist";
  return path;
}

// Returns the path for the error page to be injected.
NSString* InjectedErrorPageFilePath() {
  NSString* path =
      [base::apple::FrameworkBundle() pathForResource:@"error_page_injected"
                                               ofType:@"html"];
  DCHECK(path) << "Injected error page should exist";
  return path;
}

}  // namespace

@interface CRWErrorPageHelper ()
@property(nonatomic, strong) NSError* error;
// The error page HTML to be injected into existing page.
@property(nonatomic, strong) NSString* automaticReloadJavaScript;
@property(nonatomic, strong, readonly) NSString* failedNavigationURLString;
@end

@implementation CRWErrorPageHelper

@synthesize failedNavigationURL = _failedNavigationURL;
@synthesize errorPageFileURL = _errorPageFileURL;

- (instancetype)initWithError:(NSError*)error {
  if ((self = [super init])) {
    _error = [error copy];
  }
  return self;
}

#pragma mark - Properties

- (NSURL*)failedNavigationURL {
  if (!_failedNavigationURL) {
    _failedNavigationURL = [NSURL URLWithString:self.failedNavigationURLString];
  }
  return _failedNavigationURL;
}

- (NSString*)failedNavigationURLString {
  return self.error.userInfo[NSURLErrorFailingURLStringErrorKey];
}

- (NSURL*)errorPageFileURL {
  if (!_errorPageFileURL) {
    NSURLQueryItem* itemURL = [NSURLQueryItem
        queryItemWithName:base::SysUTF8ToNSString(kOriginalUrlKey)
                    value:EscapeHTMLCharacters(self.failedNavigationURLString)];
    NSURLQueryItem* itemDontLoad = [NSURLQueryItem queryItemWithName:@"dontLoad"
                                                               value:@"true"];
    NSURLComponents* URL = [[NSURLComponents alloc] initWithString:@"file:///"];
    URL.path = LoadedErrorPageFilePath();
    URL.queryItems = @[ itemURL, itemDontLoad ];
    DCHECK(URL.URL) << "file URL should be valid";
    _errorPageFileURL = URL.URL;
  }
  return _errorPageFileURL;
}

- (NSString*)automaticReloadJavaScript {
  if (!_automaticReloadJavaScript) {
    NSString* path = InjectedErrorPageFilePath();
    NSString* HTMLTemplate =
        [NSString stringWithContentsOfFile:path
                                  encoding:NSUTF8StringEncoding
                                     error:nil];
    NSString* failedNavigationURLString =
        EscapeHTMLCharacters(self.failedNavigationURLString);
    _automaticReloadJavaScript =
        [NSString stringWithFormat:HTMLTemplate, failedNavigationURLString];
  }
  return _automaticReloadJavaScript;
}

#pragma mark - Public

+ (GURL)failedNavigationURLFromErrorPageFileURL:(const GURL&)URL {
  if (!URL.is_valid())
    return GURL();

  if (URL.SchemeIsFile() &&
      URL.path() == base::SysNSStringToUTF8(LoadedErrorPageFilePath())) {
    std::string value;
    if (net::GetValueForKeyInQuery(URL, kOriginalUrlKey, &value)) {
      // The URL was escaped when it was added to the error URL, unescape it
      // here.
      return GURL(base::UnescapeForHTML(base::UTF8ToUTF16(value)));
    }
  }

  return GURL();
}

+ (BOOL)isErrorPageFileURL:(const GURL&)URL {
  return [self failedNavigationURLFromErrorPageFileURL:URL].is_valid();
}

- (NSString*)scriptForInjectingHTML:(NSString*)HTML
                 addAutomaticReload:(BOOL)addAutomaticReload {
  NSString* HTMLToInject = HTML;
  if (addAutomaticReload) {
    HTMLToInject =
        [HTMLToInject stringByAppendingString:self.automaticReloadJavaScript];
  }

  // Serialize as JSON to be able to inject HTML characters.
  NSString* JSON = [[NSString alloc]
      initWithData:[NSJSONSerialization dataWithJSONObject:@[ HTMLToInject ]
                                                   options:0
                                                     error:nil]
          encoding:NSUTF8StringEncoding];
  NSString* escapedHTML =
      [JSON substringWithRange:NSMakeRange(1, JSON.length - 2)];

  return
      [NSString stringWithFormat:
                    @"document.open(); document.write(%@); document.close();",
                    escapedHTML];
}

- (BOOL)isErrorPageFileURLForFailedNavigationURL:(NSURL*)URL {
  // Check that `URL` is a file URL of error page.
  if (!URL.fileURL || ![URL.path isEqualToString:self.errorPageFileURL.path]) {
    return NO;
  }
  // Check that `URL` has the same failed URL as `self`.
  NSURLComponents* URLComponents = [NSURLComponents componentsWithURL:URL
                                              resolvingAgainstBaseURL:NO];
  NSURL* failedNavigationURL = nil;
  for (NSURLQueryItem* item in URLComponents.queryItems) {
    if ([item.name isEqualToString:base::SysUTF8ToNSString(kOriginalUrlKey)]) {
      failedNavigationURL = [NSURL URLWithString:item.value];
      break;
    }
  }
  return [failedNavigationURL isEqual:self.failedNavigationURL];
}

@end
