// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_TEXT_FIELD_VIEW_CONTAINING_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_TEXT_FIELD_VIEW_CONTAINING_H_

// A protocol that defines an object that contains a text field view.
@protocol TextFieldViewContaining

// A text field view.
@property(nonatomic, readonly) UIView* textFieldView;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_TEXT_FIELD_VIEW_CONTAINING_H_
