// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/session_storage_builder.h"

#import "base/mac/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/test/fakes/crw_fake_back_forward_list.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#import "ios/web/web_state/web_state_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWFakeWebViewNavigationProxy : NSObject <CRWWebViewNavigationProxy>

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (void)setCurrentURL:(NSString*)currentItemURL
         backListURLs:(NSArray<NSString*>*)backListURLs
      forwardListURLs:(NSArray<NSString*>*)forwardListURLs;

@end

@implementation CRWFakeWebViewNavigationProxy {
  NSURL* _URL;
  CRWFakeBackForwardList* _backForwardList;
}

- (instancetype)init {
  if ((self = [super init])) {
    _backForwardList = [[CRWFakeBackForwardList alloc] init];
  }
  return self;
}

- (void)setCurrentURL:(NSString*)currentItemURL
         backListURLs:(NSArray<NSString*>*)backListURLs
      forwardListURLs:(NSArray<NSString*>*)forwardListURLs {
  [_backForwardList setCurrentURL:currentItemURL
                     backListURLs:backListURLs
                  forwardListURLs:forwardListURLs];
  _URL = [NSURL URLWithString:currentItemURL];
}

- (WKBackForwardList*)backForwardList {
  return (id)_backForwardList;
}

- (NSURL*)URL {
  return _URL;
}

@end

namespace web {

using wk_navigation_util::kMaxSessionSize;

class SessionStorageBuilderTest : public WebTest {
 public:
  SessionStorageBuilderTest() {}
  ~SessionStorageBuilderTest() override {}

  void SetUp() override {
    WebTest::SetUp();

    web_state_ = WebStateImpl::CreateWithFakeWebViewNavigationProxyForTesting(
        WebState::CreateParams(GetBrowserState()),
        [[CRWFakeWebViewNavigationProxy alloc] init]);
  }

  void TearDown() override {
    web_state_.reset();
    WebTest::TearDown();
  }

  CRWFakeWebViewNavigationProxy* fake_web_view() {
    return base::mac::ObjCCastStrict<CRWFakeWebViewNavigationProxy>(
        web_state()->GetWebViewNavigationProxy());
  }

  WebStateImpl* web_state() { return web_state_.get(); }

 private:
  std::unique_ptr<WebStateImpl> web_state_;
};

// Tests building storage for session that is longer than kMaxSessionSize with
// last committed item at the end of the session.
TEST_F(SessionStorageBuilderTest, BuildStorageForExtraLongSession) {
  // Create WebState with navigation item count that exceeds kMaxSessionSize.
  NSMutableArray* back_urls = [NSMutableArray array];
  for (int i = 0; i < kMaxSessionSize; i++) {
    [back_urls addObject:[NSString stringWithFormat:@"http://%d.test", i]];
  }
  NSString* current_url = @"http://current.test";
  [fake_web_view() setCurrentURL:current_url
                    backListURLs:back_urls
                 forwardListURLs:nil];
  int original_item_count = web_state()->GetNavigationItemCount();
  ASSERT_EQ(kMaxSessionSize + 1, original_item_count);

  // Verify that storage item count does not exceed kMaxSessionSize.
  CRWSessionStorage* storage = SessionStorageBuilder::BuildStorage(
      *web_state(), web_state()->GetNavigationManagerImpl(),
      web_state()->GetSessionCertificatePolicyCacheImpl());
  ASSERT_TRUE(storage);
  int stored_item_count = storage.itemStorages.count;
  ASSERT_EQ(kMaxSessionSize, stored_item_count);

  // Walk backwards and verify that URLs in the storage match original URLs.
  for (int i = 0; i < kMaxSessionSize; i++) {
    NavigationManager* navigation_manager = web_state()->GetNavigationManager();
    NavigationItem* item =
        navigation_manager->GetItemAtIndex(original_item_count - i - 1);
    CRWNavigationItemStorage* item_storage =
        [storage.itemStorages objectAtIndex:stored_item_count - i - 1];
    EXPECT_EQ(item->GetURL(), item_storage.URL) << "index: " << i;
  }
}

// Tests building storage for session that has items with
// ShouldSkipSerialization flag. The session length after skipping the items is
// not longer than kMaxSessionSize.
TEST_F(SessionStorageBuilderTest, ShouldSkipSerializationItems) {
  // Create WebState with navigation item count that exceeds kMaxSessionSize.
  NSMutableArray* back_urls = [NSMutableArray array];
  for (int i = 0; i < kMaxSessionSize; i++) {
    [back_urls addObject:[NSString stringWithFormat:@"http://%d.test", i]];
  }
  NSString* const current_url = @"http://current.test";
  [fake_web_view() setCurrentURL:current_url
                    backListURLs:back_urls
                 forwardListURLs:nil];
  NavigationManager* navigation_manager = web_state()->GetNavigationManager();
  int original_item_count = web_state()->GetNavigationItemCount();
  ASSERT_EQ(kMaxSessionSize + 1, original_item_count);

  const int kSkippedItemIndex = kMaxSessionSize - 1;
  web_state()
      ->GetNavigationManagerImpl()
      .GetNavigationItemImplAtIndex(kSkippedItemIndex)
      ->SetShouldSkipSerialization(true);

  // Verify that storage item count does not exceed kMaxSessionSize.
  CRWSessionStorage* storage = SessionStorageBuilder::BuildStorage(
      *web_state(), web_state()->GetNavigationManagerImpl(),
      web_state()->GetSessionCertificatePolicyCacheImpl());
  ASSERT_TRUE(storage);
  int stored_item_count = static_cast<int>(storage.itemStorages.count);
  ASSERT_EQ(kMaxSessionSize, stored_item_count);

  // Verify that URLs in the storage match original URLs without skipped item.
  for (int storage_index = 0, item_index = 0;
       storage_index < kMaxSessionSize &&
       item_index < web_state()->GetNavigationItemCount();
       storage_index++, item_index++) {
    if (item_index == kSkippedItemIndex) {
      item_index++;
    }
    NavigationItem* item = navigation_manager->GetItemAtIndex(item_index);

    CRWNavigationItemStorage* item_storage =
        [storage.itemStorages objectAtIndex:storage_index];
    EXPECT_EQ(item->GetURL(), item_storage.URL) << "item_index: " << item_index;
  }
}

// Tests building storage for session that has URL longer than
// url::kMaxURLChars.
TEST_F(SessionStorageBuilderTest, SkipLongUrls) {
  // Create WebState with navigation item count that exceeds kMaxSessionSize.
  NSString* long_url =
      [@"https://" stringByPaddingToLength:url::kMaxURLChars + 1
                                withString:@"a"
                           startingAtIndex:0];
  NSString* normal_url = @"https://foo.test";
  [fake_web_view() setCurrentURL:normal_url
                    backListURLs:@[ long_url ]
                 forwardListURLs:nil];
  ASSERT_EQ(2, web_state()->GetNavigationItemCount());

  web_state()
      ->GetNavigationManagerImpl()
      .GetNavigationItemImplAtIndex(1)
      ->SetReferrer(web::Referrer(GURL(base::SysNSStringToUTF8(long_url)),
                                  web::ReferrerPolicy::ReferrerPolicyDefault));

  // Verify that storage has single item and that item does not have a referrer.
  CRWSessionStorage* storage = SessionStorageBuilder::BuildStorage(
      *web_state(), web_state()->GetNavigationManagerImpl(),
      web_state()->GetSessionCertificatePolicyCacheImpl());
  ASSERT_TRUE(storage);
  ASSERT_EQ(1U, storage.itemStorages.count);

  EXPECT_EQ(GURL::EmptyGURL(), [storage.itemStorages.firstObject referrer].url);
}

}  // namespace web
