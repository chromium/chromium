// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_legacy.h"

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
#import "ios/chrome/browser/ui/omnibox/omnibox_constants.h"
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

// When rendering the same string in a UITextField and a UILabel with the same
// frame and the same font, the text is slightly offset.
const CGFloat kUILabelUITextfieldBaselineDeltaInPoints = 1.0;

// The default omnibox text color (used while editing).
UIColor* TextColor() {
  return [UIColor colorNamed:kTextPrimaryColor];
}

NSString* const kOmniboxFadeAnimationKey = @"OmniboxFadeAnimation";

}  // namespace

@interface OmniboxTextFieldLegacy ()

// Font to use in regular x regular size class. If not set, the regular font is
// used instead.
@property(nonatomic, strong, readonly) UIFont* largerFont;
// Font to use in Compact x Any and Any x Compact size class.
@property(nonatomic, strong, readonly) UIFont* normalFont;

// Gets the bounds of the rect covering the URL.
- (CGRect)preEditLabelRectForBounds:(CGRect)bounds;
// Creates the UILabel if it doesn't already exist and adds it as a
// subview.
- (void)createSelectionViewIfNecessary;
// Helper method used to set the text of this field.  Updates the selection view
// to contain the correct inline autocomplete text.
- (void)setTextInternal:(NSAttributedString*)text
     autocompleteLength:(NSUInteger)autocompleteLength;
// Override deleteBackward so that backspace can clear query refinement chips.
- (void)deleteBackward;
// Returns the layers affected by animations added by `-animateFadeWithStyle:`.
- (NSArray*)fadeAnimationLayers;
// Font that should be used in current size class.
- (UIFont*)currentFont;

@end

@implementation OmniboxTextFieldLegacy {
  UILabel* _selection;
  UILabel* _preEditStaticLabel;
  UIColor* _displayedTextColor;
  UIColor* _displayedTintColor;
}

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
    _displayedTextColor = textColor;
    if (tintColor) {
      [self setTintColor:tintColor];
      _displayedTintColor = tintColor;
    } else {
      _displayedTintColor = self.tintColor;
    }
    self.textColor = _displayedTextColor;
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

    // Sanity check:
    DCHECK([self conformsToProtocol:@protocol(UITextInput)]);

    // Force initial layout of internal text label.  Needed for omnibox
    // animations that will otherwise animate the text label from origin {0, 0}.
    [super setText:@" "];
  }
  return self;
}

- (instancetype)initWithCoder:(nonnull NSCoder*)aDecoder {
  NOTREACHED();
  return nil;
}

- (void)setText:(NSAttributedString*)text
    userTextLength:(size_t)userTextLength {
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
  DCHECK_LT(self.text.length, _selection.text.length)
      << "[_selection text] and self.text are out of sync. "
      << "Please email justincohen@ and rohitrao@ if you see this.";
  if (_selection && _selection.text.length > self.text.length) {
    return [_selection.text substringFromIndex:self.text.length];
  }
  return @"";
}

- (BOOL)hasAutocompleteText {
  return !!_selection;
}

- (void)clearAutocompleteText {
  if (_selection) {
    [_selection removeFromSuperview];
    _selection = nil;
    [self showTextAndCursor];
  }
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

- (UIColor*)displayedTextColor {
  return _displayedTextColor;
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
             .width +
         self.preEditStaticLabel.frame.origin.x;
}

#pragma mark pre-edit

// Creates a UILabel based on the current dimension of the text field and
// displays the URL in the UILabel so it appears properly aligned to the URL.
- (void)enterPreEditState {
  // Empty omnibox should show the insertion point immediately. There is
  // nothing to erase.
  if (!self.text.length || UIAccessibilityIsVoiceOverRunning())
    return;

  // Remembers the initial text input to compute the diff of what was there
  // and what was typed.
  [self setPreEditText:self.text];

  // Adjusts the placement so static URL lines up perfectly with UITextField.
  DCHECK(!_preEditStaticLabel);
  CGRect rect = [self preEditLabelRectForBounds:self.bounds];
  _preEditStaticLabel = [[UILabel alloc] initWithFrame:rect];
  _preEditStaticLabel.backgroundColor = UIColor.clearColor;
  _preEditStaticLabel.opaque = YES;
  _preEditStaticLabel.font = self.currentFont;
  _preEditStaticLabel.textColor = _displayedTextColor;
  _preEditStaticLabel.lineBreakMode = NSLineBreakByTruncatingHead;

  NSMutableParagraphStyle* style = [[NSMutableParagraphStyle alloc] init];
  // URLs have their text direction set to to LTR (avoids RTL characters
  // making the URL render from right to left, as per the URL rendering standard
  // described here: https://url.spec.whatwg.org/#url-rendering
  [style setBaseWritingDirection:NSWritingDirectionLeftToRight];
  NSDictionary* attributes = @{
    NSBackgroundColorAttributeName : [self selectedTextBackgroundColor],
    NSParagraphStyleAttributeName : style
  };

  NSAttributedString* preEditString =
      [[NSAttributedString alloc] initWithString:self.text
                                      attributes:attributes];
  [_preEditStaticLabel setAttributedText:preEditString];
  _preEditStaticLabel.textAlignment = [self preEditTextAlignment];
  [self addSubview:_preEditStaticLabel];
}

// Finishes pre-edit state by removing the UILabel with the URL.
- (void)exitPreEditState {
  [self setPreEditText:nil];
  if (_preEditStaticLabel) {
    [_preEditStaticLabel removeFromSuperview];
    _preEditStaticLabel = nil;
    [self showTextAndCursor];
  }
}

// Returns whether we are processing the first touch event on the text field.
- (BOOL)isPreEditing {
  return !![self preEditText];
}

#pragma mark - TestingUtilities category

// Exposed for testing.
- (UILabel*)preEditStaticLabel {
  return _preEditStaticLabel;
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
    // Hide the text when the `_selection` label is displayed.
    UIColor* textColor = _selection ? UIColor.clearColor : _displayedTextColor;
    [mutableText addAttribute:NSForegroundColorAttributeName
                        value:textColor
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
  [self layoutSelectionViewWithNewEditingRectBounds:editRect];
  return editRect;
}

#pragma mark - UITextInput

- (void)beginFloatingCursorAtPoint:(CGPoint)point {
  // Exit preedit because it blocks the view of the textfield.
  [self exitPreEditState];
  [super beginFloatingCursorAtPoint:point];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  if ([self isPreEditing]) {
    CGRect rect = [self preEditLabelRectForBounds:self.bounds];
    [_preEditStaticLabel setFrame:rect];

    // Update text alignment since the pre-edit label's frame changed.
    _preEditStaticLabel.textAlignment = [self preEditTextAlignment];
    [self hideTextAndCursor];
  } else if (!_selection) {
    [self showTextAndCursor];
  }

  if (_selection) {
    // Trigger a layout of _selection label.
    CGRect editRect = [super editingRectForBounds:self.bounds];
    [self layoutSelectionViewWithNewEditingRectBounds:editRect];
  }
}

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
  if (_selection) {
    _selection.font = self.currentFont;
  }
  if (_preEditStaticLabel) {
    _preEditStaticLabel.font = self.currentFont;
  }
}

#pragma mark - UIResponder

// Method called when the users touches the text input. This will accept the
// autocompleted text.
- (void)touchesBegan:(NSSet*)touches withEvent:(UIEvent*)event {
  if ([self isPreEditing]) {
    [self exitPreEditState];
    [super selectAll:nil];
  }

  if (!_selection) {
    [super touchesBegan:touches withEvent:event];
    return;
  }

  // Only consider a single touch.
  UITouch* touch = [touches anyObject];
  if (!touch)
    return;

  // Accept selection.
  NSString* newText = [[self displayedText] copy];
  [self clearAutocompleteText];
  [self setText:newText];
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
  if (_selection) {
    NSString* newText = [[self displayedText] copy];
    [self clearAutocompleteText];
    [self setText:newText];
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

- (void)deleteBackward {
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
      performKeyboardAction:OmniboxKeyboardActionDownArrow];
}

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
    offset = self.text.length;

    // Accept autocomplete suggestion.
    [self acceptAutocompleteText];
  }

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

// A helper to accept the current autocomplete text.
- (void)acceptAutocompleteText {
  DCHECK([self hasAutocompleteText]);
  // Strip attributes and set text as if the user typed it.
  NSAttributedString* string =
      [[NSAttributedString alloc] initWithString:_selection.text];
  [self setText:string userTextLength:string.length];
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

#pragma mark - helpers

// Gets the bounds of the rect covering the URL.
- (CGRect)preEditLabelRectForBounds:(CGRect)bounds {
  return [self editingRectForBounds:self.bounds];
}

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

- (NSTextAlignment)preEditTextAlignment {
  // If the pre-edit text is wider than the omnibox, right-align the text so it
  // ends at the same x coord as the blue selection box.
  CGSize textSize =
      [_preEditStaticLabel.text cr_pixelAlignedSizeWithFont:self.currentFont];
  // Note, this does not need to support RTL, as URLs are always LTR.
  return textSize.width < _preEditStaticLabel.frame.size.width
             ? NSTextAlignmentLeft
             : NSTextAlignmentRight;
}

- (NSString*)displayedText {
  if (_selection)
    return [_selection text];
  return self.text;
}

// Creates the SelectedTextLabel if it doesn't already exist and adds it as a
// subview.
- (void)createSelectionViewIfNecessary {
  if (_selection)
    return;

  _selection = [[UILabel alloc] initWithFrame:CGRectZero];
  [_selection setFont:self.currentFont];
  [_selection setTextColor:_displayedTextColor];
  [_selection setOpaque:NO];
  [_selection setAccessibilityIdentifier:
                  kOmniboxAutocompleteLabelAccessibilityIdentifier];
  [_selection setBackgroundColor:UIColor.clearColor];
  _selection.lineBreakMode = NSLineBreakByClipping;
  [self addSubview:_selection];
  [self hideTextAndCursor];
}

// Helper method used to set the text of this field.  Updates the selection view
// to contain the correct inline autocomplete text.
- (void)setTextInternal:(NSAttributedString*)text
     autocompleteLength:(NSUInteger)autocompleteLength {
  // Extract substrings for the permanent text and the autocomplete text.  The
  // former needs to retain any text attributes from the original string.
  NSRange fieldRange = NSMakeRange(0, [text length] - autocompleteLength);
  NSAttributedString* fieldText =
      [text attributedSubstringFromRange:fieldRange];

  if (autocompleteLength > 0) {
    // Creating `autocompleteText` from `[text string]` has the added bonus of
    // removing all the previously set attributes. This way the autocomplete
    // text doesn't have a highlighted protocol, etc.
    NSMutableAttributedString* autocompleteText =
        [[NSMutableAttributedString alloc] initWithString:[text string]];

    [self createSelectionViewIfNecessary];
    DCHECK(_selection);
    [autocompleteText
        addAttribute:NSBackgroundColorAttributeName
               value:[self selectedTextBackgroundColor]
               range:NSMakeRange([fieldText length], autocompleteLength)];
    [_selection setAttributedText:autocompleteText];
    [_selection setTextAlignment:[self bestTextAlignment]];
  } else {
    [self clearAutocompleteText];
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
  }

  // iOS changes the font to .LastResort when some unexpected unicode strings
  // are used (e.g. 𝗲𝗺𝗽𝗵𝗮𝘀𝗶𝘀).  Setting the NSFontAttributeName in the
  // attributed string to -systemFontOfSize fixes part of the problem, but the
  // baseline changes so text is out of alignment.
  [self setFont:self.currentFont];
  [self updateTextDirection];
}

- (UIColor*)selectedTextBackgroundColor {
  return [_displayedTintColor colorWithAlphaComponent:0.2];
}

- (BOOL)isColorHidden:(UIColor*)color {
  return ([color isEqual:UIColor.clearColor] ||
          CGColorGetAlpha(color.CGColor) < 0.05);
}

// Set the text field's text and cursor to their displayed colors. To be called
// when there are no overlaid views displayed.
- (void)showTextAndCursor {
  if ([self isColorHidden:self.textColor]) {
    [self setTextColor:_displayedTextColor];
  }
  if ([self isColorHidden:self.tintColor]) {
    [self setTintColor:_displayedTintColor];
  }
}

// Set the text field's text and cursor to clear so that they don't show up
// behind any overlaid views.
- (void)hideTextAndCursor {
  [self setTintColor:UIColor.clearColor];
  [self setTextColor:UIColor.clearColor];
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

// Aligns the selection UILabel to match the editing rect bounds. Takes iOS
// version-specific text rendering differences into account.
- (void)layoutSelectionViewWithNewEditingRectBounds:(CGRect)newBounds {
  // The goal is to visually align the _selection label and the `self` textfield
  // to avoid text jumping when inline autocomplete is shown or hidden.
  newBounds.origin.y -= kUILabelUITextfieldBaselineDeltaInPoints;

  // Position the selection view appropriately.
  [_selection setFrame:newBounds];

  newBounds.origin.y += kUILabelUITextfieldBaselineDeltaInPoints;
}

@end
