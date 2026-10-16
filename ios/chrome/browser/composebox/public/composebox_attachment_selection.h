// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ATTACHMENT_SELECTION_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ATTACHMENT_SELECTION_H_

#import <Foundation/Foundation.h>

#import <set>

#import "ios/web/public/web_state_id.h"

@class ComposeboxPickerImageResult;
@class ComposeboxPickerDriveResult;

// Represents a selection of tabs to attach in Composebox.
struct ComposeboxTabSelection {
  // All tabs selected for attachment.
  std::set<web::WebStateID> selected_ids;
  // Subset of `selected_ids` whose page context was pre-cached by the picker.
  std::set<web::WebStateID> cached_ids;
};

// Representation of currently selected/attached items.
@interface ComposeboxAttachmentSelection : NSObject

// The tab selection state (selected and cached tab identifiers).
@property(nonatomic, readonly) ComposeboxTabSelection tabSelection;

// Initial images to attach.
@property(nonatomic, readonly) NSArray<ComposeboxPickerImageResult*>* images;

// Initial files to attach.
@property(nonatomic, readonly) NSArray<NSURL*>* files;

// Initial Drive items to attach.
@property(nonatomic, readonly)
    NSArray<ComposeboxPickerDriveResult*>* driveItems;

// Whether there are any attachments in this selection.
@property(nonatomic, readonly) BOOL hasAttachments;

- (instancetype)
    initWithTabSelection:(ComposeboxTabSelection)tabSelection
                  images:(NSArray<ComposeboxPickerImageResult*>*)images
                   files:(NSArray<NSURL*>*)files
              driveItems:(NSArray<ComposeboxPickerDriveResult*>*)driveItems
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ATTACHMENT_SELECTION_H_
