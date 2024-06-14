// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_CELLS_BOOKMARK_TABLE_CELL_TITLE_EDITING_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_CELLS_BOOKMARK_TABLE_CELL_TITLE_EDITING_H_

@protocol BookmarkTableCellTitleEditDelegate;

// Cell title editing public interface.
@protocol BookmarkTableCellTitleEditing

// Receives the text field events.
@property(nonatomic, weak) id<BookmarkTableCellTitleEditDelegate> textDelegate;
// Start editing the `folderTitleTextField` of this cell.
- (void)startEdit;
// Stop editing the title's text of this cell. This will call textDidChangeTo:
// on `textDelegate` with the cell's title text value at the moment.
- (void)stopEdit;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_CELLS_BOOKMARK_TABLE_CELL_TITLE_EDITING_H_
