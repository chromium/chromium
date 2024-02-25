// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_COORDINATOR_H_
#define CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "third_party/blink/public/mojom/choosers/date_time_chooser.mojom.h"
#import "ui/gfx/native_widget_types.h"

using DateTimeDialogValuePtr = blink::mojom::DateTimeDialogValuePtr;

@class DateTimeChooserViewController;
@class DateTimeChooserMediator;
@class UIViewController;

namespace content {
class DateTimeChooserIOS;
}

// Holds a controller and a mediator so that communicates UI and Mojo
// implementations.
@interface DateTimeChooserCoordinator : NSObject

// Initializer.
- (instancetype)initWithDateTimeChooser:(content::DateTimeChooserIOS*)chooser
                                configs:(DateTimeDialogValuePtr)configs
                     baseViewController:(UIViewController*)baseViewController;

@end

#endif  // CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_COORDINATOR_H_
