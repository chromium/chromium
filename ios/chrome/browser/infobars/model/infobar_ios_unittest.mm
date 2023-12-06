// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/model/infobar_ios.h"

#import "ios/chrome/browser/infobars/model/test/fake_infobar_delegate.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/platform_test.h"

namespace {
// Mock InfoBarIOS::Observer.
class MockInfoBarIOSObserver : public InfoBarIOS::Observer {
 public:
  MockInfoBarIOSObserver() {}
  ~MockInfoBarIOSObserver() override {}

  MOCK_METHOD1(DidUpdateAcceptedState, void(InfoBarIOS*));

  // InfobarDetroyed not mocked so that the observer can be correctly removed
  // upon destruction.
  void InfobarDestroyed(InfoBarIOS* infobar) override {
    infobar->RemoveObserver(this);
    infobar_destroyed_ = true;
  }

  //  Whether InfobarDestroyed was called.
  bool infobar_destroyed() const { return infobar_destroyed_; }

 private:
  bool infobar_destroyed_ = false;
};
}  // namespace

using InfoBarIOSTest = PlatformTest;

// Tests that InfoBarIOS::set_accepted() updates state and notifies observers.
TEST_F(InfoBarIOSTest, AcceptedState) {
  MockInfoBarIOSObserver observer;
  InfoBarIOS infobar(InfobarType::kInfobarTypeConfirm,
                     std::make_unique<FakeInfobarDelegate>());
  infobar.AddObserver(&observer);

  ASSERT_FALSE(infobar.accepted());

  EXPECT_CALL(observer, DidUpdateAcceptedState(&infobar));
  infobar.set_accepted(true);
  EXPECT_TRUE(infobar.accepted());

  EXPECT_CALL(observer, DidUpdateAcceptedState(&infobar));
  infobar.set_accepted(false);
  EXPECT_FALSE(infobar.accepted());
}

// Tests that InfobarDestroyed() is called on destruction.
TEST_F(InfoBarIOSTest, Destroyed) {
  MockInfoBarIOSObserver observer;
  std::unique_ptr<InfoBarIOS> infobar =
      std::make_unique<InfoBarIOS>(InfobarType::kInfobarTypeConfirm,
                                   std::make_unique<FakeInfobarDelegate>());
  infobar->AddObserver(&observer);
  ASSERT_FALSE(observer.infobar_destroyed());

  infobar = nullptr;
  EXPECT_TRUE(observer.infobar_destroyed());
}
