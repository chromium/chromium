// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_PAGE_CONTEXT_WRAPPER_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_PAGE_CONTEXT_WRAPPER_API_H_

#import <string>

namespace ios::provider {

// TODO(crbug.com/460380319): Remove V2 once the migration is completed.
// Gets the portion of the PageContext script that checks whether PageContext
// should be detached from the request.
const std::u16string GetPageContextShouldDetachScriptV2();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_PAGE_CONTEXT_WRAPPER_API_H_
