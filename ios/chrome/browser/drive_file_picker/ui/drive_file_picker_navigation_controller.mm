// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_navigation_controller.h"

#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"

@implementation DriveFilePickerNavigationController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier = kDriveFilePickerAccessibilityIdentifier;
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  // Observe keyboard frame changes to update safe area insets accordingly.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillChangeFrame:)
             name:UIKeyboardWillChangeFrameNotification
           object:nil];
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardDidChangeFrame:)
             name:UIKeyboardDidChangeFrameNotification
           object:nil];
}

- (void)viewDidDisappear:(BOOL)animated {
  [super viewDidDisappear:animated];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

#pragma mark - Private

// Called when a `UIKeyboardWillChangeFrameNotification` is received.
- (void)keyboardWillChangeFrame:(NSNotification*)notification {
  CGRect keyboardFrameBegin =
      [notification.userInfo[UIKeyboardFrameBeginUserInfoKey] CGRectValue];
  CGRect keyboardFrameEnd =
      [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
  if (CGRectGetMinY(keyboardFrameBegin) > CGRectGetMinY(keyboardFrameEnd)) {
    return;
  }
  // Update the additional safe area inset asynchronously to ensure the view has
  // already moved to its new position, as it might move in reaction to the
  // change of keyboard frame.
  [self ensureViewIsUnobscuredByKeyboardFrame:keyboardFrameEnd
                               asynchronously:YES];
}

// Called when a `UIKeyboardDidChangeFrameNotification` is received.
- (void)keyboardDidChangeFrame:(NSNotification*)notification {
  CGRect keyboardFrameBegin =
      [notification.userInfo[UIKeyboardFrameBeginUserInfoKey] CGRectValue];
  CGRect keyboardFrameEnd =
      [notification.userInfo[UIKeyboardFrameEndUserInfoKey] CGRectValue];
  if (CGRectGetMinY(keyboardFrameBegin) < CGRectGetMinY(keyboardFrameEnd)) {
    return;
  }
  // Update the additional safe area inset asynchronously to ensure the view has
  // already moved to its new position, as it might move in reaction to the
  // change of keyboard frame.
  [self ensureViewIsUnobscuredByKeyboardFrame:keyboardFrameEnd
                               asynchronously:YES];
}

// Update the additional safe area insets to ensure the view is unobscured.
- (void)ensureViewIsUnobscuredByKeyboardFrame:
            (const CGRect&)keyboardFrameInWindow
                               asynchronously:(BOOL)asynchronously {
  if (asynchronously) {
    // If `asynchronously` is YES, post a task.
    __weak __typeof(self) weakSelf = self;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](DriveFilePickerNavigationController* navigationController,
               const CGRect& keyboardFrameInWindow) {
              [navigationController
                  ensureViewIsUnobscuredByKeyboardFrame:keyboardFrameInWindow
                                         asynchronously:NO];
            },
            weakSelf, keyboardFrameInWindow));
    return;
  }
  // If `asynchronously` is NO, update `additionalSafeAreaInsets` now.
  CGRect safeAreaLayoutFrameInWindow =
      [self.view convertRect:self.view.safeAreaLayoutGuide.layoutFrame
                      toView:self.view.window];
  // Adjust the additional safe area bottom inset so the bottom of the safe
  // area's frame matches the top of the keyboard's frame.
  UIEdgeInsets additionalSafeAreaInsets = self.additionalSafeAreaInsets;
  additionalSafeAreaInsets.bottom +=
      CGRectGetMaxY(safeAreaLayoutFrameInWindow) -
      CGRectGetMinY(keyboardFrameInWindow);
  // Ensure the additional inset is always positive.
  additionalSafeAreaInsets.bottom =
      std::max(0.0, additionalSafeAreaInsets.bottom);
  self.additionalSafeAreaInsets = additionalSafeAreaInsets;
}

@end
