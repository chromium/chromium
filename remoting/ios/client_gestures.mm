// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/client_gestures.h"

#import "remoting/ios/session/remoting_client.h"

#include "remoting/client/gesture_interpreter.h"

static remoting::GestureInterpreter::GestureState toGestureState(
    UIGestureRecognizerState state) {
  switch (state) {
    case UIGestureRecognizerStateBegan:
      return remoting::GestureInterpreter::GESTURE_BEGAN;
    case UIGestureRecognizerStateChanged:
      return remoting::GestureInterpreter::GESTURE_CHANGED;
    default:
      return remoting::GestureInterpreter::GESTURE_ENDED;
  }
}

@interface ClientGestures ()<UIGestureRecognizerDelegate> {
 @private
  UILongPressGestureRecognizer* _longPressRecognizer;
  UIPanGestureRecognizer* _panRecognizer;
  UIPanGestureRecognizer* _flingRecognizer;
  UIPanGestureRecognizer* _scrollRecognizer;

  // TODO(yuweih): Commented out because this makes two-finger gestures not
  // quite responsive. Clean these up if it's really unnecessary.
  //  UIPanGestureRecognizer* _threeFingerPanRecognizer;

  UIPinchGestureRecognizer* _pinchRecognizer;
  UITapGestureRecognizer* _singleTapRecognizer;
  UITapGestureRecognizer* _twoFingerTapRecognizer;
  UITapGestureRecognizer* _threeFingerTapRecognizer;
  UITapGestureRecognizer* _fourFingerTapRecognizer;

  __weak UIView* _view;

  base::WeakPtr<remoting::GestureInterpreter> _gestureInterpreter;
}
@end

@implementation ClientGestures

@synthesize delegate = _delegate;

- (instancetype)initWithView:(UIView*)view client:(RemotingClient*)client {
  _view = view;
  _gestureInterpreter = client.gestureInterpreter->GetWeakPtr();

  _longPressRecognizer = [[UILongPressGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(longPressGestureTriggered:)];
  _longPressRecognizer.delegate = self;
  [view addGestureRecognizer:_longPressRecognizer];

  _panRecognizer = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(panGestureTriggered:)];
  _panRecognizer.minimumNumberOfTouches = 1;
  _panRecognizer.maximumNumberOfTouches = 2;
  _panRecognizer.delegate = self;
  [view addGestureRecognizer:_panRecognizer];

  _flingRecognizer = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(flingGestureTriggered:)];
  _flingRecognizer.minimumNumberOfTouches = 1;
  _flingRecognizer.maximumNumberOfTouches = 1;
  _flingRecognizer.delegate = self;
  [view addGestureRecognizer:_flingRecognizer];

  _scrollRecognizer = [[UIPanGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(scrollGestureTriggered:)];
  _scrollRecognizer.minimumNumberOfTouches = 2;
  _scrollRecognizer.maximumNumberOfTouches = 2;
  _scrollRecognizer.delegate = self;
  [view addGestureRecognizer:_scrollRecognizer];

  //  _threeFingerPanRecognizer = [[UIPanGestureRecognizer alloc]
  //      initWithTarget:self
  //              action:@selector(threeFingerPanGestureTriggered:)];
  //  _threeFingerPanRecognizer.minimumNumberOfTouches = 3;
  //  _threeFingerPanRecognizer.maximumNumberOfTouches = 3;
  //  _threeFingerPanRecognizer.delegate = self;
  //  [view addGestureRecognizer:_threeFingerPanRecognizer];

  _pinchRecognizer = [[UIPinchGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(pinchGestureTriggered:)];
  _pinchRecognizer.delegate = self;
  [view addGestureRecognizer:_pinchRecognizer];

  _singleTapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(tapGestureTriggered:)];
  _singleTapRecognizer.delegate = self;
  [view addGestureRecognizer:_singleTapRecognizer];

  _twoFingerTapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(twoFingerTapGestureTriggered:)];
  _twoFingerTapRecognizer.numberOfTouchesRequired = 2;
  _twoFingerTapRecognizer.delegate = self;
  [view addGestureRecognizer:_twoFingerTapRecognizer];

  _threeFingerTapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(threeFingerTapGestureTriggered:)];
  _threeFingerTapRecognizer.numberOfTouchesRequired = 3;
  _threeFingerTapRecognizer.delegate = self;
  [view addGestureRecognizer:_threeFingerTapRecognizer];

  _fourFingerTapRecognizer = [[UITapGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(fourFingerTapGestureTriggered:)];
  _fourFingerTapRecognizer.numberOfTouchesRequired = 4;
  _fourFingerTapRecognizer.delegate = self;
  [view addGestureRecognizer:_fourFingerTapRecognizer];

  [_singleTapRecognizer requireGestureRecognizerToFail:_twoFingerTapRecognizer];
  [_twoFingerTapRecognizer
      requireGestureRecognizerToFail:_threeFingerTapRecognizer];
  [_pinchRecognizer requireGestureRecognizerToFail:_singleTapRecognizer];
  //  [_pinchRecognizer
  //  requireGestureRecognizerToFail:_threeFingerPanRecognizer];
  [_panRecognizer requireGestureRecognizerToFail:_singleTapRecognizer];
  //  [_threeFingerPanRecognizer
  //      requireGestureRecognizerToFail:_threeFingerTapRecognizer];
  [_panRecognizer requireGestureRecognizerToFail:_scrollRecognizer];

  return self;
}

// Resize the view of the desktop - Zoom in/out.  This can occur during a Pan.
- (IBAction)pinchGestureTriggered:(UIPinchGestureRecognizer*)sender {
  if (!_gestureInterpreter) {
    return;
  }

  CGPoint pivot = [sender locationInView:_view];
  _gestureInterpreter->Zoom(pivot.x, pivot.y, sender.scale,
                            toGestureState([sender state]));

  sender.scale = 1.0;  // reset scale so next iteration is a relative ratio
}

- (IBAction)tapGestureTriggered:(UITapGestureRecognizer*)sender {
  if (!_gestureInterpreter) {
    return;
  }

  CGPoint touchPoint = [sender locationInView:_view];
  _gestureInterpreter->Tap(touchPoint.x, touchPoint.y);
}

// Change position of the viewport. This can occur during a pinch or long press.
- (IBAction)panGestureTriggered:(UIPanGestureRecognizer*)sender {
  if (!_gestureInterpreter) {
    return;
  }

  if ([sender state] == UIGestureRecognizerStateChanged) {
    CGPoint translation = [sender translationInView:_view];
    _gestureInterpreter->Pan(translation.x, translation.y);

    // Reset translation so next iteration is relative
    [sender setTranslation:CGPointZero inView:_view];
  }
}

// Do fling on the viewport. This will happen at the end of the one-finger
// panning.
- (IBAction)flingGestureTriggered:(UIPanGestureRecognizer*)sender {
  if (!_gestureInterpreter) {
    return;
  }

  if ([sender state] == UIGestureRecognizerStateEnded) {
    CGPoint velocity = [sender velocityInView:_view];
    if (velocity.x != 0 || velocity.y != 0) {
      _gestureInterpreter->OneFingerFling(velocity.x, velocity.y);
    }
  }
}

// Handles the two finger scrolling gesture.
- (IBAction)scrollGestureTriggered:(UIPanGestureRecognizer*)sender {
  if (!_gestureInterpreter) {
    return;
  }

  if ([sender state] == UIGestureRecognizerStateEnded) {
    CGPoint velocity = [sender velocityInView:_view];
    _gestureInterpreter->ScrollWithVelocity(velocity.x, velocity.y);
    return;
  }

  CGPoint scrollPoint = [sender locationInView:_view];
  CGPoint translation = [sender translationInView:_view];
  _gestureInterpreter->Scroll(scrollPoint.x, scrollPoint.y, translation.x,
                              translation.y);

  // Reset translation so next iteration is relative
  [sender setTranslation:CGPointZero inView:_view];
}

// Click-Drag mouse operation.  This can occur during a Pan.
- (IBAction)longPressGestureTriggered:(UILongPressGestureRecognizer*)sender {
  if (!_gestureInterpreter) {
    return;
  }

  CGPoint touchPoint = [sender locationInView:_view];
  _gestureInterpreter->Drag(touchPoint.x, touchPoint.y,
                            toGestureState([sender state]));
}

- (IBAction)twoFingerTapGestureTriggered:(UITapGestureRecognizer*)sender {
  if (!_gestureInterpreter) {
    return;
  }

  CGPoint touchPoint = [sender locationInView:_view];
  _gestureInterpreter->TwoFingerTap(touchPoint.x, touchPoint.y);
}

- (IBAction)threeFingerTapGestureTriggered:(UITapGestureRecognizer*)sender {
  if (!_gestureInterpreter) {
    return;
  }

  CGPoint touchPoint = [sender locationInView:_view];
  _gestureInterpreter->ThreeFingerTap(touchPoint.x, touchPoint.y);
}

- (IBAction)threeFingerPanGestureTriggered:(UIPanGestureRecognizer*)sender {
  if (!_gestureInterpreter) {
    return;
  }

  if ([sender state] != UIGestureRecognizerStateEnded) {
    return;
  }

  CGPoint translation = [sender translationInView:_view];
  if (translation.y > 0) {
    // Swiped down - hide keyboard (for now)
    [_delegate keyboardShouldHide];
  } else if (translation.y < 0) {
    // Swiped up - show keyboard
    [_delegate keyboardShouldShow];
  }
}

// To trigger the menu.
- (IBAction)fourFingerTapGestureTriggered:
    (UILongPressGestureRecognizer*)sender {
  if (!_gestureInterpreter) {
    return;
  }

  [_delegate menuShouldShow];
}

#pragma mark - UIGestureRecognizerDelegate

// Allow panning and zooming to occur simultaneously.
// Allow panning and long press to occur simultaneously.
// Pinch requires 2 touches, and long press requires a single touch, so they are
// mutually exclusive regardless of if panning is the initiating gesture.
// Pinch and Scroll are both two-finger gestures. They are mutually exclusive
// and whatever comes first should disable the other gesture recognizer.
- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  if (gestureRecognizer == _pinchRecognizer ||
      (gestureRecognizer == _panRecognizer)) {
    if (otherGestureRecognizer == _pinchRecognizer ||
        otherGestureRecognizer == _panRecognizer) {
      return YES;
    }
  }

  if (gestureRecognizer == _flingRecognizer ||
      (gestureRecognizer == _panRecognizer)) {
    if (otherGestureRecognizer == _flingRecognizer ||
        otherGestureRecognizer == _panRecognizer) {
      return YES;
    }
  }

  if (gestureRecognizer == _longPressRecognizer ||
      gestureRecognizer == _panRecognizer) {
    if (otherGestureRecognizer == _longPressRecognizer ||
        otherGestureRecognizer == _panRecognizer) {
      return YES;
    }
  }

  if (gestureRecognizer == _twoFingerTapRecognizer &&
      otherGestureRecognizer == _longPressRecognizer) {
    return YES;
  }

  // TODO(nicholss): If we return NO here, it dismisses the other recognizers.
  // As we add more types of recognizers, they need to be accounted for in the
  // above logic.
  return NO;
}

@end
