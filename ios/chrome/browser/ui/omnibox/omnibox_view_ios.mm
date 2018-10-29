// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"

#import <CoreText/CoreText.h>
#import <MobileCoreServices/MobileCoreServices.h>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/ios/device_util.h"
#include "base/ios/ios_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/toolbar_model.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/ui/omnibox/chrome_omnibox_client_ios.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_text_field_paste_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#include "ios/chrome/browser/ui/omnibox/web_omnibox_edit_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/web/public/referrer.h"
#import "net/base/mac/url_conversions.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {
const CGFloat kClearTextButtonWidth = 28;
const CGFloat kClearTextButtonHeight = 28;

// The color of the rest of the URL (i.e. after the TLD) in the omnibox.
UIColor* BaseTextColor() {
  return [UIColor colorWithWhite:(161 / 255.0) alpha:1.0];
}

// The color of the https when there is an error.
UIColor* ErrorTextColor() {
  return skia::UIColorFromSkColor(gfx::kGoogleRed700);
}

// The color of the https when there is not an error.
UIColor* SecureTextColor() {
  return skia::UIColorFromSkColor(gfx::kGoogleGreen700);
}

// The color of the https when highlighted in incognito.
UIColor* IncognitoSecureTextColor() {
  return [UIColor colorWithWhite:(255 / 255.0) alpha:1.0];
}

}  // namespace

#pragma mark - AutocompleteTextFieldDelegate

// Simple Obj-C object to forward UITextFieldDelegate method calls back to the
// OmniboxViewIOS.
@interface AutocompleteTextFieldDelegate : NSObject<OmniboxTextFieldDelegate> {
 @private
  OmniboxViewIOS* editView_;  // weak, owns us

  // YES if we are already forwarding an OnDidChange() message to the edit view.
  // Needed to prevent infinite recursion.
  // TODO(rohitrao): There must be a better way.
  BOOL forwardingOnDidChange_;

  // YES if this text field is currently processing a user-initiated event,
  // such as typing in the omnibox or pressing the clear button.  Used to
  // distinguish between calls to textDidChange that are triggered by the user
  // typing vs by calls to setText.
  BOOL processingUserEvent_;
}
@end

@implementation AutocompleteTextFieldDelegate
- (id)initWithEditView:(OmniboxViewIOS*)editView {
  if ((self = [super init])) {
    editView_ = editView;
    forwardingOnDidChange_ = NO;
    processingUserEvent_ = NO;
  }
  return self;
}

- (BOOL)textField:(UITextField*)textField
    shouldChangeCharactersInRange:(NSRange)range
                replacementString:(NSString*)newText {
  processingUserEvent_ = editView_->OnWillChange(range, newText);
  return processingUserEvent_;
}

- (void)textFieldDidChange:(id)sender {
  if (forwardingOnDidChange_)
    return;

  BOOL savedProcessingUserEvent = processingUserEvent_;
  processingUserEvent_ = NO;
  forwardingOnDidChange_ = YES;
  editView_->OnDidChange(savedProcessingUserEvent);
  forwardingOnDidChange_ = NO;
}

// Delegate method for UITextField, called when user presses the "go" button.
- (BOOL)textFieldShouldReturn:(UITextField*)textField {
  editView_->OnAccept();
  return NO;
}

// Always update the text field colors when we start editing.  It's possible
// for this method to be called when we are already editing (popup focus
// change).  In this case, OnDidBeginEditing will be called multiple times.
// If that becomes an issue a boolean should be added to track editing state.
- (void)textFieldDidBeginEditing:(UITextField*)textField {
  editView_->OnDidBeginEditing();
}

// On phone, the omnibox may still be editing when the popup is open, so end
// editing is called directly in OnDidEndEditing.
- (void)textFieldDidEndEditing:(UITextField*)textField {
  if (!IsIPadIdiom() && editView_->IsPopupOpen())
    return;

  editView_->OnDidEndEditing();
}

// When editing, forward the message on to |editView_|.
- (BOOL)textFieldShouldClear:(UITextField*)textField {
  DCHECK(IsRefreshLocationBarEnabled());
  editView_->ClearText();
  processingUserEvent_ = YES;
  return YES;
}

- (BOOL)onCopy {
  return editView_->OnCopy();
}

- (void)willPaste {
  editView_->WillPaste();
}

- (void)onDeleteBackward {
  editView_->OnDeleteBackward();
}

@end

#pragma mark - OmniboxClearButtonBridge

// An ObjC bridge class to allow taps on the clear button to be sent to a C++
// class.
@interface OmniboxClearButtonBridge : NSObject

- (instancetype)initWithOmniboxView:(OmniboxViewIOS*)omniboxView
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (void)clearText;

@end

@implementation OmniboxClearButtonBridge {
  OmniboxViewIOS* _omniboxView;
}

- (instancetype)initWithOmniboxView:(OmniboxViewIOS*)omniboxView {
  self = [super init];
  if (self) {
    _omniboxView = omniboxView;
  }
  return self;
}

- (void)clearText {
  _omniboxView->ClearText();
}

@end

#pragma mark - OminboxViewIOS

OmniboxViewIOS::OmniboxViewIOS(OmniboxTextFieldIOS* field,
                               WebOmniboxEditController* controller,
                               id<OmniboxLeftImageConsumer> left_image_consumer,
                               ios::ChromeBrowserState* browser_state)
    : OmniboxView(
          controller,
          std::make_unique<ChromeOmniboxClientIOS>(controller, browser_state)),
      browser_state_(browser_state),
      field_(field),
      controller_(controller),
      left_image_consumer_(left_image_consumer),
      ignore_popup_updates_(false),
      attributing_display_string_(nil),
      popup_provider_(nullptr) {
  DCHECK(field_);
  field_delegate_ =
      [[AutocompleteTextFieldDelegate alloc] initWithEditView:this];

  if (@available(iOS 11.0, *)) {
    paste_delegate_ = [[OmniboxTextFieldPasteDelegate alloc] init];
    [field_ setPasteDelegate:paste_delegate_];
  }

  [field_ setDelegate:field_delegate_];
  [field_ addTarget:field_delegate_
                action:@selector(textFieldDidChange:)
      forControlEvents:UIControlEventEditingChanged];
  use_strikethrough_workaround_ = base::ios::IsRunningOnOrLater(10, 3, 0) &&
                                  !base::ios::IsRunningOnOrLater(11, 2, 0);

  CreateClearTextIcon(browser_state->IsOffTheRecord());
}

OmniboxViewIOS::~OmniboxViewIOS() {
  // |field_| outlives this object.
  [field_ setDelegate:nil];

  [field_ removeTarget:field_delegate_
                action:@selector(textFieldDidChange:)
      forControlEvents:UIControlEventEditingChanged];
}

void OmniboxViewIOS::OpenMatch(const AutocompleteMatch& match,
                               WindowOpenDisposition disposition,
                               const GURL& alternate_nav_url,
                               const base::string16& pasted_text,
                               size_t selected_line,
                               base::TimeTicks match_selection_timestamp) {
  // It may be unsafe to modify the contents of the field.
  if (ShouldIgnoreUserInputDueToPendingVoiceSearch()) {
    return;
  }

  OmniboxView::OpenMatch(match, disposition, alternate_nav_url, pasted_text,
                         selected_line, match_selection_timestamp);
}

base::string16 OmniboxViewIOS::GetText() const {
  return [field_ displayedText];
}

void OmniboxViewIOS::SetWindowTextAndCaretPos(const base::string16& text,
                                              size_t caret_pos,
                                              bool update_popup,
                                              bool notify_text_changed) {
  // Do not call SetUserText() here, as the user has not triggered this change.
  // Instead, set the field's text directly.
  // Set the field_ value before calling ApplyTextAttributes(), because that
  // internally calls model()->CurrentTextIsUrl(), which uses the text in the
  // field_.
  [field_ setText:base::SysUTF16ToNSString(text)];

  NSAttributedString* as = ApplyTextAttributes(text);
  [field_ setText:as userTextLength:[as length]];

  if (update_popup)
    UpdatePopup();

  if (notify_text_changed)
    model()->OnChanged();

  SetCaretPos(caret_pos);
}

void OmniboxViewIOS::SetCaretPos(size_t caret_pos) {
  DCHECK(caret_pos <= field_.text.length || caret_pos == 0);
  UITextPosition* start = field_.beginningOfDocument;
  UITextPosition* newPosition =
      [field_ positionFromPosition:start offset:caret_pos];
  field_.selectedTextRange =
      [field_ textRangeFromPosition:newPosition toPosition:newPosition];
}

void OmniboxViewIOS::RevertAll() {
  ignore_popup_updates_ = true;
  OmniboxView::RevertAll();
  ignore_popup_updates_ = false;
}

void OmniboxViewIOS::UpdatePopup() {
  model()->SetInputInProgress(true);

  if (!model()->has_focus())
    return;

  // Prevent inline-autocomplete if the IME is currently composing or if the
  // cursor is not at the end of the text.
  bool prevent_inline_autocomplete =
      IsImeComposing() ||
      NSMaxRange(current_selection_) != [[field_ text] length];
  model()->StartAutocomplete(current_selection_.length != 0,
                             prevent_inline_autocomplete);
  DCHECK(popup_provider_);
  popup_provider_->SetTextAlignment([field_ bestTextAlignment]);
}

void OmniboxViewIOS::OnTemporaryTextMaybeChanged(
    const base::string16& display_text,
    const AutocompleteMatch& match,
    bool save_original_selection,
    bool notify_text_changed) {
  SetWindowTextAndCaretPos(display_text, display_text.size(), false, false);
  model()->OnChanged();
}

bool OmniboxViewIOS::OnInlineAutocompleteTextMaybeChanged(
    const base::string16& display_text,
    size_t user_text_length) {
  if (display_text == GetText())
    return false;

  NSAttributedString* as = ApplyTextAttributes(display_text);
  [field_ setText:as userTextLength:user_text_length];
  model()->OnChanged();
  return true;
}

void OmniboxViewIOS::OnBeforePossibleChange() {
  GetState(&state_before_change_);
  marked_text_before_change_ = [[field_ markedText] copy];
}

bool OmniboxViewIOS::OnAfterPossibleChange(bool allow_keyword_ui_change) {
  State new_state;
  GetState(&new_state);
  // Manually update the selection state after calling GetState().
  new_state.sel_start = current_selection_.location;
  new_state.sel_end = current_selection_.location + current_selection_.length;

  OmniboxView::StateChanges state_changes =
      GetStateChanges(state_before_change_, new_state);

  // iOS does not supports KeywordProvider, so never allow keyword UI changes.
  const bool something_changed =
      model()->OnAfterPossibleChange(state_changes, allow_keyword_ui_change);

  model()->OnChanged();

  // TODO(justincohen): Find a different place to call this. Give the omnibox
  // a chance to update the alignment for a text direction change.
  [field_ updateTextDirection];
  return something_changed;
}

bool OmniboxViewIOS::IsImeComposing() const {
  return [field_ markedTextRange] != nil;
}

bool OmniboxViewIOS::IsIndicatingQueryRefinement() const {
  return false;
}

bool OmniboxViewIOS::IsSelectAll() const {
  return false;
}

void OmniboxViewIOS::GetSelectionBounds(base::string16::size_type* start,
                                        base::string16::size_type* end) const {
  if ([field_ isFirstResponder]) {
    NSRange selected_range = [field_ selectedNSRange];
    *start = selected_range.location;
    *end = selected_range.location + selected_range.length;
  } else {
    *start = *end = 0;
  }
}

gfx::NativeView OmniboxViewIOS::GetNativeView() const {
  return nullptr;
}

gfx::NativeView OmniboxViewIOS::GetRelativeWindowForPopup() const {
  return nullptr;
}

int OmniboxViewIOS::GetTextWidth() const {
  return 0;
}

// TODO(crbug.com/329527): [Merge r241107] implement OmniboxViewIOS::GetWidth().
int OmniboxViewIOS::GetWidth() const {
  return 0;
}

void OmniboxViewIOS::OnDidBeginEditing() {
  // Reset the changed flag.
  omnibox_interacted_while_focused_ = NO;

  // If Open from Clipboard offers a suggestion, the popup may be opened when
  // |OnSetFocus| is called on the model. The state of the popup is saved early
  // to ignore that case.
  DCHECK(popup_provider_);
  bool popup_was_open_before_editing_began = popup_provider_->IsPopupOpen();

  // Text attributes (e.g. text color) should not be shown while editing, so
  // strip them out by calling setText (as opposed to setAttributedText).
  [field_ setText:[field_ text]];
  OnBeforePossibleChange();
  // In the case where the user taps the fakebox on the Google landing page,
  // or from the secondary toolbar search button, the focus source is already
  // set to FAKEBOX or SEARCH_BUTTON respectively. Otherwise, set it to OMNIBOX.
  if (model()->focus_source() != OmniboxEditModel::FocusSource::FAKEBOX &&
      model()->focus_source() != OmniboxEditModel::FocusSource::SEARCH_BUTTON) {
    model()->set_focus_source(OmniboxEditModel::FocusSource::OMNIBOX);
  }

  model()->OnSetFocus(false);

  // If the omnibox is displaying a URL and the popup is not showing, set the
  // field into pre-editing state.  If the omnibox is displaying search terms,
  // leave the default behavior of positioning the cursor at the end of the
  // text.  If the popup is already open, that means that the omnibox is
  // regaining focus after a popup scroll took focus away, so the pre-edit
  // behavior should not be invoked.
  if (!popup_was_open_before_editing_began)
    [field_ enterPreEditState];

  UpdateRightDecorations();

  // Before UI Refresh, The controller looks at the current pre-edit state, so
  // the call to OnSetFocus() must come after entering pre-edit. In UI Refresh,
  // |controller_| is only forwarding the call to the BVC. This should only
  // happen when the omnibox is being focused and it starts showing the popup;
  // if the popup was already open, no need to call this.
  if (IsUIRefreshPhase1Enabled()) {
    if (!popup_was_open_before_editing_began)
      controller_->OnSetFocus();
  } else {
    controller_->OnSetFocus();
  }
}

void OmniboxViewIOS::OnDidEndEditing() {
  CloseOmniboxPopup();
  model()->OnWillKillFocus();
  model()->OnKillFocus();
  if ([field_ isPreEditing])
    [field_ exitPreEditState];

  UpdateRightDecorations();

  // The controller looks at the current pre-edit state, so the call to
  // OnKillFocus() must come after exiting pre-edit.
  controller_->OnKillFocus();

  // Blow away any in-progress edits.
  RevertAll();
  DCHECK(![field_ hasAutocompleteText]);

  if (!omnibox_interacted_while_focused_) {
    RecordAction(
        UserMetricsAction("Mobile_FocusedDefocusedOmnibox_WithNoAction"));
  }
}

bool OmniboxViewIOS::OnWillChange(NSRange range, NSString* new_text) {
  bool ok_to_change = true;

  // It may be unsafe to modify the contents of the field.
  if (ShouldIgnoreUserInputDueToPendingVoiceSearch()) {
    return false;
  }

  if ([field_ isPreEditing]) {
    [field_ setClearingPreEditText:YES];

    // Exit the pre-editing state in OnWillChange() instead of OnDidChange(), as
    // that allows IME to continue working.  The following code clears the text
    // field but continues the normal text editing flow, so UIKit behaves as
    // though the user had typed into an empty field.
    [field_ exitPreEditState];

    // Clearing the text field will trigger a call to OnDidChange().  This is
    // ok, because the autocomplete system will process it as if the user had
    // deleted all the omnibox text.
    [field_ setText:@""];

    // Reset |range| to be of zero-length at location zero, as the field is now
    // cleared.
    range = NSMakeRange(0, 0);
  }

  // Figure out the old and current (new) selections.  Assume the new selection
  // will be of zero-length, located at the end of |new_text|.
  NSRange old_range = range;
  NSRange new_range = NSMakeRange(range.location + [new_text length], 0);

  // We may need to fix up the old and new ranges in the case where autocomplete
  // text was showing.  If there is autocomplete text, assume it was selected.
  // If the change is deleting one character from the end of the actual text,
  // disallow the change, but clear the autocomplete text and call OnDidChange
  // directly.  If there is autocomplete text AND a text field selection, or if
  // the user entered multiple characters, clear the autocomplete text and
  // pretend it never existed.
  if ([field_ hasAutocompleteText]) {
    bool adding_text = (range.length < [new_text length]);
    bool deleting_text = (range.length > [new_text length]);

    if (adding_text) {
      // TODO(rohitrao): What about cases where [new_text length] > 1?  This
      // could happen if an IME completion inserts multiple characters at once,
      // or if the user pastes some text in.  Let's loosen this test to allow
      // multiple characters, as long as the "old range" ends at the end of the
      // permanent text.
      if ([new_text length] == 1 && range.location == [[field_ text] length]) {
        old_range = NSMakeRange([[field_ text] length],
                                [field_ autocompleteText].length());
      }
    } else if (deleting_text) {
      if ([new_text length] == 0 &&
          range.location == [[field_ text] length] - 1) {
        ok_to_change = false;
      }
    }
  }

  // Update variables needed by OnDidChange() and GetState().
  old_selection_ = old_range;
  current_selection_ = new_range;

  // Store the displayed text.  Older version of Chrome used to clear the
  // autocomplete text here as well, but on iOS7 doing this causes the inline
  // autocomplete text to flicker, so the call was moved to the start on
  // OnDidChange().
  GetState(&state_before_change_);
  // Manually update the selection state after calling GetState().
  state_before_change_.sel_start = old_selection_.location;
  state_before_change_.sel_end =
      old_selection_.location + old_selection_.length;

  if (!ok_to_change) {
    // Force a change in the autocomplete system, since we won't get an
    // OnDidChange() message.
    OnDidChange(true);
  }

  return ok_to_change;
}

void OmniboxViewIOS::OnDidChange(bool processing_user_event) {
  omnibox_interacted_while_focused_ = YES;
  DCHECK(processing_user_event);
  // Sanitize pasted text.
  if (model()->is_pasting()) {
    base::string16 pastedText = base::SysNSStringToUTF16([field_ text]);
    base::string16 newText = OmniboxView::SanitizeTextForPaste(pastedText);
    if (pastedText != newText) {
      [field_ setText:base::SysUTF16ToNSString(newText)];
    }
  }

  UpdateRightDecorations();

  // Clear the autocomplete text, since the omnibox model does not expect to see
  // it in OnAfterPossibleChange().  Clearing the text here should not cause
  // flicker as the UI will not get a chance to redraw before the new
  // autocomplete text is set by the model.
  [field_ clearAutocompleteText];
  [field_ setClearingPreEditText:NO];

  // Generally do not notify the autocomplete system of a text change unless the
  // change was a direct result of a user event.  One exception is if the marked
  // text changed, which could happen through a delayed IME recognition
  // callback.
  bool proceed_without_user_event = false;

  // The IME exception does not work for Korean text, because Korean does not
  // seem to ever have marked text.  It simply replaces or modifies previous
  // characters as you type.  Always proceed without user input if the
  // Korean keyboard is currently active.
  NSString* current_language = [[field_ textInputMode] primaryLanguage];

  if ([current_language hasPrefix:@"ko-"]) {
    proceed_without_user_event = true;
  } else {
    NSString* current_marked_text = [field_ markedText];

    // The IME exception kicks in if the current marked text is not equal to the
    // previous marked text.  Two nil strings should be considered equal, so
    // There is logic to avoid calling into isEqual: in that case.
    proceed_without_user_event =
        (marked_text_before_change_ || current_marked_text) &&
        ![current_marked_text isEqual:marked_text_before_change_];
  }

  if (!processing_user_event && !proceed_without_user_event)
    return;

  // TODO(crbug.com/564599): OnAfterPossibleChange() now takes an argument. It
  // use to not take an argument and was defaulting to false, so as it is
  // unclear what the correct value is, using what was that before seems
  // consistent.
  OnAfterPossibleChange(false);
  OnBeforePossibleChange();
}

void OmniboxViewIOS::OnAccept() {
  // It may be unsafe to modify the contents of the field.
  // Note that by happy coincidence, the |textFieldDidReturn| delegate method
  // always returns NO, which is the desired behavior in this situation.
  if (ShouldIgnoreUserInputDueToPendingVoiceSearch()) {
    return;
  }

  base::RecordAction(UserMetricsAction("MobileOmniboxUse"));

  WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB;
  model()->AcceptInput(disposition, false);
  RevertAll();
}

void OmniboxViewIOS::OnClear() {
  [field_ clearAutocompleteText];
  [field_ exitPreEditState];
}

bool OmniboxViewIOS::OnCopy() {
  omnibox_interacted_while_focused_ = YES;
  UIPasteboard* board = [UIPasteboard generalPasteboard];
  NSString* selectedText = nil;
  NSInteger start_location = 0;
  if ([field_ isPreEditing]) {
    selectedText = [field_ preEditText];
    start_location = 0;
  } else {
    UITextRange* selected_range = [field_ selectedTextRange];
    selectedText = [field_ textInRange:selected_range];
    UITextPosition* start = [field_ beginningOfDocument];
    // The following call to |-offsetFromPosition:toPosition:| gives the offset
    // in terms of the number of "visible characters."  The documentation does
    // not specify whether this means glyphs or UTF16 chars.  This does not
    // matter for the current implementation of AdjustTextForCopy(), but it may
    // become an issue at some point.
    start_location =
        [field_ offsetFromPosition:start toPosition:[selected_range start]];
  }
  base::string16 text = base::SysNSStringToUTF16(selectedText);

  GURL url;
  bool write_url = false;
  model()->AdjustTextForCopy(start_location, &text, &url, &write_url);

  // Create the pasteboard item manually because the pasteboard expects a single
  // item with multiple representations.  This is expressed as a single
  // NSDictionary with multiple keys, one for each representation.
  NSMutableDictionary* item = [NSMutableDictionary dictionaryWithCapacity:2];
  [item setObject:base::SysUTF16ToNSString(text)
           forKey:(NSString*)kUTTypePlainText];

  if (write_url)
    [item setObject:net::NSURLWithGURL(url) forKey:(NSString*)kUTTypeURL];

  board.items = [NSArray arrayWithObject:item];
  return true;
}

void OmniboxViewIOS::WillPaste() {
  model()->OnPaste();
}

// static
UIColor* OmniboxViewIOS::GetSecureTextColor(
    security_state::SecurityLevel security_level,
    bool in_dark_mode) {
  if (security_level == security_state::EV_SECURE ||
      security_level == security_state::SECURE) {
    return in_dark_mode ? IncognitoSecureTextColor() : SecureTextColor();
  }

  // Don't color strikethrough in dark mode. See https://crbug.com/635004#c6
  if (security_level == security_state::DANGEROUS && !in_dark_mode)
    return ErrorTextColor();

  return nil;
}

void OmniboxViewIOS::SetEmphasis(bool emphasize, const gfx::Range& range) {
  if (IsRefreshLocationBarEnabled()) {
    return;
  }

  NSRange ns_range = range.IsValid()
                         ? range.ToNSRange()
                         : NSMakeRange(0, [attributing_display_string_ length]);

  [attributing_display_string_
      addAttribute:NSForegroundColorAttributeName
             value:(emphasize) ? [field_ displayedTextColor] : BaseTextColor()
             range:ns_range];
}

void OmniboxViewIOS::UpdateSchemeStyle(const gfx::Range& range) {
  if (IsRefreshLocationBarEnabled()) {
    return;
  }

  if (!range.IsValid())
    return;

  const security_state::SecurityLevel security_level =
      controller()->GetToolbarModel()->GetSecurityLevel(false);

  if ((security_level == security_state::NONE) ||
      (security_level == security_state::HTTP_SHOW_WARNING)) {
    return;
  }

  DCHECK_NE(security_state::SECURE_WITH_POLICY_INSTALLED_CERT, security_level);

  if (security_level == security_state::DANGEROUS) {
    if (use_strikethrough_workaround_) {
      // Workaround: Add extra attribute to allow strikethough to apply on iOS
      // 10.3+. See https://crbug.com/699702 for discussion.
      [attributing_display_string_
          addAttribute:NSBaselineOffsetAttributeName
                 value:@0
                 range:NSMakeRange(0, [attributing_display_string_ length])];
    }

    NSRange strikethroughRange = range.ToNSRange();

    if (base::ios::IsRunningOnOrLater(11, 0, 0) &&
        !base::ios::IsRunningOnOrLater(11, 2, 0)) {
      // This is a workaround for an iOS bug (crbug.com/751801) fixed in 11.2.
      // In iOS 11, UITextField has a bug: when the first character has
      // strikethrough attribute, typing and setting text without strikethrough
      // attribute will still result in strikethrough. The following is a
      // workaround that prevents crossing out the first character.
      if (strikethroughRange.location == 0 && strikethroughRange.length > 0) {
        strikethroughRange.location += 1;
        strikethroughRange.length -= 1;
      }
    }

    // Add a strikethrough through the scheme.
    [attributing_display_string_
        addAttribute:NSStrikethroughStyleAttributeName
               value:[NSNumber numberWithInteger:NSUnderlineStyleSingle]
               range:strikethroughRange];
  }

  UIColor* color = GetSecureTextColor(security_level, [field_ incognito]);
  if (color) {
    [attributing_display_string_ addAttribute:NSForegroundColorAttributeName
                                        value:color
                                        range:range.ToNSRange()];
  }
}

NSAttributedString* OmniboxViewIOS::ApplyTextAttributes(
    const base::string16& text) {
  NSMutableAttributedString* as = [[NSMutableAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(text)];
  // Cache a pointer to the attributed string to allow the superclass'
  // virtual method invocations to add attributes.
  DCHECK(attributing_display_string_ == nil);
  base::AutoReset<NSMutableAttributedString*> resetter(
      &attributing_display_string_, as);
  UpdateTextStyle(text, model()->CurrentTextIsURL(),
                  AutocompleteSchemeClassifierImpl());
  return as;
}

void OmniboxViewIOS::UpdateAppearance() {
  // If Siri is thinking, treat that as user input being in progress.  It is
  // unsafe to modify the text field while voice entry is pending.
  if (model()->ResetDisplayTexts()) {
    // Revert everything to the baseline look.
    RevertAll();
  } else if (!model()->has_focus() &&
             !ShouldIgnoreUserInputDueToPendingVoiceSearch()) {
    // Even if the change wasn't "user visible" to the model, it still may be
    // necessary to re-color to the URL string.  Only do this if the omnibox is
    // not currently focused.
    NSAttributedString* as =
        ApplyTextAttributes(model()->GetPermanentDisplayText());
    [field_ setText:as userTextLength:[as length]];
  }
}

void OmniboxViewIOS::CreateClearTextIcon(bool is_incognito) {
  if (IsRefreshLocationBarEnabled()) {
    // In UI Refresh, the view controller sets up the clear button.
    return;
  }

  UIButton* button = [UIButton buttonWithType:UIButtonTypeCustom];
  UIImage* omniBoxClearImage = is_incognito
                                   ? NativeImage(IDR_IOS_OMNIBOX_CLEAR_OTR)
                                   : NativeImage(IDR_IOS_OMNIBOX_CLEAR);
  UIImage* omniBoxClearPressedImage =
      is_incognito ? NativeImage(IDR_IOS_OMNIBOX_CLEAR_OTR_PRESSED)
                   : NativeImage(IDR_IOS_OMNIBOX_CLEAR_PRESSED);
  [button setImage:omniBoxClearImage forState:UIControlStateNormal];
  [button setImage:omniBoxClearPressedImage forState:UIControlStateHighlighted];

  CGRect frame = CGRectZero;
  frame.size = CGSizeMake(kClearTextButtonWidth, kClearTextButtonHeight);
  [button setFrame:frame];

  clear_button_bridge_ =
      [[OmniboxClearButtonBridge alloc] initWithOmniboxView:this];
  [button addTarget:clear_button_bridge_
                action:@selector(clearText)
      forControlEvents:UIControlEventTouchUpInside];
  clear_text_button_ = button;

  SetA11yLabelAndUiAutomationName(clear_text_button_,
                                  IDS_IOS_ACCNAME_CLEAR_TEXT, @"Clear Text");
}

void OmniboxViewIOS::UpdateRightDecorations() {
  if (IsRefreshLocationBarEnabled()) {
    return;
  }

  DCHECK(clear_text_button_);
  if (!model()->has_focus()) {
    // Do nothing for iPhone. The right view will be set to nil after the
    // omnibox animation is completed.
    if (IsIPadIdiom())
      [field_ setRightView:nil];
  } else if ([field_ displayedText].empty()) {
    [field_ setRightView:nil];
  } else {
    [field_ setRightView:clear_text_button_];
    [clear_text_button_ setAlpha:1];
  }
}

void OmniboxViewIOS::OnDeleteBackward() {
  if ([field_ text].length == 0) {
    // If the user taps backspace while the pre-edit text is showing,
    // OnWillChange is invoked before this method and sets the text to an empty
    // string, so use the |clearingPreEditText| to determine if the chip should
    // be cleared or not.
    if ([field_ clearingPreEditText]) {
      // In the case where backspace is tapped while in pre-edit mode,
      // OnWillChange is called but OnDidChange is never called so ensure the
      // clearingPreEditText flag is set to false again.
      [field_ setClearingPreEditText:NO];
      // Explicitly set the input-in-progress flag. Normally this is set via
      // in model()->OnAfterPossibleChange, but in this case the text has been
      // set to the empty string by OnWillChange so when OnAfterPossibleChange
      // checks if the text has changed it does not see any difference so it
      // never sets the input-in-progress flag.
      model()->SetInputInProgress(YES);
    } else {
      RemoveQueryRefinementChip();
    }
  }
}

void OmniboxViewIOS::ClearText() {
  // It may be unsafe to modify the contents of the field.
  if (ShouldIgnoreUserInputDueToPendingVoiceSearch()) {
    return;
  }

  // Ensure omnibox is first responder. This will bring up the keyboard so the
  // user can start typing a new query.
  if (![field_ isFirstResponder])
    [field_ becomeFirstResponder];
  if ([field_ text].length == 0) {
    // If |field_| is empty, remove the query refinement chip.
    RemoveQueryRefinementChip();
  } else {
    // Otherwise, just remove the text in the omnibox.
    // Calling -[UITextField setText:] does not trigger
    // -[id<UITextFieldDelegate> textDidChange] so it must be called explicitly.
    OnClear();
    [field_ setText:@""];
    OnDidChange(YES);
  }
  // Calling OnDidChange() can trigger a scroll event, which removes focus from
  // the omnibox.
  [field_ becomeFirstResponder];
}

void OmniboxViewIOS::RemoveQueryRefinementChip() {
  controller_->OnChanged();
}

bool OmniboxViewIOS::ShouldIgnoreUserInputDueToPendingVoiceSearch() {
  // When the response of the iOS voice entry is pending a spinning wheel is
  // visible.  The spinner's location is marked in [self text] as a Unicode
  // "Object Replacement Character".
  // http://www.fileformat.info/info/unicode/char/fffc/index.htm
  NSString* objectReplacementChar =
      [NSString stringWithFormat:@"%C", (unichar)0xFFFC];
  return [[field_ text] rangeOfString:objectReplacementChar].length > 0;
}

void OmniboxViewIOS::HideKeyboardAndEndEditing() {
  [field_ resignFirstResponder];

  // Handle the case where a phone-format ombniox has already resigned first
  // responder because the popup was scrolled.  If the model still has focus,
  // dismiss again. This should only happen on iPhone.
  if (model()->has_focus()) {
    DCHECK(!IsIPadIdiom());
    this->OnDidEndEditing();
  }
}

void OmniboxViewIOS::HideKeyboard() {
  [field_ resignFirstResponder];
}

void OmniboxViewIOS::FocusOmnibox() {
  [field_ becomeFirstResponder];
}

BOOL OmniboxViewIOS::IsPopupOpen() {
  DCHECK(popup_provider_);
  return popup_provider_->IsPopupOpen();
}

int OmniboxViewIOS::GetIcon(bool offlinePage) const {
  if (!IsEditingOrEmpty()) {
    if (offlinePage) {
      return IDR_IOS_OMNIBOX_OFFLINE;
    }
    return GetIconForSecurityState(
        controller()->GetToolbarModel()->GetSecurityLevel(false));
  }
  return GetIconForAutocompleteMatchType(
      model() ? model()->CurrentMatch(nullptr).type
              : AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      /* is_starred */ false, /* is_incognito */ false);
}

int OmniboxViewIOS::GetOmniboxTextLength() const {
  return [field_ displayedText].length();
}

void OmniboxViewIOS::EmphasizeURLComponents() {
// TODO(rohitrao): Implement this function using code like below.  This code
// is being left out for now because it was not present before the OmniboxView
// rewrite.
#if 0
  // When editing is in progress, the url text is not colored, so there is
  // nothing to emphasize.  (Calling SetText() in that situation would also be
  // harmful, as it would reset the carat position to the end of the text.)
  if (!IsEditingOrEmpty())
    SetText(GetText());
#endif
}

#pragma mark - OmniboxPopupViewSuggestionsDelegate

void OmniboxViewIOS::OnTopmostSuggestionImageChanged(
    AutocompleteMatchType::Type type) {
  [left_image_consumer_ setLeftImageForAutocompleteType:type];
}

void OmniboxViewIOS::OnResultsChanged(const AutocompleteResult& result) {
  if (ignore_popup_updates_) {
    // Please contact rohitrao@ if the following DCHECK ever fires.  If
    // |ignore_popup_updates_| is true but |result| is not empty, then the new
    // prerender code in ChromeOmniboxClientIOS will incorrectly discard its
    // prerender.
    // TODO(crbug.com/754050): Remove this whole method once we are reasonably
    // confident that we are not throwing away prerenders.
    DCHECK(result.empty());
  }
}

void OmniboxViewIOS::OnPopupDidScroll() {
  if (!IsIPadIdiom()) {
    this->HideKeyboard();
  }
}

void OmniboxViewIOS::OnSelectedMatchForAppending(const base::string16& str) {
  // Exit preedit state and append the match. Refocus if necessary.
  if ([field_ isPreEditing])
    [field_ exitPreEditState];
  this->SetUserText(str);
  this->FocusOmnibox();
}

void OmniboxViewIOS::OnSelectedMatchForOpening(
    AutocompleteMatch match,
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const base::string16& pasted_text,
    size_t index) {
  this->OpenMatch(match, disposition, alternate_nav_url, pasted_text, index,
                  base::TimeTicks());
}
