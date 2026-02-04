// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_CLASSIFICATION_METRICS_TAB_HELPER_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_CLASSIFICATION_METRICS_TAB_HELPER_API_H_

namespace web {
class WebState;
}  // namespace web

namespace ios::provider {
void AttachClassificationMetricsTabHelper(web::WebState* web_state);
}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_INTELLIGENCE_CLASSIFICATION_METRICS_TAB_HELPER_API_H_
