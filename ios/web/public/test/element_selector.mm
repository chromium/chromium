// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/element_selector.h"

#import "base/strings/sys_string_conversions.h"

@interface ElementSelector ()

- (instancetype)initWithSelectorScript:(NSString*)selectorScript
                   selectorDescription:(NSString*)selectorDescription;

@end

@implementation ElementSelector

+ (ElementSelector*)selectorWithElementID:(const std::string&)elementID {
  NSString* script = [NSString
      stringWithFormat:@"document.getElementById('%s')", elementID.c_str()];
  NSString* description =
      [NSString stringWithFormat:@"with ID %s", elementID.c_str()];
  return [[ElementSelector alloc] initWithSelectorScript:script
                                     selectorDescription:description];
}

+ (ElementSelector*)selectorWithElementID:(const std::string&)elementID
                         inFrameWithIndex:(int)frameIndex {
  NSString* script = [NSString
      stringWithFormat:@"window.frames[%d].document.getElementById('%s')",
                       frameIndex, elementID.c_str()];
  NSString* description =
      [NSString stringWithFormat:@"in iframe with index %d, with ID %s",
                                 frameIndex, elementID.c_str()];
  return [[ElementSelector alloc] initWithSelectorScript:script
                                     selectorDescription:description];
}

+ (ElementSelector*)selectorWithCSSSelector:(const std::string&)selector {
  NSString* script = [NSString
      stringWithFormat:@"document.querySelector(\"%s\")", selector.c_str()];
  NSString* description =
      [NSString stringWithFormat:@"with CSS selector '%s'", selector.c_str()];
  return [[ElementSelector alloc] initWithSelectorScript:script
                                     selectorDescription:description];
}

+ (ElementSelector*)selectorWithXPathQuery:(const std::string&)query {
  NSString* script = [NSString
      stringWithFormat:
          @"document.evaluate(`%s`, document, "
          @"null,XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue",
          query.c_str()];

  NSString* description =
      [NSString stringWithFormat:@"with xpath '%s'", query.c_str()];

  return [[ElementSelector alloc] initWithSelectorScript:script
                                     selectorDescription:description];
}

+ (ElementSelector*)selectorWithScript:(NSString*)selectorScript
                   selectorDescription:(NSString*)selectorDescription {
  return [[ElementSelector alloc] initWithSelectorScript:selectorScript
                                     selectorDescription:selectorDescription];
}

- (instancetype)initWithSelectorScript:(NSString*)selectorScript
                   selectorDescription:(NSString*)selectorDescription {
  if ((self = [super init])) {
    _selectorScript = [selectorScript copy];
    _selectorDescription = [selectorDescription copy];
  }
  return self;
}

@end
