// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COUNTER_WRAPPER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COUNTER_WRAPPER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/strings/string_piece.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"

class ChromeBrowserState;
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
      base::StringPiece pref_name,
      ChromeBrowserState* browser_state,
      PrefService* pref_service,
      UpdateUICallback update_ui_callback);

  BrowsingDataCounterWrapper(const BrowsingDataCounterWrapper&) = delete;
  BrowsingDataCounterWrapper& operator=(const BrowsingDataCounterWrapper&) =
      delete;

  ~BrowsingDataCounterWrapper();

  void RestartCounter();

 private:
  BrowsingDataCounterWrapper(
      std::unique_ptr<browsing_data::BrowsingDataCounter> counter,
      PrefService* pref_service,
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

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_COUNTER_WRAPPER_H_
