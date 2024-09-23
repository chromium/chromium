// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drag_and_drop/model/table_view_url_drag_drop_handler.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

@implementation TableViewURLDragDropHandler

#pragma mark - UITableViewDragDelegate

- (NSArray<UIDragItem*>*)tableView:(UITableView*)tableView
      itemsForBeginningDragSession:(id<UIDragSession>)session
                       atIndexPath:(NSIndexPath*)indexPath {
  DCHECK(self.dragDataSource && self.origin);
  URLInfo* info = [self.dragDataSource tableView:tableView
                              URLInfoAtIndexPath:indexPath];
  // Returning nil indicates that the drag item has no content to be dropped.
  // However, drag to reorder within the table view is still enabled/controlled
  // by the UITableViewDataSource reordering methods.
  return info ? @[ CreateURLDragItem(info, self.origin) ] : nil;
}

- (NSArray<UIDragItem*>*)tableView:(UITableView*)tableView
       itemsForAddingToDragSession:(id<UIDragSession>)session
                       atIndexPath:(NSIndexPath*)indexPath
                             point:(CGPoint)point {
  // TODO(crbug.com/40138131): Enable multi-select dragging.
  return nil;
}

- (void)tableView:(UITableView*)tableView
    dragSessionWillBegin:(id<UIDragSession>)session {
  DCHECK_EQ(1U, session.items.count);
  UIDragItem* item = session.items.firstObject;
  URLInfo* info = base::apple::ObjCCastStrict<URLInfo>(item.localObject);
  session.items.firstObject.previewProvider = ^{
    return [UIDragPreview previewForURL:net::NSURLWithGURL(info.URL)
                                  title:info.title];
  };
}

#pragma mark - UITableViewDropDelegate

- (BOOL)tableView:(UITableView*)tableView
    canHandleDropSession:(id<UIDropSession>)session {
  DCHECK(self.dropDelegate);
  // TODO(crbug.com/40138131): Enable multi-item drops.
  return session.items.count == 1U &&
         [self.dropDelegate canHandleURLDropInTableView:tableView] &&
         [session
             hasItemsConformingToTypeIdentifiers:@[ UTTypeURL.identifier ]];
}

- (UITableViewDropProposal*)tableView:(UITableView*)tableView
                 dropSessionDidUpdate:(id<UIDropSession>)session
             withDestinationIndexPath:(NSIndexPath*)destinationIndexPath {
  UIDropOperation operation =
      tableView.hasActiveDrag ? UIDropOperationMove : UIDropOperationCopy;
  return [[UITableViewDropProposal alloc]
      initWithDropOperation:operation
                     intent:UITableViewDropIntentInsertAtDestinationIndexPath];
}

- (void)tableView:(UITableView*)tableView
    performDropWithCoordinator:(id<UITableViewDropCoordinator>)coordinator {
  DCHECK(self.dropDelegate);
  if ([coordinator.session canLoadObjectsOfClass:[NSURL class]]) {
    __weak TableViewURLDragDropHandler* weakSelf = self;
    [coordinator.session
        loadObjectsOfClass:[NSURL class]
                completion:^(NSArray<NSURL*>* objects) {
                  // TODO(crbug.com/40138131): Enable multi-item drops.
                  DCHECK_EQ(1U, objects.count);
                  GURL URL = net::GURLWithNSURL(objects.firstObject);
                  if (URL.is_valid()) {
                    [weakSelf.dropDelegate
                          tableView:tableView
                         didDropURL:URL
                        atIndexPath:coordinator.destinationIndexPath];
                  }
                }];
  }
}

@end
