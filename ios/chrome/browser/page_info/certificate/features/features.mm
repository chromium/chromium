// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/certificate/features/features.h"

#import "base/feature_list.h"

namespace page_info_certificate {

BASE_FEATURE(kViewCertificateInformation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsViewCertificateInformationFeatureEnabled() {
  return base::FeatureList::IsEnabled(kViewCertificateInformation);
}

}  // namespace page_info_certificate
