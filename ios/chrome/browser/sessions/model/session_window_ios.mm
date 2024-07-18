// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_window_ios.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/format_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/sessions/model/NSCoder+Compatibility.h"
#import "ios/web/public/session/crw_session_storage.h"

namespace {
// Serialization keys.
NSString* const kSessionsKey = @"sessions";
NSString* const kTabGroupsKey = @"tabGroups";
NSString* const kSessionsSummaryKey = @"sessionsSummary";
NSString* const kSelectedIndexKey = @"selectedIndex";
NSString* const kSessionStableIdentifierKey = @"stableIdentifier";
NSString* const kSessionCurrentURLKey = @"sessionCurrentURL";
NSString* const kSessionCurrentTitleKey = @"sessionCurrentTitle";

// Returns whether `index` is valid for a SessionWindowIOS with `session_count`
// entries.
BOOL IsIndexValidForSessionCount(NSUInteger index, NSUInteger session_count) {
  return (session_count == 0) ? (index == static_cast<NSUInteger>(NSNotFound))
                              : (index < session_count);
}
}  // namespace

@implementation SessionWindowIOS

@synthesize sessions = _sessions;
@synthesize selectedIndex = _selectedIndex;
@synthesize tabGroups = _tabGroups;

#pragma mark - Public

- (instancetype)initWithSessions:(NSArray<CRWSessionStorage*>*)sessions
                       tabGroups:(NSArray<SessionTabGroup*>*)tabGroups
                   selectedIndex:(NSUInteger)selectedIndex {
  DCHECK(sessions);
  DCHECK(IsIndexValidForSessionCount(selectedIndex, [sessions count]));
  self = [super init];
  if (self) {
    _sessions = [sessions copy];
    _tabGroups = [tabGroups copy];
    _selectedIndex = selectedIndex;
  }
  return self;
}

#pragma mark - NSObject

- (BOOL)isEqual:(NSObject*)object {
  SessionWindowIOS* other = base::apple::ObjCCast<SessionWindowIOS>(object);
  return [other cr_isEqualSameClass:self];
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NSUInteger selectedIndex = [aDecoder cr_decodeIndexForKey:kSelectedIndexKey];
  NSArray<CRWSessionStorage*>* sessions =
      base::apple::ObjCCast<NSArray<CRWSessionStorage*>>(
          [aDecoder decodeObjectForKey:kSessionsKey]);

  if (!sessions) {
    sessions = @[];
  }

  NSArray<SessionTabGroup*>* tabGroups =
      base::apple::ObjCCast<NSArray<SessionTabGroup*>>(
          [aDecoder decodeObjectForKey:kTabGroupsKey]);

  if (!tabGroups) {
    tabGroups = @[];
  }

  if (!IsIndexValidForSessionCount(selectedIndex, [sessions count])) {
    if (![sessions count]) {
      selectedIndex = NSNotFound;
    } else {
      selectedIndex = 0;
    }
  }

  return [self initWithSessions:sessions
                      tabGroups:tabGroups
                  selectedIndex:selectedIndex];
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  [aCoder cr_encodeIndex:_selectedIndex forKey:kSelectedIndexKey];
  [aCoder encodeObject:_sessions forKey:kSessionsKey];
  [aCoder encodeObject:_tabGroups forKey:kTabGroupsKey];
}

#pragma mark - Debugging

- (NSString*)description {
  return [NSString stringWithFormat:@"selected index: %" PRIuNS
                                     "\nsessions:\n%@\n",
                                    _selectedIndex, _sessions];
}

#pragma mark Private

- (BOOL)cr_isEqualSameClass:(SessionWindowIOS*)other {
  if (_selectedIndex != other.selectedIndex) {
    return NO;
  }

  if (_sessions != other.sessions && ![_sessions isEqual:other.sessions]) {
    return NO;
  }

  return YES;
}

@end
