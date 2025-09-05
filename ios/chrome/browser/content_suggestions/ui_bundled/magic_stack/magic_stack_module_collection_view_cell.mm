// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module_collection_view_cell.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_collection_utils.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/content_suggestions_constants.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_context_menu_interaction_handler.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_layout_attributes.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module_background_view.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module_container.h"
#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_module_container_delegate.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// The corner radius of this container.
const float kCornerRadius = 24;

}  // namespace

@interface MagicStackModuleCollectionViewCell ()

@property(nonatomic, assign) ContentSuggestionsModuleType type;

@end

@implementation MagicStackModuleCollectionViewCell {
  // Container that holds the module contents.
  MagicStackModuleContainer* _moduleContainer;
  // Context menu interaction for cell interactions.
  UIContextMenuInteraction* _contextMenuInteraction;
  // Background view to show the proper colored vs blur effect background.
  MagicStackModuleBackgroundView* _moduleBackgroundView;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _moduleBackgroundView = [[MagicStackModuleBackgroundView alloc] init];
    self.backgroundView = _moduleBackgroundView;
    self.layer.cornerRadius = kCornerRadius;
    self.clipsToBounds = YES;

    _moduleContainer =
        [[MagicStackModuleContainer alloc] initWithFrame:CGRectZero];
    _moduleContainer.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_moduleContainer];
    AddSameConstraints(_moduleContainer, self);
  }
  return self;
}

- (void)configureWithConfig:(MagicStackModule*)config {
  if (_type == config.type) {
    return;
  }
  _type = config.type;
  [_moduleContainer configureWithConfig:config];
  if (!_contextMenuInteraction) {
    _contextMenuInteraction = [[UIContextMenuInteraction alloc]
        initWithDelegate:_moduleContainer.contextMenuInteractionHandler];
    [self addInteraction:_contextMenuInteraction];
  }
}

#pragma mark - Setters

- (void)setDelegate:(id<MagicStackModuleContainerDelegate>)delegate {
  _moduleContainer.delegate = delegate;
  _delegate = delegate;
}

#pragma mark - UICollectionViewCell Overrides

- (void)prepareForReuse {
  [super prepareForReuse];
  _type = ContentSuggestionsModuleType::kInvalid;
  if (_contextMenuInteraction) {
    [_contextMenuInteraction dismissMenu];
    [self removeInteraction:_contextMenuInteraction];
    _contextMenuInteraction = nil;
  }
  [_moduleContainer resetView];
}

- (void)applyLayoutAttributes:
    (UICollectionViewLayoutAttributes*)layoutAttributes {
  MagicStackLayoutAttributes* attributes =
      base::apple::ObjCCast<MagicStackLayoutAttributes>(layoutAttributes);

  self.contentView.alpha = attributes.subviewAlpha;
  if (attributes.subviewAlpha == 1) {
    [_moduleBackgroundView fadeIn];
  } else {
    [_moduleBackgroundView fadeOut];
  }
  [super applyLayoutAttributes:attributes];
}

@end
