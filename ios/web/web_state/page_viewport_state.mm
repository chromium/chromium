// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/page_viewport_state.h"


#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Parses a double from `viewport_content`.  `viewport_content` is expected to
// have no leading whitespace.
double ParseDouble(NSString* viewport_content) {
  double value = [viewport_content doubleValue];
  // `-doubleValue` returns zero when parsing a non-numerical string, so check
  // that the string actually contains zero to verify that the value was
  // represented in the string.
  if (!value && ![viewport_content hasPrefix:@"0"])
    value = NAN;
  return value;
}
}

namespace web {

ViewportLength::ViewportLength() : use_device_length_(false), value_(NAN) {}

ViewportLength::ViewportLength(NSString* value) : ViewportLength() {
  use_device_length_ = ![value caseInsensitiveCompare:@"device-width"] ||
                       ![value caseInsensitiveCompare:@"device-height"];
  if (!use_device_length_)
    value_ = ParseDouble(value);
}

ViewportLength::~ViewportLength() {}

PageViewportState::PageViewportState()
    : viewport_tag_present_(false),
      user_scalable_(false),
      minimum_zoom_scale_(NAN),
      maximum_zoom_scale_(NAN),
      initial_zoom_scale_(NAN) {}

PageViewportState::PageViewportState(NSString* const viewport_content)
    : PageViewportState() {
  UpdateWithViewportContent(viewport_content);
}

PageViewportState::~PageViewportState() {}

void PageViewportState::UpdateWithViewportContent(
    NSString* const viewport_content) {
  viewport_tag_present_ = viewport_content.length > 0;
  if (!viewport_tag_present_)
    return;
  // Pages are scalable by default, unless prohibited by the viewport tag.
  user_scalable_ = true;

  NSCharacterSet* whitespace_set =
      [NSCharacterSet whitespaceAndNewlineCharacterSet];
  NSArray* content_items = [viewport_content componentsSeparatedByString:@","];
  for (NSString* item in content_items) {
    NSArray* components = [item componentsSeparatedByString:@"="];
    if (components.count == 2) {
      NSString* name = [[components firstObject]
          stringByTrimmingCharactersInSet:whitespace_set];
      NSString* value = [[components lastObject]
          stringByTrimmingCharactersInSet:whitespace_set];
      if (![name caseInsensitiveCompare:@"user-scalable"]) {
        user_scalable_ = [value boolValue];
      } else if (![name caseInsensitiveCompare:@"width"]) {
        width_ = ViewportLength(value);
      } else if (![name caseInsensitiveCompare:@"height"]) {
        height_ = ViewportLength(value);
      } else if (![name caseInsensitiveCompare:@"minimum-scale"]) {
        minimum_zoom_scale_ = ParseDouble(value);
      } else if (![name caseInsensitiveCompare:@"maximum-scale"]) {
        maximum_zoom_scale_ = ParseDouble(value);
      } else if (![name caseInsensitiveCompare:@"initial-scale"]) {
        initial_zoom_scale_ = ParseDouble(value);
      }
    }
  }
}

}  // namespace web
