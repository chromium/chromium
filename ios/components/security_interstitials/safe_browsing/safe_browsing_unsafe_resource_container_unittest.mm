// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"

#import "base/functional/bind.h"
#import "components/safe_browsing/ios/browser/safe_browsing_url_allow_list.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "services/network/public/mojom/fetch_api.mojom.h"
#import "testing/platform_test.h"

using security_interstitials::UnsafeResource;

// Test fixture for SafeBrowsingUnsafeResourceContainer.
class SafeBrowsingUnsafeResourceContainerTest : public PlatformTest {
 public:
  SafeBrowsingUnsafeResourceContainerTest()
      : item_(web::NavigationItem::Create()) {
    auto navigation_manager = std::make_unique<web::FakeNavigationManager>();
    navigation_manager->SetLastCommittedItem(item_.get());
    web_state_.SetNavigationManager(std::move(navigation_manager));
    SafeBrowsingUrlAllowList::CreateForWebState(&web_state_);
    SafeBrowsingUnsafeResourceContainer::CreateForWebState(&web_state_);
  }

  UnsafeResource MakePendingUnsafeResource() {
    UnsafeResource resource;
    resource.url = GURL("http://www.chromium.test");
    resource.navigation_url = resource.url;
    resource.threat_type =
        safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
    resource.callback =
        base::BindRepeating([](UnsafeResource::UrlCheckResult result) {});
    resource.weak_web_state = web_state_.GetWeakPtr();
    allow_list()->AddPendingUnsafeNavigationDecision(resource.url,
                                                     resource.threat_type);
    return resource;
  }

  SafeBrowsingUrlAllowList* allow_list() {
    return SafeBrowsingUrlAllowList::FromWebState(&web_state_);
  }
  SafeBrowsingUnsafeResourceContainer* container() {
    return SafeBrowsingUnsafeResourceContainer::FromWebState(&web_state_);
  }

 protected:
  std::unique_ptr<web::NavigationItem> item_;
  web::FakeWebState web_state_;
};

// Tests that main frame resources are correctly stored in and released from the
// container.
TEST_F(SafeBrowsingUnsafeResourceContainerTest, MainFrameResource) {
  UnsafeResource resource = MakePendingUnsafeResource();

  // The container should not have any unsafe main frame resources initially.
  EXPECT_FALSE(container()->GetMainFrameUnsafeResource());

  // Store `resource` in the container.
  container()->StoreMainFrameUnsafeResource(resource);
  const UnsafeResource* resource_copy =
      container()->GetMainFrameUnsafeResource();
  ASSERT_TRUE(resource_copy);
  EXPECT_EQ(resource.url, resource_copy->url);

  // Remove the pending decision and verify that the resource is removed from
  // the container.
  allow_list()->RemovePendingUnsafeNavigationDecisions(resource.url);
  EXPECT_FALSE(container()->GetMainFrameUnsafeResource());
}
