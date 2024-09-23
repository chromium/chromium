// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/follow/ui_bundled/first_follow_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/follow/ui_bundled/followed_web_channel.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_container_view.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

constexpr CGFloat customSpacingBeforeImageIfNoNavigationBar = 24;
constexpr CGFloat customSpacingAfterImage = 24;

}  // namespace

@implementation FirstFollowViewController {
  std::u16string _webSiteTitle;
  BOOL _webSiteHasActiveContent;
  FirstFollowFaviconSource _faviconSource;
}

- (instancetype)initWithTitle:(NSString*)title
                       active:(BOOL)active
                faviconSource:(FirstFollowFaviconSource)faviconSource {
  if ((self = [super init])) {
    _webSiteTitle = base::SysNSStringToUTF16(title);
    _webSiteHasActiveContent = active;
    _faviconSource = faviconSource;
  }
  return self;
}

- (void)viewDidLoad {
  self.imageHasFixedSize = YES;
  self.imageEnclosedWithShadowAndBadge = YES;
  self.showDismissBarButton = NO;
  self.customSpacingBeforeImageIfNoNavigationBar =
      customSpacingBeforeImageIfNoNavigationBar;
  self.customSpacingAfterImage = customSpacingAfterImage;
  // With Follow UI update enabled, the longer text should be more compact.
  if (IsFollowUIUpdateEnabled()) {
    self.customSpacing = 0;
  }
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.topAlignedLayout = YES;

  self.titleString =
      l10n_util::GetNSStringF(IDS_IOS_FIRST_FOLLOW_TITLE, _webSiteTitle);
  self.subtitleString =
      IsFollowUIUpdateEnabled()
          ? l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_BODY_UPDATE)
          : l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_BODY);

  if (_webSiteHasActiveContent) {
    self.secondaryTitleString =
        IsFollowUIUpdateEnabled()
            ? l10n_util::GetNSStringF(IDS_IOS_FIRST_FOLLOW_SUBTITLE_UPDATE,
                                      _webSiteTitle)
            : l10n_util::GetNSStringF(IDS_IOS_FIRST_FOLLOW_SUBTITLE,
                                      _webSiteTitle);
    // Go To Feed button is only displayed if the web channel is available.
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_GO_TO_FOLLOWING);
    self.secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_GOT_IT);
  } else {
    self.secondaryTitleString =
        IsFollowUIUpdateEnabled()
            ? l10n_util::GetNSStringF(
                  IDS_IOS_FIRST_FOLLOW_SUBTITLE_NO_CONTENT_UPDATE,
                  _webSiteTitle)
            : l10n_util::GetNSStringF(IDS_IOS_FIRST_FOLLOW_SUBTITLE_NO_CONTENT,
                                      _webSiteTitle);
    // Only one button is visible, and it is a primary action button (with a
    // solid background color).
    self.primaryActionString =
        l10n_util::GetNSString(IDS_IOS_FIRST_FOLLOW_GOT_IT);
  }

  // TODO(crbug.com/40220465): Favicon styling needs more whitespace, shadow,
  // and corner green checkmark badge.
  if (_faviconSource) {
    __weak __typeof(self) weakSelf = self;
    _faviconSource(^(UIImage* favicon) {
      weakSelf.image = favicon;
    });
  }

  [super viewDidLoad];
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSecondaryTitle:(UITextView*)secondaryTitle {
  secondaryTitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  secondaryTitle.textColor = [UIColor colorNamed:kTextSecondaryColor];
}

- (void)customizeSubtitle:(UITextView*)subtitle {
  subtitle.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  subtitle.textColor = [UIColor colorNamed:kTextTertiaryColor];
}

@end
