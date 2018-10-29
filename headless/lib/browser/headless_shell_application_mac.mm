// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_shell_application_mac.h"

#include "base/auto_reset.h"
#include "base/observer_list.h"
#include "content/public/browser/native_event_processor_mac.h"
#include "content/public/browser/native_event_processor_observer_mac.h"

@interface HeadlessShellCrApplication ()<NativeEventProcessor> {
  base::ObserverList<content::NativeEventProcessorObserver>::Unchecked
      observers_;
}
@end

@implementation HeadlessShellCrApplication

- (BOOL)isHandlingSendEvent {
  // Since headless mode is non-interactive, always return false.
  return false;
}

// This method is required to allow handling some input events on Mac OS.
- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
}

- (void)sendEvent:(NSEvent*)event {
  content::ScopedNotifyNativeEventProcessorObserver scopedObserverNotifier(
      &observers_, event);
  [super sendEvent:event];
}

- (void)addNativeEventProcessorObserver:
    (content::NativeEventProcessorObserver*)observer {
  observers_.AddObserver(observer);
}

- (void)removeNativeEventProcessorObserver:
    (content::NativeEventProcessorObserver*)observer {
  observers_.RemoveObserver(observer);
}

@end
