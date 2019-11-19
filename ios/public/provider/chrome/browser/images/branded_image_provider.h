// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_IMAGES_BRANDED_IMAGE_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_IMAGES_BRANDED_IMAGE_PROVIDER_H_

#import <UIKit/UIKit.h>

#include "base/macros.h"
#include "ios/public/provider/chrome/browser/images/branded_image_icon_types.h"

// BrandedImageProvider vends images that contain embedder-specific branding.
// When adding method to this class, do not forget to add Chromium specific
// implementation to ChromiumBrandedImageProvider (the file may not be in the
// Xcode project if you are using internal sources).
class BrandedImageProvider {
 public:
  BrandedImageProvider();
  virtual ~BrandedImageProvider();

  // Returns the 24pt x 24pt image to use for the "activity controls" item on
  // the accounts list screen.
  virtual UIImage* GetAccountsListActivityControlsImage();

  // Returns the 24pt x 24pt image to use for the "account and activity" item on
  // the clear browsing data settings screen.
  virtual UIImage* GetClearBrowsingDataAccountActivityImage();

  // Returns the 24pt x 24pt image to use for the "account and activity" item on
  // the clear browsing data settings screen.
  virtual UIImage* GetClearBrowsingDataSiteDataImage();

  // Returns the 16pt x 16pt image to use for the "sync settings" item on the
  // signin confirmation screen.
  virtual UIImage* GetSigninConfirmationSyncSettingsImage();

  // Returns the 16pt x 16pt image to use for the "personalize services" item on
  // the signin confirmation screen.
  virtual UIImage* GetSigninConfirmationPersonalizeServicesImage();

  // Returns two 24pt x 24pt images to use for toolbar voice search button. The
  // images corresponds to the normal and pressed state.
  virtual NSArray<UIImage*>* GetToolbarVoiceSearchButtonImages(bool incognito);

  // Returns the 24pt x 24pt image corresponding to the given icon |type|.
  virtual UIImage* GetWhatsNewIconImage(WhatsNewIcon type);

  // Returns the 24pt x 24pt image to use for the "Download Google Drive" icon
  // on Download Manager UI.
  virtual UIImage* GetDownloadGoogleDriveImage();

  // Returns the 28pt x 28pt image to use for the "Search" icon in the toolbar.
  virtual UIImage* GetToolbarSearchIcon(SearchEngineIcon type,
                                        bool dark_version);

  // Returns the 30pt x 30pt image to use for the fallback icon for answers in
  // the omnibox popup and in the omnibox as the default search engine icon.
  virtual UIImage* GetOmniboxAnswerIcon();

 private:
  DISALLOW_COPY_AND_ASSIGN(BrandedImageProvider);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_IMAGES_BRANDED_IMAGE_PROVIDER_H_
