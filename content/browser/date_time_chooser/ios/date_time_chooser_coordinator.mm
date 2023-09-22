// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/date_time_chooser/ios/date_time_chooser_coordinator.h"

#import "content/browser/date_time_chooser/ios/date_time_chooser_mediator.h"
#import "content/browser/date_time_chooser/ios/date_time_chooser_view_controller.h"
#import "ui/gfx/native_widget_types.h"

@interface DateTimeChooserCoordinator ()
// The controller that has UI components.
@property(nonatomic, strong)
    DateTimeChooserViewController* dateTimeChooserController;

// The mediator that has DateTimeChooser.
@property(nonatomic, strong) DateTimeChooserMediator* dateTimeChooserMediator;
@end

@implementation DateTimeChooserCoordinator

- (instancetype)initWithDateTimeChooser:(content::DateTimeChooserIOS*)chooser
                                configs:(DateTimeDialogValuePtr)configs
                     baseViewController:(UIViewController*)baseViewController {
  if (!(self = [super init])) {
    return nil;
  }
  _dateTimeChooserMediator =
      [[DateTimeChooserMediator alloc] initWithDateTimeChooser:chooser];

  _dateTimeChooserController = [[DateTimeChooserViewController alloc]
      initWithConfigs:std::move(configs)];
  _dateTimeChooserController.delegate = _dateTimeChooserMediator;
  _dateTimeChooserController.view.backgroundColor = [UIColor whiteColor];
  _dateTimeChooserController.modalInPresentation = true;
  _dateTimeChooserController.modalPresentationStyle =
      UIModalPresentationPopover;
  _dateTimeChooserController.popoverPresentationController.delegate =
      _dateTimeChooserController;
  _dateTimeChooserController.popoverPresentationController.sourceView =
      baseViewController.view;
  _dateTimeChooserController.popoverPresentationController.sourceRect =
      baseViewController.view.bounds;

  [baseViewController presentViewController:_dateTimeChooserController
                                   animated:true
                                 completion:nil];
  return self;
}

@end
