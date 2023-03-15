// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COMPONENT_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COMPONENT_FACTORY_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/ntp/new_tab_page_component_factory_protocol.h"

// A factory which generates various NTP components for the
// NewTabPageCoordinator.
@interface NewTabPageComponentFactory
    : NSObject <NewTabPageComponentFactoryProtocol>
@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_NEW_TAB_PAGE_COMPONENT_FACTORY_H_
