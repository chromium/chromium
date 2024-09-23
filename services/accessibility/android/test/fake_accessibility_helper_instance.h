// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_ANDROID_TEST_FAKE_ACCESSIBILITY_HELPER_INSTANCE_H_
#define SERVICES_ACCESSIBILITY_ANDROID_TEST_FAKE_ACCESSIBILITY_HELPER_INSTANCE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/accessibility/android/public/mojom/accessibility_helper.mojom.h"

namespace arc {

class FakeAccessibilityHelperInstance
    : public ax::android::mojom::AccessibilityHelperInstance {
 public:
  FakeAccessibilityHelperInstance();

  FakeAccessibilityHelperInstance(const FakeAccessibilityHelperInstance&) =
      delete;
  FakeAccessibilityHelperInstance& operator=(
      const FakeAccessibilityHelperInstance&) = delete;

  ~FakeAccessibilityHelperInstance() override;

  void Init(mojo::PendingRemote<ax::android::mojom::AccessibilityHelperHost>
                host_remote,
            InitCallback callback) override;
  void SetFilter(
      ax::android::mojom::AccessibilityFilterType filter_type) override;
  void PerformAction(
      ax::android::mojom::AccessibilityActionDataPtr action_data_ptr,
      PerformActionCallback callback) override;
  void SetNativeChromeVoxArcSupportForFocusedWindow(
      bool enabled,
      SetNativeChromeVoxArcSupportForFocusedWindowCallback callback) override;
  void SetExploreByTouchEnabled(bool enabled) override;
  void RefreshWithExtraData(
      ax::android::mojom::AccessibilityActionDataPtr action_data_ptr,
      RefreshWithExtraDataCallback callback) override;
  void RequestSendAccessibilityTree(
      ax::android::mojom::AccessibilityWindowKeyPtr window_ptr) override;

  ax::android::mojom::AccessibilityFilterType filter_type() {
    return filter_type_;
  }
  bool explore_by_touch_enabled() { return explore_by_touch_enabled_; }
  ax::android::mojom::AccessibilityActionData* last_requested_action() {
    return last_requested_action_.get();
  }
  ax::android::mojom::AccessibilityWindowKey* last_requested_tree_window_key() {
    return last_requested_tree_window_key_.get();
  }
  RefreshWithExtraDataCallback refresh_with_extra_data_callback() {
    return std::move(refresh_with_extra_data_callback_);
  }

 private:
  ax::android::mojom::AccessibilityFilterType filter_type_ =
      ax::android::mojom::AccessibilityFilterType::OFF;

  // Explore-by-touch is enabled by default in ARC++, so we default it to 'true'
  // in this test as well.
  bool explore_by_touch_enabled_ = true;

  ax::android::mojom::AccessibilityActionDataPtr last_requested_action_;
  ax::android::mojom::AccessibilityWindowKeyPtr last_requested_tree_window_key_;
  RefreshWithExtraDataCallback refresh_with_extra_data_callback_;
};

}  // namespace arc

#endif  // SERVICES_ACCESSIBILITY_ANDROID_TEST_FAKE_ACCESSIBILITY_HELPER_INSTANCE_H_
