// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_FEATURES_H_

#include "base/feature_list.h"

// The feature to enable or disable the Reading List Messages.
extern const base::Feature kReadingListMessages;

// The feature to enable or disable Reading List Time To Read.
extern const base::Feature kReadingListTimeToRead;

// Whether the Reading List Messages feature is turned on, including the
// JavaScript exeuction and Messages presentation.
bool IsReadingListMessagesEnabled();

// Whether the Reading List Time to Read feature is turned on.
bool IsReadingListTimeToReadEnabled();

// Whether only the JavaScript should be executed (e.g. do not show the Message
// even if the heuristics are met).
bool ShouldNotPresentReadingListMessage();

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_FEATURES_H_
