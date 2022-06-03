// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_QR_GENERATOR_QR_GENERATOR_UTIL_H_
#define IOS_CHROME_BROWSER_UI_QR_GENERATOR_QR_GENERATOR_UTIL_H_

#import <UIKit/UIKit.h>

// Generate a QR code image for |data| and scaling it to have its height and
// width equal to |imageLength| in points.
UIImage* GenerateQRCode(NSData* data, CGFloat imageLength);

#endif  // IOS_CHROME_BROWSER_UI_QR_GENERATOR_QR_GENERATOR_UTIL_H_
