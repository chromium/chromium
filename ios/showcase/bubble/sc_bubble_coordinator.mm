// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/bubble/sc_bubble_coordinator.h"

#import "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCBubbleCoordinator ()
@property(nonatomic, strong) UIViewController* containerViewController;
@property(nonatomic, strong) BubbleViewController* bubbleViewController;
@end

@implementation SCBubbleCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize containerViewController = _containerViewController;
@synthesize bubbleViewController = _bubbleViewController;

- (void)start {
  self.containerViewController = [[UIViewController alloc] init];
  UIView* containerView = self.containerViewController.view;
  containerView.backgroundColor = [UIColor whiteColor];
  self.containerViewController.title = @"Bubble";

  BubbleArrowDirection direction = BubbleArrowDirectionUp;
  BubbleAlignment alignment = BubbleAlignmentTrailing;
  CGFloat bubbleAlignmentOffset = bubble_util::BubbleDefaultAlignmentOffset();
  self.bubbleViewController =
      [[BubbleViewController alloc] initWithText:@"Lorem ipsum dolor"
                                           title:nil
                                           image:nil
                                  arrowDirection:direction
                                       alignment:alignment
                                  bubbleViewType:BubbleViewTypeDefault
                                        delegate:nil];

  // Mock UI element for the bubble to be anchored on. Set the x-coordinate of
  // the origin to be two-thirds of the container's width.
  UIView* elementView = [[UIView alloc]
      initWithFrame:CGRectMake(containerView.frame.size.width * 2.0f / 3.0f,
                               20.0f, 20.0f, 20.0f)];
  elementView.backgroundColor = [UIColor grayColor];
  [containerView addSubview:elementView];
  CGPoint anchorPoint = bubble_util::AnchorPoint(elementView.frame, direction);
  // Maximum width of the bubble such that it stays within |containerView|.
  CGSize maxBubbleSize =
      bubble_util::BubbleMaxSize(anchorPoint, bubbleAlignmentOffset, direction,
                                 alignment, containerView.frame.size);

  CGSize bubbleSize =
      [self.bubbleViewController.view sizeThatFits:maxBubbleSize];
  CGRect bubbleFrame = bubble_util::BubbleFrame(
      anchorPoint, bubbleAlignmentOffset, bubbleSize, direction, alignment,
      containerView.frame.size.width);

  [self addBubbleViewControllerWithFrame:bubbleFrame];
  [self.baseViewController pushViewController:self.containerViewController
                                     animated:YES];
}

#pragma mark - Private methods

// Add |bubbleViewController| as a child view controller of the container and
// set its frame.
- (void)addBubbleViewControllerWithFrame:(CGRect)bubbleFrame {
  [self.containerViewController
      addChildViewController:self.bubbleViewController];
  self.bubbleViewController.view.frame = bubbleFrame;
  [self.containerViewController.view addSubview:self.bubbleViewController.view];
  [self.bubbleViewController
      didMoveToParentViewController:self.containerViewController];
  [self.bubbleViewController animateContentIn];
}

@end
