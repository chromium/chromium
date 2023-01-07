// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_

#import <Foundation/Foundation.h>

#import <map>

#import "base/containers/small_map.h"
#import "third_party/abseil-cpp/absl/types/optional.h"

@class ImpressionLimit;

namespace promos_manager {
enum class Promo;
}  // namespace promos_manager

// Centralized promos manager for coordinating and scheduling the display of
// app-wide promos. Feature teams interested in displaying promos should
// leverage this manager, and only use the following methods:
// 1. RegisterPromoForSingleDisplay
// 2. RegisterPromoForContinuousDisplay
// 3. DeregisterPromo
class PromosManager {
 public:
  PromosManager();
  virtual ~PromosManager();

#pragma mark - Public-facing APIs

  // Registers `promo` for continuous display, and persists registration status
  // across app launches.
  virtual void RegisterPromoForContinuousDisplay(
      promos_manager::Promo promo) = 0;

  // Registers `promo` for single (one-time) display, and persists registration
  // status across app launches.
  virtual void RegisterPromoForSingleDisplay(promos_manager::Promo promo) = 0;

  // Deregisters `promo` (stopping `promo` from being displayed) by removing the
  // promo entry from the single-display and continuous-display active promos
  // lists.
  virtual void DeregisterPromo(promos_manager::Promo promo) = 0;

#pragma mark - Internal APIs

  // Initialize the Promos Manager by restoring state from Prefs. Must be called
  // after creation and before any other operation.
  virtual void Init();

  // Ingests promo-specific impression limits and stores them in-memory for
  // later reference.
  virtual void InitializePromoImpressionLimits(
      base::small_map<
          std::map<promos_manager::Promo, NSArray<ImpressionLimit*>*>>
          promo_impression_limits) = 0;

  // Records the impression of `promo` in the impression history.
  //
  // NOTE: If `promo` is a single-display promo, it will be automatically
  // deregistered.
  virtual void RecordImpression(promos_manager::Promo promo) = 0;

  // Returns the next promo for display, if any.
  virtual absl::optional<promos_manager::Promo> NextPromoForDisplay() const = 0;
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_
