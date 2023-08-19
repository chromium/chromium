// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_manage_devices_item.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/send_tab_to_self/send_tab_to_self_modal_delegate.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const CGFloat kAvatarSize = 24;

}  // namespace

#pragma mark - SendTabtoSelfManageDevicesCell

// Cell class for SendTabToSelfManageDevicesItem.
@interface SendTabtoSelfManageDevicesCell : TableViewCell

// A left-aligned round badge showing the account avatar.
@property(nonatomic, readonly, strong) UIImageView* avatarBadge;
// A view containing the account email and the link to the devices page.
@property(nonatomic, readonly, strong) UITextView* linkAndEmailTextView;

@end

@implementation SendTabtoSelfManageDevicesCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];

  if (!self) {
    return nil;
  }

  _avatarBadge = [[UIImageView alloc] init];
  _avatarBadge.translatesAutoresizingMaskIntoConstraints = NO;
  _avatarBadge.contentMode = UIViewContentModeScaleAspectFit;
  _avatarBadge.layer.cornerRadius = kAvatarSize / 2;
  _avatarBadge.clipsToBounds = YES;
  [self.contentView addSubview:_avatarBadge];

  _linkAndEmailTextView = CreateUITextViewWithTextKit1();
  _linkAndEmailTextView.translatesAutoresizingMaskIntoConstraints = NO;
  _linkAndEmailTextView.scrollEnabled = NO;
  _linkAndEmailTextView.editable = NO;
  _linkAndEmailTextView.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  _linkAndEmailTextView.textColor = [UIColor colorNamed:kTextSecondaryColor];
  _linkAndEmailTextView.backgroundColor = [UIColor clearColor];
  // Remove built-in padding.
  _linkAndEmailTextView.textContainer.lineFragmentPadding = 0;
  [_linkAndEmailTextView setTextContainerInset:UIEdgeInsetsZero];
  [self.contentView addSubview:_linkAndEmailTextView];

  [NSLayoutConstraint activateConstraints:@[
    [_avatarBadge.centerYAnchor
        constraintEqualToAnchor:self.contentView.centerYAnchor],
    [_avatarBadge.heightAnchor constraintEqualToConstant:kAvatarSize],
    [_avatarBadge.widthAnchor constraintEqualToConstant:kAvatarSize],
    [_avatarBadge.leftAnchor
        constraintEqualToAnchor:self.contentView.leftAnchor
                       constant:kTableViewHorizontalSpacing],
    [_avatarBadge.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                    constant:
                                        kTableViewTwoLabelsCellVerticalSpacing],
    [_linkAndEmailTextView.centerYAnchor
        constraintEqualToAnchor:_avatarBadge.centerYAnchor],
    [_linkAndEmailTextView.leftAnchor
        constraintEqualToAnchor:_avatarBadge.rightAnchor
                       constant:kTableViewImagePadding],
    [_linkAndEmailTextView.rightAnchor
        constraintEqualToAnchor:self.contentView.rightAnchor
                       constant:-kTableViewHorizontalSpacing],
    [_linkAndEmailTextView.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                    constant:
                                        kTableViewTwoLabelsCellVerticalSpacing]
  ]];

  return self;
}

@end

#pragma mark - SendTabToSelfManageDevicesItem

@interface SendTabToSelfManageDevicesItem () <UITextViewDelegate>
@end

@implementation SendTabToSelfManageDevicesItem

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  // The built-in click handling opens the link in the default browser app. If
  // that app isn't signed in to the Google Account, the user won't see the
  // correct page. So use a custom handling in the delegate to ensure Chrome
  // opens the page.
  DCHECK(self.delegate) << "Delegate not set";
  DCHECK(self.showManageDevicesLink);
  [self.delegate openManageDevicesTab];
  return NO;
}

#pragma mark - SendTabToSelfManageDevicesItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SendTabtoSelfManageDevicesCell class];
  }
  return self;
}

- (void)configureCell:(TableViewCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  SendTabtoSelfManageDevicesCell* accountCell =
      base::apple::ObjCCastStrict<SendTabtoSelfManageDevicesCell>(cell);

  if (self.showManageDevicesLink) {
    NSString* text =
        l10n_util::GetNSStringF(IDS_SEND_TAB_TO_SELF_MANAGE_DEVICES_LINK,
                                base::SysNSStringToUTF16(self.accountEmail));
    NSDictionary* textAttributes = @{
      NSFontAttributeName :
          [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
      NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
    };
    // Opening the link is handled by the delegate, so `NSLinkAttributeName`
    // can be arbitrary.
    NSDictionary* linkAttributes = @{NSLinkAttributeName : @""};
    accountCell.linkAndEmailTextView.attributedText =
        AttributedStringFromStringWithLink(text, textAttributes,
                                           linkAttributes);
    accountCell.linkAndEmailTextView.delegate = self;
  } else {
    accountCell.linkAndEmailTextView.text = self.accountEmail;
  }

  accountCell.avatarBadge.image = self.accountAvatar;
}

@end
