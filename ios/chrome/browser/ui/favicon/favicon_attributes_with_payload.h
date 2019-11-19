// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FAVICON_FAVICON_ATTRIBUTES_WITH_PAYLOAD_H_
#define IOS_CHROME_BROWSER_UI_FAVICON_FAVICON_ATTRIBUTES_WITH_PAYLOAD_H_

#import "ios/chrome/common/favicon/favicon_attributes.h"

#include "components/favicon_base/favicon_types.h"

// FaviconAttributes with a payload which is not part of UI. This is to be
// created by mediator and used as a FaviconAttributes by UI elements.
@interface FaviconAttributesWithPayload : FaviconAttributes

// Type of the icon used to create with FaviconAttributes. Only valid if the
// favicon has an image.
@property(nonatomic, assign) favicon_base::IconType iconType;

@end

#endif  // IOS_CHROME_BROWSER_UI_FAVICON_FAVICON_ATTRIBUTES_WITH_PAYLOAD_H_
