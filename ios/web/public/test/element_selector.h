// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_ELEMENT_SELECTOR_H_
#define IOS_WEB_PUBLIC_TEST_ELEMENT_SELECTOR_H_

#import <Foundation/Foundation.h>

#include <string>

// An ElementSelector is used to generate the proper javascript to retrieve an
// element on a web page. It encapsulates the various means of finding an
// element and is intended to be passed around.
@interface ElementSelector : NSObject

// The javascript to invoke on a page to retrieve the element.
@property(nonatomic, readonly, copy) NSString* selectorScript;

// A human readable description of the query.
@property(nonatomic, readonly, copy) NSString* selectorDescription;

// Returns an ElementSelector to retrieve an element by ID.
+ (ElementSelector*)selectorWithElementID:(const std::string&)elementID;

// Returns an ElementSelector to retrieve an element in iframe by ID. iframe
// is an immediate child of the main frame with the given index. The script of
// this selector will throw an exception if target iframe has a different
// origin from the main frame.
+ (ElementSelector*)selectorWithElementID:(const std::string&)elementID
                         inFrameWithIndex:(int)frameIndex;

// Returns an ElementSelector to retrieve an element by a CSS selector.
+ (ElementSelector*)selectorWithCSSSelector:(const std::string&)selector;

// Returns an ElementSelector to retrieve an element by a xpath query.
+ (ElementSelector*)selectorWithXPathQuery:(const std::string&)query;

// Returns an ElementSelector to retrieve an element described by
// `selectorDescription` using `selectorScript`.
+ (ElementSelector*)selectorWithScript:(NSString*)selectorScript
                   selectorDescription:(NSString*)selectorDescription;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_WEB_PUBLIC_TEST_ELEMENT_SELECTOR_H_
