// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/gaia_auth_fetcher_ios.h"

#import <memory>

#import "base/run_loop.h"
#import "google_apis/gaia/gaia_constants.h"
#import "google_apis/gaia/gaia_urls.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/web_task_environment.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#import "services/network/test/test_url_loader_factory.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

class FakeGaiaAuthFetcherIOSBridge : public GaiaAuthFetcherIOSBridge {
 public:
  FakeGaiaAuthFetcherIOSBridge(
      GaiaAuthFetcherIOSBridge::GaiaAuthFetcherIOSBridgeDelegate* delegate)
      : GaiaAuthFetcherIOSBridge(delegate) {}
  ~FakeGaiaAuthFetcherIOSBridge() override {}

  void Fetch(const GURL& url,
             const std::string& headers,
             const std::string& body,
             bool should_use_xml_http_request) override {
    fetch_called_ = true;
    url_ = url;
  }

  void Cancel() override { cancel_called_ = true; }

  void NotifyDelegateFetchSuccess(const std::string& data) {
    EXPECT_TRUE(fetch_called_);
    const int kSuccessResponseCode = 200;
    delegate()->OnFetchComplete(url_, data, net::Error::OK,
                                kSuccessResponseCode);
    fetch_called_ = false;
  }

  void NotifyDelegateFetchError(net::Error net_error) {
    EXPECT_TRUE(fetch_called_);
    const int kSomeErrorResponseCode = 500;
    delegate()->OnFetchComplete(url_, "", net_error, kSomeErrorResponseCode);
    fetch_called_ = false;
  }

  void NotifyDelegateFetchAborted() {
    EXPECT_TRUE(cancel_called_);
    const int kIgnoredResponseCode = 0;
    delegate()->OnFetchComplete(url_, "", net::ERR_ABORTED,
                                kIgnoredResponseCode);
    fetch_called_ = false;
    cancel_called_ = false;
  }

  bool fetch_called() const { return fetch_called_; }
  bool cancel_called() const { return cancel_called_; }

 private:
  bool fetch_called_ = false;
  bool cancel_called_ = false;
  GURL url_;
};

class MockGaiaConsumer : public GaiaAuthConsumer {
 public:
  MockGaiaConsumer() {}
  ~MockGaiaConsumer() override {}

  MOCK_METHOD1(OnLogOutFailure, void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(OnGetCheckConnectionInfoSuccess, void(const std::string& data));
};
}

// Tests fixture for GaiaAuthFetcherIOS
class GaiaAuthFetcherIOSTest : public PlatformTest {
 protected:
  GaiaAuthFetcherIOSTest() {
    profile_ = TestProfileIOS::Builder().Build();

    gaia_auth_fetcher_.reset(new GaiaAuthFetcherIOS(
        &consumer_, gaia::GaiaSource::kChrome,
        test_url_loader_factory_.GetSafeWeakWrapper(), profile_.get()));
    gaia_auth_fetcher_->bridge_.reset(
        new FakeGaiaAuthFetcherIOSBridge(gaia_auth_fetcher_.get()));
  }

  ~GaiaAuthFetcherIOSTest() override {
    gaia_auth_fetcher_.reset();
  }

  FakeGaiaAuthFetcherIOSBridge* GetBridge() {
    return static_cast<FakeGaiaAuthFetcherIOSBridge*>(
        gaia_auth_fetcher_->bridge_.get());
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<ProfileIOS> profile_;
  MockGaiaConsumer consumer_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<GaiaAuthFetcherIOS> gaia_auth_fetcher_;
};

// Tests that the failure case works properly by starting a LogOut request,
// making it fail, and controlling that the consumer is properly called.
TEST_F(GaiaAuthFetcherIOSTest, StartLogOutError) {
  gaia_auth_fetcher_->StartLogOut();
  EXPECT_TRUE(GetBridge()->fetch_called());

  GoogleServiceAuthError expected_error =
      GoogleServiceAuthError::FromConnectionError(net::Error::ERR_FAILED);
  EXPECT_CALL(consumer_, OnLogOutFailure(expected_error));
  GetBridge()->NotifyDelegateFetchError(net::Error::ERR_FAILED);
}

// Tests that requests that do not require cookies are using the original
// GaiaAuthFetcher and not the GaiaAuthFetcherIOS specialization.
TEST_F(GaiaAuthFetcherIOSTest, StartGetCheckConnectionInfo) {
  std::string data(
      "[{\"carryBackToken\": \"token1\", \"url\": \"http://www.google.com\"}]");
  EXPECT_CALL(consumer_, OnGetCheckConnectionInfoSuccess(data)).Times(1);

  // Set up the fake response.
  test_url_loader_factory_.AddResponse(
      GaiaUrls::GetInstance()
          ->GetCheckConnectionInfoURLWithSource(GaiaConstants::kChromeSource)
          .spec(),
      data);

  gaia_auth_fetcher_->StartGetCheckConnectionInfo();
  EXPECT_FALSE(GetBridge()->fetch_called());

  base::RunLoop().RunUntilIdle();
}
