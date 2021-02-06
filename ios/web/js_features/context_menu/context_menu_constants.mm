// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_features/context_menu/context_menu_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

NSString* const kContextMenuElementRequestId = @"requestId";
NSString* const kContextMenuElementHyperlink = @"href";
NSString* const kContextMenuElementSource = @"src";
NSString* const kContextMenuElementTitle = @"title";
NSString* const kContextMenuElementReferrerPolicy = @"referrerPolicy";
NSString* const kContextMenuElementInnerText = @"innerText";
NSString* const kContextMenuElementAlt = @"alt";

}  // namespace web
