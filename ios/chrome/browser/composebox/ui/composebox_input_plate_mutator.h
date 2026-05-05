// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_MUTATOR_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_MUTATOR_H_

@class ComposeboxInputItem;
enum class ComposeboxModelOption;
class GURL;

namespace omnibox {
class SearchboxConfig;
}
@class TabInfo;

namespace web {
class WebState;
class WebStateID;
}  // namespace web

/// Mutator for the composebox input plate.
@protocol ComposeboxInputPlateMutator

/// Removes the given `item` from the context.
- (void)removeItem:(ComposeboxInputItem*)item;

/// Sends `text` to start a query.
- (void)sendText:(NSString*)text;

/// Attaches the current tab's content to the context.
- (void)attachCurrentTabContent;

/// Requests a refresh of UI.
- (void)requestUIRefresh;

/// Processes the given `fileURL` for a file.
- (void)processFileURL:(GURL)fileURL
                 isPDF:(BOOL)isPDF
            completion:(void (^)(void))completion;

/// Processes the given `fileURL` for a file.
- (void)processFileURL:(GURL)fileURL isPDF:(BOOL)isPDF;

/// Processes the given `itemProvider` for an image.
- (void)processImageItemProvider:(NSItemProvider*)itemProvider
                         assetID:(NSString*)assetID;

/// Processes the given `itemProvider` for an image.
- (void)processImageItemProvider:(NSItemProvider*)itemProvider
                         assetID:(NSString*)assetID
                      completion:(void (^)(void))completion;

/// Processes a tab with the given `webState` and `webStateID`.
- (void)processTab:(web::WebState*)webState
        webStateID:(web::WebStateID)webStateID;

/// Processes the given `text`.
- (void)processText:(NSString*)text;

/// Sets the model option to use in queries, specifying whether the choice was
/// caused by an explicitly user action (e.g.; picked from the menu).
- (void)setModelOption:(ComposeboxModelOption)modelOption
    explicitUserAction:(BOOL)explicitUserAction;

/// Whether the omnibox is focused.
- (void)setOmniboxFocused:(bool)focused;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_INPUT_PLATE_MUTATOR_H_
