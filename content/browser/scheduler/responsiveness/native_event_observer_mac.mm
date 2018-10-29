// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/native_event_observer.h"

#import <AppKit/AppKit.h>

#import "content/public/browser/native_event_processor_mac.h"

namespace content {
namespace responsiveness {

void NativeEventObserver::RegisterObserver() {
  DCHECK([NSApp conformsToProtocol:@protocol(NativeEventProcessor)]);
  id<NativeEventProcessor> processor =
      static_cast<id<NativeEventProcessor>>(NSApp);
  [processor addNativeEventProcessorObserver:this];
}
void NativeEventObserver::DeregisterObserver() {
  DCHECK([NSApp conformsToProtocol:@protocol(NativeEventProcessor)]);
  id<NativeEventProcessor> processor =
      static_cast<id<NativeEventProcessor>>(NSApp);
  [processor removeNativeEventProcessorObserver:this];
}

void NativeEventObserver::WillRunNativeEvent(const void* opaque_identifier) {
  will_run_event_callback_.Run(opaque_identifier);
}
void NativeEventObserver::DidRunNativeEvent(const void* opaque_identifier) {
  did_run_event_callback_.Run(opaque_identifier);
}

}  // namespace responsiveness
}  // namespace content
