// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/memory/model/memory_debugger.h"

#import <stdint.h>

#import <memory>

#import "ios/chrome/browser/memory/model/memory_metrics.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/device_util.h"

namespace {
// The number of bytes in a megabyte.
const CGFloat kNumBytesInMB = 1024 * 1024;
// The horizontal and vertical padding between subviews.
const CGFloat kPadding = 10;
}  // namespace

@implementation MemoryDebugger {
  // A timer to trigger refreshes.
  NSTimer* _refreshTimer;

  // A timer to trigger continuous memory warnings.
  NSTimer* _memoryWarningTimer;

  // The font to use.
  UIFont* _font;

  // Labels for memory metrics.
  UILabel* _physicalFreeMemoryLabel;
  UILabel* _realMemoryUsedLabel;
  UILabel* _xcodeGaugeLabel;
  UILabel* _dirtyVirtualMemoryLabel;

  // Inputs for memory commands.
  UITextField* _bloatField;
  UITextField* _refreshField;
  UITextField* _continuousMemoryWarningField;

  // A place to store the artificial memory bloat.
  std::unique_ptr<uint8_t> _bloat;

  // Distance the view was pushed up to accommodate the keyboard.
  CGFloat _keyboardOffset;

  // The current orientation of the device.
  BOOL _currentOrientation;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _font = [UIFont systemFontOfSize:14];
    self.backgroundColor = [UIColor colorWithWhite:0.8f alpha:0.9f];
    self.opaque = NO;

    [self addSubviews];
    [self sizeToFit];
    [self registerForNotifications];
  }
  return self;
}

// NSTimers create a retain cycle so they must be invalidated before this
// instance can be deallocated.
- (void)invalidateTimers {
  [_refreshTimer invalidate];
  [_memoryWarningTimer invalidate];
}

#pragma mark UIView methods

- (CGSize)sizeThatFits:(CGSize)size {
  CGFloat width = 0;
  CGFloat height = 0;
  for (UIView* subview in self.subviews) {
    width = MAX(width, CGRectGetMaxX(subview.frame));
    height = MAX(height, CGRectGetMaxY(subview.frame));
  }
  return CGSizeMake(width + kPadding, height + kPadding);
}

#pragma mark initialization helpers

- (void)addSubviews {
  // `index` is used to calculate the vertical position of each element in
  // the debugger view.
  NSUInteger index = 0;

  // Display some metrics.
  _physicalFreeMemoryLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  [self addMetricWithName:@"Physical Free"
                  atIndex:index++
               usingLabel:_physicalFreeMemoryLabel];
  _realMemoryUsedLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  [self addMetricWithName:@"Real Memory Used"
                  atIndex:index++
               usingLabel:_realMemoryUsedLabel];
  _xcodeGaugeLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  [self addMetricWithName:@"Xcode Gauge"
                  atIndex:index++
               usingLabel:_xcodeGaugeLabel];
  _dirtyVirtualMemoryLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  [self addMetricWithName:@"Dirty VM"
                  atIndex:index++
               usingLabel:_dirtyVirtualMemoryLabel];

// Since _performMemoryWarning is a private API it can't be compiled into
// official builds.
// TODO(lliabraa): Figure out how to support memory warnings (or something
// like them) in official builds.
#if !defined(OFFICIAL_BUILD)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundeclared-selector"
  [self addButtonWithTitle:@"Trigger Memory Warning"
                    target:[UIApplication sharedApplication]
                    action:@selector(_performMemoryWarning)
                withOrigin:[self originForSubviewAtIndex:index++]];
#pragma clang diagnostic pop
#endif  // !defined(OFFICIAL_BUILD)

  // Display a text input to set the amount of artificial memory bloat and a
  // button to reset the bloat to zero.
  _bloatField = [[UITextField alloc] initWithFrame:CGRectZero];
  [self addLabelWithText:@"Set bloat (MB)"
                   input:_bloatField
             inputTarget:self
             inputAction:@selector(updateBloat)
         buttonWithTitle:@"Clear"
            buttonTarget:self
            buttonAction:@selector(clearBloat)
                 atIndex:index++];
  [_bloatField setText:@"0"];
  [self updateBloat];

// Since _performMemoryWarning is a private API it can't be compiled into
// official builds.
// TODO(lliabraa): Figure out how to support memory warnings (or something
// like them) in official builds.
#if !defined(OFFICIAL_BUILD)
  // Display a text input to control the rate of continuous memory warnings.
  _continuousMemoryWarningField =
      [[UITextField alloc] initWithFrame:CGRectZero];
  [self addLabelWithText:@"Set memory warning interval (secs)"
                   input:_continuousMemoryWarningField
             inputTarget:self
             inputAction:@selector(updateMemoryWarningInterval)
                 atIndex:index++];
  [_continuousMemoryWarningField setText:@"0.0"];
#endif  // !defined(OFFICIAL_BUILD)

  // Display a text input to control the refresh rate of the memory debugger.
  _refreshField = [[UITextField alloc] initWithFrame:CGRectZero];
  [self addLabelWithText:@"Set refresh interval (secs)"
                   input:_refreshField
             inputTarget:self
             inputAction:@selector(updateRefreshInterval)
                 atIndex:index++];
  [_refreshField setText:@"0.5"];
  [self updateRefreshInterval];
}

- (void)registerForNotifications {
  // Register to receive memory warning.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(lowMemoryWarningReceived:)
             name:UIApplicationDidReceiveMemoryWarningNotification
           object:nil];

  // Register to receive keyboard will show notification.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillShow:)
             name:UIKeyboardWillShowNotification
           object:nil];

  // Register to receive keyboard will hide notification.
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(keyboardWillHide:)
             name:UIKeyboardWillHideNotification
           object:nil];
}

// Adds subviews for the specified metric, the value of which will be displayed
// in `label`.
- (void)addMetricWithName:(NSString*)name
                  atIndex:(NSUInteger)index
               usingLabel:(UILabel*)label {
  // The width of the view for the metric's name.
  const CGFloat kNameWidth = 150;
  // The width of the view for each metric.
  const CGFloat kMetricWidth = 100;
  CGPoint nameOrigin = [self originForSubviewAtIndex:index];
  CGRect nameFrame =
      CGRectMake(nameOrigin.x, nameOrigin.y, kNameWidth, [_font lineHeight]);
  UILabel* nameLabel = [[UILabel alloc] initWithFrame:nameFrame];
  [nameLabel setText:[NSString stringWithFormat:@"%@: ", name]];
  [nameLabel setFont:_font];
  [self addSubview:nameLabel];
  label.frame = CGRectMake(CGRectGetMaxX(nameFrame), nameFrame.origin.y,
                           kMetricWidth, [_font lineHeight]);
  [label setFont:_font];
  [label setTextAlignment:NSTextAlignmentRight];
  [self addSubview:label];
}

// Adds a subview for a button with the given title and target/action.
- (void)addButtonWithTitle:(NSString*)title
                    target:(id)target
                    action:(SEL)action
                withOrigin:(CGPoint)origin {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setTitle:title forState:UIControlStateNormal];
  [button titleLabel].font = _font;
  [[button titleLabel] setTextAlignment:NSTextAlignmentCenter];
  [button sizeToFit];
  [button setFrame:CGRectMake(origin.x, origin.y, [button frame].size.width,
                              [_font lineHeight])];
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  [self addSubview:button];
}

// Adds subviews for a UI component with label and input text field.
//
// -------------------------
// | labelText  | <input>  |
// -------------------------
//
// The inputTarget/inputAction will be invoked when the user finishes editing
// in `input`.
- (void)addLabelWithText:(NSString*)labelText
                   input:(UITextField*)input
             inputTarget:(id)inputTarget
             inputAction:(SEL)inputAction
                 atIndex:(NSUInteger)index {
  [self addLabelWithText:labelText
                   input:input
             inputTarget:inputTarget
             inputAction:inputAction
         buttonWithTitle:nil
            buttonTarget:nil
            buttonAction:nil
                 atIndex:index];
}

// Adds subviews for a UI component with label, input text field and button.
//
// -------------------------------------
// | labelText  | <input>  |  <button> |
// -------------------------------------
//
// The inputTarget/inputAction will be invoked when the user finishes editing
// in `input`.
- (void)addLabelWithText:(NSString*)labelText
                   input:(UITextField*)input
             inputTarget:(id)inputTarget
             inputAction:(SEL)inputAction
         buttonWithTitle:(NSString*)buttonTitle
            buttonTarget:(id)buttonTarget
            buttonAction:(SEL)buttonAction
                 atIndex:(NSUInteger)index {
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  if (labelText) {
    [label setText:[NSString stringWithFormat:@"%@: ", labelText]];
  }
  [label setFont:_font];
  [label sizeToFit];
  CGPoint labelOrigin = [self originForSubviewAtIndex:index];
  [label setFrame:CGRectOffset([label frame], labelOrigin.x, labelOrigin.y)];
  [self addSubview:label];
  if (input) {
    // The width of the views for each input text field.
    const CGFloat kInputWidth = 50;
    input.frame =
        CGRectMake(CGRectGetMaxX([label frame]) + kPadding,
                   [label frame].origin.y, kInputWidth, [_font lineHeight]);
    input.font = _font;
    input.backgroundColor = [UIColor whiteColor];
    input.delegate = self;
    input.keyboardType = UIKeyboardTypeNumbersAndPunctuation;
    input.adjustsFontSizeToFitWidth = YES;
    input.textAlignment = NSTextAlignmentRight;
    [input addTarget:inputTarget
                  action:inputAction
        forControlEvents:UIControlEventEditingDidEnd];

    [self addSubview:input];
  }

  if (buttonTitle) {
    const CGFloat kButtonXOffset =
        input ? CGRectGetMaxX(input.frame) : CGRectGetMaxX([label frame]);
    CGPoint origin =
        CGPointMake(kButtonXOffset + kPadding, [label frame].origin.y);
    [self addButtonWithTitle:buttonTitle
                      target:buttonTarget
                      action:buttonAction
                  withOrigin:origin];
  }
}

// Returns the CGPoint of the origin of the subview at `index`.
- (CGPoint)originForSubviewAtIndex:(NSUInteger)index {
  return CGPointMake(kPadding,
                     (index + 1) * kPadding + index * [_font lineHeight]);
}

#pragma mark Refresh callback

// Updates content and ensures the view is visible.
- (void)refresh:(NSTimer*)timer {
  [self.superview bringSubviewToFront:self];
  [self updateMemoryInfo];
}

#pragma mark Memory inspection

// Updates the memory metrics shown.
- (void)updateMemoryInfo {
  CGFloat value = memory_util::GetFreePhysicalBytes() / kNumBytesInMB;
  [_physicalFreeMemoryLabel
      setText:[NSString stringWithFormat:@"%.2f MB", value]];
  value = memory_util::GetRealMemoryUsedInBytes() / kNumBytesInMB;
  [_realMemoryUsedLabel setText:[NSString stringWithFormat:@"%.2f MB", value]];
  value = memory_util::GetInternalVMBytes() / kNumBytesInMB;
  [_xcodeGaugeLabel setText:[NSString stringWithFormat:@"%.2f MB", value]];
  value = memory_util::GetDirtyVMBytes() / kNumBytesInMB;
  [_dirtyVirtualMemoryLabel
      setText:[NSString stringWithFormat:@"%.2f MB", value]];
}

#pragma mark Memory Warning notification callback

// Flashes the debugger to indicate memory warning.
- (void)lowMemoryWarningReceived:(NSNotification*)notification {
  UIColor* originalColor = self.backgroundColor;
  self.backgroundColor =
      [UIColor colorWithRed:1.0 green:0.0 blue:0.0 alpha:0.9];
  [UIView animateWithDuration:1.0
                        delay:0.0
                      options:UIViewAnimationOptionAllowUserInteraction
                   animations:^{
                     self.backgroundColor = originalColor;
                   }
                   completion:nil];
}

#pragma mark Rotation notification callback

- (void)didMoveToSuperview {
  UIView* superview = [self superview];
  if (superview)
    [self setCenter:[superview center]];
}

#pragma mark Keyboard notification callbacks

// Ensures the debugger is visible by shifting it up as the keyboard animates
// in.
- (void)keyboardWillShow:(NSNotification*)notification {
  NSDictionary* userInfo = [notification userInfo];
  NSValue* keyboardFrameValue =
      [userInfo valueForKey:UIKeyboardFrameEndUserInfoKey];
  CGFloat keyboardHeight = CurrentKeyboardHeight(keyboardFrameValue);

  // Get the coord of the bottom of the debugger's frame.
  CGFloat bottomOfFrame = CGRectGetMaxY(self.frame);

  // Shift the debugger up by the "height" of the keyboard, but since the
  // keyboard rect is in screen coords, use the orientation to find the height.
  CGFloat distanceFromBottom = CurrentScreenHeight() - bottomOfFrame;
  _keyboardOffset = -1 * fmax(0.0f, keyboardHeight - distanceFromBottom);
  [self animateForKeyboardNotification:notification
                            withOffset:CGPointMake(0, _keyboardOffset)];
}

// Shifts the debugger back down when the keyboard is hidden.
- (void)keyboardWillHide:(NSNotification*)notification {
  [self animateForKeyboardNotification:notification
                            withOffset:CGPointMake(0, -_keyboardOffset)];
}

- (void)animateForKeyboardNotification:(NSNotification*)notification
                            withOffset:(CGPoint)offset {
  // Account for orientation.
  offset = CGPointApplyAffineTransform(offset, self.transform);
  // Normally this would use an animation block, but there is no API to
  // convert the UIKeyboardAnimationCurveUserInfoKey's value from a
  // UIViewAnimationCurve to a UIViewAnimationOption. Awesome!
  NSDictionary* userInfo = [notification userInfo];
  NSTimeInterval duration =
      [userInfo[UIKeyboardAnimationDurationUserInfoKey] doubleValue];

  // The keyboard notification contains a UIViewAnimationCurve, but there is no
  // function to convert that to UIViewAnimationOptions. Use the default
  // instead, even if it doesn't exactly match the keyboard's curve.
  [UIView animateWithDuration:duration
                        delay:0
                      options:UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     self.frame = CGRectOffset(self.frame, offset.x, offset.y);
                   }
                   completion:nil];
}

#pragma mark Artificial memory bloat methods

- (void)updateBloat {
  double bloatSizeMB;
  NSScanner* scanner = [NSScanner scannerWithString:[_bloatField text]];
  if (![scanner scanDouble:&bloatSizeMB] || bloatSizeMB < 0.0) {
    bloatSizeMB = 0;
    NSString* errorMessage =
        [NSString stringWithFormat:@"Invalid value \"%@\" for bloat size.\n"
                                   @"Must be a positive number.\n"
                                   @"Resetting to %.1f MB",
                                   [_bloatField text], bloatSizeMB];
    [self alert:errorMessage];
    [_bloatField setText:[NSString stringWithFormat:@"%.1f", bloatSizeMB]];
  }
  const CGFloat kBloatSizeBytes = ceil(bloatSizeMB * kNumBytesInMB);
  const uint64_t kNumberOfBytes = static_cast<uint64_t>(kBloatSizeBytes);
  _bloat.reset(kNumberOfBytes ? new uint8_t[kNumberOfBytes] : nullptr);
  if (_bloat) {
    memset(_bloat.get(), -1, kNumberOfBytes);  // Occupy memory.
  } else {
    if (kNumberOfBytes) {
      [self alert:@"Could not allocate memory."];
    }
  }
}

- (void)clearBloat {
  [_bloatField setText:@"0"];
  [_bloatField resignFirstResponder];
  [self updateBloat];
}

#pragma mark Refresh interval methods

- (void)updateRefreshInterval {
  double refreshTimerValue;
  NSScanner* scanner = [NSScanner scannerWithString:[_refreshField text]];
  if (![scanner scanDouble:&refreshTimerValue] || refreshTimerValue < 0.0) {
    refreshTimerValue = 0.5;
    NSString* errorMessage = [NSString
        stringWithFormat:@"Invalid value \"%@\" for refresh interval.\n"
                         @"Must be a positive number.\n" @"Resetting to %.1f",
                         [_refreshField text], refreshTimerValue];
    [self alert:errorMessage];
    [_refreshField
        setText:[NSString stringWithFormat:@"%.1f", refreshTimerValue]];
    return;
  }
  [_refreshTimer invalidate];
  _refreshTimer = [NSTimer scheduledTimerWithTimeInterval:refreshTimerValue
                                                   target:self
                                                 selector:@selector(refresh:)
                                                 userInfo:nil
                                                  repeats:YES];
}

#pragma mark Memory warning interval methods

// Since _performMemoryWarning is a private API it can't be compiled into
// official builds.
// TODO(lliabraa): Figure out how to support memory warnings (or something
// like them) in official builds.
#if !defined(OFFICIAL_BUILD)
- (void)updateMemoryWarningInterval {
  [_memoryWarningTimer invalidate];
  double timerValue;
  NSString* text = [_continuousMemoryWarningField text];
  NSScanner* scanner = [NSScanner scannerWithString:text];
  BOOL valueFound = [scanner scanDouble:&timerValue];
  // If the text field is empty or contains 0, return early to turn off
  // continuous memory warnings.
  if (![text length] || timerValue == 0.0) {
    return;
  }
  // If no value could be parsed or a non-positive value was found, throw up an
  // error message and return early to turn off continuous memory warnings.
  if (!valueFound || timerValue <= 0.0) {
    NSString* errorMessage = [NSString
        stringWithFormat:@"Invalid value \"%@\" for memory warning interval.\n"
                         @"Must be a positive number.\n"
                         @"Turning off continuous memory warnings",
                         text];
    [self alert:errorMessage];
    [_continuousMemoryWarningField setText:@""];
    return;
  }
  // If a valid value was found have the timer start triggering continuous
  // memory warnings.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundeclared-selector"
  _memoryWarningTimer =
      [NSTimer scheduledTimerWithTimeInterval:timerValue
                                       target:[UIApplication sharedApplication]
                                     selector:@selector(_performMemoryWarning)
                                     userInfo:nil
                                      repeats:YES];
#pragma clang diagnostic push
}
#endif  // !defined(OFFICIAL_BUILD)

#pragma mark UITextViewDelegate methods

// Dismisses the keyboard if the user hits return.
- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  [textField resignFirstResponder];
  return YES;
}

#pragma mark UIResponder methods

// Allows the debugger to be dragged around the screen.
- (void)touchesMoved:(NSSet*)touches withEvent:(UIEvent*)event {
  UITouch* touch = [touches anyObject];
  CGPoint start = [touch previousLocationInView:self];
  CGPoint end = [touch locationInView:self];
  CGPoint offset = CGPointMake(end.x - start.x, end.y - start.y);
  offset = CGPointApplyAffineTransform(offset, self.transform);
  self.frame = CGRectOffset(self.frame, offset.x, offset.y);
}

#pragma mark Error handling

// Shows an alert with the given `errorMessage`.
- (void)alert:(NSString*)errorMessage {
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:@"Error"
                                          message:errorMessage
                                   preferredStyle:UIAlertControllerStyleAlert];
  [alert addAction:[UIAlertAction actionWithTitle:@"OK"
                                            style:UIAlertActionStyleDefault
                                          handler:nil]];
  [[self.window rootViewController] presentViewController:alert
                                                 animated:YES
                                               completion:nil];
}

@end
