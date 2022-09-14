// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WIDGET_KIT_WIDGET_METRICS_UTIL_H_
#define IOS_CHROME_BROWSER_WIDGET_KIT_WIDGET_METRICS_UTIL_H_

#import <Foundation/Foundation.h>

@interface WidgetMetricsUtil : NSObject

+ (void)logInstalledWidgets API_AVAILABLE(ios(14));

@end

#endif  // IOS_CHROME_BROWSER_WIDGET_KIT_WIDGET_METRICS_UTIL_H_
