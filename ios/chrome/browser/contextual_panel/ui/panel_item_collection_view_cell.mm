// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/ui/panel_item_collection_view_cell.h"

#import <optional>

#import "base/time/time.h"

@implementation PanelItemCollectionViewCell {
  std::optional<base::Time> _appearanceTime;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _appearanceTime = std::nullopt;
}

- (void)cellWillAppear {
  _appearanceTime = base::Time::Now();
}

- (void)cellDidDisappear {
  _appearanceTime = std::nullopt;
}

- (base::TimeDelta)timeSinceAppearance {
  if (_appearanceTime) {
    return base::Time::Now() - _appearanceTime.value();
  }
  return base::TimeDelta();
}

@end
