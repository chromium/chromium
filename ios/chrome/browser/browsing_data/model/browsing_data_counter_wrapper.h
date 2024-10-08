// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_COUNTER_WRAPPER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_COUNTER_WRAPPER_H_

#import <memory>
#import <string_view>

#import "base/functional/callback_forward.h"
#import "components/browsing_data/core/counters/browsing_data_counter.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios_forward.h"

class PrefService;

// Wrapper around a browsing data volume counter that bridges the update counter
// UI callback to the UI.
class BrowsingDataCounterWrapper {
 public:
  using UpdateUICallback = base::RepeatingCallback<void(
      const browsing_data::BrowsingDataCounter::Result&)>;

  // This method returns the counter corresponding to the data type specified by
  // `pref_name` or null if there is no such counter.
  static std::unique_ptr<BrowsingDataCounterWrapper> CreateCounterWrapper(
      std::string_view pref_name,
      ProfileIOS* profile,
      PrefService* pref_service,
      UpdateUICallback update_ui_callback);

  static std::unique_ptr<BrowsingDataCounterWrapper> CreateCounterWrapper(
      std::string_view pref_name,
      ProfileIOS* profile,
      PrefService* pref_service,
      base::Time begin_time,
      UpdateUICallback update_ui_callback);

  BrowsingDataCounterWrapper(const BrowsingDataCounterWrapper&) = delete;
  BrowsingDataCounterWrapper& operator=(const BrowsingDataCounterWrapper&) =
      delete;

  ~BrowsingDataCounterWrapper();

  void RestartCounter();

  void SetBeginTime(base::Time beginTime);

 private:
  BrowsingDataCounterWrapper(
      std::unique_ptr<browsing_data::BrowsingDataCounter> counter,
      PrefService* pref_service,
      UpdateUICallback update_ui_callback);

  BrowsingDataCounterWrapper(
      std::unique_ptr<browsing_data::BrowsingDataCounter> counter,
      PrefService* pref_service,
      base::Time begin_time,
      UpdateUICallback update_ui_callback);

  // Method to be passed as callback to the counter. This will be invoked when
  // the result is ready.
  void UpdateWithResult(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result);

  // The counter for which the interaction is managed by this wrapper.
  std::unique_ptr<browsing_data::BrowsingDataCounter> counter_;

  // Callback that updates the UI once the counter result is ready. This is
  // invoked by UpdateWithResult.
  UpdateUICallback update_ui_callback_;
};

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_BROWSING_DATA_COUNTER_WRAPPER_H_
