// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import <MaterialComponents/MaterialTypography.h>

#include "base/strings/sys_string_conversions.h"
#include "remoting/base/string_resources.h"
#import "remoting/ios/app/host_collection_view_cell.h"
#import "remoting/ios/app/remoting_theme.h"
#import "remoting/ios/app/view_utils.h"
#import "remoting/ios/domain/host_info.h"
#include "ui/base/l10n/l10n_util.h"

static const CGFloat kLinePadding = 2.f;
static const CGFloat kHostCardIconInset = 10.f;
static const CGFloat kHostCardPadding = 4.f;
static const CGFloat kHostCardIconSize = 45.f;
static const CGFloat kTitleOpacity = 0.87f;
static const CGFloat kCaptionOpacity = 0.54f;

static NSString* const kSuccessExitOfflineReason = @"SUCCESS_EXIT";

// Maps an offline reason enum string to the l10n ID used to retrieve the
// localized message.
static NSDictionary<NSString*, NSNumber*>* const kOfflineReasonL10nId = @{
  @"INITIALIZATION_FAILED" : @(IDS_OFFLINE_REASON_INITIALIZATION_FAILED),
  @"INVALID_HOST_CONFIGURATION" :
      @(IDS_OFFLINE_REASON_INVALID_HOST_CONFIGURATION),
  @"INVALID_HOST_ID" : @(IDS_OFFLINE_REASON_INVALID_HOST_ID),
  @"INVALID_OAUTH_CREDENTIALS" :
      @(IDS_OFFLINE_REASON_INVALID_OAUTH_CREDENTIALS),
  @"INVALID_HOST_DOMAIN" : @(IDS_OFFLINE_REASON_INVALID_HOST_DOMAIN),
  @"LOGIN_SCREEN_NOT_SUPPORTED" :
      @(IDS_OFFLINE_REASON_LOGIN_SCREEN_NOT_SUPPORTED),
  @"POLICY_READ_ERROR" : @(IDS_OFFLINE_REASON_POLICY_READ_ERROR),
  @"POLICY_CHANGE_REQUIRES_RESTART" :
      @(IDS_OFFLINE_REASON_POLICY_CHANGE_REQUIRES_RESTART),
  @"USERNAME_MISMATCH" : @(IDS_OFFLINE_REASON_USERNAME_MISMATCH),
  @"X_SERVER_RETRIES_EXCEEDED" :
      @(IDS_OFFLINE_REASON_X_SERVER_RETRIES_EXCEEDED),
  @"SESSION_RETRIES_EXCEEDED" : @(IDS_OFFLINE_REASON_SESSION_RETRIES_EXCEEDED),
  @"HOST_RETRIES_EXCEEDED" : @(IDS_OFFLINE_REASON_HOST_RETRIES_EXCEEDED),
};

@interface HostCollectionViewCell () {
  UIImageView* _imageView;
  UILabel* _statusLabel;
  UILabel* _titleLabel;
  UIView* _labelView;
}
@end

//
// This is the implementation of the info card for a host's status shown in
// the host list. This will also be the selection for which host to connect
// to and other managements actions for a host in this list.
//
@implementation HostCollectionViewCell

@synthesize hostInfo = _hostInfo;

- (id)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor = [UIColor clearColor];
    [self commonInit];
  }
  return self;
}

- (void)commonInit {
  self.isAccessibilityElement = YES;

  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  _imageView.contentMode = UIViewContentModeCenter;
  _imageView.backgroundColor = RemotingTheme.hostOfflineColor;
  _imageView.layer.cornerRadius = kHostCardIconSize / 2.f;
  _imageView.layer.masksToBounds = YES;
  [self.contentView addSubview:_imageView];

  // Holds both of the labels.
  _labelView = [[UIView alloc] init];
  _labelView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:_labelView];

  _titleLabel = [[UILabel alloc] init];
  _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  UIFont* subheadFont = MDCTypography.subheadFont;
  UIFontDescriptor* subheadFontDescriptor = [subheadFont.fontDescriptor
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  subheadFontDescriptor = subheadFontDescriptor ?: subheadFont.fontDescriptor;
  UIFont* boldSubheadFont = [UIFont fontWithDescriptor:subheadFontDescriptor
                                                  size:subheadFont.pointSize];

  _titleLabel.font = boldSubheadFont;
  _titleLabel.alpha = kTitleOpacity;
  _titleLabel.textColor = RemotingTheme.hostCellTitleColor;
  [_labelView addSubview:_titleLabel];

  _statusLabel = [[UILabel alloc] init];
  _statusLabel.translatesAutoresizingMaskIntoConstraints = NO;
  _statusLabel.font = MDCTypography.captionFont;
  _statusLabel.alpha = kCaptionOpacity;
  _statusLabel.textColor = RemotingTheme.hostCellStatusTextColor;
  [_labelView addSubview:_statusLabel];

  UILayoutGuide* safeAreaLayoutGuide =
      remoting::SafeAreaLayoutGuideForView(self);

  // Constraints
  NSArray* constraints = @[
    // +------------+---------------+
    // | +--------+ |               |
    // | |        | | [Host Name]   |
    // | |  Icon  | | - - - - - - - | <- Center Y
    // | |        | | [Host Status] |
    // | +---^----+ |               |
    // +-----|------+-------^-------+
    //       |              |
    //  Image View     Label View
    [[_imageView leadingAnchor]
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                       constant:kHostCardIconInset],
    [[_imageView centerYAnchor]
        constraintEqualToAnchor:safeAreaLayoutGuide.centerYAnchor],
    [[_imageView widthAnchor] constraintEqualToConstant:kHostCardIconSize],
    [[_imageView heightAnchor] constraintEqualToConstant:kHostCardIconSize],

    [[_labelView leadingAnchor]
        constraintEqualToAnchor:[_imageView trailingAnchor]
                       constant:kHostCardIconInset],
    [[_labelView trailingAnchor]
        constraintEqualToAnchor:safeAreaLayoutGuide.trailingAnchor
                       constant:-kHostCardPadding / 2.f],
    [[_labelView topAnchor]
        constraintEqualToAnchor:safeAreaLayoutGuide.topAnchor],
    [[_labelView bottomAnchor]
        constraintEqualToAnchor:safeAreaLayoutGuide.bottomAnchor],

    // Put titleLabel and statusLabel symmetrically around centerY.
    [[_titleLabel leadingAnchor]
        constraintEqualToAnchor:[_labelView leadingAnchor]],
    [[_titleLabel trailingAnchor]
        constraintEqualToAnchor:[_labelView trailingAnchor]],
    [[_titleLabel bottomAnchor]
        constraintEqualToAnchor:[_labelView centerYAnchor]],

    [[_statusLabel leadingAnchor]
        constraintEqualToAnchor:[_labelView leadingAnchor]],
    [[_statusLabel trailingAnchor]
        constraintEqualToAnchor:[_labelView trailingAnchor]],
    [[_statusLabel topAnchor] constraintEqualToAnchor:[_labelView centerYAnchor]
                                             constant:kLinePadding],
  ];

  [NSLayoutConstraint activateConstraints:constraints];
}

#pragma mark - HostCollectionViewCell Public

- (void)populateContentWithHostInfo:(HostInfo*)hostInfo {
  _hostInfo = hostInfo;

  _titleLabel.text = _hostInfo.hostName;

  _imageView.image = RemotingTheme.desktopIcon;

  if (_hostInfo.isOnline) {
    _imageView.backgroundColor = RemotingTheme.hostOnlineColor;
    _statusLabel.text = l10n_util::GetNSString(IDS_HOST_ONLINE_SUBTITLE);
  } else {
    NSString* statusText =
        hostInfo.updatedTime
            ? l10n_util::GetNSStringF(
                  IDS_LAST_ONLINE_SUBTITLE,
                  base::SysNSStringToUTF16(hostInfo.updatedTime))
            : l10n_util::GetNSString(IDS_HOST_OFFLINE_SUBTITLE);
    NSString* localizedOfflineReason = nil;
    if (hostInfo.offlineReason.length > 0 &&
        ![hostInfo.offlineReason isEqualToString:kSuccessExitOfflineReason]) {
      NSNumber* offlineReasonId = kOfflineReasonL10nId[hostInfo.offlineReason];
      if (offlineReasonId) {
        localizedOfflineReason =
            l10n_util::GetNSString(offlineReasonId.intValue);
      } else {
        localizedOfflineReason = l10n_util::GetNSStringF(
            IDS_OFFLINE_REASON_UNKNOWN,
            base::SysNSStringToUTF16(hostInfo.offlineReason));
      }
    }

    if (localizedOfflineReason) {
      _imageView.backgroundColor = RemotingTheme.hostWarningColor;
      _statusLabel.text = [NSString
          stringWithFormat:@"%@ %@", localizedOfflineReason, statusText];
    } else {
      _imageView.backgroundColor = RemotingTheme.hostOfflineColor;
      _statusLabel.text = statusText;
    }
  }

  self.accessibilityLabel = [NSString
      stringWithFormat:@"%@\n%@", _titleLabel.text, _statusLabel.text];
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  _hostInfo = nil;
  _statusLabel.text = nil;
  _titleLabel.text = nil;
  self.accessibilityLabel = nil;
}

@end
