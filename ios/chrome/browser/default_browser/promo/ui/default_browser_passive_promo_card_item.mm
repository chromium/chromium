// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/default_browser/promo/ui/default_browser_passive_promo_card_item.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/default_browser/promo/ui/default_browser_passive_card_view_delegate.h"
#import "ios/chrome/browser/default_browser/promo/ui/default_browser_passive_promo_card_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation DefaultBrowserPassivePromoCardItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [DefaultBrowserPassivePromoCardCell class];
  }
  return self;
}

- (void)configureCell:(LegacyTableViewCell*)cell {
  [super configureCell:cell];
  DefaultBrowserPassivePromoCardCell* cardCell =
      base::apple::ObjCCastStrict<DefaultBrowserPassivePromoCardCell>(cell);
  cardCell.target = self.target;
  cardCell.closeAction = self.closeAction;
  cardCell.primaryAction = self.primaryAction;
}

@end

@interface DefaultBrowserPassivePromoCardCell () <
    DefaultBrowserPassivePromoCardViewDelegate>
@end

@implementation DefaultBrowserPassivePromoCardCell {
  DefaultBrowserPassivePromoCardView* _cardView;
}

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.selectionStyle = UITableViewCellSelectionStyleNone;

    _cardView =
        [[DefaultBrowserPassivePromoCardView alloc] initWithFrame:CGRectZero];
    _cardView.delegate = self;
    [self.contentView addSubview:_cardView];

    self.contentView.clipsToBounds = YES;

    AddSameConstraintsToSides(_cardView, self.contentView,
                              LayoutSides::kTop | LayoutSides::kLeading |
                                  LayoutSides::kTrailing |
                                  LayoutSides::kBottom);
  }
  return self;
}

#pragma mark - DefaultBrowserPassivePromoCardViewDelegate

- (void)didTapCloseInDefaultBrowserPassivePromoCardView:
    (DefaultBrowserPassivePromoCardView*)view {
  if (self.target && self.closeAction) {
    [[UIApplication sharedApplication] sendAction:self.closeAction
                                               to:self.target
                                             from:self
                                         forEvent:nil];
  }
}

- (void)didTapActionInDefaultBrowserPassivePromoCardView:
    (DefaultBrowserPassivePromoCardView*)view {
  if (self.target && self.primaryAction) {
    [[UIApplication sharedApplication] sendAction:self.primaryAction
                                               to:self.target
                                             from:self
                                         forEvent:nil];
  }
}

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.target = nil;
  self.closeAction = nil;
  self.primaryAction = nil;
}

@end
