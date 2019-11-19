// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_

#import <UIKit/UIKit.h>

namespace content_suggestions {

extern const int kSearchFieldBackgroundColor;

extern const CGFloat kHintTextScale;

// Returns the proper height for the doodle. |logoIsShowing| refers to the
// Google logo or the doodle.
CGFloat doodleHeight(BOOL logoIsShowing);
// Returns the proper margin to the top of the header for the doodle.
// If |toolbarPresent| is true, the top margin include a space to display the
// toolbar.  Adds |topInset| to non-RxR displays.
CGFloat doodleTopMargin(BOOL toolbarPresent, CGFloat topInset);
// Returns the proper margin to the bottom of the doodle for the search field.
CGFloat searchFieldTopMargin();
// Returns the proper width for the search field inside a view with a |width|.
CGFloat searchFieldWidth(CGFloat superviewWidth);
// TODO(crbug.com/761817): Remove |toolbarPresent| once the transition to the
// new architecture is completed.
// Returns the expected height of the header. |logoIsShowing| refers to the
// Google logo or the doodle. |promoCanShow| represents whether a what's new
// promo can be displayed.  |toolbarPresent| represent whether the height should
// take into account a space to show the toolbar.
CGFloat heightForLogoHeader(BOOL logoIsShowing,
                            BOOL promoCanShow,
                            BOOL toolbarPresent,
                            CGFloat topInset);
// Configure the |searchHintLabel| for the fake omnibox.  |hintLabelContainer|
// is added to the |searchTapTarget| with autolayout and |searchHintLabel| is
// added to |hintLabelContainer| with autoresizing.  This is done due to the
// way searchHintLabel is later tranformed.
void configureSearchHintLabel(UILabel* searchHintLabel,
                              UIView* searchTapTarget);
// Configure the |voiceSearchButton|, adding it to the |searchTapTarget| and
// constraining it.
void configureVoiceSearchButton(UIButton* voiceSearchButton,
                                UIView* searchTapTarget);

// Returns the nearest ancestor of |view| that is kind of |aClass|.
UIView* nearestAncestor(UIView* view, Class aClass);

}  // namespace content_suggestions

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CONTENT_SUGGESTIONS_COLLECTION_UTILS_H_
