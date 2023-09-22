// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_MEDIATOR_H_
#define CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "content/browser/date_time_chooser/ios/date_time_chooser_delegate.h"

namespace content {
class DateTimeChooserIOS;
}  // namespace content

// Communicates with content::DateTimeChooserIOS.
@interface DateTimeChooserMediator : NSObject <DateTimeChooserDelegate>

// Initializer.
- (instancetype)initWithDateTimeChooser:
    (content::DateTimeChooserIOS*)dateTimeChooser;
@end

#endif  // CONTENT_BROWSER_DATE_TIME_CHOOSER_IOS_DATE_TIME_CHOOSER_MEDIATOR_H_
