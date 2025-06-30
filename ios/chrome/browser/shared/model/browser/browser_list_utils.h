// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_UTILS_H_
#define IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_UTILS_H_

class Browser;
class BrowserList;

namespace browser_list_utils {

// Returns the most recently foregrounded regular browser from `browser_list`.
// If no regular browser is foregrounded, it returns the most recently
// foregrounded regular browser in the background. If no regular browser
// exists, it returns `nullptr`.
Browser* GetMostActiveSceneBrowser(BrowserList* browser_list);

}  // namespace browser_list_utils

#endif  // IOS_CHROME_BROWSER_SHARED_MODEL_BROWSER_BROWSER_LIST_UTILS_H_
