// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/crash_reporter_url_observer.h"

#import <Foundation/Foundation.h>

#import <map>

#import "base/check.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "components/crash/core/common/crash_key.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

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

// A CrashReporterParameterSetter that forward parameters to crash keys and
// PreviousSessionInfo.
@interface CrashReporterParameterSetter
    : NSObject <CrashReporterParameterSetter>
@end

@implementation CrashReporterParameterSetter
- (void)removeReportParameter:(NSNumber*)key pending:(BOOL)pending {
  int index = key.intValue;
  DCHECK(index < kNumberOfURLsToSend);
  if (pending) {
    pending_url_crash_keys[index].Clear();
  } else {
    url_crash_keys[index].Clear();
    if (index == 0) {
      // Only sync (and clear) the first non-pending URL to PreviousSessionInfo.
      [[PreviousSessionInfo sharedInstance]
          removeReportParameterForKey:@"url0"];
    }
  }
}
- (void)setReportParameterURL:(const GURL&)URL
                       forKey:(NSNumber*)key
                      pending:(BOOL)pending {
  int index = key.intValue;
  DCHECK(index < kNumberOfURLsToSend);
  if (pending) {
    pending_url_crash_keys[index].Set(URL.spec());
  } else {
    url_crash_keys[index].Set(URL.spec());
    if (index == 0) {
      // Only sync (and clear) the first non-pending URL to PreviousSessionInfo.
      [[PreviousSessionInfo sharedInstance]
          setReportParameterValue:base::SysUTF8ToNSString(URL.spec())
                           forKey:@"url0"];
    }
  }
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
  crash_key_by_group_ =
      [[NSMutableDictionary alloc] initWithCapacity:kNumberOfURLsToSend];
  crash_keys_ = [[NSMutableArray alloc] initWithCapacity:kNumberOfURLsToSend];
  for (int i = 0; i < kNumberOfURLsToSend; ++i) {
    [crash_keys_ addObject:[NSNumber numberWithInt:i]];
  }
}

CrashReporterURLObserver::~CrashReporterURLObserver() {}

#pragma mark - Group operations

std::string CrashReporterURLObserver::GroupForWebStateList(
    WebStateList* web_state_list) {
  return base::StringPrintf("WebStateList:%p", web_state_list);
}

void CrashReporterURLObserver::RemoveGroup(const std::string& group) {
  NSString* ns_group = base::SysUTF8ToNSString(group);
  NSNumber* key = [crash_key_by_group_ objectForKey:ns_group];
  if (!key) {
    return;
  }
  [params_setter_ removeReportParameter:key pending:NO];
  [params_setter_ removeReportParameter:key pending:YES];
  [crash_key_by_group_ removeObjectForKey:ns_group];
  [crash_keys_ removeObject:key];
  [crash_keys_ insertObject:key atIndex:0];
  current_web_states_.erase(group);
}

void CrashReporterURLObserver::RemoveWebStateList(
    WebStateList* web_state_list) {
  RemoveGroup(GroupForWebStateList(web_state_list));
}

#pragma mark - Record URLs

void CrashReporterURLObserver::RecordURL(const GURL& url,
                                         const web::WebState* web_state,
                                         bool pending) {
  DCHECK(!web_state->GetBrowserState()->IsOffTheRecord());
  std::string group = web_state_to_group_[web_state];
  DCHECK(group.size());
  NSString* ns_group = base::SysUTF8ToNSString(group);
  NSNumber* crash_key = [crash_key_by_group_ objectForKey:ns_group];
  BOOL reusing_key = NO;
  if (!crash_key) {
    // Get the first crash key and push it back at the end of the keys.
    crash_key = [crash_keys_ objectAtIndex:0];

    // Remove the current mapping to the crash key.
    for (NSString* used_group in
         [crash_key_by_group_ allKeysForObject:crash_key]) {
      reusing_key = YES;
      current_web_states_.erase(base::SysNSStringToUTF8(used_group));
      [crash_key_by_group_ removeObjectForKey:used_group];
    }
    // Associate the crash key to the tab id.
    [crash_key_by_group_ setObject:crash_key forKey:ns_group];
  }
  [crash_keys_ removeObject:crash_key];
  [crash_keys_ addObject:crash_key];

  current_web_states_[group] = web_state;
  if (pending) {
    if (reusing_key) {
      [params_setter_ removeReportParameter:crash_key pending:NO];
    }
    [params_setter_ setReportParameterURL:url forKey:crash_key pending:YES];
  } else {
    [params_setter_ setReportParameterURL:url forKey:crash_key pending:NO];
    [params_setter_ removeReportParameter:crash_key pending:YES];
  }
}

void CrashReporterURLObserver::RecordURLForWebState(
    const web::WebState* web_state) {
  // web_state is const, so GetNavigationManager won't force its realization
  // (which is intended).
  const web::NavigationManager* manager = web_state->GetNavigationManager();
  const web::NavigationItem* pending_item =
      manager ? manager->GetPendingItem() : nullptr;
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

void CrashReporterURLObserver::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // The activation is handled after this switch statement.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      web::WebState* detached_web_state = detach_change.detached_web_state();
      web_state_to_group_.erase(detached_web_state);
      if (detached_web_state ==
          current_web_states_[GroupForWebStateList(web_state_list)]) {
        RemoveGroup(GroupForWebStateList(web_state_list));
      }
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      web_state_to_group_.erase(replace_change.replaced_web_state());
      web::WebState* inserted_web_state = replace_change.inserted_web_state();
      web_state_to_group_[inserted_web_state] =
          GroupForWebStateList(web_state_list);
      if (web_state_list->GetActiveWebState() == inserted_web_state) {
        RecordURLForWebState(inserted_web_state);
      }
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      web::WebState* inserted_web_state = insert_change.inserted_web_state();
      web_state_to_group_[inserted_web_state] =
          GroupForWebStateList(web_state_list);
      if (status.active_web_state_change()) {
        RecordURLForWebState(inserted_web_state);
      }
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // Do nothing when a group is created.
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Do nothing when a tab group's visual data are updated.
      break;
    case WebStateListChange::Type::kGroupMove:
      // Do nothing when a tab group is moved.
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Do nothing when a group is deleted.
      break;
  }

  if (status.active_web_state_change() && status.new_active_web_state) {
    // Update WebStateList map in case tabs were moved to another window.
    web_state_to_group_[status.new_active_web_state] =
        GroupForWebStateList(web_state_list);
    RecordURLForWebState(status.new_active_web_state);
  }
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
