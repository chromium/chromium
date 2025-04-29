// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_LOGO_VENDOR_PROVIDER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_LOGO_VENDOR_PROVIDER_H_

@protocol LogoVendor;

// A protocol for providing a logo vendor object used in Home customization.
@protocol HomeCustomizationLogoVendorProvider

// Provides the logo vendor object.
- (id<LogoVendor>)provideLogoVendor;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_LOGO_VENDOR_PROVIDER_H_
