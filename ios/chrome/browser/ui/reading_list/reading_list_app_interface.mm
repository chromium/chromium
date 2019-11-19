// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_app_interface.h"

#include "base/memory/singleton.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/reading_list/core/reading_list_model.h"
#include "ios/chrome/browser/reading_list/features.h"
#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/static_html_view_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/testing/nserror_util.h"
#include "net/base/mac/url_conversions.h"
#include "net/base/network_change_notifier.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns the reading list model.
ReadingListModel* GetReadingListModel(NSError** error) {
  ReadingListModel* model =
      ReadingListModelFactory::GetInstance()->GetForBrowserState(
          chrome_test_util::GetOriginalBrowserState());
  if (!base::test::ios::WaitUntilConditionOrTimeout(2, ^{
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

  ConnectionType GetCurrentConnectionType() const override {
    return CONNECTION_WIFI;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WifiNetworkChangeNotifier);
};

// Overrides the NetworkChangeNotifier to enable distillation even if the device
// does not have network.
class ConnectionTypeOverrider {
 public:
  static ConnectionTypeOverrider* SharedInstance() {
    return base::Singleton<ConnectionTypeOverrider>::get();
  }

  ConnectionTypeOverrider() {}

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

  DISALLOW_COPY_AND_ASSIGN(ConnectionTypeOverrider);
};

}  // namespace

@implementation ReadingListAppInterface

+ (NSError*)clearEntries {
  NSError* error = nil;
  ReadingListModel* model = GetReadingListModel(&error);
  if (error) {
    return error;
  }
  for (const GURL& url : model->Keys())
    model->RemoveEntryByURL(url);
  return nil;
}

+ (NSError*)addEntryWithURL:(NSURL*)url title:(NSString*)title read:(BOOL)read {
  NSError* error = nil;
  ReadingListModel* model = GetReadingListModel(&error);
  if (error) {
    return error;
  }
  model->AddEntry(net::GURLWithNSURL(url), base::SysNSStringToUTF8(title),
                  reading_list::ADDED_VIA_CURRENT_APP);
  if (read) {
    model->SetReadStatus(net::GURLWithNSURL(url), true);
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

+ (BOOL)staticHTMLViewContainingText:(NSString*)text {
  return chrome_test_util::StaticHtmlViewContainingText(
      chrome_test_util::GetCurrentWebState(), base::SysNSStringToUTF8(text));
}

+ (BOOL)isOfflinePageWithoutNativeContentEnabled {
  return reading_list::IsOfflinePageWithoutNativeContentEnabled();
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
