// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_SHOWCASE_CORE_SHOWCASE_MODEL_H_
#define IOS_SHOWCASE_CORE_SHOWCASE_MODEL_H_

#import <Foundation/Foundation.h>

#import "ios/showcase/core/showcase_view_controller.h"

// Model object of rows to display in Showcase.
@interface ShowcaseModel : NSObject
+ (NSArray<showcase::ModelRow*>*)model;
@end

#endif  // IOS_SHOWCASE_CORE_SHOWCASE_MODEL_H_
