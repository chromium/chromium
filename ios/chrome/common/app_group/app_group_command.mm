// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/app_group/app_group_command.h"

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/app_group/app_group_constants.h"

namespace {
// Using GURL in the extension is not wanted as it includes ICU which makes the
// extension binary much larger; therefore, ios/chrome/common/x_callback_url.h
// cannot be used. This class makes a very basic use of x-callback-url, so no
// full implementation is required.
NSString* const kXCallbackURLHost = @"x-callback-url";

void PutCommandInNSUserDefault(NSDictionary* command) {
  NSUserDefaults* shared_defaults = app_group::GetGroupUserDefaults();
  NSString* defaults_key =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandPreference);

  [shared_defaults setObject:command forKey:defaults_key];
  [shared_defaults synchronize];
}
}  // namespace

@interface AppGroupCommand ()
// The identifier of the extension that sent the order.
@property(nonatomic, strong) NSString* sourceApp;

// A block that can be used to open a URL.
@property(nonatomic, strong) URLOpenerBlock opener;
@end

@implementation AppGroupCommand
- (instancetype)initWithSourceApp:(NSString*)sourceApp
                   URLOpenerBlock:(URLOpenerBlock)opener {
  self = [super init];
  if (self) {
    _sourceApp = sourceApp;
    _opener = opener;
  }
  return self;
}

- (NSMutableDictionary*)baseCommandDictionary:(NSString*)command {
  NSString* timePrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTimePreference);
  NSString* appPrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandAppPreference);
  NSString* commandPrefKey = base::SysUTF8ToNSString(
      app_group::kChromeAppGroupCommandCommandPreference);

  return [NSMutableDictionary dictionaryWithDictionary:@{
    timePrefKey : [NSDate date],
    appPrefKey : _sourceApp,
    commandPrefKey : command,
  }];
}

- (void)prepareWithCommandID:(NSString*)commandID {
  PutCommandInNSUserDefault([self baseCommandDictionary:commandID]);
}

- (void)prepareToOpenURL:(NSURL*)URL {
  NSMutableDictionary* command = [self
      baseCommandDictionary:base::SysUTF8ToNSString(
                                app_group::kChromeAppGroupOpenURLCommand)];
  NSString* textPrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTextPreference);
  command[textPrefKey] = URL.absoluteString;
  PutCommandInNSUserDefault(command);
}

- (void)prepareToSearchText:(NSString*)text {
  NSMutableDictionary* command = [self
      baseCommandDictionary:base::SysUTF8ToNSString(
                                app_group::kChromeAppGroupSearchTextCommand)];
  NSString* textPrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTextPreference);
  command[textPrefKey] = text;
  PutCommandInNSUserDefault(command);
}

- (void)prepareToSearchImage:(UIImage*)image {
  NSMutableDictionary* command = [self
      baseCommandDictionary:base::SysUTF8ToNSString(
                                app_group::kChromeAppGroupSearchImageCommand)];
  NSString* dataPrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandDataPreference);
  command[dataPrefKey] = UIImageJPEGRepresentation(image, 1.0);
  PutCommandInNSUserDefault(command);
}

- (void)prepareToOpenItem:(NSURL*)URL index:(NSNumber*)index {
  NSMutableDictionary* command = [self
      baseCommandDictionary:base::SysUTF8ToNSString(
                                app_group::kChromeAppGroupOpenURLCommand)];
  NSString* textPrefKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandTextPreference);
  NSString* indexKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupCommandIndexPreference);
  command[textPrefKey] = URL.absoluteString;
  command[indexKey] = index;
  PutCommandInNSUserDefault(command);
}

- (void)executeInApp {
  NSString* scheme =
      base::apple::ObjCCast<NSString>([base::apple::FrameworkBundle()
          objectForInfoDictionaryKey:@"KSChannelChromeScheme"]);
  if (!scheme)
    return;

  NSURLComponents* urlComponents = [[NSURLComponents alloc] init];
  urlComponents.scheme = scheme;
  urlComponents.host = kXCallbackURLHost;
  urlComponents.path = [@"/"
      stringByAppendingString:base::SysUTF8ToNSString(
                                  app_group::kChromeAppGroupXCallbackCommand)];

  NSURL* openURL = [urlComponents URL];
  _opener(openURL);
}

@end
