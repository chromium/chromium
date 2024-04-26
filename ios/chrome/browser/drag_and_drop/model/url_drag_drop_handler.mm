// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drag_and_drop/model/url_drag_drop_handler.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

@implementation URLDragDropHandler

#pragma mark - UIDragInteractionDelegate

- (NSArray<UIDragItem*>*)dragInteraction:(UIDragInteraction*)interaction
                itemsForBeginningSession:(id<UIDragSession>)session {
  DCHECK(self.dragDataSource);
  URLInfo* info = [self.dragDataSource URLInfoForView:interaction.view];
  // Returning nil indicates that the drag item has no content to be dropped.
  // However, drag to reorder within the table view is still enabled/controlled
  // by the UITableViewDataSource reordering methods.
  return info ? @[ CreateURLDragItem(info, self.origin) ] : nil;
}

- (NSArray<UIDragItem*>*)dragInteraction:(UIDragInteraction*)interaction
                 itemsForAddingToSession:(id<UIDragSession>)session
                        withTouchAtPoint:(CGPoint)point {
  return nil;
}

- (UITargetedDragPreview*)dragInteraction:(UIDragInteraction*)interaction
                    previewForLiftingItem:(UIDragItem*)item
                                  session:(id<UIDragSession>)session {
  return [self previewForDragInteraction:interaction];
}

- (UITargetedDragPreview*)dragInteraction:(UIDragInteraction*)interaction
                 previewForCancellingItem:(UIDragItem*)item
                              withDefault:
                                  (UITargetedDragPreview*)defaultPreview {
  return [self previewForDragInteraction:interaction];
}

- (void)dragInteraction:(UIDragInteraction*)interaction
                             item:(UIDragItem*)item
    willAnimateCancelWithAnimator:(id<UIDragAnimating>)animator {
  [animator addAnimations:^{
    // It looks better to fade the interaction view as the translucent preview
    // flocks back to its original position above the view during cancellation.
    interaction.view.alpha = 0.1;
  }];
  [animator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    interaction.view.alpha = 1.0;
  }];
}

- (void)dragInteraction:(UIDragInteraction*)interaction
       sessionWillBegin:(id<UIDragSession>)session {
  DCHECK_EQ(1U, session.items.count);
  UIDragItem* item = session.items.firstObject;
  URLInfo* info = base::apple::ObjCCastStrict<URLInfo>(item.localObject);
  session.items.firstObject.previewProvider = ^{
    return [UIDragPreview previewForURL:net::NSURLWithGURL(info.URL)
                                  title:info.title];
  };
}

#pragma mark - Private drag helper

- (UITargetedDragPreview*)previewForDragInteraction:
    (UIDragInteraction*)interaction {
  UIDragPreviewParameters* parameters = [[UIDragPreviewParameters alloc] init];
  parameters.visiblePath =
      [self.dragDataSource visiblePathForView:interaction.view];
  return [[UITargetedDragPreview alloc] initWithView:interaction.view
                                          parameters:parameters];
}

#pragma mark - UIDropInteractionDelegate

- (BOOL)dropInteraction:(UIDropInteraction*)interaction
       canHandleSession:(id<UIDropSession>)session {
  DCHECK(self.dropDelegate);
  // TODO(crbug.com/40138131): Enable multi-item drops.
  return session.items.count == 1U &&
         [self.dropDelegate canHandleURLDropInView:interaction.view] &&
         [session
             hasItemsConformingToTypeIdentifiers:@[ UTTypeURL.identifier ]];
}

- (UIDropProposal*)dropInteraction:(UIDropInteraction*)interaction
                  sessionDidUpdate:(id<UIDropSession>)session {
  return [[UIDropProposal alloc] initWithDropOperation:UIDropOperationCopy];
}

- (void)dropInteraction:(UIDropInteraction*)interaction
            performDrop:(id<UIDropSession>)session {
  DCHECK(self.dropDelegate);
  if ([session canLoadObjectsOfClass:[NSURL class]]) {
    __weak URLDragDropHandler* weakSelf = self;
    [session
        loadObjectsOfClass:[NSURL class]
                completion:^(NSArray<NSURL*>* objects) {
                  // TODO(crbug.com/40138131): Enable multi-item drops.
                  DCHECK_EQ(1U, objects.count);
                  GURL URL = net::GURLWithNSURL(objects.firstObject);
                  if (URL.is_valid()) {
                    [weakSelf.dropDelegate
                              view:interaction.view
                        didDropURL:URL
                           atPoint:[session locationInView:interaction.view]];
                  }
                }];
  }
}

@end
