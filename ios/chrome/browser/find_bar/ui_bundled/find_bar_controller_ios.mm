// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_controller_ios.h"

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/format_macros.h"
#import "base/i18n/rtl.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/find_in_page/model/constants.h"
#import "ios/chrome/browser/find_in_page/model/find_in_page_model.h"
#import "ios/chrome/browser/shared/public/commands/find_in_page_commands.h"
#import "ios/chrome/browser/shared/ui/util/image/image_util.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_constants.h"
#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_view.h"
#import "ios/chrome/browser/find_bar/ui_bundled/find_bar_view_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/base/resource/resource_bundle.h"

namespace {
// For the first `kSearchDelayChars` characters, delay by `kSearchLongDelay`
// For the remaining characters, delay by `kSearchShortDelay`.
const NSUInteger kSearchDelayChars = 3;
const NSTimeInterval kSearchLongDelay = 1.0;
const NSTimeInterval kSearchShortDelay = 0.100;

}  // anonymous namespace

#pragma mark - FindBarControllerIOS

@interface FindBarControllerIOS () <FindBarViewControllerDelegate,
                                    UITextFieldDelegate>

// Responds to touches that make editing changes on the text field, triggering
// find-in-page searches for the field's current value.
- (void)editingChanged;
// Selects text in such way that selection menu does not appear and
// a11y label is read. When -[UITextField selectAll:] is used, iOS
// will read "Select All" instead of a11y label.
- (void)selectAllText;

// Redefined to be readwrite
@property(nonatomic, strong, readwrite)
    FindBarViewController* findBarViewController;

// Typing delay timer.
@property(nonatomic, strong) NSTimer* delayTimer;
// Yes if incognito.
@property(nonatomic, assign) BOOL isIncognito;
@end

@implementation FindBarControllerIOS

#pragma mark - Lifecycle

- (instancetype)initWithIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _isIncognito = isIncognito;
  }
  return self;
}

#pragma mark - Public

- (FindBarViewController*)findBarViewController {
  if (_findBarViewController) {
    return _findBarViewController;
  }
  _findBarViewController =
      [[FindBarViewController alloc] initWithDarkAppearance:self.isIncognito];
  _findBarViewController.delegate = self;

  _findBarViewController.findBarView.inputField.delegate = self;
  [_findBarViewController.findBarView.inputField
             addTarget:self
                action:@selector(editingChanged)
      forControlEvents:UIControlEventEditingChanged];
  [_findBarViewController.findBarView.nextButton
             addTarget:self.commandHandler
                action:@selector(findNextStringInPage)
      forControlEvents:UIControlEventTouchUpInside];
  [_findBarViewController.findBarView.nextButton
             addTarget:self
                action:@selector(hideKeyboard:)
      forControlEvents:UIControlEventTouchUpInside];
  [_findBarViewController.findBarView.previousButton
             addTarget:self.commandHandler
                action:@selector(findPreviousStringInPage)
      forControlEvents:UIControlEventTouchUpInside];
  [_findBarViewController.findBarView.previousButton
             addTarget:self
                action:@selector(hideKeyboard:)
      forControlEvents:UIControlEventTouchUpInside];
  [_findBarViewController.findBarView.closeButton
             addTarget:self
                action:@selector(dismiss)
      forControlEvents:UIControlEventTouchUpInside];
  // By default, VoiceOver can get confused as to what order we expect the
  // elements to be sorted when navigating through them. To specify a different
  // ordering of elements, setting the containing view’s `accessibilityElements`
  // property.
  _findBarViewController.findBarView.accessibilityElements = @[
    _findBarViewController.findBarView.inputField,
    _findBarViewController.findBarView.previousButton,
    _findBarViewController.findBarView.nextButton,
    _findBarViewController.findBarView.closeButton,
  ];

  return _findBarViewController;
}

- (void)findBarViewWillHide {
  self.findBarViewController.findBarView.inputField.selectedTextRange = nil;
  [self.delayTimer invalidate];
  self.delayTimer = nil;
}

- (void)findBarViewDidHide {
  self.findBarViewController = nil;
}

- (NSString*)searchTerm {
  return [self.findBarViewController.findBarView.inputField text];
}

- (BOOL)isFindInPageShown {
  return self.findBarViewController.findBarView != nil;
}

- (BOOL)isFocused {
  return [self.findBarViewController.findBarView.inputField isFirstResponder];
}

- (void)updateResultsCount:(FindInPageModel*)model {
  [self updateWithMatchNumber:model.currentIndex
                   matchCount:model.matches
                   searchText:model.text];
}

- (void)updateView:(FindInPageModel*)model
     initialUpdate:(BOOL)initialUpdate
    focusTextfield:(BOOL)focusTextfield {
  [self.delayTimer invalidate];
  self.delayTimer = nil;

  if (initialUpdate) {
    // Set initial text and first search.
    [self.findBarViewController.findBarView.inputField setText:model.text];
    [self editingChanged];
  }

  // Focus input field if necessary.
  if (focusTextfield) {
    [self.findBarViewController.findBarView.inputField becomeFirstResponder];
  } else {
    [self.findBarViewController.findBarView.inputField resignFirstResponder];
  }

  [self updateWithMatchNumber:model.currentIndex
                   matchCount:model.matches
                   searchText:model.text];
}

- (void)updateWithMatchNumber:(NSUInteger)matchNumber
                   matchCount:(NSUInteger)matchCount
                   searchText:(NSString*)searchText {
  NSString* text = nil;
  if (searchText.length != 0) {
    NSString* indexStr = [NSString stringWithFormat:@"%" PRIdNS, matchNumber];
    NSString* matchesStr = [NSString stringWithFormat:@"%" PRIdNS, matchCount];
    text = l10n_util::GetNSStringF(IDS_FIND_IN_PAGE_COUNT,
                                   base::SysNSStringToUTF16(indexStr),
                                   base::SysNSStringToUTF16(matchesStr));
  }
  [self.findBarViewController.findBarView updateResultsLabelWithText:text];

  BOOL enabled = matchCount != 0;
  self.findBarViewController.findBarView.nextButton.enabled = enabled;
  self.findBarViewController.findBarView.previousButton.enabled = enabled;
}

- (void)hideKeyboard:(id)sender {
  [self.findBarViewController.findBarView endEditing:YES];
}

- (void)selectAllText {
  UITextRange* wholeTextRange =
      [self.findBarViewController.findBarView.inputField
          textRangeFromPosition:self.findBarViewController.findBarView
                                    .inputField.beginningOfDocument
                     toPosition:self.findBarViewController.findBarView
                                    .inputField.endOfDocument];
  self.findBarViewController.findBarView.inputField.selectedTextRange =
      wholeTextRange;
}

#pragma mark - Internal

- (void)editingChanged {
  [self.delayTimer invalidate];
  NSUInteger length = [[self searchTerm] length];
  if (length == 0) {
    [self.commandHandler searchFindInPage];
    return;
  }

  // Delay delivery of the search text event to give time for a user to type out
  // a longer word.  Use a longer delay when the input length is short, as short
  // words are currently very inefficient and lock up the web view.
  NSTimeInterval delay =
      (length > kSearchDelayChars) ? kSearchShortDelay : kSearchLongDelay;
  self.delayTimer =
      [NSTimer scheduledTimerWithTimeInterval:delay
                                       target:self.commandHandler
                                     selector:@selector(searchFindInPage)
                                     userInfo:nil
                                      repeats:NO];
}

#pragma mark - FindBarViewControllerDelegate

- (void)dismiss {
  [self.commandHandler closeFindInPage];
}

#pragma mark - UITextFieldDelegate

- (BOOL)textFieldShouldBeginEditing:(UITextField*)textField {
  DCHECK(textField == self.findBarViewController.findBarView.inputField);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kFindBarTextFieldWillBecomeFirstResponderNotification
                    object:self];
  return YES;
}

- (void)textFieldDidEndEditing:(UITextField*)textField {
  DCHECK(textField == self.findBarViewController.findBarView.inputField);
  [[NSNotificationCenter defaultCenter]
      postNotificationName:kFindBarTextFieldDidResignFirstResponderNotification
                    object:self];
}

- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  DCHECK(textField == self.findBarViewController.findBarView.inputField);
  [self.findBarViewController.findBarView.inputField resignFirstResponder];
  return YES;
}

@end
