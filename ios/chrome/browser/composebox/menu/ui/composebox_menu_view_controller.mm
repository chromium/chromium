// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_view_controller.h"

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_attachment_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The height of the attachments stack view.
const CGFloat kAttachmentStackViewHeight = 80.0f;

// The spacing of the attachments stack view.
const CGFloat kAttachmentStackViewSpacing = 6.0f;

// Insets for the safe area (top, left, bottom, right).
const UIEdgeInsets kSafeAreaInsets = {20.0, 15.0, 0.0, 15.0};

}  // namespace

@implementation ComposeboxMenuViewController {
  // The stack view containing the attachments
  UIStackView* _attachmentStackView;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  [self setAdditionalSafeAreaInsets:kSafeAreaInsets];
  self.view.backgroundColor = [UIColor colorNamed:kGrey100Color];
  [self setUpAttachmentStackViewWithItems:[self availableAttachmentViews]];
}

#pragma mark - Private

// Returns the available attachment views.
- (NSArray<UIView*>*)availableAttachmentViews {
  UIView* tabsAttachment =
      [self attachmentViewWithTitle:l10n_util::GetNSString(
                                        IDS_IOS_COMPOSEBOX_SELECT_TAB_ACTION)
                             symbol:DefaultSymbolWithPointSize(
                                        kNewTabGroupActionSymbol,
                                        kSymbolActionPointSize)];
  UIView* cameraAttachment = [self
      attachmentViewWithTitle:l10n_util::GetNSString(
                                  IDS_IOS_COMPOSEBOX_CAMERA_ACTION)
                       symbol:DefaultSymbolWithPointSize(
                                  kSystemCameraSymbol, kSymbolActionPointSize)];
  UIView* galleryAttachment =
      [self attachmentViewWithTitle:l10n_util::GetNSString(
                                        IDS_IOS_COMPOSEBOX_GALLERY_ACTION)
                             symbol:DefaultSymbolWithPointSize(
                                        kPhotoOnRectangleAngled,
                                        kSymbolActionPointSize)];
  UIView* filesAttachment =
      [self attachmentViewWithTitle:l10n_util::GetNSString(
                                        IDS_IOS_COMPOSEBOX_FILES_ACTION)
                             symbol:DefaultSymbolWithPointSize(
                                        kFolderSymbol, kSymbolActionPointSize)];

  return
      @[ tabsAttachment, cameraAttachment, galleryAttachment, filesAttachment ];
}

// Sets up the container for the attachment options.
- (void)setUpAttachmentStackViewWithItems:(NSArray<UIView*>*)items {
  _attachmentStackView = [[UIStackView alloc] initWithArrangedSubviews:items];
  _attachmentStackView.translatesAutoresizingMaskIntoConstraints = NO;
  _attachmentStackView.distribution = UIStackViewDistributionFillEqually;
  _attachmentStackView.alignment = UIStackViewAlignmentFill;
  _attachmentStackView.axis = UILayoutConstraintAxisHorizontal;
  _attachmentStackView.spacing = kAttachmentStackViewSpacing;

  [self.view addSubview:_attachmentStackView];

  [NSLayoutConstraint activateConstraints:@[
    [_attachmentStackView.heightAnchor
        constraintEqualToConstant:kAttachmentStackViewHeight],
    [_attachmentStackView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor],
    [_attachmentStackView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor],
    [_attachmentStackView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
  ]];
}

// Create an attachment view with the given title and symbol.
- (ComposeboxMenuAttachmentView*)attachmentViewWithTitle:(NSString*)title
                                                  symbol:(UIImage*)symbol {
  ComposeboxMenuAttachmentView* attachmentView =
      [[ComposeboxMenuAttachmentView alloc] init];
  attachmentView.translatesAutoresizingMaskIntoConstraints = NO;
  attachmentView.title = title;
  attachmentView.image =
      SymbolWithPalette(symbol, @[ [UIColor colorNamed:kTextPrimaryColor] ]);
  return attachmentView;
}

@end
