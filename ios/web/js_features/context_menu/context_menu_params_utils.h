// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_CONTEXT_MENU_CONTEXT_MENU_PARAMS_UTILS_H_
#define IOS_WEB_JS_FEATURES_CONTEXT_MENU_CONTEXT_MENU_PARAMS_UTILS_H_

#import "ios/web/public/ui/context_menu_params.h"

#import "base/values.h"

namespace web {

// Maximum allowed size for a screenshot. (CGSize.width * CGSize.height)
inline constexpr double kContextMenuMaxScreenshotSize = 1e7;

// Creates a ContextMenuParams from a base::Value dictionary representing an
// HTML element. The fields "href", "src", "title", "referrerPolicy" and
// "innerText" will be used (if present) to generate the ContextMenuParams.
// If set, all these fields must have String values.
// This constructor does not set fields relative to the touch event (view and
// location).
ContextMenuParams ContextMenuParamsFromElementDictionary(
    const base::Value::Dict& element);

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_CONTEXT_MENU_CONTEXT_MENU_PARAMS_UTILS_H_
