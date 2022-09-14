// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/crash_reporter_url_observer.h"

#import <Foundation/Foundation.h>

#import <map>

#import "base/check.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/crash/core/common/crash_key.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/crash_report/crash_helper.h"
#import "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using crash_reporter::CrashKeyString;

namespace {

// Max number of urls to send. This represent 1 URL per WebState group.
const int kNumberOfURLsToSend = 3;

// Keep the following two CrashKey arrays in sync with `kNumberOfURLsToSend`.
static crash_reporter::CrashKeyString<1024> url_crash_keys[] = {
    {"url0", CrashKeyString<1024>::Tag::kArray},
    {"url1", CrashKeyString<1024>::Tag::kArray},
    {"url2", CrashKeyString<1024>::Tag::kArray},
};
static CrashKeyString<1024> pending_url_crash_keys[] = {
    {"url0-pending", CrashKeyString<1024>::Tag::kArray},
    {"url1-pending", CrashKeyString<1024>::Tag::kArray},
    {"url2-pending", CrashKeyString<1024>::Tag::kArray},
};

// The group for preload WebStates.
const char kPreloadWebStateGroup[] = "PreloadGroup";

}  // namespace

// A CrashReporterParameterSetter that forward parameters to Breakpad and
// PreviousSessionInfo.
@interface CrashReporterParameterSetter
    : NSObject <CrashReporterParameterSetter>
@end

@implementation CrashReporterParameterSetter
- (void)removeReportParameter:(NSNumber*)key pending:(BOOL)pending {
  int index = key.intValue;
  DCHECK(index < kNumberOfURLsToSend);
  if (pending)
    pending_url_crash_keys[index].Clear();
  else
    url_crash_keys[index].Clear();
}
- (void)setReportParameterURL:(const GURL&)URL
                       forKey:(NSNumber*)key
                      pending:(BOOL)pending {
  int index = key.intValue;
  DCHECK(index < kNumberOfURLsToSend);
  if (pending)
    pending_url_crash_keys[index].Set(URL.spec());
  else
    url_crash_keys[index].Set(URL.spec());
}
@end

#pragma mark - Life Cycle

// static
CrashReporterURLObserver* CrashReporterURLObserver::GetSharedInstance() {
  static CrashReporterURLObserver* instance =
      new CrashReporterURLObserver([[CrashReporterParameterSetter alloc] init]);
  return instance;
}

CrashReporterURLObserver::CrashReporterURLObserver(
    id<CrashReporterParameterSetter> setter) {
  params_setter_ = setter;
  breakpad_key_by_group_ =
      [[NSMutableDictionary alloc] initWithCapacity:kNumberOfURLsToSend];
  breakpad_keys_ =
      [[NSMutableArray alloc] initWithCapacity:kNumberOfURLsToSend];
  for (int i = 0; i < kNumberOfURLsToSend; ++i)
    [breakpad_keys_ addObject:[NSNumber numberWithInt:i]];
}

CrashReporterURLObserver::~CrashReporterURLObserver() {}

#pragma mark - Group operations

std::string CrashReporterURLObserver::GroupForWebStateList(
    WebStateList* web_state_list) {
  return base::StringPrintf("WebStateList:%p", web_state_list);
}

void CrashReporterURLObserver::RemoveGroup(const std::string& group) {
  NSString* ns_group = base::SysUTF8ToNSString(group);
  NSNumber* key = [breakpad_key_by_group_ objectForKey:ns_group];
  if (!key)
    return;
  [params_setter_ removeReportParameter:key pending:NO];
  [params_setter_ removeReportParameter:key pending:YES];
  [breakpad_key_by_group_ removeObjectForKey:ns_group];
  [breakpad_keys_ removeObject:key];
  [breakpad_keys_ insertObject:key atIndex:0];
  current_web_states_.erase(group);
}

void CrashReporterURLObserver::RemoveWebStateList(
    WebStateList* web_state_list) {
  RemoveGroup(GroupForWebStateList(web_state_list));
}

#pragma mark - Record URLs

void CrashReporterURLObserver::RecordURL(const GURL& url,
                                         web::WebState* web_state,
                                         bool pending) {
  DCHECK(!web_state->GetBrowserState()->IsOffTheRecord());
  std::string group = web_state_to_group_[web_state];
  DCHECK(group.size());
  NSString* ns_group = base::SysUTF8ToNSString(group);
  NSNumber* breakpad_key = [breakpad_key_by_group_ objectForKey:ns_group];
  BOOL reusing_key = NO;
  if (!breakpad_key) {
    // Get the first breakpad key and push it back at the end of the keys.
    breakpad_key = [breakpad_keys_ objectAtIndex:0];

    // Remove the current mapping to the breakpad key.
    for (NSString* used_group in
         [breakpad_key_by_group_ allKeysForObject:breakpad_key]) {
      reusing_key = YES;
      current_web_states_.erase(base::SysNSStringToUTF8(used_group));
      [breakpad_key_by_group_ removeObjectForKey:used_group];
    }
    // Associate the breakpad key to the tab id.
    [breakpad_key_by_group_ setObject:breakpad_key forKey:ns_group];
  }
  [breakpad_keys_ removeObject:breakpad_key];
  [breakpad_keys_ addObject:breakpad_key];

  current_web_states_[group] = web_state;
  if (pending) {
    if (reusing_key)
      [params_setter_ removeReportParameter:breakpad_key pending:NO];
    [params_setter_ setReportParameterURL:url forKey:breakpad_key pending:YES];
  } else {
    [params_setter_ setReportParameterURL:url forKey:breakpad_key pending:NO];
    [params_setter_ removeReportParameter:breakpad_key pending:YES];
  }
}

void CrashReporterURLObserver::RecordURLForWebState(web::WebState* web_state) {
  web::NavigationItem* pending_item =
      web_state->GetNavigationManager()->GetPendingItem();
  const GURL& url =
      pending_item ? pending_item->GetURL() : web_state->GetLastCommittedURL();
  RecordURL(url, web_state, pending_item != nullptr);
}

#pragma mark - Start/Stop observing

void CrashReporterURLObserver::ObservePreloadWebState(
    web::WebState* web_state) {
  web_state->AddObserver(this);
  web_state_to_group_[web_state] = kPreloadWebStateGroup;
}

void CrashReporterURLObserver::StopObservingPreloadWebState(
    web::WebState* web_state) {
  web_state->RemoveObserver(this);
  // Check the WebState has not been attached to another group yet.
  if (web_state_to_group_.count(web_state) &&
      web_state_to_group_[web_state] == kPreloadWebStateGroup) {
    web_state_to_group_.erase(web_state);
  }
  if (current_web_states_[kPreloadWebStateGroup] == web_state) {
    RemoveGroup(kPreloadWebStateGroup);
  }
}

void CrashReporterURLObserver::ObserveWebStateList(
    WebStateList* web_state_list) {
  web_state_list->AddObserver(this);
  DCHECK(!all_web_state_observation_forwarders_[web_state_list]);
  // Observe all webStates of this webStateList, so that Tab states are saved in
  // cases of crashing.
  all_web_state_observation_forwarders_[web_state_list] =
      std::make_unique<AllWebStateObservationForwarder>(web_state_list, this);
  for (int i = 0; i < web_state_list->count(); ++i) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    web_state_to_group_[web_state] = GroupForWebStateList(web_state_list);
  }
}

void CrashReporterURLObserver::StopObservingWebStateList(
    WebStateList* web_state_list) {
  std::string group = GroupForWebStateList(web_state_list);
  for (auto it = web_state_to_group_.cbegin();
       it != web_state_to_group_.cend();) {
    if (it->second == group) {
      it = web_state_to_group_.erase(it);
    } else {
      ++it;
    }
  }
  current_web_states_.erase(group);
  RemoveGroup(group);
  all_web_state_observation_forwarders_[web_state_list] = nullptr;
  web_state_list->RemoveObserver(this);
}

#pragma mark - WebStateListObserver

void CrashReporterURLObserver::WebStateDetachedAt(WebStateList* web_state_list,
                                                  web::WebState* web_state,
                                                  int index) {
  web_state_to_group_.erase(web_state);
  if (web_state == current_web_states_[GroupForWebStateList(web_state_list)]) {
    RemoveGroup(GroupForWebStateList(web_state_list));
  }
}

void CrashReporterURLObserver::WebStateInsertedAt(WebStateList* web_state_list,
                                                  web::WebState* web_state,
                                                  int index,
                                                  bool activating) {
  web_state_to_group_[web_state] = GroupForWebStateList(web_state_list);
  if (activating) {
    RecordURLForWebState(web_state);
  }
}

void CrashReporterURLObserver::WebStateReplacedAt(WebStateList* web_state_list,
                                                  web::WebState* old_web_state,
                                                  web::WebState* new_web_state,
                                                  int index) {
  if (old_web_state) {
    web_state_to_group_.erase(old_web_state);
  }
  if (new_web_state) {
    web_state_to_group_[new_web_state] = GroupForWebStateList(web_state_list);
  }
  if (web_state_list->GetActiveWebState() == new_web_state) {
    RecordURLForWebState(new_web_state);
  }
}

void CrashReporterURLObserver::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  if (!new_web_state)
    return;
  // Update WebStateList map in case tabs were moved to another window.
  web_state_to_group_[new_web_state] = GroupForWebStateList(web_state_list);
  RecordURLForWebState(new_web_state);
}

#pragma mark - WebStateObserver

void CrashReporterURLObserver::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->GetUrl().spec().empty() ||
      web_state->GetBrowserState()->IsOffTheRecord()) {
    return;
  }
  RecordURL(navigation_context->GetUrl(), web_state, true);
}

void CrashReporterURLObserver::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (navigation_context->GetUrl().spec().empty() ||
      web_state->GetBrowserState()->IsOffTheRecord()) {
    return;
  }
  RecordURL(navigation_context->GetUrl(), web_state, false);
}
