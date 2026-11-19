// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_NAVIGATION_UI_DATA_H_
#define CONTENT_PUBLIC_TEST_TEST_NAVIGATION_UI_DATA_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "content/public/browser/navigation_ui_data.h"

namespace content {

// Subclass for `//content` tests to check `NavigationUIData` plumbing and
// handle population.
class TestNavigationUIData : public content::NavigationUIData {
 public:
  TestNavigationUIData();
  explicit TestNavigationUIData(int precursor_data);
  ~TestNavigationUIData() override;

  std::unique_ptr<NavigationUIData> Clone() override;
  std::unique_ptr<NavigationUIData> MaybeCreateForRedispatchedNavigation()
      override;
  void PopulateFromNavigationHandle(
      NavigationHandle& navigation_handle) override;

  int precursor_data() const { return precursor_data_; }
  std::optional<int64_t> populated_navigation_id() const {
    return populated_navigation_id_;
  }

 private:
  int precursor_data_ = 0;
  std::optional<int64_t> populated_navigation_id_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_NAVIGATION_UI_DATA_H_
