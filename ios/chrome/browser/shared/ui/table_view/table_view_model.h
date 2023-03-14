// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_MODEL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_MODEL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewModel acts as a model class for table view controllers.
@interface TableViewModel<__covariant ObjectType : TableViewItem*>
    : ListModel <ObjectType, TableViewHeaderFooterItem*>

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_TABLE_VIEW_TABLE_VIEW_MODEL_H_
