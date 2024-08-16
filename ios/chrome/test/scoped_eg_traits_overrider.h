// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SCOPED_EG_TRAITS_OVERRIDER_H_
#define IOS_CHROME_TEST_SCOPED_EG_TRAITS_OVERRIDER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Change view controller trait overrides for EarlGrey tests and returns back to
// the original values in destructor.
class ScopedTraitOverrider {
 public:
  ScopedTraitOverrider(UIViewController* top_view_controller);

  ScopedTraitOverrider(const ScopedTraitOverrider&) = delete;
  ScopedTraitOverrider& operator=(const ScopedTraitOverrider&) = delete;

  ~ScopedTraitOverrider();

  // For other trait overrides, add other methods here. The `traitOverrides` is
  // read only.
  void SetContentSizeCategory(UIContentSizeCategory new_content_size_category);

 private:
  UIContentSizeCategory original_content_size_category_;
  UIViewController* top_view_controller_;
};

#endif  // IOS_CHROME_TEST_SCOPED_EG_TRAITS_OVERRIDER_H_
