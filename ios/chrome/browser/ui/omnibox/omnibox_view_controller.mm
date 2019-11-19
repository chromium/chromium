// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_view_controller.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/load_query_commands.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_container_view.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_text_change_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_delegate.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/dynamic_color_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {

const CGFloat kClearButtonSize = 28.0f;

}  // namespace

@interface OmniboxViewController () <OmniboxTextFieldDelegate> {
  // Weak, acts as a delegate
  OmniboxTextChangeDelegate* _textChangeDelegate;
}

// Override of UIViewController's view with a different type.
@property(nonatomic, strong) OmniboxContainerView* view;

// Whether the default search engine supports search-by-image. This controls the
// edit menu option to do an image search.
@property(nonatomic, assign) BOOL searchByImageEnabled;

@property(nonatomic, assign) BOOL incognito;

// YES if we are already forwarding an OnDidChange() message to the edit view.
// Needed to prevent infinite recursion.
// TODO(crbug.com/1015413): There must be a better way.
@property(nonatomic, assign) BOOL forwardingOnDidChange;

// YES if this text field is currently processing a user-initiated event,
// such as typing in the omnibox or pressing the clear button.  Used to
// distinguish between calls to textDidChange that are triggered by the user
// typing vs by calls to setText.
@property(nonatomic, assign) BOOL processingUserEvent;

// A flag that is set whenever any input or copy/paste event happened in the
// omnibox while it was focused. Used to count event "user focuses the omnibox
// to view the complete URL and immediately defocuses it".
@property(nonatomic, assign) BOOL omniboxInteractedWhileFocused;

@end

@implementation OmniboxViewController
@dynamic view;

- (instancetype)initWithIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _incognito = isIncognito;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  UIColor* textColor = color::DarkModeDynamicColor(
      [UIColor colorNamed:kTextPrimaryColor], self.incognito,
      [UIColor colorNamed:kTextPrimaryDarkColor]);
  UIColor* textFieldTintColor = color::DarkModeDynamicColor(
      [UIColor colorNamed:kBlueColor], self.incognito,
      [UIColor colorNamed:kBlueDarkColor]);
  UIColor* iconTintColor;
  if (base::FeatureList::IsEnabled(kNewOmniboxPopupLayout)) {
    iconTintColor = color::DarkModeDynamicColor(
        [UIColor colorNamed:kToolbarButtonColor], self.incognito,
        [UIColor colorNamed:kToolbarButtonDarkColor]);
  } else {
    iconTintColor = color::DarkModeDynamicColor(
        [UIColor colorNamed:kToolbarButtonColor], self.incognito,
        [UIColor colorNamed:kToolbarButtonDarkColor]);
  }

  self.view = [[OmniboxContainerView alloc]
      initWithFrame:CGRectZero
          textColor:textColor
      textFieldTint:textFieldTintColor
           iconTint:iconTintColor];
  self.view.incognito = self.incognito;

  self.textField.delegate = self;

  SetA11yLabelAndUiAutomationName(self.textField, IDS_ACCNAME_LOCATION,
                                  @"Address");
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Add Paste and Go option to the editing menu
  UIMenuController* menu = [UIMenuController sharedMenuController];
  UIMenuItem* searchCopiedImage = [[UIMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_IMAGE)
             action:@selector(searchCopiedImage:)];
  UIMenuItem* visitCopiedLink = [[UIMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_VISIT_COPIED_LINK)
             action:@selector(visitCopiedLink:)];
  UIMenuItem* searchCopiedText = [[UIMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_IOS_SEARCH_COPIED_TEXT)
             action:@selector(searchCopiedText:)];
  [menu setMenuItems:@[ searchCopiedImage, visitCopiedLink, searchCopiedText ]];

  self.textField.placeholderTextColor = [self placeholderAndClearButtonColor];
  self.textField.placeholder = l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  [self setupClearButton];

  [NSNotificationCenter.defaultCenter
      addObserver:self
         selector:@selector(textInputModeDidChange)
             name:UITextInputCurrentInputModeDidChangeNotification
           object:nil];

  [self updateLeadingImageVisibility];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];

  [self.view attachLayoutGuides];
}

- (void)viewWillDisappear:(BOOL)animated {
  [super viewWillDisappear:animated];
  self.textField.selectedTextRange =
      [self.textField textRangeFromPosition:self.textField.beginningOfDocument
                                 toPosition:self.textField.beginningOfDocument];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [self updateLeadingImageVisibility];
}

- (void)setTextChangeDelegate:(OmniboxTextChangeDelegate*)textChangeDelegate {
  _textChangeDelegate = textChangeDelegate;
}

#pragma mark - public methods

- (OmniboxTextFieldIOS*)textField {
  return self.view.textField;
}

#pragma mark - OmniboxTextFieldDelegate

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)newText {
  DCHECK(_textChangeDelegate);
  self.processingUserEvent = _textChangeDelegate->OnWillChange(range, newText);
  return self.processingUserEvent;
}

- (void)textFieldDidChange:(id)sender {
  // If the text is empty, update the leading image.
  if (self.textField.text.length == 0) {
    [self.view setLeadingImage:self.emptyTextLeadingImage];
  }

  [self updateClearButtonVisibility];
  self.semanticContentAttribute = [self.textField bestSemanticContentAttribute];

  if (self.forwardingOnDidChange)
    return;

  // Reset the changed flag.
  self.omniboxInteractedWhileFocused = YES;

  BOOL savedProcessingUserEvent = self.processingUserEvent;
  self.processingUserEvent = NO;
  self.forwardingOnDidChange = YES;
  DCHECK(_textChangeDelegate);
  _textChangeDelegate->OnDidChange(savedProcessingUserEvent);
  self.forwardingOnDidChange = NO;
}

// Delegate method for UITextField, called when user presses the "go" button.
- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  DCHECK(_textChangeDelegate);
  _textChangeDelegate->OnAccept();
  return NO;
}

// Always update the text field colors when we start editing.  It's possible
// for this method to be called when we are already editing (popup focus
// change).  In this case, OnDidBeginEditing will be called multiple times.
// If that becomes an issue a boolean should be added to track editing state.
- (void)textFieldDidBeginEditing:(UITextField*)textField {
  // Update the clear button state.
  [self updateClearButtonVisibility];
  [self.view setLeadingImage:self.textField.text.length
                                 ? self.defaultLeadingImage
                                 : self.emptyTextLeadingImage];

  self.semanticContentAttribute = [self.textField bestSemanticContentAttribute];

  self.omniboxInteractedWhileFocused = NO;
  DCHECK(_textChangeDelegate);
  _textChangeDelegate->OnDidBeginEditing();
}

- (BOOL)textFieldShouldEndEditing:(UITextField*)textField {
  DCHECK(_textChangeDelegate);
  _textChangeDelegate->OnWillEndEditing();
  return YES;
}

// When editing, forward the message on to |_textChangeDelegate|.
- (void)textFieldDidEndEditing:(UITextField*)textField
                        reason:(UITextFieldDidEndEditingReason)reason {
  if (!self.omniboxInteractedWhileFocused) {
    RecordAction(
        UserMetricsAction("Mobile_FocusedDefocusedOmnibox_WithNoAction"));
  }
  DCHECK(_textChangeDelegate);
  _textChangeDelegate->EndEditing();
}

- (BOOL)textFieldShouldClear:(UITextField*)textField {
  DCHECK(_textChangeDelegate);
  _textChangeDelegate->ClearText();
  self.processingUserEvent = YES;
  return YES;
}

- (void)onCopy {
  self.omniboxInteractedWhileFocused = YES;
  DCHECK(_textChangeDelegate);
  _textChangeDelegate->OnCopy();
}

- (void)willPaste {
  DCHECK(_textChangeDelegate);
  _textChangeDelegate->WillPaste();
}

- (void)onDeleteBackward {
  DCHECK(_textChangeDelegate);
  _textChangeDelegate->OnDeleteBackward();
}

#pragma mark - OmniboxConsumer

- (void)updateAutocompleteIcon:(UIImage*)icon {
  [self.view setLeadingImage:icon];
}

- (void)updateSearchByImageSupported:(BOOL)searchByImageSupported {
  self.searchByImageEnabled = searchByImageSupported;
}

#pragma mark - EditViewAnimatee

- (void)setLeadingIconFaded:(BOOL)faded {
  [self.view setLeadingImageAlpha:faded ? 0 : 1];
}

- (void)setClearButtonFaded:(BOOL)faded {
  self.textField.rightView.alpha = faded ? 0 : 1;
}

#pragma mark - LocationBarOffsetProvider

- (CGFloat)xOffsetForString:(NSString*)string {
  return [self.textField offsetForString:string];
}

#pragma mark - private

- (void)updateLeadingImageVisibility {
  BOOL newOmniboxPopupLayout =
      base::FeatureList::IsEnabled(kNewOmniboxPopupLayout);
  [self.view setLeadingImageHidden:!newOmniboxPopupLayout &&
                                   !IsRegularXRegularSizeClass(self)];
}

// Tint color for the textfield placeholder and the clear button.
- (UIColor*)placeholderAndClearButtonColor {
  return color::DarkModeDynamicColor(
      [UIColor colorNamed:kTextfieldPlaceholderColor], self.incognito,
      [UIColor colorNamed:kTextfieldPlaceholderDarkColor]);
}

#pragma mark notification callbacks

// Called on UITextInputCurrentInputModeDidChangeNotification for self.textField
- (void)textInputModeDidChange {
  // Only respond to language changes when the omnibox is first responder.
  if (![self.textField isFirstResponder]) {
    return;
  }

  [self.textField updateTextDirection];
  self.semanticContentAttribute = [self.textField bestSemanticContentAttribute];

  [self.delegate omniboxViewControllerTextInputModeDidChange:self];
}

#pragma mark clear button

// Omnibox uses a custom clear button. It has a custom tint and image, but
// otherwise it should act exactly like a system button. To achieve this, a
// custom button is used as the |rightView|. Textfield's setRightViewMode: is
// used to make the button invisible when the textfield is empty; the visibility
// is updated on textfield text changes and clear button presses.
- (void)setupClearButton {
  // Do not use the system clear button. Use a custom "right view" instead.
  // Note that |rightView| is an incorrect name, it's really a trailing view.
  [self.textField setClearButtonMode:UITextFieldViewModeNever];
  [self.textField setRightViewMode:UITextFieldViewModeAlways];

  UIButton* clearButton = [UIButton buttonWithType:UIButtonTypeSystem];
  clearButton.frame = CGRectMake(0, 0, kClearButtonSize, kClearButtonSize);
  [clearButton setImage:[self clearButtonIcon] forState:UIControlStateNormal];
  [clearButton addTarget:self
                  action:@selector(clearButtonPressed)
        forControlEvents:UIControlEventTouchUpInside];
  self.textField.rightView = clearButton;

  clearButton.tintColor = [self placeholderAndClearButtonColor];
  SetA11yLabelAndUiAutomationName(clearButton, IDS_IOS_ACCNAME_CLEAR_TEXT,
                                  @"Clear Text");

  // Observe text changes to show the clear button when there is text and hide
  // it when the textfield is empty.
  [self.textField addTarget:self
                     action:@selector(textFieldDidChange:)
           forControlEvents:UIControlEventEditingChanged];
}

- (UIImage*)clearButtonIcon {
  UIImage* image = [[UIImage imageNamed:@"omnibox_clear_icon"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];

  return image;
}

- (void)clearButtonPressed {
  // Emulate a system button clear callback.
  BOOL shouldClear =
      [self.textField.delegate textFieldShouldClear:self.textField];
  if (shouldClear) {
    [self.textField setText:@""];
    // Calling setText: does not trigger UIControlEventEditingChanged, so update
    // the clear button visibility manually.
    [self.textField sendActionsForControlEvents:UIControlEventEditingChanged];
  }
}

// Hides the clear button if the textfield is empty; shows it otherwise.
- (void)updateClearButtonVisibility {
  BOOL hasText = self.textField.text.length > 0;
  [self.textField setRightViewMode:hasText ? UITextFieldViewModeAlways
                                           : UITextFieldViewModeNever];
}

// Handle the updates to semanticContentAttribute by passing the changes along
// to the necessary views.
- (void)setSemanticContentAttribute:
    (UISemanticContentAttribute)semanticContentAttribute {
  _semanticContentAttribute = semanticContentAttribute;

  if (!base::FeatureList::IsEnabled(kNewOmniboxPopupLayout)) {
    return;
  }

  self.view.semanticContentAttribute = self.semanticContentAttribute;
  self.textField.semanticContentAttribute = self.semanticContentAttribute;
}

#pragma mark - UIMenuItem

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  if (action == @selector(searchCopiedImage:) ||
      action == @selector(visitCopiedLink:) ||
      action == @selector(searchCopiedText:)) {
    ClipboardRecentContent* clipboardRecentContent =
        ClipboardRecentContent::GetInstance();
    if (self.searchByImageEnabled &&
        clipboardRecentContent->GetRecentImageFromClipboard().has_value()) {
      return action == @selector(searchCopiedImage:);
    }
    if (clipboardRecentContent->GetRecentURLFromClipboard().has_value()) {
      return action == @selector(visitCopiedLink:);
    }
    if (clipboardRecentContent->GetRecentTextFromClipboard().has_value()) {
      return action == @selector(searchCopiedText:);
    }
    return NO;
  }
  return NO;
}

- (void)searchCopiedImage:(id)sender {
  RecordAction(
      UserMetricsAction("Mobile.OmniboxContextMenu.SearchCopiedImage"));
  self.omniboxInteractedWhileFocused = YES;
  if (base::Optional<gfx::Image> optionalImage =
          ClipboardRecentContent::GetInstance()
              ->GetRecentImageFromClipboard()) {
    UIImage* image = optionalImage.value().ToUIImage();
    [self.dispatcher searchByImage:image];
    [self.dispatcher cancelOmniboxEdit];
  }
}

- (void)visitCopiedLink:(id)sender {
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.VisitCopiedLink"));
  [self pasteAndGo:sender];
}

- (void)searchCopiedText:(id)sender {
  RecordAction(UserMetricsAction("Mobile.OmniboxContextMenu.SearchCopiedText"));
  [self pasteAndGo:sender];
}

// Both actions are performed the same, but need to be enabled differently,
// so we need two different selectors.
- (void)pasteAndGo:(id)sender {
  NSString* query;
  ClipboardRecentContent* clipboardRecentContent =
      ClipboardRecentContent::GetInstance();
  if (base::Optional<GURL> optionalUrl =
          clipboardRecentContent->GetRecentURLFromClipboard()) {
    query = base::SysUTF8ToNSString(optionalUrl.value().spec());
  } else if (base::Optional<base::string16> optionalText =
                 clipboardRecentContent->GetRecentTextFromClipboard()) {
    query = base::SysUTF16ToNSString(optionalText.value());
  }
  self.omniboxInteractedWhileFocused = YES;
  [self.dispatcher loadQuery:query immediately:YES];
  [self.dispatcher cancelOmniboxEdit];
}

@end
