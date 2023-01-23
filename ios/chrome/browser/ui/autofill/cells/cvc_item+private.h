// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_CVC_ITEM_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_CVC_ITEM_PRIVATE_H_

// Class extension exposing private properties of CVCCell for testing.
@interface CVCCell ()
@property(nonatomic, strong) UIView* dateContainerView;
@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CELLS_CVC_ITEM_PRIVATE_H_
