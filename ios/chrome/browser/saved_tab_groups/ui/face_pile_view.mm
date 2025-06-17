// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/ui/face_pile_view.h"

#import "ios/chrome/browser/share_kit/model/share_kit_avatar_primitive.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation FacePileView {
  // Array of avatar primitive objects, each representing a face in the pile.
  NSArray<id<ShareKitAvatarPrimitive>>* _avatars;
  // A UIStackView to arrange and display the individual avatar views.
  UIStackView* _facesStackView;
  // Label to display text when the face pile is empty.
  UILabel* _emptyFacePileLabel;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _facesStackView = [[UIStackView alloc] init];
    _facesStackView.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_facesStackView];
    AddSameConstraints(self, _facesStackView);
  }
  return self;
}

#pragma mark - FacePileConsumer

- (void)setShowsTextWhenEmpty:(BOOL)showsTextWhenEmpty {
  if (showsTextWhenEmpty) {
    [self addEmptyFacePileLabel];
    _emptyFacePileLabel.hidden = ![self isEmpty];
  } else {
    [_emptyFacePileLabel removeFromSuperview];
    _emptyFacePileLabel = nil;
  }
}

- (void)updateWithFaces:(NSArray<id<ShareKitAvatarPrimitive>>*)faces
            totalNumber:(NSInteger)totalNumber {
  // Remove all existing avatar views from the stack.
  for (UIView* view in _facesStackView.arrangedSubviews) {
    [view removeFromSuperview];
  }
  _avatars = faces;
  for (id<ShareKitAvatarPrimitive> avatar in _avatars) {
    [avatar resolve];
    // TODO(crbug.com/422737259):
    // - have the correct layout with overlap.
    // - Add accessibility container + identifier.
    [_facesStackView addArrangedSubview:[avatar view]];
  }
  _emptyFacePileLabel.hidden = ![self isEmpty];
  // TODO(crbug.com/422737259): Add the "+X" label.
}

#pragma mark - Private

// Adds and configures the `_emptyFacePileLabel`.
- (void)addEmptyFacePileLabel {
  _emptyFacePileLabel = [[UILabel alloc] init];
  _emptyFacePileLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:_emptyFacePileLabel];
  AddSameConstraints(self, _emptyFacePileLabel);
  _emptyFacePileLabel.text =
      l10n_util::GetNSString(IDS_IOS_SHARED_GROUP_SHARE_GROUP_BUTTON_TEXT);
}

// Returns whether this facepile is empty.
- (BOOL)isEmpty {
  return _facesStackView.arrangedSubviews.count == 0;
}

@end
