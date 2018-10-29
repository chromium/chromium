// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NATIVE_EVENT_PROCESSOR_OBSERVER_MAC_H_
#define CONTENT_PUBLIC_BROWSER_NATIVE_EVENT_PROCESSOR_OBSERVER_MAC_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"

#if defined(__OBJC__)
@class NSEvent;
#else   // __OBJC__
class NSEvent;
#endif  // __OBJC__

namespace content {

class NativeEventProcessorObserver {
 public:
  // Called right before a native event is run.
  virtual void WillRunNativeEvent(const void* opaque_identifier) = 0;

  // Called right after a native event is run.
  virtual void DidRunNativeEvent(const void* opaque_identifier) = 0;
};

// The constructor sends a WillRunNativeEvent callback to each observer.
// The destructor sends a DidRunNativeEvent callback to each observer.
class CONTENT_EXPORT ScopedNotifyNativeEventProcessorObserver {
 public:
  ScopedNotifyNativeEventProcessorObserver(
      base::ObserverList<NativeEventProcessorObserver>::Unchecked*
          observer_list,
      NSEvent* event);
  ~ScopedNotifyNativeEventProcessorObserver();

 private:
  base::ObserverList<NativeEventProcessorObserver>::Unchecked* observer_list_;
  NSEvent* event_;
  DISALLOW_COPY_AND_ASSIGN(ScopedNotifyNativeEventProcessorObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_EVENT_PROCESSOR_OBSERVER_MAC_H_
