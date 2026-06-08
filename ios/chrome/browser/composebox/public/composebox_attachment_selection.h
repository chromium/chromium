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

// Representation of currently selected/attached items.
@interface ComposeboxAttachmentSelection : NSObject

// Initial tab identifiers to attach.
@property(nonatomic, readonly) std::set<web::WebStateID> tabIDs;

// Tab identifiers that have their content cached.
@property(nonatomic, readonly) std::set<web::WebStateID> cachedWebStateIDs;

// Initial images to attach.
@property(nonatomic, readonly) NSArray<ComposeboxPickerImageResult*>* images;

// Initial files to attach.
@property(nonatomic, readonly) NSArray<NSURL*>* files;

// Initial Drive items to attach.
@property(nonatomic, readonly)
    NSArray<ComposeboxPickerDriveResult*>* driveItems;

// Whether there are any attachments in this selection.
@property(nonatomic, readonly) BOOL hasAttachments;

- (instancetype)initWithTabIDs:(std::set<web::WebStateID>)tabIDs
             cachedWebStateIDs:(std::set<web::WebStateID>)cachedWebStateIDs
                        images:(NSArray<ComposeboxPickerImageResult*>*)images
                         files:(NSArray<NSURL*>*)files
                    driveItems:
                        (NSArray<ComposeboxPickerDriveResult*>*)driveItems
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_PUBLIC_COMPOSEBOX_ATTACHMENT_SELECTION_H_
