// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_utils.h"

#import "base/time/time.h"
#import "components/lens/lens_overlay_invocation_source.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/content_suggestions/most_visited_tiles/ui/most_visited_tiles_collection_view.h"
#import "ios/chrome/browser/first_run/public/first_run_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_constants.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_feature.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/ntp/ui_bundled/ntp_card_background_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#import "third_party/omnibox_proto/model_mode.pb.h"

namespace {
// Bottom padding between the MVT collection view and the bottom of its
// container.
constexpr CGFloat kMVTContainerBottomPadding = 10.0;
}  // namespace

bool ShouldShowTopOfFeedSyncPromo() {
  // Checks the flag and ensures that the user is not in first run.
  return IsDiscoverFeedTopSyncPromoEnabled() &&
         !ShouldPresentFirstRunExperience();
}

GURL GetUrlForAim(TemplateURLService* turl_service,
                  base::Time query_start_time) {
  return GetUrlForAim(turl_service,
                      omnibox::IOS_CHROME_NTP_FAKE_OMNIBOX_ENTRY_POINT,
                      query_start_time, /*query_text=*/u"",
                      lens::LensOverlayInvocationSource::kNtpContextualQuery,
                      /*additional_params=*/{});
}

UIButtonConfigurationUpdateHandler CreateThemedButtonConfigurationUpdateHandler(
    UIColor* unthemedTintColor,
    PaletteColorProvider paletteBackgroundColorProvider,
    UIBlurEffectStyle imageBlurEffectStyleOverride) {
  return ^(UIButton* updateButton) {
    UIButtonConfiguration* updateConfiguration = updateButton.configuration;
    if ([updateButton.traitCollection boolForNewTabPageImageBackgroundTrait]) {
      UIVisualEffect* blurEffect =
          [UIBlurEffect effectWithStyle:imageBlurEffectStyleOverride];
      UIVisualEffectView* blurBackgroundView =
          [[UIVisualEffectView alloc] initWithEffect:blurEffect];
      updateConfiguration.background.customView = blurBackgroundView;
      updateConfiguration.background.backgroundColor = UIColor.clearColor;

      updateConfiguration.baseForegroundColor =
          [UIColor colorNamed:kTextPrimaryColor];

      updateButton.configuration = updateConfiguration;
      return;
    }
    NewTabPageColorPalette* colorPalette =
        [updateButton.traitCollection objectForNewTabPageTrait];
    updateConfiguration.background.customView = nil;

    updateConfiguration.background.backgroundColor =
        paletteBackgroundColorProvider(colorPalette);

    updateConfiguration.baseForegroundColor =
        (colorPalette) ? colorPalette.tintColor : unthemedTintColor;

    updateButton.configuration = updateConfiguration;
  };
}

UIView* CreateMostVisitedContainerView(
    MostVisitedTilesCollectionView* collectionView,
    BOOL hasBackground) {
  if (!collectionView) {
    return nil;
  }

  UIView* container = [[UIView alloc] init];
  container.translatesAutoresizingMaskIntoConstraints = NO;

  if (hasBackground) {
    UIView* backgroundView = [[NTPCardBackgroundView alloc] init];
    backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:backgroundView];
    AddSameConstraints(container, backgroundView);
    container.layer.cornerRadius = kHomeModuleContainerCornerRadius;
    container.clipsToBounds = YES;
  }

  collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:collectionView];

  [NSLayoutConstraint activateConstraints:@[
    [collectionView.topAnchor constraintEqualToAnchor:container.topAnchor],
    [collectionView.leadingAnchor
        constraintEqualToAnchor:container.leadingAnchor],
    [collectionView.trailingAnchor
        constraintEqualToAnchor:container.trailingAnchor],
    [collectionView.bottomAnchor
        constraintEqualToAnchor:container.bottomAnchor
                       constant:-kMVTContainerBottomPadding],
  ]];

  return container;
}

CGFloat MostVisitedContainerHeight(UIView* containerView,
                                   UIView* mostVisitedView) {
  if (!containerView) {
    return 0.0;
  }
  CGFloat mvtHeight = CGRectGetHeight(containerView.bounds);
  if (mvtHeight <= 0 && mostVisitedView) {
    mvtHeight = [mostVisitedView
                    systemLayoutSizeFittingSize:UILayoutFittingCompressedSize]
                    .height;
  }
  return mvtHeight;
}
