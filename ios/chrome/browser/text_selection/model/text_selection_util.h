// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_SELECTION_UTIL_H_
#define IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_SELECTION_UTIL_H_

// Feature parameters for long-press and one-tap for
// `kEnableExpKitTextClassifierAddress` feature.
extern const char kTextClassifierAddressParameterName[];

// Feature parameters for long-press and one-tap for
// `kEnableExpKitTextClassifierPhoneNumber` feature.
extern const char kTextClassifierPhoneNumberParameterName[];

// Feature parameters for long-press and one-tap for
// `kEnableExpKitTextClassifierEmail` feature.
extern const char kTextClassifierEmailParameterName[];

// Command line parameter to force annotating a pages. A domain passed via this
// parameter will ignore IsEntitySelectionAllowedForURL result.
extern const char kForceAllowDomainForEntitySelection[];

#endif  // IOS_CHROME_BROWSER_TEXT_SELECTION_MODEL_TEXT_SELECTION_UTIL_H_
