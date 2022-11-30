// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/synthesized_session_restore.h"

#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/synthesized_history_entry_data.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Session history entry data keys.
NSString* const kEntryData = @"SessionHistoryEntryData";
NSString* const kEntryOriginalURL = @"SessionHistoryEntryOriginalURL";
NSString* const kEntryExternalURLPolicy =
    @"SessionHistoryEntryShouldOpenExternalURLsPolicyKey";
NSString* const kEntryTitle = @"SessionHistoryEntryTitle";
NSString* const kEntryURL = @"SessionHistoryEntryURL";

// Session history keys.
NSString* const kSessionHistory = @"SessionHistory";
NSString* const kSessionHistoryCurrentIndex = @"SessionHistoryCurrentIndex";
NSString* const kSessionHistoryEntries = @"SessionHistoryEntries";
NSString* const kSessionHistoryVersion = @"SessionHistoryVersion";
NSString* const kIsAppInitiated = @"IsAppInitiated";

}  // namespace

namespace web {

SynthesizedSessionRestore::SynthesizedSessionRestore() {}
SynthesizedSessionRestore::~SynthesizedSessionRestore() {}

void SynthesizedSessionRestore::Init(
    int last_committed_item_index,
    const std::vector<std::unique_ptr<NavigationItem>>& items,
    bool off_the_record) {
  if (!IsEnabled()) {
    return;
  }

  DCHECK(last_committed_item_index >= 0 &&
         last_committed_item_index < static_cast<int>(items.size()));
  int external_url_policy = off_the_record ? 0 : 1;
  NSMutableArray* entries =
      [[NSMutableArray alloc] initWithCapacity:items.size()];
  for (size_t i = 0; i < items.size(); i++) {
    NavigationItem* item = items[i].get();

    // SessionHistoryEntryData, and NSDictionaries below, come from:
    // https://github.com/WebKit/WebKit/blob/674bd0ec/Source/WebKit/UIProcess/mac/LegacySessionStateCoding.cpp
    SynthesizedHistoryEntryData entry_data;
    entry_data.SetReferrer(item->GetReferrer().url);
    [entries addObject:@{
      kEntryData : entry_data.AsNSData(),
      kEntryOriginalURL : base::SysUTF8ToNSString(item->GetURL().spec()),
      kEntryExternalURLPolicy : @(external_url_policy),
      kEntryTitle : base::SysUTF16ToNSString(item->GetTitle()),
      kEntryURL : base::SysUTF8ToNSString(item->GetURL().spec()),
    }];
  }

  NSDictionary* state_dictionary = @{
    kSessionHistory : @{
      kSessionHistoryCurrentIndex : @(last_committed_item_index),
      kSessionHistoryEntries : entries,
      kSessionHistoryVersion : @1,
    },
    kIsAppInitiated : @NO,
  };

  static constexpr uint8_t version[] = {0, 0, 0, 2};
  NSMutableData* interaction_data = [NSMutableData data];
  [interaction_data appendData:[NSData dataWithBytes:&version
                                              length:sizeof(version)]];
  NSData* property_list_data = [NSPropertyListSerialization
      dataWithPropertyList:state_dictionary
                    format:NSPropertyListBinaryFormat_v1_0
                   options:0
                     error:nil];
  [interaction_data appendData:property_list_data];
  cached_data_ = interaction_data;
}

bool SynthesizedSessionRestore::Restore(WebState* web_state) {
  if (!IsEnabled() || cached_data_.length == 0)
    return false;
  NSData* data = cached_data_;
  cached_data_ = nil;

  bool restore_session_succeeded = web_state->SetSessionStateData(data);
  UMA_HISTOGRAM_BOOLEAN("Session.WebStates.NativeRestoreSession",
                        restore_session_succeeded);
  if (!restore_session_succeeded)
    return false;

  DCHECK(web_state->GetNavigationItemCount());
  web::GetWebClient()->CleanupNativeRestoreURLs(web_state);
  return true;
}

// static
bool SynthesizedSessionRestore::IsEnabled() {
  if (!base::FeatureList::IsEnabled(
          web::features::kSynthesizedRestoreSession)) {
    return false;
  }

  return base::ios::IsRunningOnIOS15OrLater();
}

}  // namespace web
