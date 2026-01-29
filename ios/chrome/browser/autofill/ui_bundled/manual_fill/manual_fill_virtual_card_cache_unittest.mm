// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_virtual_card_cache.h"

#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
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

// Tests that a card can be cached and retrieved by its GUID.
TEST_F(ManualFillVirtualCardCacheTest, CacheAndRetrieveCard) {
  CreditCard card = GetVirtualCard();
  std::string guid = card.guid();

  // Initially, cache should be empty.
  EXPECT_EQ(nullptr, cache()->GetUnmaskedCard(guid));

  // Cache the card.
  cache()->CacheUnmaskedCard(card);

  // Verify retrieval.
  const CreditCard* retrieved_card = cache()->GetUnmaskedCard(guid);
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

  // Cache in the main WebState.
  cache()->CacheUnmaskedCard(card);

  // Verify it is NOT available in the other WebState.
  EXPECT_EQ(nullptr, other_cache->GetUnmaskedCard(card.guid()));
}

// Tests that the cache is CLEARED when navigating to a new document (e.g. new
// URL).
TEST_F(ManualFillVirtualCardCacheTest, ClearsOnNewDocumentNavigation) {
  CreditCard card = GetVirtualCard();
  cache()->CacheUnmaskedCard(card);
  ASSERT_NE(nullptr, cache()->GetUnmaskedCard(card.guid()));

  // Simulate a navigation to a new page.
  web::FakeNavigationContext context;
  context.SetIsSameDocument(false);  // New document

  // Trigger the observer method manually (since FakeWebState doesn't
  // auto-dispatch strictly). Note: in production, WebState dispatches this
  // automatically. We cast to the observer interface to simulate the event.
  static_cast<web::WebStateObserver*>(cache())->DidFinishNavigation(&web_state_,
                                                                    &context);

  // Verify cache is cleared.
  EXPECT_EQ(nullptr, cache()->GetUnmaskedCard(card.guid()));
}

// Tests that the cache PERSISTS when navigating within the same document (e.g.
// anchor click).
TEST_F(ManualFillVirtualCardCacheTest, PersistsOnSameDocumentNavigation) {
  CreditCard card = GetVirtualCard();
  cache()->CacheUnmaskedCard(card);
  ASSERT_NE(nullptr, cache()->GetUnmaskedCard(card.guid()));

  // Simulate a same-document navigation (e.g. #fragment change).
  web::FakeNavigationContext context;
  context.SetIsSameDocument(true);  // Same document

  static_cast<web::WebStateObserver*>(cache())->DidFinishNavigation(&web_state_,
                                                                    &context);

  // Verify cache still exists.
  EXPECT_NE(nullptr, cache()->GetUnmaskedCard(card.guid()));
}

}  // namespace
