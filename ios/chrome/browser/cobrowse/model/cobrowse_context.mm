// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"

#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

namespace {

const char kBaseSearchURL[] = "https://www.google.com/search?udm=50";

}  // namespace

@implementation CobrowseContext {
  GURL _url;
}

@synthesize url = _url;

- (instancetype)initWithURL:(const GURL&)url {
  self = [super init];
  if (self) {
    NSString* overrideURL = experimental_flags::GetCobrowseGwsURL();
    if (overrideURL) {
      DVLOG(1)
          << "\n"
          << "***********************************************************\n"
          << "*                                                         *\n"
          << "*   COBROWSE GWS URL OVERRIDDEN VIA EXPERIMENTAL SETTINGS *\n"
          << "*   URL: " << base::SysNSStringToUTF8(overrideURL) << "\n"
          << "*                                                         *\n"
          << "***********************************************************\n";
      _url = GURL(base::SysNSStringToUTF8(overrideURL));
    } else {
      _url = url;
    }
    _url = net::AppendOrReplaceQueryParameter(_url, "gsc", "2");
    _url = net::AppendOrReplaceQueryParameter(_url, "sourceid", "chrome-mobile");
    _url = net::AppendOrReplaceQueryParameter(_url, "gsas", "4");

    std::string value;
    if (net::GetValueForKeyInQuery(_url, "mstk", &value)) {
      _conversationTurnID = base::SysUTF8ToNSString(value);
    }
    if (net::GetValueForKeyInQuery(_url, "q", &value)) {
      _searchQuery = base::SysUTF8ToNSString(value);
    }
    if (net::GetValueForKeyInQuery(_url, "mtid", &value)) {
      _serverID = base::SysUTF8ToNSString(value);
    }
  }
  return self;
}

+ (instancetype)defaultContext {
  return [[self alloc] initWithURL:GURL(kBaseSearchURL)];
}

@end
