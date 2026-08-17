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

// The base search URL used for fallback/debugging.
// Note: `udm=50` (Unified Drilldown Mode) corresponds to the Modeless Filter
// ID. Value 50 explicitly isolates the search into AI Mode (AIM).
const char kBaseSearchURL[] = "https://www.google.com/search?udm=50";

// The query parameter key for the search query text.
const char kSearchQueryKey[] = "q";

// AI Mode (AIM) Specific Parameters

// Thread ID (mtid): Identifies the unique conversation thread ID in the
// current AI Mode session.
const char kThreadIDKey[] = "mtid";

// State Token (mstk): The primary opaque token holding the state of an AIM
// session.
const char kStateTokenKey[] = "mstk";

// Contextual Input Tokens (cinpts): Represents the opaque state of attachments
// (like images or tab context) parsed by the server.
const char kContextualInputTokensKey[] = "cinpts";

// Conversation Search UI Restore (csuir): A boolean flag (1 for true) telling
// the backend/UI to restore the conversation view using the provided mstk state
// rather than starting a completely fresh query thread.
const char kConversationRestoreKey[] = "csuir";
const char kConversationSearchUIRestoreValue[] = "1";

const char kGoogleSearchClientKey[] = "gsc";
const char kGoogleSearchClientValue[] = "2";

const char kSourceIDKey[] = "sourceid";
const char kSourceIDValue[] = "chrome-mobile";

const char kGoogleSearchAppStateKey[] = "gsas";
const char kGoogleSearchAppStateValue[] = "4";

}  // namespace

@implementation CobrowseContext {
  GURL _url;
}

@synthesize url = _url;

- (BOOL)hasServerSessionTokens {
  std::string dummy;
  return net::GetValueForKeyInQuery(self.url, kStateTokenKey, &dummy) ||
         net::GetValueForKeyInQuery(self.url, kThreadIDKey, &dummy) ||
         net::GetValueForKeyInQuery(self.url, kContextualInputTokensKey,
                                    &dummy);
}

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
    _url = net::AppendOrReplaceQueryParameter(_url, kGoogleSearchClientKey,
                                              kGoogleSearchClientValue);
    _url =
        net::AppendOrReplaceQueryParameter(_url, kSourceIDKey, kSourceIDValue);
    _url = net::AppendOrReplaceQueryParameter(_url, kGoogleSearchAppStateKey,
                                              kGoogleSearchAppStateValue);
    _url = net::AppendOrReplaceQueryParameter(
        _url, kConversationRestoreKey, kConversationSearchUIRestoreValue);

    std::string value;
    if (net::GetValueForKeyInQuery(_url, kSearchQueryKey, &value)) {
      _searchQuery = base::SysUTF8ToNSString(value);
    }
    if (net::GetValueForKeyInQuery(_url, kThreadIDKey, &value)) {
      _serverID = base::SysUTF8ToNSString(value);
    }
  }
  return self;
}

+ (instancetype)defaultContext {
  return [[self alloc] initWithURL:GURL(kBaseSearchURL)];
}

- (BOOL)isEqual:(id)other {
  if (other == self) {
    return YES;
  }
  if (![other isKindOfClass:[CobrowseContext class]]) {
    return NO;
  }
  CobrowseContext* otherContext = (CobrowseContext*)other;
  return self.url.EqualsIgnoringRef(otherContext.url) &&
         [self.searchQuery isEqual:otherContext.searchQuery] &&
         [self.serverID isEqual:otherContext.serverID] &&
         [self.attachedItems isEqual:otherContext.attachedItems];
}

- (NSUInteger)hash {
  return [self.searchQuery hash] ^ [self.serverID hash] ^
         [self.attachedItems hash];
}

@end
