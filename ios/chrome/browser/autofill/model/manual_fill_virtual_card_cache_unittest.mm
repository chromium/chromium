// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/autofill/model/manual_fill_virtual_card_cache.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "components/autofill/ios/form_util/form_activity_observer_bridge.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

using autofill::CreditCard;
using autofill::test::GetVirtualCard;

class ManualFillVirtualCardCacheTest : public PlatformTest {
 protected:
  ManualFillVirtualCardCacheTest() {
    // Setup the FakeWebState
    ManualFillVirtualCardCache::CreateForWebState(&web_state_);
  }

  // Helper to get the cache attached to the test WebState.
  ManualFillVirtualCardCache* cache() {
    return ManualFillVirtualCardCache::FromWebState(&web_state_);
  }

  web::FakeWebState web_state_;
};

// Tests that a card can be cached and retrieved by its server_id.
TEST_F(ManualFillVirtualCardCacheTest, CacheAndRetrieveCard) {
  CreditCard card = GetVirtualCard();
  card.set_server_id("test_server_id");
  std::string server_id = card.server_id();

  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  // Initially, cache should be empty.
  EXPECT_EQ(nullptr, cache()->GetUnmaskedCard(server_id, origin));

  // Cache the card.
  cache()->CacheUnmaskedCard(card, origin);

  // Verify retrieval.
  const CreditCard* retrieved_card =
      cache()->GetUnmaskedCard(server_id, origin);
  ASSERT_NE(nullptr, retrieved_card);
  EXPECT_EQ(card.guid(), retrieved_card->guid());
  EXPECT_EQ(card.number(), retrieved_card->number());
}

// Tests that the cache is isolated per WebState (security check).
TEST_F(ManualFillVirtualCardCacheTest, CacheIsolation) {
  web::FakeWebState other_web_state;
  ManualFillVirtualCardCache::CreateForWebState(&other_web_state);
  ManualFillVirtualCardCache* other_cache =
      ManualFillVirtualCardCache::FromWebState(&other_web_state);

  CreditCard card = GetVirtualCard();
  card.set_server_id("test_server_id");

  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  // Cache in the main WebState.
  cache()->CacheUnmaskedCard(card, origin);

  // Verify it is NOT available in the other WebState.
  EXPECT_EQ(nullptr, other_cache->GetUnmaskedCard(card.server_id(), origin));
}

// Tests that the cache is CLEARED when navigating to a new document (e.g. new
// URL).
TEST_F(ManualFillVirtualCardCacheTest, ClearsOnNewDocumentNavigation) {
  CreditCard card = GetVirtualCard();
  card.set_server_id("test_server_id");
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  cache()->CacheUnmaskedCard(card, origin);
  ASSERT_NE(nullptr, cache()->GetUnmaskedCard(card.server_id(), origin));

  // Simulate a navigation to a new page.
  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);  // New document

  // Trigger the observer method manually.
  static_cast<web::WebStateObserver*>(cache())->DidFinishNavigation(&web_state_,
                                                                    &context);

  // Verify cache is cleared.
  EXPECT_EQ(nullptr, cache()->GetUnmaskedCard(card.server_id(), origin));
}

// Tests that the cache PERSISTS when navigating within the same document (e.g.
// anchor click).
TEST_F(ManualFillVirtualCardCacheTest, PersistsOnSameDocumentNavigation) {
  CreditCard card = GetVirtualCard();
  card.set_server_id("test_server_id");
  url::Origin origin = url::Origin::Create(GURL("https://example.com"));
  cache()->CacheUnmaskedCard(card, origin);
  ASSERT_NE(nullptr, cache()->GetUnmaskedCard(card.server_id(), origin));

  // Simulate a same-document navigation (e.g. #fragment change).
  web::FakeNavigationContext context;
  context.SetIsSameDocument(true);  // Same document

  static_cast<web::WebStateObserver*>(cache())->DidFinishNavigation(&web_state_,
                                                                    &context);

  // Verify cache still exists.
  EXPECT_NE(nullptr, cache()->GetUnmaskedCard(card.server_id(), origin));
}

// Tests that the cache is isolated per origin.
TEST_F(ManualFillVirtualCardCacheTest, OriginIsolated) {
  CreditCard card = GetVirtualCard();
  card.set_server_id("test_server_id");

  url::Origin origin_a = url::Origin::Create(GURL("https://a.com"));
  url::Origin origin_b = url::Origin::Create(GURL("https://b.com"));

  // Cache card for Origin A.
  cache()->CacheUnmaskedCard(card, origin_a);

  // Verify retrieval with Origin A works.
  EXPECT_NE(nullptr, cache()->GetUnmaskedCard("test_server_id", origin_a));

  // Verify retrieval with Origin B returns null (isolated).
  EXPECT_EQ(nullptr, cache()->GetUnmaskedCard("test_server_id", origin_b));
}

// Tests that SetUnmaskingOrigin asserts (crashes) if called multiple times
// without the transient state being consumed first.
TEST_F(ManualFillVirtualCardCacheTest, SetUnmaskingOriginMultipleTimesCrashes) {
  url::Origin origin_a = url::Origin::Create(GURL("https://a.com"));
  url::Origin origin_b = url::Origin::Create(GURL("https://b.com"));

  cache()->SetUnmaskingOrigin(origin_a);
  // Calling it a second time without consuming should fail the CHECK and crash.
  EXPECT_DEATH_IF_SUPPORTED(cache()->SetUnmaskingOrigin(origin_b), "");
}

}  // namespace
