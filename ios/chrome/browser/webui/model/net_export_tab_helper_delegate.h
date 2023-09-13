// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_MODEL_NET_EXPORT_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_WEBUI_MODEL_NET_EXPORT_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

class NetExportTabHelper;
@class ShowMailComposerContext;

// A delegate for the NetExportTabHelper that displays UI that sends emails.
@protocol NetExportTabHelperDelegate <NSObject>

// Shows the Mail Composer UI. `context` provides information to populate the
// email.
- (void)netExportTabHelper:(NetExportTabHelper*)tabHelper
    showMailComposerWithContext:(ShowMailComposerContext*)context;

@end

#endif  // IOS_CHROME_BROWSER_WEBUI_MODEL_NET_EXPORT_TAB_HELPER_DELEGATE_H_
