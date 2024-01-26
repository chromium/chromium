// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/window_activities/model/move_tab_activity_type_buildflags.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_id.h"
#import "net/base/apple/url_conversions.h"

// Activity types.
NSString* const kLoadURLActivityType = @"org.chromium.load.url";
NSString* const kLoadIncognitoURLActivityType = @"org.chromium.load.otr-url";

// User info keys.
NSString* const kURLKey = @"LoadParams_URL";
NSString* const kReferrerURLKey = @"LoadParams_ReferrerURL";
NSString* const kReferrerPolicyKey = @"LoadParams_ReferrerPolicy";
NSString* const kOriginKey = @"LoadParams_Origin";
NSString* const kTabIdentifierKey = @"TabIdentifier";
NSString* const kTabIncognitoKey = @"TabIncognito";

namespace {

// Helper for any activity that opens URLs.
NSUserActivity* BaseActivityForURLOpening(BOOL in_incognito) {
  NSString* type =
      in_incognito ? kLoadIncognitoURLActivityType : kLoadURLActivityType;
  NSUserActivity* activity = [[NSUserActivity alloc] initWithActivityType:type];
  return activity;
}

}  // namespace

NSUserActivity* ActivityToLoadURL(WindowActivityOrigin origin,
                                  const GURL& url,
                                  const web::Referrer& referrer,
                                  BOOL in_incognito) {
  NSUserActivity* activity = BaseActivityForURLOpening(in_incognito);
  NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
  params[kOriginKey] = [NSNumber numberWithInteger:origin];
  if (!url.is_empty()) {
    params[kURLKey] = net::NSURLWithGURL(url);
  }
  if (!referrer.url.is_empty()) {
    params[kReferrerURLKey] = net::NSURLWithGURL(referrer.url);
    params[kReferrerPolicyKey] = @(static_cast<int>(referrer.policy));
  }
  [activity addUserInfoEntriesFromDictionary:params];
  return activity;
}

NSUserActivity* ActivityToLoadURL(WindowActivityOrigin origin,
                                  const GURL& url) {
  NSUserActivity* activity = BaseActivityForURLOpening(false);
  NSMutableDictionary* params = [[NSMutableDictionary alloc] init];
  params[kOriginKey] = [NSNumber numberWithInteger:origin];
  if (!url.is_empty()) {
    params[kURLKey] = net::NSURLWithGURL(url);
  }
  [activity addUserInfoEntriesFromDictionary:params];
  return activity;
}

NSUserActivity* ActivityToMoveTab(web::WebStateID tab_id, BOOL incognito) {
  NSString* moveTabActivityType =
      base::SysUTF8ToNSString(BUILDFLAG(IOS_MOVE_TAB_ACTIVITY_TYPE));
  NSUserActivity* activity =
      [[NSUserActivity alloc] initWithActivityType:moveTabActivityType];
  NSNumber* origin = @(WindowActivityOrigin::WindowActivityTabDragOrigin);
  NSDictionary* params = @{
    kOriginKey : origin,
    kTabIdentifierKey : @(tab_id.identifier()),
    kTabIncognitoKey : @(incognito)
  };
  [activity addUserInfoEntriesFromDictionary:params];
  return activity;
}

NSUserActivity* AdaptUserActivityToIncognito(NSUserActivity* activity_to_adapt,
                                             BOOL incognito) {
  if (([activity_to_adapt.activityType
           isEqualToString:kLoadIncognitoURLActivityType] &&
       !incognito) ||
      ([activity_to_adapt.activityType isEqualToString:kLoadURLActivityType] &&
       incognito)) {
    NSUserActivity* activity = BaseActivityForURLOpening(incognito);
    [activity addUserInfoEntriesFromDictionary:activity_to_adapt.userInfo];
    return activity;
  }

  return activity_to_adapt;
}

BOOL ActivityIsURLLoad(NSUserActivity* activity) {
  return [activity.activityType isEqualToString:kLoadURLActivityType] ||
         [activity.activityType isEqualToString:kLoadIncognitoURLActivityType];
}

BOOL ActivityIsTabMove(NSUserActivity* activity) {
  NSString* moveTabActivityType =
      base::SysUTF8ToNSString(BUILDFLAG(IOS_MOVE_TAB_ACTIVITY_TYPE));
  return [activity.activityType isEqualToString:moveTabActivityType];
}

UrlLoadParams LoadParamsFromActivity(NSUserActivity* activity) {
  if (!ActivityIsURLLoad(activity)) {
    return UrlLoadParams();
  }

  BOOL incognito =
      [activity.activityType isEqualToString:kLoadIncognitoURLActivityType];
  NSURL* passed_url = base::apple::ObjCCast<NSURL>(activity.userInfo[kURLKey]);
  NSURL* referer_url =
      base::apple::ObjCCast<NSURL>(activity.userInfo[kReferrerURLKey]);

  GURL url = net::GURLWithNSURL(passed_url);
  UrlLoadParams params = UrlLoadParams::InNewTab(url);
  params.in_incognito = incognito;
  if (referer_url) {
    NSNumber* policy_value =
        base::apple::ObjCCast<NSNumber>(activity.userInfo[kReferrerPolicyKey]);
    web::ReferrerPolicy policy =
        static_cast<web::ReferrerPolicy>(policy_value.intValue);
    params.web_params.referrer =
        web::Referrer(net::GURLWithNSURL(referer_url), policy);
  }

  return params;
}

WindowActivityOrigin OriginOfActivity(NSUserActivity* activity) {
  NSNumber* origin = activity.userInfo[kOriginKey];
  return origin ? static_cast<WindowActivityOrigin>(origin.intValue)
                : WindowActivityUnknownOrigin;
}

web::WebStateID GetTabIDFromActivity(NSUserActivity* activity) {
  if (!ActivityIsTabMove(activity)) {
    return web::WebStateID();
  }
  NSNumber* tabIDNumber = activity.userInfo[kTabIdentifierKey];
  return web::WebStateID::FromSerializedValue(tabIDNumber.integerValue);
}

BOOL GetIncognitoFromTabMoveActivity(NSUserActivity* activity) {
  if (!ActivityIsTabMove(activity)) {
    return NO;
  }
  return [activity.userInfo[kTabIncognitoKey] boolValue];
}
