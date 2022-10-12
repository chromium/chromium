// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_CONTEXT_MENU_CONTEXT_MENU_CONSTANTS_H_
#define IOS_WEB_JS_FEATURES_CONTEXT_MENU_CONTEXT_MENU_CONSTANTS_H_

// Contains keys present in dictionary created by __gCrWeb.findElementAtPoint
// to represent the DOM element.

namespace web {

// Required key. Represents a unique string request ID that is passed through
// directly from a call to findElementAtPoint to the response dictionary. The
// request ID should be used to correlate a response with a previous call to
// findElementAtPoint.
extern const char kContextMenuElementRequestId[];

// Optional key. Represents element's tagName attribute.
extern const char kContextMenuElementTagName[];

// Optional key. Represents element's href attribute if present or parent's href
// if element is an image.
extern const char kContextMenuElementHyperlink[];

// Optional key. Represents element's src attribute if present (<img> element
// only).
extern const char kContextMenuElementSource[];

// Optional key. Represents element's title attribute if present (<img> element
// only).
extern const char kContextMenuElementTitle[];

// Optional key. Represents referrer policy to use for navigations away from the
// current page. Key is present if `kContextMenuElementError` is `NO_ERROR`.
extern const char kContextMenuElementReferrerPolicy[];

// Optional key. Represents element's innerText attribute if present (<a>
// elements with href only or any other text element).
extern const char kContextMenuElementInnerText[];

// Optional key. Represents element's offset into innerText where tap occurred
// (text elements only).
extern const char kContextMenuElementTextOffset[];

// Optional key. Represents element's alt attribute if present (<img> element
// only).
extern const char kContextMenuElementAlt[];

// Optional key. Reprensents the extended text surrounding the selected
// character.
extern const char kContextMenuElementSurroundingText[];

// Optional key. Reprensents the extended offset of the a selected character
// within its surrounding text.
extern const char kContextMenuElementSurroundingTextOffset[];

}  // namespace web

#endif  // IOS_WEB_JS_FEATURES_CONTEXT_MENU_CONTEXT_MENU_CONSTANTS_H_
