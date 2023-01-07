// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_COLLECTION_VIEW_MODEL_H_
#define IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_COLLECTION_VIEW_MODEL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"

// CollectionViewModel acts as a model class for collection view controllers.
@interface CollectionViewModel<__covariant ObjectType : CollectionViewItem*>
    : ListModel<ObjectType, CollectionViewItem*>

@end

#endif  // IOS_CHROME_BROWSER_UI_COLLECTION_VIEW_COLLECTION_VIEW_MODEL_H_
