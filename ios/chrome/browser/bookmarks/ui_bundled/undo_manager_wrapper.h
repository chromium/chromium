// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_UNDO_MANAGER_WRAPPER_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_UNDO_MANAGER_WRAPPER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

// This object is a convenience ObjC wrapper around UndoManager.
// On construction, it registers itself as an observer of the UndoManager.
// On destruction, it unregisters itself as an observer of the UndoManager.
// Provides a convenient interface to the UndoManager.
// The general usage of this class should be:
//  - startGroupingActions
//  - *make changes to BookmarkModel*
//  - stopGroupingActions
//  - resetUndoManagerChanged
// At a later point in time, an undo should only be attempted if
// hasUndoManagerChanged returns NO.
@interface UndoManagerWrapper : NSObject

// Designated initializer.
- (instancetype)initWithBrowserState:(ProfileIOS*)profile;

// Subsequent changes to the BookmarkModel are grouped together so that a single
// undo will revert all changes.
- (void)startGroupingActions;
// Stops grouping changes to the BookmarkModel. Calls to this method must mirror
// calls to startGroupingActions.
- (void)stopGroupingActions;

// Resets the internal state for whether the UndoManager has changed to NO.
- (void)resetUndoManagerChanged;
// Returns YES if the UndoManager has changed since the last call to
// resetUndoManagerChanged.
- (BOOL)hasUndoManagerChanged;
// Reverts the last change made to the BookmarkModel.
- (void)undo;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_UNDO_MANAGER_WRAPPER_H_
