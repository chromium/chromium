// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/spotlight/fake_searchable_item_factory.h"

#import "base/strings/sys_string_conversions.h"
#import "components/favicon_base/favicon_types.h"

@implementation FakeSearchableItemFactory {
  // Domain identifier of the searchableItems managed by the factory.
  spotlight::Domain _spotlightDomain;
}

- (instancetype)initWithDomain:(spotlight::Domain)domain {
  self = [super init];

  if (self) {
    _spotlightDomain = domain;
  }

  return self;
}

- (void)generateSearchableItem:(const GURL&)URL
                         title:(NSString*)title
            additionalKeywords:(NSArray<NSString*>*)keywords
             completionHandler:(void (^)(CSSearchableItem*))completionHandler {
  NSString* domainID = spotlight::StringFromSpotlightDomain(_spotlightDomain);

  CSSearchableItem* item = [[CSSearchableItem alloc]
      initWithUniqueIdentifier:[self spotlightIDForURL:URL title:title]
              domainIdentifier:domainID
                  attributeSet:[[CSSearchableItemAttributeSet alloc]
                                   initWithContentType:UTTypeURL]];

  completionHandler(item);
}

- (NSString*)spotlightIDForURL:(const GURL&)URL title:(NSString*)title {
  NSString* spotlightID = [NSString
      stringWithFormat:@"%@ %@", base::SysUTF8ToNSString(URL.spec()), title];
  return spotlightID;
}

- (NSString*)spotlightIDForURL:(const GURL&)URL {
  return base::SysUTF8ToNSString(URL.spec());
}

- (void)cancelItemsGeneration {
  self.cancelItemsGenerationCallCount++;
}

@end
