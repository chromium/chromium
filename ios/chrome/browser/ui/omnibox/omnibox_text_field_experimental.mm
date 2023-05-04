// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_experimental.h"

#import <CoreText/CoreText.h>

#import "base/check_op.h"
#import "base/command_line.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/grit/components_scaled_resources.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/animation_util.h"
#import "ios/chrome/browser/shared/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/shared/ui/util/reversed_animation.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "skia/ext/skia_utils_ios.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "ui/gfx/color_palette.h"
#import "ui/gfx/image/image.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"
#import "ui/gfx/scoped_cg_context_save_gstate_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The default omnibox text color (used while editing).
UIColor* TextColor() {
  return [UIColor colorNamed:kTextPrimaryColor];
}

NSString* const kOmniboxFadeAnimationKey = @"OmniboxFadeAnimation";

}  // namespace

@interface OmniboxTextFieldExperimental ()

// Font to use in regular x regular size class. If not set, the regular font is
// used instead.
@property(nonatomic, strong, readonly) UIFont* largerFont;
// Font to use in Compact x Any and Any x Compact size class.
@property(nonatomic, strong, readonly) UIFont* normalFont;

// Returns the layers affected by animations added by `-animateFadeWithStyle:`.
- (NSArray*)fadeAnimationLayers;
// Font that should be used in current size class.
- (UIFont*)currentFont;

// Length of autocomplete text.
@property(nonatomic) size_t autocompleteTextLength;

@end

@implementation OmniboxTextFieldExperimental

@dynamic delegate;

#pragma mark - Public methods
// Overload to allow for code-based initialization.
- (instancetype)initWithFrame:(CGRect)frame {
  return [self initWithFrame:frame textColor:TextColor() tintColor:nil];
}

- (instancetype)initWithFrame:(CGRect)frame
                    textColor:(UIColor*)textColor
                    tintColor:(UIColor*)tintColor {
  self = [super initWithFrame:frame];
  if (self) {
    if (tintColor) {
      [self setTintColor:tintColor];
    }
    self.textColor = textColor;
    self.autocorrectionType = UITextAutocorrectionTypeNo;
    self.autocapitalizationType = UITextAutocapitalizationTypeNone;
    self.enablesReturnKeyAutomatically = YES;
    self.returnKeyType = UIReturnKeyGo;
    self.contentVerticalAlignment = UIControlContentVerticalAlignmentCenter;
    self.spellCheckingType = UITextSpellCheckingTypeNo;
    self.textAlignment = NSTextAlignmentNatural;
    self.keyboardType = UIKeyboardTypeWebSearch;
    self.smartQuotesType = UITextSmartQuotesTypeNo;

    // Disable drag on iPhone because there's nowhere to drag to
    if (ui::GetDeviceFormFactor() != ui::DEVICE_FORM_FACTOR_TABLET) {
      self.textDragInteraction.enabled = NO;
    }

    // Force initial layout of internal text label.  Needed for omnibox
    // animations that will otherwise animate the text label from origin {0, 0}.
    [super setText:@" "];

    [self addGestureRecognizer:[[UITapGestureRecognizer alloc]
                                   initWithTarget:self
                                           action:@selector(handleTap)]];
  }
  return self;
}

- (instancetype)initWithCoder:(nonnull NSCoder*)aDecoder {
  NOTREACHED();
  return nil;
}

- (void)setText:(NSAttributedString*)text
    userTextLength:(size_t)userTextLength {
  self.autocompleteTextLength = text.length - userTextLength;

  DCHECK_LE(userTextLength, text.length);
  if (userTextLength > 0) {
    [self exitPreEditState];
  }

  NSUInteger autocompleteLength = text.length - userTextLength;
  [self setTextInternal:text autocompleteLength:autocompleteLength];
}

- (void)insertTextWhileEditing:(NSString*)text {
  // This method should only be called while editing.
  DCHECK([self isFirstResponder]);

  if ([self markedTextRange] != nil)
    [self unmarkText];

  NSRange selectedNSRange = [self selectedNSRange];
  if (!self.delegate || [self.delegate textField:self
                            shouldChangeCharactersInRange:selectedNSRange
                                        replacementString:text]) {
    [self replaceRange:[self selectedTextRange] withText:text];
  }
}

- (NSString*)autocompleteText {
  if (self.autocompleteTextLength > 0) {
    // In crbug.com/1237851, sometimes self.autocompleteTextLength is greater
    // than self.text.length, causing the subtraction below to overflow,
    // breaking
    // `-substringToIndex:`. This shouldn't happen, so use the DCHECK to catch
    // it to help debug and default to the end of the string if an overflow
    // would occur.
    DCHECK(self.text.length >= self.autocompleteTextLength);
    NSUInteger userTextEndIndex =
        self.text.length >= self.autocompleteTextLength
            ? self.text.length - self.autocompleteTextLength
            : self.text.length;
    return [self.text substringFromIndex:userTextEndIndex];
  }
  return @"";
}

- (NSString*)userText {
  // In crbug.com/1237851, sometimes self.autocompleteTextLength is greater than
  // self.text.length, causing the subtraction below to overflow, breaking
  // `-substringToIndex:`. This shouldn't happen, so use the DCHECK to catch it
  // to help debug and default to the end of the string if an overflow would
  // occur.
  DCHECK(self.text.length >= self.autocompleteTextLength);
  NSUInteger userTextEndIndex =
      self.text.length >= self.autocompleteTextLength
          ? self.text.length - self.autocompleteTextLength
          : self.text.length;
  return [self.text substringToIndex:userTextEndIndex];
}

- (NSString*)markedText {
  DCHECK([self conformsToProtocol:@protocol(UITextInput)]);
  return [self textInRange:[self markedTextRange]];
}

- (NSRange)selectedNSRange {
  DCHECK([self isFirstResponder]);
  UITextPosition* beginning = [self beginningOfDocument];
  UITextRange* selectedRange = [self selectedTextRange];
  NSInteger start = [self offsetFromPosition:beginning
                                  toPosition:[selectedRange start]];
  NSInteger length = [self offsetFromPosition:[selectedRange start]
                                   toPosition:[selectedRange end]];
  return NSMakeRange(start, length);
}

- (NSTextAlignment)bestTextAlignment {
  if ([self isFirstResponder]) {
    return [self bestAlignmentForText:self.text];
  }
  return NSTextAlignmentNatural;
}

- (UISemanticContentAttribute)bestSemanticContentAttribute {
  // This method will be called in response to
  // UITextInputCurrentInputModeDidChangeNotification. At this
  // point, the baseWritingDirectionForPosition doesn't yet return the correct
  // direction if the text field is empty. Instead, treat this as a special case
  // and calculate the direction from the keyboard locale if there is no text.
  if (self.text.length == 0) {
    NSLocaleLanguageDirection direction = [NSLocale
        characterDirectionForLanguage:self.textInputMode.primaryLanguage];
    return direction == NSLocaleLanguageDirectionRightToLeft
               ? UISemanticContentAttributeForceRightToLeft
               : UISemanticContentAttributeForceLeftToRight;
  }

  [self setTextAlignment:NSTextAlignmentNatural];

  NSWritingDirection textDirection =
      [self baseWritingDirectionForPosition:[self beginningOfDocument]
                                inDirection:UITextStorageDirectionForward];
  NSLocaleLanguageDirection currentLocaleDirection = [NSLocale
      characterDirectionForLanguage:NSLocale.currentLocale.languageCode];

  if ((textDirection == NSWritingDirectionLeftToRight &&
       currentLocaleDirection == NSLocaleLanguageDirectionLeftToRight) ||
      (textDirection == NSWritingDirectionRightToLeft &&
       currentLocaleDirection == NSLocaleLanguageDirectionRightToLeft)) {
    return UISemanticContentAttributeUnspecified;
  }

  return textDirection == NSWritingDirectionRightToLeft
             ? UISemanticContentAttributeForceRightToLeft
             : UISemanticContentAttributeForceLeftToRight;
}

// Normally NSTextAlignmentNatural would handle text alignment automatically,
// but there are numerous edge case issues with it, so it's simpler to just
// manually update the text alignment and writing direction of the UITextField.
- (void)updateTextDirection {
  // If the keyboard language direction does not match the device
  // language direction, the alignment of the placeholder text will be off.
  if (self.text.length == 0) {
    NSLocaleLanguageDirection direction = [NSLocale
        characterDirectionForLanguage:self.textInputMode.primaryLanguage];
    if (direction == NSLocaleLanguageDirectionRightToLeft) {
      [self setTextAlignment:NSTextAlignmentRight];
    } else {
      [self setTextAlignment:NSTextAlignmentLeft];
    }
  } else {
    [self setTextAlignment:NSTextAlignmentNatural];
  }
}

#pragma mark animations

- (void)animateFadeWithStyle:(OmniboxTextFieldFadeStyle)style {
  // Animation values
  BOOL isFadingIn = (style == OMNIBOX_TEXT_FIELD_FADE_STYLE_IN);
  CGFloat beginOpacity = isFadingIn ? 0.0 : 1.0;
  CGFloat endOpacity = isFadingIn ? 1.0 : 0.0;
  CAMediaTimingFunction* opacityTiming = MaterialTimingFunction(
      isFadingIn ? MaterialCurveEaseOut : MaterialCurveEaseIn);
  CFTimeInterval delay = isFadingIn ? kMaterialDuration8 : 0.0;

  CAAnimation* labelAnimation = OpacityAnimationMake(beginOpacity, endOpacity);
  labelAnimation.duration =
      isFadingIn ? kMaterialDuration6 : kMaterialDuration8;
  labelAnimation.timingFunction = opacityTiming;
  labelAnimation = DelayedAnimationMake(labelAnimation, delay);
  CAAnimation* auxillaryViewAnimation =
      OpacityAnimationMake(beginOpacity, endOpacity);
  auxillaryViewAnimation.duration = kMaterialDuration8;
  auxillaryViewAnimation.timingFunction = opacityTiming;
  auxillaryViewAnimation = DelayedAnimationMake(auxillaryViewAnimation, delay);

  for (UIView* subview in self.subviews) {
    if ([subview isKindOfClass:[UILabel class]]) {
      [subview.layer addAnimation:labelAnimation
                           forKey:kOmniboxFadeAnimationKey];
    } else {
      [subview.layer addAnimation:auxillaryViewAnimation
                           forKey:kOmniboxFadeAnimationKey];
    }
  }
}

- (void)cleanUpFadeAnimations {
  RemoveAnimationForKeyFromLayers(kOmniboxFadeAnimationKey,
                                  [self fadeAnimationLayers]);
}

#pragma mark - UI Refresh animation public helpers

- (CGFloat)offsetForString:(NSString*)string {
  // Sometimes `string` is not contained in self.text, for example for
  // https://en.m.wikipedia.org/foo the `string` might be "en.wikipedia.org" if
  // the scheme and the "m." trivial subdomain are stripped. In this case,
  // default to a reasonable prefix string to give a plausible offset.
  NSString* prefixString = @"https://";

  if (string.length > 0 && [self.text containsString:string]) {
    NSRange range = [self.text rangeOfString:string];
    NSRange prefixRange = NSMakeRange(0, range.location);
    prefixString = [self.text substringWithRange:prefixRange];
  }

  return [prefixString
             sizeWithAttributes:@{NSFontAttributeName : self.currentFont}]
      .width;
}

#pragma mark pre-edit

// Creates a UILabel based on the current dimension of the text field and
// displays the URL in the UILabel so it appears properly aligned to the URL.
- (void)enterPreEditState {
  // Empty omnibox should show the insertion point immediately. There is
  // nothing to erase.
  if (!self.text.length || UIAccessibilityIsVoiceOverRunning())
    return;

  self.preEditing = true;

  self.defaultTextAttributes = @{
    NSBackgroundColorAttributeName : self.selectedTextBackgroundColor,
    NSFontAttributeName : self.currentFont
  };

  self.clearsOnInsertion = true;
}

// Finishes pre-edit state by removing the UILabel with the URL.
- (void)exitPreEditState {
  self.preEditing = false;
  self.clearsOnInsertion = false;

  self.defaultTextAttributes = @{
    NSFontAttributeName : self.currentFont,
    NSBackgroundColorAttributeName : UIColor.clearColor
  };
}

#pragma mark - Properties

- (UIFont*)largerFont {
  return PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleBody, self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryAccessibilityExtraLarge);
}

- (UIFont*)normalFont {
  return PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleSubheadline,
      self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryAccessibilityExtraLarge);
}

- (UIFont*)currentFont {
  return IsCompactWidth(self) ? self.normalFont : self.largerFont;
}

#pragma mark - Private methods

#pragma mark - UITextField

// Ensures that attributedText always uses the proper style attributes.
- (void)setAttributedText:(NSAttributedString*)attributedText {
  NSMutableAttributedString* mutableText = [attributedText mutableCopy];
  NSRange entireString = NSMakeRange(0, [mutableText length]);

  // Set the font.
  [mutableText addAttribute:NSFontAttributeName
                      value:self.currentFont
                      range:entireString];

  // When editing, use the default text color for all text.
  if (self.editing) {
    [mutableText addAttribute:NSForegroundColorAttributeName
                        value:self.textColor
                        range:entireString];
  } else {
    NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
    // URLs have their text direction set to to LTR (avoids RTL characters
    // making the URL render from right to left, as per the URL rendering
    // standard described here: https://url.spec.whatwg.org/#url-rendering
    [style setBaseWritingDirection:NSWritingDirectionLeftToRight];

    // Set linebreak mode to 'clipping' to ensure the text is never elided.
    // This is a workaround for iOS 6, where it appears that
    // [self.attributedText size] is not wide enough for the string (e.g. a URL
    // else ending with '.com' will be elided to end with '.c...'). It appears
    // to be off by one point so clipping is acceptable as it doesn't actually
    // cut off any of the text.
    [style setLineBreakMode:NSLineBreakByClipping];

    [mutableText addAttribute:NSParagraphStyleAttributeName
                        value:style
                        range:entireString];
  }

  [super setAttributedText:mutableText];
}

- (void)setPlaceholder:(NSString*)placeholder {
  if (placeholder && self.placeholderTextColor) {
    NSDictionary* attributes =
        @{NSForegroundColorAttributeName : self.placeholderTextColor};
    self.attributedPlaceholder =
        [[NSAttributedString alloc] initWithString:placeholder
                                        attributes:attributes];
  } else {
    [super setPlaceholder:placeholder];
  }
}

- (void)setText:(NSString*)text {
  NSAttributedString* as = [[NSAttributedString alloc] initWithString:text];
  if (self.text.length > 0 && as.length == 0) {
    // Remove the fade animations before the subviews are removed.
    [self cleanUpFadeAnimations];
  }
  [self setTextInternal:as autocompleteLength:0];
}

- (CGRect)textRectForBounds:(CGRect)bounds {
  CGRect newBounds = [super textRectForBounds:bounds];

  LayoutRect textRectLayout =
      LayoutRectForRectInBoundingRect(newBounds, bounds);

  return LayoutRectGetRect(textRectLayout);
}

- (CGRect)editingRectForBounds:(CGRect)bounds {
  CGRect editRect = [super editingRectForBounds:bounds];
  return editRect;
}

- (CGRect)caretRectForPosition:(UITextPosition*)position {
  return ([self hasAutocompleteText]) ? CGRectZero
                                      : [super caretRectForPosition:position];
}

#pragma mark - UITextInput

- (void)beginFloatingCursorAtPoint:(CGPoint)point {
  // Exit preedit because it blocks the view of the textfield.
  [self exitPreEditState];
  // Remove selection and put the caret at the end of the string.
  self.selectedTextRange = [self textRangeFromPosition:self.endOfDocument
                                            toPosition:self.endOfDocument];
  [super beginFloatingCursorAtPoint:point];
}

#pragma mark - UIView

- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  // Anything in the narrow bar above OmniboxTextFieldIOS view
  // will also activate the text field.
  if (point.y < 0)
    point.y = 0;
  return [super hitTest:point withEvent:event];
}

#pragma mark - UITraitCollection

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  // Reset the fonts to the appropriate ones in this size class.
  [self setFont:self.currentFont];
  // Reset the attributed text to apply the new font.
  [self setAttributedText:self.attributedText];
}

#pragma mark - UIResponder

// Triggered on tap gesture recognizer.
- (void)handleTap {
  if ([self isPreEditing]) {
    [self exitPreEditState];
    [super selectAll:self];
  } else if ([self hasAutocompleteText]) {
    // Accept selection.
    [self acceptAutocompleteText];
  } else {
    [self becomeFirstResponder];
    UIMenuController* menuController = [UIMenuController sharedMenuController];
    if (menuController.isMenuVisible) {
      [menuController hideMenu];
    } else {
      [menuController showMenuFromView:self rect:self.bounds];
    }
  }
}

- (void)select:(id)sender {
  if ([self isPreEditing]) {
    [self exitPreEditState];
  }
  [super select:sender];
}

- (void)selectAll:(id)sender {
  if ([self isPreEditing]) {
    [self exitPreEditState];
  }
  if ([self hasAutocompleteText]) {
    [self acceptAutocompleteText];
  }
  [super selectAll:sender];
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
  // If the text is not empty and there is selected text, show copy and cut.
  if ([self textInRange:self.selectedTextRange].length > 0 &&
      (action == @selector(cut:) || action == @selector(copy:))) {
    return YES;
  }

  // If the text is not empty and there is no selected text, show select
  if (self.text.length > 0 &&
      [self textInRange:self.selectedTextRange].length == 0 &&
      action == @selector(select:)) {
    return YES;
  }

  // If selected text is les than the text length, show selectAll.
  if ([self textInRange:self.selectedTextRange].length != self.text.length &&
      action == @selector(selectAll:)) {
    return YES;
  }

  // If there is pasteboard content, show paste.
  if (UIPasteboard.generalPasteboard.hasStrings && action == @selector
                                                       (paste:)) {
    return YES;
  }

  // Arrow keys are handled by OmniboxKeyboardDelegates, if they don't handle
  // them, default behavior of UITextField applies.
  if (action == @selector(forwardKeyCommandUp)) {
    return [self.omniboxKeyboardDelegate
        canPerformKeyboardAction:OmniboxKeyboardActionUpArrow];
  } else if (action == @selector(forwardKeyCommandDown)) {
    return [self.omniboxKeyboardDelegate
        canPerformKeyboardAction:OmniboxKeyboardActionDownArrow];
  } else if (action == @selector(forwardKeyCommandLeft)) {
    return [self.omniboxKeyboardDelegate
        canPerformKeyboardAction:OmniboxKeyboardActionLeftArrow];
  } else if (action == @selector(forwardKeyCommandRight)) {
    return [self.omniboxKeyboardDelegate
        canPerformKeyboardAction:OmniboxKeyboardActionRightArrow];
  }

  // Handle pre-edit shortcuts.
  if ([self isPreEditing]) {
    // Allow cut and copy in preedit.
    if ((action == @selector(copy:)) || (action == @selector(cut:))) {
      return YES;
    }
  }

  // Note that this NO does not keep other elements in the responder chain from
  // adding actions they handle to the menu.
  return NO;
}

#pragma mark Copy/Paste

// Overridden to allow for custom omnibox copy behavior.  This includes
// preprending http:// to the copied URL if needed.
- (void)copy:(id)sender {
  id<OmniboxTextFieldDelegate> delegate = self.delegate;

  // Must test for the onCopy method, since it's optional.
  if ([delegate respondsToSelector:@selector(onCopy)]) {
    [delegate onCopy];
  } else {
    [super copy:sender];
  }
}

- (void)cut:(id)sender {
  if ([self isPreEditing]) {
    [self copy:sender];
    [self exitPreEditState];
    NSAttributedString* emptyString = [[NSAttributedString alloc] init];
    [self setText:emptyString userTextLength:0];
  } else {
    [super cut:sender];
  }
}

// Overridden to notify the delegate that a paste is in progress.
- (void)paste:(id)sender {
  id delegate = self.delegate;
  if ([delegate respondsToSelector:@selector(willPaste)])
    [delegate willPaste];
  [super paste:sender];
}

#pragma mark UIPasteConfigurationSupporting

// Used by UIPasteControl to check if can paste.
- (BOOL)canPasteItemProviders:(NSArray<NSItemProvider*>*)itemProviders {
  if ([self.delegate respondsToSelector:@selector(canPasteItemProviders:)]) {
    return [self.delegate canPasteItemProviders:itemProviders];
  } else {
    return NO;
  }
}

// Used by UIPasteControl to paste.
- (void)pasteItemProviders:(NSArray<NSItemProvider*>*)itemProviders {
  if ([self.delegate respondsToSelector:@selector(pasteItemProviders:)]) {
    [self.delegate pasteItemProviders:itemProviders];
  }
}

#pragma mark UIKeyInput

// Override deleteBackward so that backspace clear autocomplete text.
- (void)deleteBackward {
  if ([self hasAutocompleteText]) {
    [self clearAutocompleteText];
    return;
  }
  // Must test for the onDeleteBackward method, since it's optional.
  if ([self.delegate respondsToSelector:@selector(onDeleteBackward)])
    [self.delegate onDeleteBackward];
  [super deleteBackward];
}

#pragma mark Key Command Forwarding

- (void)forwardKeyCommandUp {
  [self.omniboxKeyboardDelegate
      performKeyboardAction:OmniboxKeyboardActionUpArrow];
}

- (void)forwardKeyCommandDown {
  [self.omniboxKeyboardDelegate
      performKeyboardAction:OmniboxKeyboardActionDownArrow];
}

- (void)forwardKeyCommandLeft {
  [self.omniboxKeyboardDelegate
      performKeyboardAction:OmniboxKeyboardActionLeftArrow];
}

- (void)forwardKeyCommandRight {
  [self.omniboxKeyboardDelegate
      performKeyboardAction:OmniboxKeyboardActionRightArrow];
}

// Arrow keys are forwarded to the main OmniboxKeyboardDelegate that will
// dispatch them to OmniboxPopupViewController or OmniboxViewController, if they
// don't handle them, default behavior of UITextField applies.
- (NSArray<UIKeyCommand*>*)keyCommands {
  UIKeyCommand* commandUp =
      [UIKeyCommand keyCommandWithInput:UIKeyInputUpArrow
                          modifierFlags:0
                                 action:@selector(forwardKeyCommandUp)];
  UIKeyCommand* commandDown =
      [UIKeyCommand keyCommandWithInput:UIKeyInputDownArrow
                          modifierFlags:0
                                 action:@selector(forwardKeyCommandDown)];
  UIKeyCommand* commandLeft =
      [UIKeyCommand keyCommandWithInput:UIKeyInputLeftArrow
                          modifierFlags:0
                                 action:@selector(forwardKeyCommandLeft)];
  UIKeyCommand* commandRight =
      [UIKeyCommand keyCommandWithInput:UIKeyInputRightArrow
                          modifierFlags:0
                                 action:@selector(forwardKeyCommandRight)];

#if defined(__IPHONE_15_0) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_15_0
  if (@available(iOS 15, *)) {
    commandUp.wantsPriorityOverSystemBehavior = YES;
    commandDown.wantsPriorityOverSystemBehavior = YES;
    commandLeft.wantsPriorityOverSystemBehavior = YES;
    commandRight.wantsPriorityOverSystemBehavior = YES;
  }
#endif
  return @[ commandUp, commandDown, commandLeft, commandRight ];
}

#pragma mark - OmniboxKeyboardDelegate

- (BOOL)canPerformKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  switch (keyboardAction) {
      // These up/down arrow key commands override the standard UITextInput
      // handling of up/down arrow key. The standard behavior is to go to the
      // beginning/end of the text. Remove this behavior to avoid inconsistent
      // behavior when popup can and cannot move up and down.
    case OmniboxKeyboardActionUpArrow:
    case OmniboxKeyboardActionDownArrow:
      return YES;
      // React to left and right keys when in preedit state to exit preedit and
      // put cursor to the beginning/end of the textfield; or if there is inline
      // suggestion displayed, accept it and put the cursor before/after the
      // suggested text.
    case OmniboxKeyboardActionLeftArrow:
    case OmniboxKeyboardActionRightArrow:
      return ([self isPreEditing] || [self hasAutocompleteText]);
  }
}

- (void)performKeyboardAction:(OmniboxKeyboardAction)keyboardAction {
  DCHECK([self canPerformKeyboardAction:keyboardAction]);
  switch (keyboardAction) {
    case OmniboxKeyboardActionUpArrow:
    case OmniboxKeyboardActionDownArrow:
      // Up and down arrow do nothing instead of standard behavior. The standard
      // behavior is to go to the beginning/end of the text.
      break;
    case OmniboxKeyboardActionLeftArrow:
      [self keyCommandLeft];
      break;
    case OmniboxKeyboardActionRightArrow:
      [self keyCommandRight];
      break;
  }
}

#pragma mark preedit and inline autocomplete key commands

- (void)keyCommandLeft {
  DCHECK([self isPreEditing] || [self hasAutocompleteText]);

  // Cursor offset.
  NSInteger offset = 0;
  if ([self isPreEditing]) {
    [self exitPreEditState];
  }

  if ([self hasAutocompleteText]) {
    // The cursor should stay in the end of the user input.
    offset = self.userText.length;

    // Accept autocomplete suggestion.
    [self acceptAutocompleteText];
  }

  // Place the cursor at computed offset.
  UITextPosition* beginning = self.beginningOfDocument;
  UITextPosition* cursorPosition = [self positionFromPosition:beginning
                                                       offset:offset];
  UITextRange* textRange = [self textRangeFromPosition:cursorPosition
                                            toPosition:cursorPosition];
  self.selectedTextRange = textRange;
}

- (void)keyCommandRight {
  DCHECK([self isPreEditing] || [self hasAutocompleteText]);

  if ([self isPreEditing]) {
    [self exitPreEditState];
  }

  if ([self hasAutocompleteText]) {
    [self acceptAutocompleteText];
  }

  // Put the cursor to the end of the input.
  UITextPosition* end = self.endOfDocument;
  UITextRange* textRange = [self textRangeFromPosition:end toPosition:end];

  self.selectedTextRange = textRange;
}

- (void)acceptAutocompleteText {
  [self setText:self.text];
  self.autocompleteTextLength = 0;
}

- (BOOL)hasAutocompleteText {
  return self.autocompleteTextLength > 0;
}

- (void)clearAutocompleteText {
  if ([self hasAutocompleteText]) {
    self.text = self.userText;
    self.autocompleteTextLength = 0;
  }
}

#pragma mark - helpers

- (NSTextAlignment)bestAlignmentForText:(NSString*)text {
  if (text.length) {
    NSString* lang = CFBridgingRelease(CFStringTokenizerCopyBestStringLanguage(
        (CFStringRef)text, CFRangeMake(0, text.length)));

    if ([NSLocale characterDirectionForLanguage:lang] ==
        NSLocaleLanguageDirectionRightToLeft) {
      return NSTextAlignmentRight;
    }
  }
  return NSTextAlignmentLeft;
}

- (NSString*)displayedText {
  return self.text;
}

// Helper method used to set the text of this field.
- (void)setTextInternal:(NSAttributedString*)text
     autocompleteLength:(NSUInteger)autocompleteLength {
  // Extract substrings for the permanent text and the autocomplete text.  The
  // former needs to retain any text attributes from the original string.
  NSUInteger beginningOfAutocomplete = text.length - autocompleteLength;
  NSRange userTextRange = NSMakeRange(0, beginningOfAutocomplete);

  NSMutableAttributedString* fieldText =
      [[text attributedSubstringFromRange:userTextRange] mutableCopy];

  if (autocompleteLength > 0) {
    // Creating `autocompleteText` from `[text string]` has the added bonus of
    // removing all the previously set attributes. This way the autocomplete
    // text doesn't have a highlighted protocol, etc.
    NSMutableAttributedString* autocompleteText =
        [[NSMutableAttributedString alloc]
            initWithString:[text.string
                               substringFromIndex:beginningOfAutocomplete]];

    [autocompleteText addAttribute:NSBackgroundColorAttributeName
                             value:self.selectedTextBackgroundColor
                             range:NSMakeRange(0, autocompleteLength)];
    [fieldText appendAttributedString:autocompleteText];
  }

  // The following BOOL was introduced to workaround a UIKit bug
  // (crbug.com/737589, rdar/32817402). The bug relates to third party keyboards
  // that check the value of textDocumentProxy.documentContextBeforeInput to
  // show keyboard suggestions. It appears that calling setAttributedText during
  // an EditingChanged UIControlEvent somehow triggers this bug. The reason we
  // update the attributed text here is to change the colors of the omnibox
  // (such as host, protocol) when !self.editing, but also to hide real
  // UITextField text under the _selection text when self.editing. Since we will
  // correct the omnibox editing text color anytime `self.text` is different
  // than `fieldText`, it seems it's OK to skip calling self.attributedText
  // during the condition added below. If we change mobile omnibox to match
  // desktop and also color the omnibox while self.editing, this workaround will
  // no longer work. The check for `autocompleteLength` reduces the scope of
  // this workaround, without it having introduced crbug.com/740075.
  BOOL updateText = YES;
  if (experimental_flags::IsThirdPartyKeyboardWorkaroundEnabled()) {
    updateText =
        (!self.editing || ![self.text isEqualToString:fieldText.string] ||
         autocompleteLength == 0);
  }
  if (updateText) {
    self.attributedText = fieldText;
    UITextPosition* endOfUserText =
        [self positionFromPosition:self.endOfDocument
                            offset:-autocompleteLength];
    // Move the cursor to the beginning of the field before setting the position
    // to the end of the user input so if the text is very wide, the user sees
    // the beginning of the text instead of the end.
    self.selectedTextRange =
        [self textRangeFromPosition:self.beginningOfDocument
                         toPosition:self.beginningOfDocument];
    // Preserve the cursor position at the end of the user input.
    self.selectedTextRange = [self textRangeFromPosition:endOfUserText
                                              toPosition:endOfUserText];
  }

  // iOS changes the font to .LastResort when some unexpected unicode strings
  // are used (e.g. 𝗲𝗺𝗽𝗵𝗮𝘀𝗶𝘀).  Setting the NSFontAttributeName in the
  // attributed string to -systemFontOfSize fixes part of the problem, but the
  // baseline changes so text is out of alignment.
  [self setFont:self.currentFont];
  [self updateTextDirection];
}

- (UIColor*)selectedTextBackgroundColor {
  return [self.tintColor colorWithAlphaComponent:0.2];
}

- (NSArray*)fadeAnimationLayers {
  NSMutableArray* layers = [NSMutableArray array];
  for (UIView* subview in self.subviews)
    [layers addObject:subview.layer];
  return layers;
}

- (BOOL)isTextFieldLTR {
  return [[self class] userInterfaceLayoutDirectionForSemanticContentAttribute:
                           self.semanticContentAttribute] ==
         UIUserInterfaceLayoutDirectionLeftToRight;
}

- (CGRect)layoutLeftViewForBounds:(CGRect)bounds {
  return CGRectZero;
}

@end
