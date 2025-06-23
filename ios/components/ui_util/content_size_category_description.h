// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_UI_UTIL_CONTENT_SIZE_CATEGORY_DESCRIPTION_H_
#define IOS_COMPONENTS_UI_UTIL_CONTENT_SIZE_CATEGORY_DESCRIPTION_H_

#import <UIKit/UIKit.h>

namespace ui_util {
enum class IOSContentSizeCategory;
}  // namespace ui_util

// Description related for UIContentSizeCategory.
@interface ContentSizeCategoryDescription : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCategory:(ui_util::IOSContentSizeCategory)category
                      multiplier:(float)multiplier NS_DESIGNATED_INITIALIZER;

@property(nonatomic, assign) ui_util::IOSContentSizeCategory category;
@property(nonatomic, assign) float multiplier;

@end

#endif  // IOS_COMPONENTS_UI_UTIL_CONTENT_SIZE_CATEGORY_DESCRIPTION_H_
