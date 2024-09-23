// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_ios_uiview_textinput.h"

#include "base/strings/sys_string_conversions.h"
#include "ui/base/ime/text_input_type.h"

@implementation RenderWidgetUIViewTextInput {
  BOOL _hasText;
}
@synthesize selectedTextRange;
@synthesize markedTextRange;
@synthesize markedTextStyle;
@synthesize beginningOfDocument;
@synthesize endOfDocument;
@synthesize inputDelegate;
@synthesize tokenizer;

- (nullable NSString*)textInRange:(UITextRange*)range {
  return nil;
}

- (void)replaceRange:(UITextRange*)range withText:(NSString*)text {
}

- (void)setMarkedText:(nullable NSString*)markedText
        selectedRange:(NSRange)selectedRange {
  std::u16string text = base::SysNSStringToUTF16(markedText);
  std::vector<ui::ImeTextSpan> spans{ui::ImeTextSpan(
      ui::ImeTextSpan::Type::kComposition, 0, text.length(),
      ui::ImeTextSpan::Thickness::kThin,
      ui::ImeTextSpan::UnderlineStyle::kSolid, SK_ColorTRANSPARENT,
      SK_ColorTRANSPARENT, std::vector<std::string>())};
  int start = selectedRange.location;
  int end = start + selectedRange.length;
  _view->ImeSetComposition(text, spans, gfx::Range::InvalidRange(), start, end);
}

- (void)unmarkText {
}

- (nullable UITextRange*)textRangeFromPosition:(UITextPosition*)fromPosition
                                    toPosition:(UITextPosition*)toPosition {
  return nil;
}

- (nullable UITextPosition*)positionFromPosition:(UITextPosition*)position
                                          offset:(NSInteger)offset {
  return nil;
}

- (nullable UITextPosition*)positionFromPosition:(UITextPosition*)position
                                     inDirection:
                                         (UITextLayoutDirection)direction
                                          offset:(NSInteger)offset {
  return nil;
}

- (NSComparisonResult)comparePosition:(UITextPosition*)position
                           toPosition:(UITextPosition*)other {
  return NSOrderedSame;
}

- (NSInteger)offsetFromPosition:(UITextPosition*)from
                     toPosition:(UITextPosition*)toPosition {
  return 0;
}

- (nullable UITextPosition*)positionWithinRange:(UITextRange*)range
                            farthestInDirection:
                                (UITextLayoutDirection)direction {
  return nil;
}

- (nullable UITextRange*)
    characterRangeByExtendingPosition:(UITextPosition*)position
                          inDirection:(UITextLayoutDirection)direction {
  return nil;
}

- (NSWritingDirection)baseWritingDirectionForPosition:(UITextPosition*)position
                                          inDirection:(UITextStorageDirection)
                                                          direction {
  return NSWritingDirectionNatural;
}

- (void)setBaseWritingDirection:(NSWritingDirection)writingDirection
                       forRange:(UITextRange*)range {
}

- (CGRect)firstRectForRange:(UITextRange*)range {
  return CGRectZero;
}

- (CGRect)caretRectForPosition:(UITextPosition*)position {
  return CGRectZero;
}

- (NSArray<UITextSelectionRect*>*)selectionRectsForRange:(UITextRange*)range {
  return @[];
}

- (nullable UITextPosition*)closestPositionToPoint:(CGPoint)point {
  return nil;
}

- (nullable UITextPosition*)closestPositionToPoint:(CGPoint)point
                                       withinRange:(UITextRange*)range {
  return nil;
}

- (nullable UITextRange*)characterRangeAtPoint:(CGPoint)point {
  return nil;
}

- (instancetype)initWithWidget:
    (base::WeakPtr<content::RenderWidgetHostViewIOS>)view {
  _view = view;
  _hasText = NO;
  self.multipleTouchEnabled = YES;
  self.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  return [self init];
}

- (void)onUpdateTextInputState:(const ui::mojom::TextInputState&)state
                    withBounds:(CGRect)bounds {
  // Check for the visibility request and policy if VK APIs are enabled.
  if (state.vk_policy == ui::mojom::VirtualKeyboardPolicy::MANUAL) {
    // policy is manual.
    if (state.last_vk_visibility_request ==
        ui::mojom::VirtualKeyboardVisibilityRequest::SHOW) {
      [self showKeyboard:(state.value && !state.value->empty())
              withBounds:bounds];
    } else if (state.last_vk_visibility_request ==
               ui::mojom::VirtualKeyboardVisibilityRequest::HIDE) {
      [self hideKeyboard];
    }
  } else {
    bool hide = state.always_hide_ime ||
                state.mode == ui::TextInputMode::TEXT_INPUT_MODE_NONE ||
                state.type == ui::TextInputType::TEXT_INPUT_TYPE_NONE;
    if (hide) {
      [self hideKeyboard];
    } else if (state.show_ime_if_needed) {
      [self showKeyboard:(state.value && !state.value->empty())
              withBounds:bounds];
    }
  }
}

- (void)showKeyboard:(bool)has_text withBounds:(CGRect)bounds {
  self.frame = bounds;
  [self becomeFirstResponder];
  _hasText = has_text;
}

- (void)hideKeyboard {
  [self resignFirstResponder];
  _hasText = NO;
}

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (BOOL)hasText {
  return _hasText;
}

- (void)insertText:(NSString*)text {
  CHECK(_view);
  _view->ImeCommitText(base::SysNSStringToUTF16(text),
                       gfx::Range::InvalidRange(), 0);
}

- (void)deleteBackward {
  CHECK(_view);
  std::vector<ui::ImeTextSpan> ime_text_spans;
  _view->ImeSetComposition(std::u16string(), ime_text_spans,
                           gfx::Range::InvalidRange(), -1, 0);
  _view->ImeCommitText(std::u16string(), gfx::Range::InvalidRange(), 0);
}

- (BOOL)becomeFirstResponder {
  BOOL result = [super becomeFirstResponder];
  if (result && _view) {
    _view->OnFirstResponderChanged();
  }
  return result;
}

- (BOOL)resignFirstResponder {
  BOOL result = [super resignFirstResponder];
  if (result && _view) {
    _view->OnFirstResponderChanged();
  }
  return result;
}

@end
