// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_MODAL_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_MODAL_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/infobars/modals/infobar_modal_delegate.h"

// Delegate to handle Translate Infobar Modal actions.
@protocol InfobarTranslateModalDelegate <InfobarModalDelegate>

// Indicates the user chose to always translate sites in the source language.
- (void)alwaysTranslateSourceLanguage;

// Indicates the user chose to never translate sites in the source language.
- (void)neverTranslateSourceLanguage;

// TODO(crbug.com/1014959): Consider implementing neverTranslateSite.

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_INFOBAR_TRANSLATE_MODAL_DELEGATE_H_
