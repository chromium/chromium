// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_navigation_ui_data.h"

#include <memory>

#include "content/public/browser/navigation_handle.h"

namespace content {

TestNavigationUIData::TestNavigationUIData() = default;

TestNavigationUIData::TestNavigationUIData(int precursor_data)
    : precursor_data_(precursor_data) {}

TestNavigationUIData::~TestNavigationUIData() = default;

std::unique_ptr<NavigationUIData> TestNavigationUIData::Clone() {
  auto copy = std::make_unique<TestNavigationUIData>(precursor_data_);
  copy->populated_navigation_id_ = populated_navigation_id_;
  return copy;
}

std::unique_ptr<NavigationUIData>
TestNavigationUIData::MaybeCreateForRedispatchedNavigation() {
  return std::make_unique<TestNavigationUIData>(precursor_data_);
}

void TestNavigationUIData::PopulateFromNavigationHandle(
    NavigationHandle& navigation_handle) {
  populated_navigation_id_ = navigation_handle.GetNavigationId();
}

}  // namespace content
