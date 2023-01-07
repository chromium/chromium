// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_FACTORY_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_FACTORY_DELEGATE_H_

// Delegate for the ReadingList item factory.
@protocol ReadingListListItemFactoryDelegate

// Returns whether the incognito mode is forced.
- (BOOL)isIncognitoForced;

// Returns whether the incognito mode is available.
- (BOOL)isIncognitoAvailable;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_ITEM_FACTORY_DELEGATE_H_
