// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/crw_fake_back_forward_list.h"

#import <WebKit/WebKit.h>

#import "base/check.h"
#import "third_party/ocmock/OCMock/OCMock.h"

@interface CRWFakeBackForwardList (PrivateMethods)
- (NSArray*)mockSublistWithURLArray:(NSArray<NSString*>*)URLs;
@end

@implementation CRWFakeBackForwardList

@synthesize backList;
@synthesize forwardList;
@synthesize currentItem;

+ (WKBackForwardListItem*)itemWithURLString:(NSString*)URL {
  id mock = OCMClassMock([WKBackForwardListItem class]);
  OCMStub([mock URL]).andReturn([NSURL URLWithString:URL]);
  return mock;
}

- (WKBackForwardListItem*)itemAtIndex:(NSInteger)index {
  if (index == 0) {
    return self.currentItem;
  } else if (index > 0 && self.forwardList.count) {
    return self.forwardList[index - 1];
  } else if (self.backList.count) {
    return self.backList[self.backList.count + index];
  }
  return nil;
}

- (void)setCurrentURL:(NSString*)currentItemURL {
  [self setCurrentURL:currentItemURL backListURLs:nil forwardListURLs:nil];
}

- (void)setCurrentURL:(NSString*)currentItemURL
         backListURLs:(nullable NSArray<NSString*>*)backListURLs
      forwardListURLs:(nullable NSArray<NSString*>*)forwardListURLs {
  self.currentItem = [CRWFakeBackForwardList itemWithURLString:currentItemURL];
  self.backList = [self mockSublistWithURLArray:backListURLs];
  self.forwardList = [self mockSublistWithURLArray:forwardListURLs];
}

- (void)moveCurrentToIndex:(NSUInteger)index {
  NSMutableArray* logicalList = [[NSMutableArray alloc] init];
  if (self.backList)
    [logicalList addObjectsFromArray:self.backList];
  if (self.currentItem)
    [logicalList addObject:self.currentItem];
  if (self.forwardList)
    [logicalList addObjectsFromArray:self.forwardList];

  NSUInteger count = logicalList.count;
  CHECK(index < count);

  self.currentItem = logicalList[index];
  if (index == 0) {
    self.backList = nil;
  } else {
    NSRange range = NSMakeRange(0, index);
    self.backList = [logicalList subarrayWithRange:range];
  }
  if (index + 1 == count) {
    self.forwardList = nil;
  } else {
    NSRange range = NSMakeRange(index + 1, count - index - 1);
    self.forwardList = [logicalList subarrayWithRange:range];
  }
}

- (NSArray*)mockSublistWithURLArray:(NSArray<NSString*>*)URLs {
  NSMutableArray* array = [NSMutableArray arrayWithCapacity:URLs.count];
  for (NSString* URL : URLs) {
    [array addObject:[CRWFakeBackForwardList itemWithURLString:URL]];
  }
  return [NSArray arrayWithArray:array];
}

- (WKBackForwardListItem*)backItem {
  return self.backList.lastObject;
}

- (WKBackForwardListItem*)forwardItem {
  return self.forwardList.firstObject;
}

@end
