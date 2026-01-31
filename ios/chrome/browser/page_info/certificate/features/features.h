// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_CERTIFICATE_FEATURES_FEATURES_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_CERTIFICATE_FEATURES_FEATURES_H_

#include "base/feature_list.h"

namespace page_info_certificate {

// Controls the Certificate Information viewing feature in Page Info.
BASE_DECLARE_FEATURE(kViewCertificateInformation);

// Whether the Certificate Information viewing feature is enabled.
bool IsViewCertificateInformationFeatureEnabled();

}  // namespace page_info_certificate

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_CERTIFICATE_FEATURES_FEATURES_H_
