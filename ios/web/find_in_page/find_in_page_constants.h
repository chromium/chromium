// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_CONSTANTS_H_
#define IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_CONSTANTS_H_

namespace web {

// The name of JavaScript function which finds all matches of a string.
extern const char kFindInPageSearch[];
// The name of JavaScript function which continues an unfinished find.
extern const char kFindInPagePump[];
// The name of JavaScript function which selects and scrolls to a match.
extern const char kFindInPageSelectAndScrollToMatch[];
// Dictionary key that holds value of updated match count in
// kFindInPageSelectAndScrollToMatch response.
extern const char kSelectAndScrollResultMatches[];
// Dictionary key that holds value of currently selected index in
// kFindInPageSelectAndScrollToMatch resoonse.
extern const char kSelectAndScrollResultIndex[];
// Dictionary key in kFindInPageSelectAndScrollToMatch response that
// holds the value of the context for the currently selected match.
extern const char kSelectAndScrollResultContextString[];
// The name of JavaScript function which stops Find in Page.
extern const char kFindInPageStop[];

}  // namespace web

#endif  // IOS_WEB_FIND_IN_PAGE_FIND_IN_PAGE_CONSTANTS_H_
