// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_CONTEXT_MENU_PARAMS_UTILS_H_
#define IOS_WEB_WEB_STATE_CONTEXT_MENU_PARAMS_UTILS_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/ui/context_menu_params.h"

namespace web {

// Returns true if the |element| dictionary contains enough information to
// present a context menu. (A valid url for either kContextMenuElementHyperlink
// or kContextMenuElementSource must exist in the dicitionary.)
BOOL CanShowContextMenuForElementDictionary(NSDictionary* element);

// creates a ContextMenuParams from a NSDictionary representing an HTML element.
// The fields "href", "src", "title", "referrerPolicy" and "innerText" will
// be used (if present) to generate the ContextMenuParams.
// All these fields must be NSString*.
// This constructor does not set fields relative to the touch event (view and
// location).
ContextMenuParams ContextMenuParamsFromElementDictionary(NSDictionary* element);

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_CONTEXT_MENU_PARAMS_UTILS_H_
