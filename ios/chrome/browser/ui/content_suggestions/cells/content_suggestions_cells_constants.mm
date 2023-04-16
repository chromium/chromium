// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

CGFloat ReturnToRecentTabHeight() {
  return kReturnToRecentTabSize.height;
}

const CGSize kReturnToRecentTabSize = {/*width=*/343, /*height=*/72};

NSString* const kContentSuggestionsWhatsNewIdentifier =
    @"ContentSuggestionsWhatsNewIdentifier";

NSString* const kQuerySuggestionViewA11yIdentifierPrefix =
    @"QuerySuggestionViewA11yIdentifierPrefix";
