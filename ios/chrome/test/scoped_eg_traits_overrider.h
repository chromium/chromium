// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_SCOPED_EG_TRAITS_OVERRIDER_H_
#define IOS_CHROME_TEST_SCOPED_EG_TRAITS_OVERRIDER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Overrides traits of a view controller for EarlGrey tests, and reverts the
// overrides in the destructor. A trait that was already overridden when this
// object was created gets its original override back. Any other override is
// removed, so that the view controller inherits that trait from its
// environment again (e.g. live text size changes).
class ScopedTraitOverrider {
 public:
  // `top_view_controller` is the eDO proxy of an app-side view controller,
  // usually the top presented one. Nothing is overridden if it is `nil`.
  explicit ScopedTraitOverrider(UIViewController* top_view_controller);

  ScopedTraitOverrider(const ScopedTraitOverrider&) = delete;
  ScopedTraitOverrider& operator=(const ScopedTraitOverrider&) = delete;

  ~ScopedTraitOverrider();

  // Overrides the preferred content size category of the view controller with
  // `new_content_size_category` until this object is destroyed. To override
  // other traits, add a method per trait here and in
  // `ScopedTraitOverriderAppInterface`, and revert that trait in the
  // destructor: `traitOverrides` can't be replaced as a whole, so each trait is
  // overridden and reverted individually.
  void SetContentSizeCategory(UIContentSizeCategory new_content_size_category);

 private:
  // The content size category override of `top_view_controller_` when this
  // object was created, or `nil` if it had none.
  UIContentSizeCategory original_content_size_category_override_ = nil;
  // The view controller whose traits are overridden.
  UIViewController* top_view_controller_ = nil;
};

#endif  // IOS_CHROME_TEST_SCOPED_EG_TRAITS_OVERRIDER_H_
