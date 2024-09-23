// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/model/crash_reporter_url_observer.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class FakeWebState : public web::FakeWebState {
 public:
  void LoadURL(const GURL& url) {
    SetCurrentURL(url);
    web::FakeNavigationContext context;
    context.SetUrl(url);
    web::FakeNavigationManager* navigation_manager =
        static_cast<web::FakeNavigationManager*>(GetNavigationManager());
    navigation_manager->SetPendingItem(nullptr);
    pending_item_.reset();
    OnNavigationFinished(&context);
  }

  void LoadPendingURL(const GURL& url) {
    SetCurrentURL(url);
    web::FakeNavigationContext context;
    context.SetUrl(url);
    web::FakeNavigationManager* navigation_manager =
        static_cast<web::FakeNavigationManager*>(GetNavigationManager());
    DCHECK(!pending_item_);
    pending_item_ = web::NavigationItem::Create();
    pending_item_->SetURL(url);
    navigation_manager->SetPendingItem(pending_item_.get());
    OnNavigationStarted(&context);
  }

 private:
  std::unique_ptr<web::NavigationItem> pending_item_;
};

NSString* NumberToKey(NSNumber* number, bool pending) {
  return [NSString stringWithFormat:@"url%d%@", number.intValue,
                                    pending ? @"-pending" : @""];
}

}  // namespace

@interface DictionaryParameterSetter : NSObject <CrashReporterParameterSetter>
@property(nonatomic) NSMutableDictionary* params;
@end

@implementation DictionaryParameterSetter

- (instancetype)init {
  self = [super init];
  if (self) {
    _params = [[NSMutableDictionary alloc] init];
  }
  return self;
}

- (void)removeReportParameter:(NSNumber*)number pending:(BOOL)pending {
  NSString* key = NumberToKey(number, pending);
  [_params removeObjectForKey:key];
}

- (void)setReportParameterURL:(const GURL&)URL
                       forKey:(NSNumber*)number
                      pending:(BOOL)pending {
  NSString* key = NumberToKey(number, pending);
  [_params setObject:base::SysUTF8ToNSString(URL.spec()) forKey:key];
}

@end

class CrashReporterURLObserverTest : public PlatformTest {
 public:
  CrashReporterURLObserverTest() {
    TestProfileIOS::Builder test_profile_builder;
    test_profile_ = std::move(test_profile_builder).Build();
    params_ = [[DictionaryParameterSetter alloc] init];
    observer_ = std::make_unique<CrashReporterURLObserver>(params_);
  }

  FakeWebState* CreateWebState(WebStateList* web_state_list) {
    auto test_web_state = std::make_unique<FakeWebState>();
    test_web_state->SetBrowserState(test_profile_.get());
    test_web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    FakeWebState* test_web_state_ptr = test_web_state.get();
    web_state_list->InsertWebState(std::move(test_web_state));
    return test_web_state_ptr;
  }

  std::unique_ptr<FakeWebState> CreatePreloadWebState() {
    auto test_web_state = std::make_unique<FakeWebState>();
    test_web_state->SetBrowserState(test_profile_.get());
    test_web_state->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
    observer_->ObservePreloadWebState(test_web_state.get());
    return test_web_state;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> test_profile_;
  FakeWebStateListDelegate web_state_list_delegate_;
  DictionaryParameterSetter* params_;
  std::unique_ptr<CrashReporterURLObserver> observer_;
};

TEST_F(CrashReporterURLObserverTest, TestBasicBehaviors) {
  EXPECT_NSEQ(@{}, params_.params);

  // Create 5 WebStateLists to have 5 groups
  WebStateList web_state_list_1(&web_state_list_delegate_);
  observer_->ObserveWebStateList(&web_state_list_1);
  WebStateList web_state_list_2(&web_state_list_delegate_);
  observer_->ObserveWebStateList(&web_state_list_2);
  WebStateList web_state_list_3(&web_state_list_delegate_);
  observer_->ObserveWebStateList(&web_state_list_3);
  WebStateList web_state_list_4(&web_state_list_delegate_);
  observer_->ObserveWebStateList(&web_state_list_4);
  WebStateList web_state_list_5(&web_state_list_delegate_);
  observer_->ObserveWebStateList(&web_state_list_5);

  FakeWebState* web_state_11 = CreateWebState(&web_state_list_1);
  FakeWebState* web_state_12 = CreateWebState(&web_state_list_1);
  FakeWebState* web_state_21 = CreateWebState(&web_state_list_2);
  FakeWebState* web_state_31 = CreateWebState(&web_state_list_3);
  FakeWebState* web_state_41 = CreateWebState(&web_state_list_4);
  FakeWebState* web_state_51 = CreateWebState(&web_state_list_5);

  // Load in every group in turn. The last 3 should be reported.
  web_state_11->LoadURL(GURL("http://example11.test/"));
  NSDictionary* expected = @{@"url0" : @"http://example11.test/"};
  EXPECT_NSEQ(expected, params_.params);

  web_state_21->LoadURL(GURL("http://example21.test/"));
  expected = @{
    @"url0" : @"http://example11.test/",
    @"url1" : @"http://example21.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  web_state_31->LoadURL(GURL("http://example31.test/"));
  expected = @{
    @"url0" : @"http://example11.test/",
    @"url1" : @"http://example21.test/",
    @"url2" : @"http://example31.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  web_state_41->LoadURL(GURL("http://example41.test/"));
  expected = @{
    @"url0" : @"http://example41.test/",
    @"url1" : @"http://example21.test/",
    @"url2" : @"http://example31.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  web_state_51->LoadURL(GURL("http://example51.test/"));
  expected = @{
    @"url0" : @"http://example41.test/",
    @"url1" : @"http://example51.test/",
    @"url2" : @"http://example31.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  web_state_11->LoadURL(GURL("http://example12.test/"));
  expected = @{
    @"url0" : @"http://example41.test/",
    @"url1" : @"http://example51.test/",
    @"url2" : @"http://example12.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  // Load again in group 4. URL 0 should be updated.
  web_state_41->LoadURL(GURL("http://example42.test/"));
  expected = @{
    @"url0" : @"http://example42.test/",
    @"url1" : @"http://example51.test/",
    @"url2" : @"http://example12.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  // Load again in group 2.
  web_state_21->LoadURL(GURL("http://example22.test/"));
  expected = @{
    @"url0" : @"http://example42.test/",
    @"url1" : @"http://example22.test/",
    @"url2" : @"http://example12.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  // Load again in group 1, on multiple WebState.
  web_state_11->LoadURL(GURL("http://example13.test/"));
  expected = @{
    @"url0" : @"http://example42.test/",
    @"url1" : @"http://example22.test/",
    @"url2" : @"http://example13.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  web_state_12->LoadURL(GURL("http://example14.test/"));
  expected = @{
    @"url0" : @"http://example42.test/",
    @"url1" : @"http://example22.test/",
    @"url2" : @"http://example14.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  // Activate different WebState
  web_state_list_1.ActivateWebStateAt(0);
  expected = @{
    @"url0" : @"http://example42.test/",
    @"url1" : @"http://example22.test/",
    @"url2" : @"http://example13.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  web_state_list_1.ActivateWebStateAt(1);
  expected = @{
    @"url0" : @"http://example42.test/",
    @"url1" : @"http://example22.test/",
    @"url2" : @"http://example14.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  // Load a pending URL in a group already reported, then load it.
  web_state_41->LoadPendingURL(GURL("http://example43.test/"));
  expected = @{
    @"url0" : @"http://example42.test/",
    @"url0-pending" : @"http://example43.test/",
    @"url1" : @"http://example22.test/",
    @"url2" : @"http://example14.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  web_state_41->LoadURL(GURL("http://example43.test/"));
  expected = @{
    @"url0" : @"http://example43.test/",
    @"url1" : @"http://example22.test/",
    @"url2" : @"http://example14.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  // Load a pending URL in a group not already reported, then load it.
  web_state_51->LoadPendingURL(GURL("http://example53.test/"));
  expected = @{
    @"url0" : @"http://example43.test/",
    @"url1-pending" : @"http://example53.test/",
    @"url2" : @"http://example14.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  web_state_51->LoadURL(GURL("http://example53.test/"));
  expected = @{
    @"url0" : @"http://example43.test/",
    @"url1" : @"http://example53.test/",
    @"url2" : @"http://example14.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  // Remove a group and some reload URLs
  observer_->RemoveWebStateList(&web_state_list_5);
  expected = @{
    @"url0" : @"http://example43.test/",
    @"url2" : @"http://example14.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  web_state_31->LoadURL(GURL("http://example33.test/"));
  expected = @{
    @"url0" : @"http://example43.test/",
    @"url1" : @"http://example33.test/",
    @"url2" : @"http://example14.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  web_state_51->LoadURL(GURL("http://example54.test/"));
  expected = @{
    @"url0" : @"http://example43.test/",
    @"url1" : @"http://example33.test/",
    @"url2" : @"http://example54.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  // Remove a WebState
  web_state_12->LoadURL(GURL("http://example14.test/"));
  expected = @{
    @"url0" : @"http://example14.test/",
    @"url1" : @"http://example33.test/",
    @"url2" : @"http://example54.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  // This should activate the other WebState.
  std::unique_ptr<web::WebState> tmp_web_state =
      web_state_list_1.DetachWebStateAt(1);
  expected = @{
    @"url0" : @"http://example13.test/",
    @"url1" : @"http://example33.test/",
    @"url2" : @"http://example54.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  web_state_list_1.InsertWebState(
      std::move(tmp_web_state),
      WebStateList::InsertionParams::Automatic().Activate());
  expected = @{
    @"url0" : @"http://example14.test/",
    @"url1" : @"http://example33.test/",
    @"url2" : @"http://example54.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  std::unique_ptr<web::WebState> tmp_web_state2 =
      web_state_list_3.DetachWebStateAt(0);
  expected = @{
    @"url0" : @"http://example14.test/",
    @"url2" : @"http://example54.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  web_state_list_3.InsertWebState(
      std::move(tmp_web_state2),
      WebStateList::InsertionParams::Automatic().Activate());
  expected = @{
    @"url0" : @"http://example14.test/",
    @"url1" : @"http://example33.test/",
    @"url2" : @"http://example54.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  auto preload_web_state = CreatePreloadWebState();
  FakeWebState* preload_web_state_ptr = preload_web_state.get();
  expected = @{
    @"url0" : @"http://example14.test/",
    @"url1" : @"http://example33.test/",
    @"url2" : @"http://example54.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  preload_web_state->LoadPendingURL(GURL("http://example-preload.test/"));
  expected = @{
    @"url0" : @"http://example14.test/",
    @"url1" : @"http://example33.test/",
    @"url2-pending" : @"http://example-preload.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  observer_->StopObservingPreloadWebState(preload_web_state.get());
  web_state_list_3.ReplaceWebStateAt(0, std::move(preload_web_state));
  expected = @{
    @"url0" : @"http://example14.test/",
    @"url1" : @"http://example33.test/",
    @"url1-pending" : @"http://example-preload.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  preload_web_state_ptr->LoadURL(GURL("http://example-preload.test/"));
  expected = @{
    @"url0" : @"http://example14.test/",
    @"url1" : @"http://example-preload.test/"
  };
  EXPECT_NSEQ(expected, params_.params);

  observer_->StopObservingWebStateList(&web_state_list_1);
  observer_->StopObservingWebStateList(&web_state_list_2);
  observer_->StopObservingWebStateList(&web_state_list_3);
  observer_->StopObservingWebStateList(&web_state_list_4);
  observer_->StopObservingWebStateList(&web_state_list_5);
}
