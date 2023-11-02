// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NATIVE_EVENT_PROCESSOR_MAC_H_
#define CONTENT_PUBLIC_BROWSER_NATIVE_EVENT_PROCESSOR_MAC_H_

namespace content {
class NativeEventProcessorObserver;
}  // namespace content

// The application's NSApplication subclass should implement this protocol to
// give observers additional information about the native events being run in
// -[NSApplication sendEvent:].
@protocol NativeEventProcessor
- (void)addNativeEventProcessorObserver:
    (content::NativeEventProcessorObserver*)observer;
- (void)removeNativeEventProcessorObserver:
    (content::NativeEventProcessorObserver*)observer;
@end

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_EVENT_PROCESSOR_MAC_H_
