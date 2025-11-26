// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_constants.h"

namespace {

// The horizontal inset for the content within this container.
const CGFloat kContentHorizontalInset = 20.0f;

// The top inset for the content within this container.
const CGFloat kContentTopInset = 16.0f;

}  // namespace

NSString* const kMostVisitedSectionIdentifier = @"MostVisitedSectionIdentifier";
NSString* const kMagicStackSectionIdentifier = @"MagicStackSectionIdentifier";
NSString* const kMagicStackEditSectionIdentifier =
    @"MagicStackEditSectionIdentifier";

const int kMaxTextSizeForStyleFootnote = 24;

const NSDirectionalEdgeInsets kMagicStackContainerInsets = {
    kContentTopInset, kContentHorizontalInset, 0, kContentHorizontalInset};

const CGFloat kMagicStackSpacing = 12.0f;

const CGFloat kMagicStackPeekInset = kMagicStackSpacing + 10;
const CGFloat kMagicStackPeekInsetLandscape = kMagicStackSpacing * 2 + 28;

const float kMagicStackMinimumPaginationScrollVelocity = 0.2f;

const float kMagicStackEditButtonWidth = 61;
const float kMagicStackEditButtonIconPointSize = 22;

const float kMagicStackEditButtonMargin = 32;

NSString* const kMagicStackEditButtonContainerAccessibilityIdentifier =
    @"MagicStackEditButtonContainerAccessibilityIdentifier";

NSString* const kMagicStackEditButtonAccessibilityIdentifier =
    @"MagicStackEditButtonAccessibilityIdentifier";
