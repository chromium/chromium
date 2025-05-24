// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_COOKIE_MANAGER_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_COOKIE_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "net/base/isolation_info.h"
#include "net/cookies/cookie_partition_key.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"
#include "third_party/blink/public/test/mojom/cookie_manager/cookie_manager_automation.test-mojom.h"

class GURL;

namespace content {

class WebTestCookieManager
    : public blink::test::mojom::CookieManagerAutomation {
 public:
  explicit WebTestCookieManager(
      network::mojom::CookieManager* const cookie_manager,
      const GURL& url,
      const net::IsolationInfo& isolation_info);
  ~WebTestCookieManager() override = default;
  WebTestCookieManager(const WebTestCookieManager&) = delete;
  WebTestCookieManager& operator=(const WebTestCookieManager&) = delete;

  // blink::test::mojom::CookieManagerAutomation implementation
  void DeleteAllCookies(
      blink::test::mojom::CookieManagerAutomation::DeleteAllCookiesCallback)
      override;
  void GetAllCookies(
      blink::test::mojom::CookieManagerAutomation::GetAllCookiesCallback)
      override;
  void GetNamedCookie(
      const std::string& name,
      blink::test::mojom::CookieManagerAutomation::GetNamedCookieCallback)
      override;

 private:
  std::optional<net::CookiePartitionKey> GetCookiePartitionKey() const;

  const raw_ptr<network::mojom::CookieManager> cookie_manager_;
  const raw_ref<const GURL> url_;
  const raw_ref<const net::IsolationInfo> isolation_info_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_COOKIE_MANAGER_H_
