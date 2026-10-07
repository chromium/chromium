// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/scoped_eg_traits_overrider.h"

#import "ios/chrome/test/scoped_eg_traits_overrider_app_interface.h"

ScopedTraitOverrider::ScopedTraitOverrider(
    UIViewController* top_view_controller)
    : top_view_controller_(top_view_controller) {
  if (!top_view_controller_) {
    return;
  }

  // Records the override rather than the effective trait value, which may be
  // inherited and must not be pinned as an override by the destructor.
  original_content_size_category_override_ = [ScopedTraitOverriderAppInterface
      contentSizeCategoryOverrideForViewController:top_view_controller_];
}

ScopedTraitOverrider::~ScopedTraitOverrider() {
  if (!top_view_controller_) {
    return;
  }

  // A `nil` original override removes the override, so that the view
  // controller inherits the trait from its environment again.
  [ScopedTraitOverriderAppInterface
      setContentSizeCategoryOverride:original_content_size_category_override_
                   forViewController:top_view_controller_];
}

void ScopedTraitOverrider::SetContentSizeCategory(
    UIContentSizeCategory new_content_size_category) {
  if (!top_view_controller_) {
    return;
  }

  [ScopedTraitOverriderAppInterface
      setContentSizeCategoryOverride:new_content_size_category
                   forViewController:top_view_controller_];
}
