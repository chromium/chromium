// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_AUTOCOMPLETE_MATCH_CELL_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_AUTOCOMPLETE_MATCH_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_match_formatter.h"

extern NSString* const kAutocompleteMatchCellReuseIdentifier;

/// UITableViewCell to display an autocomplete match in the omnibox debugger.
@interface AutocompleteMatchCell : UITableViewCell

- (void)setupWithAutocompleteMatchFormatter:
            (AutocompleteMatchFormatter*)matchFormatter
                           showProviderType:(BOOL)shouldShowProviderType;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_POPUP_DEBUGGER_AUTOCOMPLETE_MATCH_CELL_H_
