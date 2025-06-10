// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_host_view_tvos_uiview.h"

#include "components/input/native_web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"

static void* kObservingContext = &kObservingContext;

@implementation RenderWidgetUIView

#pragma mark - Public

- (instancetype)initWithWidget:
    (base::WeakPtr<content::RenderWidgetHostViewIOS>)view {
  self = [self init];
  if (self) {
    _view = view;
    self.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

    UITapGestureRecognizer* tapGesture =
        [[UITapGestureRecognizer alloc] initWithTarget:self
                                                action:@selector(tapGesture:)];
    [self addGestureRecognizer:tapGesture];

    UISwipeGestureRecognizer* swipeLeftGesture =
        [[UISwipeGestureRecognizer alloc]
            initWithTarget:self
                    action:@selector(swipeGesture:)];
    swipeLeftGesture.direction = UISwipeGestureRecognizerDirectionLeft;
    [self addGestureRecognizer:swipeLeftGesture];

    UISwipeGestureRecognizer* swipeRightGesture =
        [[UISwipeGestureRecognizer alloc]
            initWithTarget:self
                    action:@selector(swipeGesture:)];
    swipeRightGesture.direction = UISwipeGestureRecognizerDirectionRight;
    [self addGestureRecognizer:swipeRightGesture];

    UISwipeGestureRecognizer* swipeUpGesture = [[UISwipeGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(swipeGesture:)];
    swipeUpGesture.direction = UISwipeGestureRecognizerDirectionUp;
    [self addGestureRecognizer:swipeUpGesture];

    UISwipeGestureRecognizer* swipeDownGesture =
        [[UISwipeGestureRecognizer alloc]
            initWithTarget:self
                    action:@selector(swipeGesture:)];
    swipeDownGesture.direction = UISwipeGestureRecognizerDirectionDown;
    [self addGestureRecognizer:swipeDownGesture];
  }
  return self;
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

- (void)tapGesture:(UIGestureRecognizer*)gestureRecognizer {
  if ([gestureRecognizer state] != UIGestureRecognizerStateEnded) {
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

  const UISwipeGestureRecognizerDirection direction =
      gestureRecognizer.direction;

  blink::WebKeyboardEvent event(blink::WebInputEvent::Type::kKeyDown,
                                blink::WebInputEvent::kNoModifiers,
                                ui::EventTimeForNow());

  switch (direction) {
    case UISwipeGestureRecognizerDirectionLeft:
      event.native_key_code = UIKeyboardHIDUsageKeyboardLeftArrow;
      event.dom_code = static_cast<int>(ui::DomCode::ARROW_LEFT);
      event.dom_key = ui::DomKey::ARROW_LEFT;
      event.windows_key_code = ui::VKEY_LEFT;
      break;
    case UISwipeGestureRecognizerDirectionRight:
      event.native_key_code = UIKeyboardHIDUsageKeyboardRightArrow;
      event.dom_code = static_cast<int>(ui::DomCode::ARROW_RIGHT);
      event.dom_key = ui::DomKey::ARROW_RIGHT;
      event.windows_key_code = ui::VKEY_RIGHT;
      break;
    case UISwipeGestureRecognizerDirectionUp:
      event.native_key_code = UIKeyboardHIDUsageKeyboardUpArrow;
      event.dom_code = static_cast<int>(ui::DomCode::ARROW_UP);
      event.dom_key = ui::DomKey::ARROW_UP;
      event.windows_key_code = ui::VKEY_UP;
      break;
    case UISwipeGestureRecognizerDirectionDown:
      event.native_key_code = UIKeyboardHIDUsageKeyboardDownArrow;
      event.dom_code = static_cast<int>(ui::DomCode::ARROW_DOWN);
      event.dom_key = ui::DomKey::ARROW_DOWN;
      event.windows_key_code = ui::VKEY_DOWN;
      break;
  }

  _view->SendKeyEvent(
      input::NativeWebKeyboardEvent(event, _view->GetNativeView()));
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
