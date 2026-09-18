// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/native_event_observer.h"

#import <AppKit/AppKit.h>

#import "content/public/browser/native_event_processor_mac.h"

namespace content::responsiveness {

void BrowserUINativeEventObserver::RegisterObserver() {
  CHECK([NSApp conformsToProtocol:@protocol(NativeEventProcessor)],
        base::NotFatalUntil::M160);
  id<NativeEventProcessor> processor =
      static_cast<id<NativeEventProcessor>>(NSApp);
  [processor addNativeEventProcessorObserver:this];
}
void BrowserUINativeEventObserver::UnregisterObserver() {
  CHECK([NSApp conformsToProtocol:@protocol(NativeEventProcessor)],
        base::NotFatalUntil::M160);
  id<NativeEventProcessor> processor =
      static_cast<id<NativeEventProcessor>>(NSApp);
  [processor removeNativeEventProcessorObserver:this];
}

void BrowserUINativeEventObserver::WillRunNativeEvent(
    uintptr_t opaque_identifier) {
  will_run_event_callback_.Run(opaque_identifier);
}
void BrowserUINativeEventObserver::DidRunNativeEvent(
    uintptr_t opaque_identifier) {
  did_run_event_callback_.Run(opaque_identifier);
}

}  // namespace content::responsiveness
