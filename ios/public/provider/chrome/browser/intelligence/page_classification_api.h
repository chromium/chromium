// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_API_H_

#import <string_view>
#import <vector>

namespace ios::provider {

// Returns the list of approved taxonomy prefixes for Education page
// classification.
std::vector<std::string_view> GetEducationCategoryPrefixes();

// Returns the list of approved entity MIDs for Education page classification.
std::vector<std::string_view> GetAcademicEntityMIDs();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_PAGE_CLASSIFICATION_API_H_
