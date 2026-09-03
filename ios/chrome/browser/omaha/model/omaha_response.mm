// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omaha/model/omaha_response.h"

#import <Foundation/Foundation.h>

#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"

// XML parser for the server response.
@interface OmahaResponseParser : NSObject <NSXMLParserDelegate>

// Initialization method. `appId` is the application id one expects to find in
// the response message.
- (instancetype)initWithAppId:(std::string_view)appId;

// Returns YES if the message has been correctly parsed.
- (BOOL)isCorrect;

// If an upgrade is available, returns the details of the notification to send,
// and returns if Chrome is up to date.
- (std::optional<UpgradeRecommendedDetails>)upgradeRecommendedDetails;

// If the response was successfully parsed, returns the date according to the
// server.
- (int)serverDate;

@end

@implementation OmahaResponseParser {
  BOOL _hasError;
  BOOL _responseIsParsed;
  BOOL _appIsParsed;
  BOOL _updateCheckIsParsed;
  BOOL _urlIsParsed;
  BOOL _manifestIsParsed;
  BOOL _eventIsParsed;
  BOOL _dayStartIsParsed;
  NSString* _appId;
  int _serverDate;
  std::optional<UpgradeRecommendedDetails> _updateInformation;
}

- (instancetype)initWithAppId:(std::string_view)appId {
  if ((self = [super init])) {
    _appId = base::SysUTF8ToNSString(appId);
  }
  return self;
}

- (BOOL)isCorrect {
  // A response should have either an updatecheck ACK or an event ACK,
  // depending on the contents of the request.
  return !_hasError && (_updateCheckIsParsed || _eventIsParsed);
}

- (std::optional<UpgradeRecommendedDetails>)upgradeRecommendedDetails {
  return std::exchange(_updateInformation, std::nullopt);
}

- (int)serverDate {
  return _serverDate;
}

// This method is parsing a message with the following type:
// <response...>
//   <daystart elapsed_days="???" .../>
//   <app...>
//     <updatecheck status="ok">
//       <urls>
//         <url codebase="???"/>
//       </urls>
//       <manifest version="???">
//         <packages>
//           <package hash="0" name="Chrome" required="true" size="0"/>
//         </packages>
//         <actions>
//           <action event="update" run="Chrome"/>
//           <action event="postinstall"/>
//         </actions>
//       </manifest>
//     </updatecheck>
//     <ping.../>
//   </app>
// </response>
// --- OR ---
// <response...>
//   <daystart.../>
//   <app...>
//     <event.../>
//   </app>
// </response>
// See http://code.google.com/p/omaha/wiki/ServerProtocol for details.
- (void)parser:(NSXMLParser*)parser
    didStartElement:(NSString*)elementName
       namespaceURI:(NSString*)namespaceURI
      qualifiedName:(NSString*)qualifiedName
         attributes:(NSDictionary*)attributeDict {
  if (_hasError) {
    return;
  }

  // Array of uninteresting tags in the Omaha xml response.
  NSArray* ignoredTagNames =
      @[ @"action", @"actions", @"package", @"packages", @"ping", @"urls" ];
  if ([ignoredTagNames containsObject:elementName]) {
    return;
  }

  if (!_responseIsParsed) {
    if ([elementName isEqualToString:@"response"] &&
        [[attributeDict valueForKey:@"protocol"] isEqualToString:@"3.0"] &&
        [[attributeDict valueForKey:@"server"] isEqualToString:@"prod"]) {
      _responseIsParsed = YES;
    } else {
      _hasError = YES;
    }
  } else if (!_dayStartIsParsed) {
    if ([elementName isEqualToString:@"daystart"]) {
      _dayStartIsParsed = YES;
      _serverDate = [[attributeDict valueForKey:@"elapsed_days"] integerValue];
    } else {
      _hasError = YES;
    }
  } else if (!_appIsParsed) {
    if ([elementName isEqualToString:@"app"] &&
        [[attributeDict valueForKey:@"status"] isEqualToString:@"ok"] &&
        [[attributeDict valueForKey:@"appid"] isEqualToString:_appId]) {
      _appIsParsed = YES;
    } else {
      _hasError = YES;
    }
  } else if (!_eventIsParsed && !_updateCheckIsParsed) {
    if ([elementName isEqualToString:@"updatecheck"]) {
      _updateCheckIsParsed = YES;
      NSString* status = [attributeDict valueForKey:@"status"];
      _updateInformation = UpgradeRecommendedDetails{};
      if ([status isEqualToString:@"noupdate"]) {
        // No update is available on the Market, so we won't get a <url> or
        // <manifest> tag.
        _urlIsParsed = YES;
        _manifestIsParsed = YES;
        _updateInformation->is_up_to_date = true;
      } else if ([status isEqualToString:@"ok"]) {
        _updateInformation->is_up_to_date = false;
      } else {
        _updateInformation = std::nullopt;
        _hasError = YES;
      }
    } else if ([elementName isEqualToString:@"event"]) {
      if ([[attributeDict valueForKey:@"status"] isEqualToString:@"ok"]) {
        _eventIsParsed = YES;
      } else {
        _hasError = YES;
      }
    } else {
      _hasError = YES;
    }
  } else if (!_urlIsParsed) {
    if ([elementName isEqualToString:@"url"] &&
        [[attributeDict valueForKey:@"codebase"] length] > 0) {
      _urlIsParsed = YES;
      DCHECK(_updateInformation.has_value());
      NSString* url = [attributeDict valueForKey:@"codebase"];
      if ([[url substringFromIndex:([url length] - 1)] isEqualToString:@"/"]) {
        url = [url substringToIndex:([url length] - 1)];
      }
      _updateInformation->upgrade_url = GURL(base::SysNSStringToUTF8(url));
      if (!_updateInformation->upgrade_url.is_valid()) {
        _hasError = YES;
      }
    } else {
      _hasError = YES;
    }
  } else if (!_manifestIsParsed) {
    if ([elementName isEqualToString:@"manifest"] &&
        [attributeDict valueForKey:@"version"]) {
      _manifestIsParsed = YES;
      DCHECK(_updateInformation.has_value());
      _updateInformation->next_version =
          base::SysNSStringToUTF8([attributeDict valueForKey:@"version"]);
    } else {
      _hasError = YES;
    }
  } else {
    _hasError = YES;
  }
}

@end

base::expected<OmahaResponse, OmahaParsingError> ParseOmahaResponse(
    std::string_view application_id,
    std::string_view response_body) {
  if (response_body.empty()) {
    return base::unexpected(OmahaParsingError::kServerError);
  }

  OmahaResponseParser* delegate =
      [[OmahaResponseParser alloc] initWithAppId:application_id];

  NSData* xml = [NSData dataWithBytes:response_body.data()
                               length:response_body.length()];
  NSXMLParser* parser = [[NSXMLParser alloc] initWithData:xml];
  parser.delegate = delegate;

  if (![parser parse]) {
    return base::unexpected(OmahaParsingError::kInvalidXML);
  }

  if (![delegate isCorrect]) {
    return base::unexpected(OmahaParsingError::kInvalidResponse);
  }

  return OmahaResponse{
      .server_date = [delegate serverDate],
      .details = [delegate upgradeRecommendedDetails],
  };
}
