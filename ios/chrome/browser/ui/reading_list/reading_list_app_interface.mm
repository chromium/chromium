// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_app_interface.h"

#import "base/location.h"
#import "base/memory/singleton.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/nserror_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/base/network_change_notifier.h"

namespace {
// Returns the reading list model.
ReadingListModel* GetReadingListModel(NSError** error) {
  ReadingListModel* model =
      ReadingListModelFactory::GetInstance()->GetForProfile(
          chrome_test_util::GetOriginalProfile());
  if (!base::test::ios::WaitUntilConditionOrTimeout(base::Seconds(2), ^{
        return model->loaded();
      })) {
    *error = testing::NSErrorWithLocalizedDescription(
        @"Reading List model did not load");
  }
  return model;
}

// Overrides the NetworkChangeNotifier to enable distillation even if the device
// does not have network.
class WifiNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  WifiNetworkChangeNotifier() : net::NetworkChangeNotifier() {}

  WifiNetworkChangeNotifier(const WifiNetworkChangeNotifier&) = delete;
  WifiNetworkChangeNotifier& operator=(const WifiNetworkChangeNotifier&) =
      delete;

  ConnectionType GetCurrentConnectionType() const override {
    return CONNECTION_WIFI;
  }
};

// Overrides the NetworkChangeNotifier to enable distillation even if the device
// does not have network.
class ConnectionTypeOverrider {
 public:
  static ConnectionTypeOverrider* SharedInstance() {
    return base::Singleton<ConnectionTypeOverrider>::get();
  }

  ConnectionTypeOverrider() {}

  ConnectionTypeOverrider(const ConnectionTypeOverrider&) = delete;
  ConnectionTypeOverrider& operator=(const ConnectionTypeOverrider&) = delete;

  void OverrideConnectionType() {
    network_change_disabler_.reset(
        new net::NetworkChangeNotifier::DisableForTest());
    wifi_network_.reset(new WifiNetworkChangeNotifier());
  }

  void ResetConnectionType() {
    wifi_network_.reset();
    network_change_disabler_.reset();
  }

 private:
  std::unique_ptr<net::NetworkChangeNotifier::DisableForTest>
      network_change_disabler_;
  std::unique_ptr<WifiNetworkChangeNotifier> wifi_network_;
};

}  // namespace

@implementation ReadingListAppInterface

+ (NSError*)clearEntries {
  NSError* error = nil;
  ReadingListModel* model = GetReadingListModel(&error);
  if (error) {
    return error;
  }
  for (const GURL& url : model->GetKeys()) {
    model->RemoveEntryByURL(url, FROM_HERE);
  }
  return nil;
}

+ (NSError*)addEntryWithURL:(NSURL*)url title:(NSString*)title read:(BOOL)read {
  NSError* error = nil;
  ReadingListModel* model = GetReadingListModel(&error);
  if (error) {
    return error;
  }
  model->AddOrReplaceEntry(net::GURLWithNSURL(url),
                           base::SysNSStringToUTF8(title),
                           reading_list::ADDED_VIA_CURRENT_APP,
                           /*estimated_read_time=*/base::TimeDelta());
  if (read) {
    model->SetReadStatusIfExists(net::GURLWithNSURL(url), true);
  }
  return error;
}

+ (NSInteger)readEntriesCount {
  NSError* error = nil;
  ReadingListModel* model = GetReadingListModel(&error);
  if (error) {
    return -1;
  }
  return model->size() - model->unread_size();
}

+ (NSInteger)unreadEntriesCount {
  NSError* error = nil;
  ReadingListModel* model = GetReadingListModel(&error);
  if (error) {
    return -1;
  }
  return model->unread_size();
}

+ (void)forceConnectionToWifi {
  ConnectionTypeOverrider::SharedInstance()->OverrideConnectionType();
}

+ (void)notifyWifiConnection {
  if (net::NetworkChangeNotifier::IsOffline()) {
    net::NetworkChangeNotifier::NotifyObserversOfConnectionTypeChangeForTests(
        net::NetworkChangeNotifier::CONNECTION_WIFI);
  }
}

+ (void)resetConnectionType {
  ConnectionTypeOverrider::SharedInstance()->ResetConnectionType();
}

@end
