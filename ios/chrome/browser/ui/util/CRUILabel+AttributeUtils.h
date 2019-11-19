// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_CRUILABEL_ATTRIBUTEUTILS_H_
#define IOS_CHROME_BROWSER_UI_UTIL_CRUILABEL_ATTRIBUTEUTILS_H_

#import <UIKit/UIKit.h>

@interface UILabel (CRUILabelAttributeUtils)
// The line height for the text in the receiver.
// Make sure to create a LabelObserver for this label and start observing before
// setting this property. When the last LabelObserver is removed for a label
// where this property is set, there is no expected behavior for the line
// height on further text changes -- it may be retained or overwritten.
//
// TODO(crbug.com/980510) : When iOS12 support is removed, determine if this
//   property is still needed, or if (under iOS13+) setting the line height
//   in a paragraph style persits across label frame changes.
//
@property(nonatomic, assign, setter=cr_setLineHeight:) CGFloat cr_lineHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_UTIL_CRUILABEL_ATTRIBUTEUTILS_H_
