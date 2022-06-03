// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/selector_picker_view_controller.h"

#include "base/check_op.h"
#import "base/mac/foundation_util.h"
#include "base/notreached.h"
#import "ios/chrome/browser/ui/elements/selector_view_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Font size of text in the picker view.
CGFloat kUIPickerFontSize = 26;
}  // namespace

@interface SelectorPickerViewController ()<UIPickerViewDelegate,
                                           UIPickerViewDataSource> {
  __weak id<SelectorViewControllerDelegate> _delegate;
}
// Options to display.
@property(nonatomic, copy) NSOrderedSet<NSString*>* options;
// The default option.
@property(nonatomic, copy) NSString* defaultOption;
// The displayed UINavigationBar. Exposed for testing.
@property(nonatomic, strong) UINavigationBar* navigationBar;
// The displayed UIPickerView. Exposed for testing.
@property(nonatomic, strong) UIPickerView* pickerView;
// Action for the "Done" button.
- (void)doneButtonPressed;
// Action for the "Cancel" button.
- (void)cancelButtonPressed;
@end

@implementation SelectorPickerViewController

@synthesize pickerView = _pickerView;
@synthesize navigationBar = _navigationBar;

@synthesize options = _options;
@synthesize defaultOption = _defaultOption;

- (instancetype)initWithOptions:(NSOrderedSet<NSString*>*)options
                        default:(NSString*)defaultOption {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _defaultOption = [defaultOption copy];
    _options = [options copy];
  }
  return self;
}

- (instancetype)initWithNibName:(NSString*)nibName bundle:(NSBundle*)nibBundle {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NOTREACHED();
  return nil;
}

- (void)loadView {
  self.pickerView = [[UIPickerView alloc] initWithFrame:CGRectZero];
  self.navigationBar = [[UINavigationBar alloc] initWithFrame:CGRectZero];
  self.pickerView.translatesAutoresizingMaskIntoConstraints = NO;
  self.navigationBar.translatesAutoresizingMaskIntoConstraints = NO;
  UIStackView* stackView = [[UIStackView alloc]
      initWithArrangedSubviews:@[ self.navigationBar, self.pickerView ]];
  stackView.axis = UILayoutConstraintAxisVertical;
  self.view = stackView;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor whiteColor];

  self.pickerView.backgroundColor = [UIColor whiteColor];
  self.pickerView.delegate = self;
  self.pickerView.dataSource = self;
  NSInteger initialIndex = [self.options indexOfObject:self.defaultOption];
  if (initialIndex != NSNotFound) {
    [self.pickerView selectRow:initialIndex inComponent:0 animated:NO];
  }

  UINavigationItem* navigationItem =
      [[UINavigationItem alloc] initWithTitle:@""];
  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(doneButtonPressed)];
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonPressed)];
  [navigationItem setRightBarButtonItem:doneButton];
  [navigationItem setLeftBarButtonItem:cancelButton];
  [navigationItem setHidesBackButton:YES];
  [self.navigationBar pushNavigationItem:navigationItem animated:NO];
}

- (id<SelectorViewControllerDelegate>)delegate {
  return _delegate;
}

- (void)setDelegate:(id<SelectorViewControllerDelegate>)delegate {
  _delegate = delegate;
}

#pragma mark UIPickerViewDataSource

- (NSInteger)numberOfComponentsInPickerView:(UIPickerView*)pickerView {
  return 1;
}

- (NSInteger)pickerView:(UIPickerView*)pickerView
    numberOfRowsInComponent:(NSInteger)component {
  return [self.options count];
}

#pragma mark UIPickerViewDelegate

- (UIView*)pickerView:(UIPickerView*)pickerView
           viewForRow:(NSInteger)row
         forComponent:(NSInteger)component
          reusingView:(UIView*)view {
  DCHECK_EQ(0, component);
  UILabel* label = [view isKindOfClass:[UILabel class]]
                       ? base::mac::ObjCCastStrict<UILabel>(view)
                       : [[UILabel alloc] init];
  NSString* text = self.options[row];
  label.text = text;
  label.textAlignment = NSTextAlignmentCenter;
  label.font = [text isEqualToString:self.defaultOption]
                   ? [UIFont boldSystemFontOfSize:kUIPickerFontSize]
                   : [UIFont systemFontOfSize:kUIPickerFontSize];
  return label;
}

#pragma mark Private methods

- (void)doneButtonPressed {
  NSInteger selectedIndex = [self.pickerView selectedRowInComponent:0];
  [_delegate selectorViewController:self
                    didSelectOption:self.options[selectedIndex]];
}

- (void)cancelButtonPressed {
  [_delegate selectorViewController:self didSelectOption:self.defaultOption];
}

@end
