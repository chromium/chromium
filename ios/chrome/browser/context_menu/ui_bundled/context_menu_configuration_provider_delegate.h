// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_CONTEXT_MENU_CONFIGURATION_PROVIDER_DELEGATE_H_
#define IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_CONTEXT_MENU_CONFIGURATION_PROVIDER_DELEGATE_H_

@class ContextMenuConfigurationProvider;
class GURL;

/// Delegate for events in ContextMenuConfigurationProvider.
@protocol ContextMenuConfigurationProviderDelegate <NSObject>

/// Called when the context menu did open a new tab in the background.
- (void)contextMenuConfigurationProvider:
            (ContextMenuConfigurationProvider*)configurationProvider
        didOpenNewTabInBackgroundWithURL:(GURL)URL;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXT_MENU_UI_BUNDLED_CONTEXT_MENU_CONFIGURATION_PROVIDER_DELEGATE_H_
