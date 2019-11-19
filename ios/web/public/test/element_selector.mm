// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/element_selector.h"

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

- (instancetype)initWithSelectorScript:(NSString*)selectorScript
                   selectorDescription:(NSString*)selectorDescription {
  if ((self = [super init])) {
    _selectorScript = [selectorScript copy];
    _selectorDescription = [selectorDescription copy];
  }
  return self;
}

@end
