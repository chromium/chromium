// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_TEST_UTILS_H_
#define CONTENT_BROWSER_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_TEST_UTILS_H_

#include "content/public/browser/content_browser_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class BrowserContext;

template <class SuperClass>
class MockCookieDeprecationLabelContentBrowserClientBase : public SuperClass {
 public:
  // ContentBrowserClient:
  MOCK_METHOD(bool,
              IsCookieDeprecationLabelAllowed,
              (content::BrowserContext * browser_context),
              (override));
  MOCK_METHOD(bool,
              IsCookieDeprecationLabelAllowedForContext,
              (content::BrowserContext * browser_context,
               const url::Origin& top_frame_origin,
               const url::Origin& context_origin),
              (override));
};

}  // namespace content

#endif  // CONTENT_BROWSER_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_TEST_UTILS_H_
