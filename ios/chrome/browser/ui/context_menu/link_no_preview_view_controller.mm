// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/context_menu/link_no_preview_view_controller.h"

#import "ios/chrome/browser/ui/context_menu/link_no_preview_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LinkNoPreviewViewController ()

@property(nonatomic, copy) NSString* contextMenuTitle;
@property(nonatomic, copy) NSString* subtitle;
@property(nonatomic, strong) LinkNoPreviewView* view;

@end

@implementation LinkNoPreviewViewController

@dynamic view;

- (instancetype)initWithTitle:(NSString*)title subtitle:(NSString*)subtitle {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _contextMenuTitle = title;
    _subtitle = subtitle;
  }
  return self;
}

- (void)configureFaviconWithAttributes:(FaviconAttributes*)attributes {
  [self.view configureWithAttributes:attributes];
}

- (void)loadView {
  self.view = [[LinkNoPreviewView alloc] initWithTitle:self.contextMenuTitle
                                              subtitle:self.subtitle];
  self.view.backgroundColor = UIColor.systemBackgroundColor;
}

- (void)viewDidLayoutSubviews {
  self.preferredContentSize = [self computePreferredContentSize];
}

- (CGSize)computePreferredContentSize {
  CGFloat width = self.view.bounds.size.width;
  CGSize minimalSize =
      [self.view systemLayoutSizeFittingSize:CGSizeMake(width, 0)];
  return CGSizeMake(width, minimalSize.height);
}

@end
