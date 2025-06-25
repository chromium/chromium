// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_tvos_uiview.h"

#include "base/strings/sys_string_conversions.h"
#include "components/input/native_web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/base/ime/mojom/ime_types.mojom-shared.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

static void* kObservingContext = &kObservingContext;

namespace {

typedef NS_ENUM(NSInteger, NavigationDirection) {
  kUp,
  kDown,
  kLeft,
  kRight,
  kNone
};

// The minimum velocity to generate left/right direction events from
// UIPanGestureRecognizer.
const CGFloat kMinVelocity = 100;

UIKeyboardType keyboardTypeForInputType(ui::TextInputType inputType) {
  // TODO(crbug.com/411452047): Implement textFieldShouldEndEditing to detect
  // invalid contents in the text field. When texts are inserted via a H/W
  // connected keyboard, it's still possible to insert invalid contents.
  switch (inputType) {
    case ui::TextInputType::TEXT_INPUT_TYPE_SEARCH:
      return UIKeyboardTypeWebSearch;
    case ui::TextInputType::TEXT_INPUT_TYPE_EMAIL:
      return UIKeyboardTypeEmailAddress;
    case ui::TextInputType::TEXT_INPUT_TYPE_NUMBER:
      return UIKeyboardTypeNumberPad;
    case ui::TextInputType::TEXT_INPUT_TYPE_TELEPHONE:
      return UIKeyboardTypePhonePad;
    case ui::TextInputType::TEXT_INPUT_TYPE_URL:
      return UIKeyboardTypeURL;
    default:
      return UIKeyboardTypeASCIICapable;
  }
}

}  // namespace

@implementation RenderWidgetUIView

#pragma mark - Public

- (instancetype)initWithWidget:
    (base::WeakPtr<content::RenderWidgetHostViewIOS>)view {
  self = [self init];
  if (self) {
    _view = view;
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    // tvOS supports multiple types of input events from the Remote, including
    // the clickpad (touch surface), the clickpad ring (directional control),
    // and various physical buttons.
    // Add a tap gesture recognizer to handle center-clickpad press events.
    UITapGestureRecognizer* tapGesture =
        [[UITapGestureRecognizer alloc] initWithTarget:self
                                                action:@selector(tapGesture:)];
    [self addGestureRecognizer:tapGesture];

    // Create and add swipe gesture recognizers for all directions originating
    // from the clickpad buttons.
    [self addSwipeGestureRecognizerWithDirection:
              UISwipeGestureRecognizerDirectionUp];
    [self addSwipeGestureRecognizerWithDirection:
              UISwipeGestureRecognizerDirectionLeft];
    [self addSwipeGestureRecognizerWithDirection:
              UISwipeGestureRecognizerDirectionRight];
    [self addSwipeGestureRecognizerWithDirection:
              UISwipeGestureRecognizerDirectionDown];

    // Add a pan gesture recognizer to capture input from the clickpad ring,
    // which allows for continuous movement to the left or right.
    UIPanGestureRecognizer* panGesture =
        [[UIPanGestureRecognizer alloc] initWithTarget:self
                                                action:@selector(handlePan:)];
    panGesture.delegate = self;
    [self addGestureRecognizer:panGesture];

    // Only allow the pan gesture to activate if the swipe gesture fails to
    // recognize.
    for (UIGestureRecognizer* swipeGesture in self.gestureRecognizers) {
      if ([swipeGesture isKindOfClass:[UISwipeGestureRecognizer class]]) {
        [panGesture requireGestureRecognizerToFail:swipeGesture];
      }
    }
  }
  return self;
}

// Contrary to iOS, on tvOS the on-screen keyboard takes the entire screen, so
// instead of showing it when an input element is focused, we require a tap to
// do it.
- (void)onUpdateTextInputState:(const ui::mojom::TextInputState&)state
                    withBounds:(CGRect)bounds {
  // Check for the visibility request and policy if VK APIs are enabled.
  if (state.vk_policy == ui::mojom::VirtualKeyboardPolicy::MANUAL) {
    // policy is manual.
    if (state.last_vk_visibility_request ==
        ui::mojom::VirtualKeyboardVisibilityRequest::SHOW) {
      [self showKeyboard:state];
    } else if (state.last_vk_visibility_request ==
               ui::mojom::VirtualKeyboardVisibilityRequest::HIDE) {
      [self hideAndDeleteKeyboard];
    }
  } else {
    bool hide = state.always_hide_ime ||
                state.mode == ui::TextInputMode::TEXT_INPUT_MODE_NONE ||
                state.type == ui::TextInputType::TEXT_INPUT_TYPE_NONE;
    if (hide) {
      [self hideAndDeleteKeyboard];
    } else if (state.show_ime_if_needed) {
      [self showKeyboard:state];
    }
  }
}

- (void)updateView:(UIScrollView*)view {
  [view addSubview:self];
  view.scrollEnabled = NO;
  // Remove all existing gestureRecognizers since the header might be reused.
  for (UIGestureRecognizer* recognizer in view.gestureRecognizers) {
    [view removeGestureRecognizer:recognizer];
  }
  [view addObserver:self
         forKeyPath:NSStringFromSelector(@selector(contentInset))
            options:NSKeyValueObservingOptionNew | NSKeyValueObservingOptionOld
            context:kObservingContext];
}

- (void)removeView {
  UIScrollView* view = (UIScrollView*)[self superview];
  [view removeObserver:self
            forKeyPath:NSStringFromSelector(@selector(contentInset))];
  [self removeFromSuperview];
}

#pragma mark - Private

// Helper method to add swipe gestures for `direction`.
- (void)addSwipeGestureRecognizerWithDirection:
    (UISwipeGestureRecognizerDirection)direction {
  UISwipeGestureRecognizer* swipeGesture = [[UISwipeGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(swipeGesture:)];
  swipeGesture.direction = direction;
  swipeGesture.delegate = self;
  [self addGestureRecognizer:swipeGesture];
}

- (void)tapGesture:(UIGestureRecognizer*)gestureRecognizer {
  if ([gestureRecognizer state] != UIGestureRecognizerStateEnded) {
    return;
  }

  const ui::mojom::TextInputState* state = [self editState];
  if (state && state->mode != ui::TextInputMode::TEXT_INPUT_MODE_NONE &&
      state->type != ui::TextInputType::TEXT_INPUT_TYPE_NONE) {
    [self showKeyboard:*state];
    return;
  }

  blink::WebKeyboardEvent event(blink::WebInputEvent::Type::kKeyDown,
                                blink::WebInputEvent::kNoModifiers,
                                ui::EventTimeForNow());
  event.native_key_code = UIKeyboardHIDUsageKeyboardReturnOrEnter;
  event.dom_code = static_cast<int>(ui::DomCode::ENTER);
  event.dom_key = ui::DomKey::ENTER;
  event.windows_key_code = ui::VKEY_RETURN;

  // Copied from components/input/web_input_event_builders_mac.mm's
  // WebKeyboardEventBuilder::Build().
  // This is necessary due to way some HTML elements process keyboard activation
  // (e.g. blink::HTMLElement::HandleKeyboardActivation()).
  event.text[0] = '\r';
  event.unmodified_text[0] = '\r';

  _view->SendKeyEvent(
      input::NativeWebKeyboardEvent(event, _view->GetNativeView()));

  // We also need to send a keyup event so that e.g. checkboxes are properly
  // activated/deactivated with the keyboard.
  event.SetType(blink::WebInputEvent::Type::kKeyUp);
  event.SetTimeStamp(ui::EventTimeForNow());
  _view->SendKeyEvent(
      input::NativeWebKeyboardEvent(event, _view->GetNativeView()));
}

- (void)swipeGesture:(UISwipeGestureRecognizer*)gestureRecognizer {
  if ([gestureRecognizer state] != UIGestureRecognizerStateEnded) {
    return;
  }

  NavigationDirection direction = kNone;
  switch (gestureRecognizer.direction) {
    case UISwipeGestureRecognizerDirectionLeft:
      direction = kLeft;
      break;
    case UISwipeGestureRecognizerDirectionRight:
      direction = kRight;
      break;
    case UISwipeGestureRecognizerDirectionUp:
      direction = kUp;
      break;
    case UISwipeGestureRecognizerDirectionDown:
      direction = kDown;
      break;
  }
  [self sendKeyEventWithDirection:direction];
}

- (void)handlePan:(UIPanGestureRecognizer*)gesture {
  CGPoint velocity = [gesture velocityInView:self];

  // Detect left and right swipes with the velocity.
  if (gesture.state == UIGestureRecognizerStateEnded ||
      gesture.state == UIGestureRecognizerStateChanged) {
    // Use `kMinVelocity` to avoid excessive events.
    if (velocity.x > kMinVelocity) {
      [self sendKeyEventWithDirection:kRight];
    } else if (velocity.x < -kMinVelocity) {
      [self sendKeyEventWithDirection:kLeft];
    }
  }
}

// Generates four-directional events when buttons on the clickpad ring are
// pressed.
- (void)pressesEnded:(NSSet<UIPress*>*)presses
           withEvent:(UIPressesEvent*)event {
  for (UIPress* press in presses) {
    NavigationDirection direction = kNone;
    switch (press.type) {
      case UIPressTypeUpArrow:
        direction = kUp;
        break;
      case UIPressTypeDownArrow:
        direction = kDown;
        break;
      case UIPressTypeLeftArrow:
        direction = kLeft;
        break;
      case UIPressTypeRightArrow:
        direction = kRight;
        break;
      default:
        [super pressesEnded:presses withEvent:event];
        break;
    }
    [self sendKeyEventWithDirection:direction];
  }
}

// Helper method to generate WebKeyboardEvent with `direction`.
- (void)sendKeyEventWithDirection:(NavigationDirection)direction {
  blink::WebKeyboardEvent event(blink::WebInputEvent::Type::kKeyDown,
                                blink::WebInputEvent::kNoModifiers,
                                ui::EventTimeForNow());

  switch (direction) {
    case kLeft:
      event.native_key_code = UIKeyboardHIDUsageKeyboardLeftArrow;
      event.dom_code = static_cast<int>(ui::DomCode::ARROW_LEFT);
      event.dom_key = ui::DomKey::ARROW_LEFT;
      event.windows_key_code = ui::VKEY_LEFT;
      break;
    case kRight:
      event.native_key_code = UIKeyboardHIDUsageKeyboardRightArrow;
      event.dom_code = static_cast<int>(ui::DomCode::ARROW_RIGHT);
      event.dom_key = ui::DomKey::ARROW_RIGHT;
      event.windows_key_code = ui::VKEY_RIGHT;
      break;
    case kUp:
      event.native_key_code = UIKeyboardHIDUsageKeyboardUpArrow;
      event.dom_code = static_cast<int>(ui::DomCode::ARROW_UP);
      event.dom_key = ui::DomKey::ARROW_UP;
      event.windows_key_code = ui::VKEY_UP;
      break;
    case kDown:
      event.native_key_code = UIKeyboardHIDUsageKeyboardDownArrow;
      event.dom_code = static_cast<int>(ui::DomCode::ARROW_DOWN);
      event.dom_key = ui::DomKey::ARROW_DOWN;
      event.windows_key_code = ui::VKEY_DOWN;
      break;
    case kNone:
      return;
  }

  _view->SendKeyEvent(
      input::NativeWebKeyboardEvent(event, _view->GetNativeView()));
}

- (void)showKeyboard:(const ui::mojom::TextInputState&)state {
  if (_textFieldForAllTextInput) {
    return;
  }

  _textFieldForAllTextInput = [[UITextField alloc] init];
  _textFieldForAllTextInput.delegate = self;
  _textFieldForAllTextInput.keyboardType = keyboardTypeForInputType(state.type);
  if (state.value.has_value()) {
    _textFieldForAllTextInput.text =
        base::SysUTF16ToNSString(state.value.value());
  }
  if (state.type == ui::TextInputType::TEXT_INPUT_TYPE_PASSWORD) {
    _textFieldForAllTextInput.secureTextEntry = YES;
    // state.value.value() contains "\u2022" characters instead of actual text,
    // so it makes more sense to just start with an empty field to avoid the
    // case of a bogus value being set and used if the user just presses "done"
    // on the on-screen keyboard.
    _textFieldForAllTextInput.text = nil;
  }

  [self addSubview:_textFieldForAllTextInput];
  [_textFieldForAllTextInput becomeFirstResponder];
}

- (void)hideAndDeleteKeyboard {
  [_textFieldForAllTextInput removeFromSuperview];
  _textFieldForAllTextInput = nil;
}

- (const ui::mojom::TextInputState*)editState {
  if (!_view || !_view->GetTextInputManager()) {
    return nil;
  }
  return _view->GetTextInputManager()->GetTextInputState();
}

#pragma mark - CALayerFrameSinkProvider

- (ui::CALayerFrameSink*)frameSink {
  return _view.get();
}

#pragma mark - NSObject

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  CHECK(_view);
  if (context == kObservingContext) {
    _view->ContentInsetChanged();
  } else {
    [super observeValueForKeyPath:keyPath
                         ofObject:object
                           change:change
                          context:context];
  }
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (BOOL)isFirstResponder {
  return
      [super isFirstResponder] || [_textFieldForAllTextInput isFirstResponder];
}

- (BOOL)becomeFirstResponder {
  CHECK(_view);
  const BOOL result = [super becomeFirstResponder];
  const BOOL keyboard_is_hidden =
      !_textFieldForAllTextInput || [_textFieldForAllTextInput isHidden];
  if (keyboard_is_hidden &&
      (result || _view->CanBecomeFirstResponderForTesting())) {
    _view->OnFirstResponderChanged();
  }
  return result;
}

- (BOOL)resignFirstResponder {
  const BOOL result = [super resignFirstResponder];
  const BOOL keyboard_is_hidden =
      !_textFieldForAllTextInput || [_textFieldForAllTextInput isHidden];
  if (_view && keyboard_is_hidden &&
      (result || _view->CanResignFirstResponderForTesting())) {
    _view->OnFirstResponderChanged();
  }
  return result;
}

#pragma mark - UITextFieldDelegate

- (void)textFieldDidEndEditing:(UITextField*)textField
                        reason:(UITextFieldDidEndEditingReason)reason {
  CHECK_EQ(textField, _textFieldForAllTextInput);
  if (reason == UITextFieldDidEndEditingReasonCommitted) {
    const ui::mojom::TextInputState* state = [self editState];
    const gfx::Range range = state ? gfx::Range(0, state->selection.GetMax())
                                   : gfx::Range::InvalidRange();
    _view->ImeCommitText(base::SysNSStringToUTF16(textField.text), range, 0);
  }

  [self hideAndDeleteKeyboard];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

#pragma mark - UIView

- (BOOL)canBecomeFocused {
  return YES;
}

- (void)layoutSubviews {
  CHECK(_view);
  [super layoutSubviews];
  _view->UpdateScreenInfo();

  // TODO(dtapuska): This isn't correct, we need to figure out when the window
  // gains/loses focus.
  _view->SetActive(true);
}

@end
