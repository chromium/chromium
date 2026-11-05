// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_TASK_REQUEST_UTILS_H_
#define IOS_CHROME_APP_TASK_REQUEST_UTILS_H_

#import <Foundation/Foundation.h>

class GURL;
class ProfileIOS;

// Returns the results URL of the default search provider of `profile` for
// `search_query`, or an empty URL if there is no valid default search provider.
GURL GenerateResultGURLFromSearchQuery(NSString* search_query,
                                       ProfileIOS* profile);

#endif  // IOS_CHROME_APP_TASK_REQUEST_UTILS_H_
