// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/android/test/fake_accessibility_helper_instance.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"

namespace arc {

FakeAccessibilityHelperInstance::FakeAccessibilityHelperInstance() = default;
FakeAccessibilityHelperInstance::~FakeAccessibilityHelperInstance() = default;

void FakeAccessibilityHelperInstance::Init(
    mojo::PendingRemote<ax::android::mojom::AccessibilityHelperHost>
        host_remote,
    InitCallback callback) {
  std::move(callback).Run();
}

void FakeAccessibilityHelperInstance::SetFilter(
    ax::android::mojom::AccessibilityFilterType filter_type) {
  filter_type_ = filter_type;
}

void FakeAccessibilityHelperInstance::PerformAction(
    ax::android::mojom::AccessibilityActionDataPtr action_data_ptr,
    PerformActionCallback callback) {
  last_requested_action_ = std::move(action_data_ptr);
  std::move(callback).Run(true);
}

void FakeAccessibilityHelperInstance::
    SetNativeChromeVoxArcSupportForFocusedWindow(
        bool enabled,
        SetNativeChromeVoxArcSupportForFocusedWindowCallback callback) {
  std::move(callback).Run(
      ax::android::mojom::SetNativeChromeVoxResponse::SUCCESS);
}

void FakeAccessibilityHelperInstance::SetExploreByTouchEnabled(bool enabled) {
  explore_by_touch_enabled_ = enabled;
}

void FakeAccessibilityHelperInstance::RefreshWithExtraData(
    ax::android::mojom::AccessibilityActionDataPtr action_data_ptr,
    RefreshWithExtraDataCallback callback) {
  last_requested_action_ = std::move(action_data_ptr);
  refresh_with_extra_data_callback_ = std::move(callback);
}

void FakeAccessibilityHelperInstance::RequestSendAccessibilityTree(
    ax::android::mojom::AccessibilityWindowKeyPtr window_ptr) {
  last_requested_tree_window_key_ = std::move(window_ptr);
}

}  // namespace arc
