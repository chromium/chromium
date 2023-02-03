// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_MEDIATOR_H_

#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_mutator.h"

#import <Foundation/Foundation.h>

@protocol BookmarksEditorConsumer;
@protocol BookmarksEditorMediatorDelegate;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

class PrefService;

// Mediator for the bookmark editor
@interface BookmarksEditorMediator : NSObject <BookmarksEditorMutator>

// Reference to the bookmark model.
@property(nonatomic, assign) bookmarks::BookmarkModel* bookmarkModel;
// BookmarkNode to edit.
@property(nonatomic, assign) const bookmarks::BookmarkNode* bookmark;
// Parent of `_bookmark` if the user tap on "save".
@property(nonatomic, assign) const bookmarks::BookmarkNode* folder;
// Delegate to change the view displayed.
@property(nonatomic, weak) id<BookmarksEditorMediatorDelegate> delegate;
// Consumer to reflect user’s change in the model.
@property(nonatomic, weak) id<BookmarksEditorConsumer> consumer;

// Designated initializer.
// `bookmark`: mustn't be NULL at initialization time. It also must be a URL.
// `parent`: mustn't be NULL at initialization time. It also must not be a
// folder. `bookmarkModel` should be loaded.
- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                             bookmark:(const bookmarks::BookmarkNode*)bookmark
                                prefs:(PrefService*)prefs
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

// Changes `self.folder` and updates the UI accordingly.
// The change is not committed until the user taps the Save button.
- (void)changeFolder:(const bookmarks::BookmarkNode*)folder;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_MEDIATOR_H_
