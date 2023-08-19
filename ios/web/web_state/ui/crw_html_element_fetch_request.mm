// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_html_element_fetch_request.h"

#import "base/time/time.h"
#import "ios/web/public/ui/context_menu_params.h"

@interface CRWHTMLElementFetchRequest ()
// Completion handler to call with found DOM element.
@property(nonatomic, copy) void (^foundElementHandler)
    (const web::ContextMenuParams&);
@end

@implementation CRWHTMLElementFetchRequest

@synthesize creationTime = _creationTime;
@synthesize foundElementHandler = _foundElementHandler;

- (instancetype)initWithFoundElementHandler:
    (void (^)(const web::ContextMenuParams&))foundElementHandler {
  self = [super init];
  if (self) {
    _creationTime = base::TimeTicks::Now();
    _foundElementHandler = foundElementHandler;
  }
  return self;
}

- (void)runHandlerWithResponse:(const web::ContextMenuParams&)response {
  if (_foundElementHandler) {
    _foundElementHandler(response);
  }
}

- (void)invalidate {
  _foundElementHandler = nullptr;
}

@end
