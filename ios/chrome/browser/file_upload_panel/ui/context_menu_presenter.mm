// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/file_upload_panel/ui/context_menu_presenter.h"

@interface ContextMenuPresenterButton : UIButton
@property(nonatomic, weak) id<UIContextMenuInteractionDelegate>
    contextMenuInteractionDelegate;
@end

@implementation ContextMenuPresenterButton

- (UIContextMenuConfiguration*)contextMenuInteraction:
                                   (UIContextMenuInteraction*)interaction
                       configurationForMenuAtLocation:(CGPoint)location {
  if ([_contextMenuInteractionDelegate
          respondsToSelector:@selector(contextMenuInteraction:
                                 configurationForMenuAtLocation:)]) {
    return [_contextMenuInteractionDelegate contextMenuInteraction:interaction
                                    configurationForMenuAtLocation:location];
  }

  return [super contextMenuInteraction:interaction
        configurationForMenuAtLocation:location];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
    willDisplayMenuForConfiguration:(UIContextMenuConfiguration*)configuration
                           animator:
                               (id<UIContextMenuInteractionAnimating>)animator {
  [super contextMenuInteraction:interaction
      willDisplayMenuForConfiguration:configuration
                             animator:animator];

  if ([_contextMenuInteractionDelegate
          respondsToSelector:@selector(contextMenuInteraction:
                                 willDisplayMenuForConfiguration:animator:)]) {
    [_contextMenuInteractionDelegate contextMenuInteraction:interaction
                            willDisplayMenuForConfiguration:configuration
                                                   animator:animator];
  }
}

- (void)contextMenuInteraction:(UIContextMenuInteraction*)interaction
       willEndForConfiguration:(UIContextMenuConfiguration*)configuration
                      animator:(id<UIContextMenuInteractionAnimating>)animator {
  [super contextMenuInteraction:interaction
        willEndForConfiguration:configuration
                       animator:animator];

  if ([_contextMenuInteractionDelegate
          respondsToSelector:@selector(contextMenuInteraction:
                                      willEndForConfiguration:animator:)]) {
    [_contextMenuInteractionDelegate contextMenuInteraction:interaction
                                    willEndForConfiguration:configuration
                                                   animator:animator];
  }
}

@end

@implementation ContextMenuPresenter {
  __weak UIView* _rootView;
  ContextMenuPresenterButton* _button;
}

@dynamic contextMenuInteractionDelegate;

- (instancetype)initWithRootView:(UIView*)rootView {
  self = [super init];
  if (self) {
    _rootView = rootView;
    _button = [ContextMenuPresenterButton buttonWithType:UIButtonTypeSystem];
    _button.hidden = YES;
    _button.userInteractionEnabled = NO;
    _button.contextMenuInteractionEnabled = YES;
    _button.showsMenuAsPrimaryAction = YES;
  }
  return self;
}

#pragma mark - Public properties

- (void)setContextMenuInteractionDelegate:
    (id<UIContextMenuInteractionDelegate>)contextMenuInteractionDelegate {
  _button.contextMenuInteractionDelegate = contextMenuInteractionDelegate;
}

- (id<UIContextMenuInteractionDelegate>)contextMenuInteractionDelegate {
  return _button.contextMenuInteractionDelegate;
}

- (UIContextMenuInteraction*)contextMenuInteraction {
  return _button.contextMenuInteraction;
}

#pragma mark - Public

- (void)presentAtLocationInRootView:(CGPoint)locationInRootView {
  if (!_rootView.window) {
    return;
  }
  _button.frame = CGRectMake(locationInRootView.x, locationInRootView.y, 0, 0);
  if (!_button.superview) {
    [_rootView addSubview:_button];
  }
  [_button performPrimaryAction];
}

- (void)dismiss {
  [_button.contextMenuInteraction dismissMenu];
  [_button removeFromSuperview];
}

@end
