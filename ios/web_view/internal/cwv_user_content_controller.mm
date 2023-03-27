// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_user_content_controller.h"
#import "ios/web_view/internal/cwv_user_content_controller_internal.h"

#import "base/json/json_writer.h"
#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/web_view_early_page_script_provider.h"
#import "ios/web_view/internal/web_view_message_handler_java_script_feature.h"
#import "ios/web_view/public/cwv_user_script.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Converts base::Value::Dict to NSDictionary.
NSDictionary* NSDictionaryFromDictValue(const base::Value::Dict& value) {
  std::string json;
  const bool success = base::JSONWriter::Write(value, &json);
  DCHECK(success) << "Failed to convert base::Value to JSON";

  NSData* json_data = [NSData dataWithBytes:json.c_str() length:json.length()];
  NSDictionary* ns_dictionary = base::mac::ObjCCastStrict<NSDictionary>(
      [NSJSONSerialization JSONObjectWithData:json_data
                                      options:kNilOptions
                                        error:nil]);
  DCHECK(ns_dictionary) << "Failed to convert JSON to NSDictionary";
  return ns_dictionary;
}

}  // namespace

@interface CWVUserContentController ()
@property(weak, nonatomic) CWVWebViewConfiguration* configuration;
@end

@implementation CWVUserContentController {
  NSMutableArray<CWVUserScript*>* _userScripts;
}

@synthesize configuration = _configuration;

- (nonnull instancetype)initWithConfiguration:
    (nonnull __weak CWVWebViewConfiguration*)configuration {
  self = [super init];
  if (self) {
    _configuration = configuration;
    _userScripts = [[NSMutableArray alloc] init];
  }
  return self;
}

- (void)addUserScript:(nonnull CWVUserScript*)userScript {
  [_userScripts addObject:userScript];
  [self updateEarlyPageScript];
}

- (void)removeAllUserScripts {
  [_userScripts removeAllObjects];
  [self updateEarlyPageScript];
}

- (nonnull NSArray<CWVUserScript*>*)userScripts {
  return _userScripts;
}

// Updates the early page script associated with the BrowserState with the
// content of _userScripts.
- (void)updateEarlyPageScript {
  NSMutableString* joinedScript = [[NSMutableString alloc] init];
  for (CWVUserScript* script in _userScripts) {
    [joinedScript appendString:script.source];
    // Inserts "\n" between scripts to make it safer to join multiple scripts,
    // in case the first script doesn't end with ";" or "\n".
    [joinedScript appendString:@"\n"];
  }
  ios_web_view::WebViewEarlyPageScriptProvider::FromBrowserState(
      _configuration.browserState)
      .SetScript(joinedScript);
}

- (void)addMessageHandler:(void (^)(NSDictionary* payload))handler
               forCommand:(NSString*)nsCommand {
  DCHECK(handler);
  std::string command = base::SysNSStringToUTF8(nsCommand);
  WebViewMessageHandlerJavaScriptFeature::FromBrowserState(
      _configuration.browserState)
      ->RegisterHandler(
          command, base::BindRepeating(^(const base::Value::Dict& payload) {
            handler(NSDictionaryFromDictValue(payload));
          }));
}

- (void)removeMessageHandlerForCommand:(NSString*)nsCommand {
  std::string command = base::SysNSStringToUTF8(nsCommand);
  WebViewMessageHandlerJavaScriptFeature::FromBrowserState(
      _configuration.browserState)
      ->UnregisterHandler(command);
}

@end
