// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_LINK_PREVIEW_LINK_PREVIEW_MEDIATOR_H_
#define IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_LINK_PREVIEW_LINK_PREVIEW_MEDIATOR_H_

#import <Foundation/Foundation.h>
#include "url/gurl.h"

namespace web {
struct Referrer;
class WebState;
}

@protocol LinkPreviewConsumer;

// The preview mediator that observes changes of the model and updates the
// corresponding consumer.
@interface LinkPreviewMediator : NSObject

// The consumer that is updated by this mediator.
@property(nonatomic, weak) id<LinkPreviewConsumer> consumer;

// Init the LinkPreviewMediator with a `webState`, the `previewURL` and the
// `referrer`.
- (instancetype)initWithWebState:(web::WebState*)webState
                      previewURL:(const GURL&)previewURL
                        referrer:(const web::Referrer&)referrer;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_LINK_PREVIEW_LINK_PREVIEW_MEDIATOR_H_
