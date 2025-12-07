// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TIPS_MANAGER_MODEL_TIPS_MANAGER_IOS_H_
#define IOS_CHROME_BROWSER_TIPS_MANAGER_MODEL_TIPS_MANAGER_IOS_H_

#import "components/segmentation_platform/embedder/home_modules/tips_manager/tips_manager.h"

class PrefService;
namespace segmentation_platform {
enum class TipIdentifier;
enum class TipPresentationContext;
}  // namespace segmentation_platform

// IOS implementation of `segmentation_platform::TipsManager`.
// `TipsManagerIOS` is responsible for managing and
// coordinating in-product tips within Chrome iOS.
class TipsManagerIOS : public segmentation_platform::TipsManager {
 public:
  // Constructor.
  explicit TipsManagerIOS(PrefService* pref_service,
                          PrefService* local_pref_service)
      : segmentation_platform::TipsManager(pref_service, local_pref_service) {}

  TipsManagerIOS(const TipsManagerIOS&) = delete;
  TipsManagerIOS& operator=(const TipsManagerIOS&) = delete;

  ~TipsManagerIOS() override = default;

  // `segmentation_platform::TipsManager` override.
  // This method is called when a user interacts with a `tip` in the given
  // `context`.
  void HandleInteraction(
      segmentation_platform::TipIdentifier tip,
      segmentation_platform::TipPresentationContext context) override;
};

#endif  // IOS_CHROME_BROWSER_TIPS_MANAGER_MODEL_TIPS_MANAGER_IOS_H_
