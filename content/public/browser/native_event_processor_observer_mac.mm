// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/native_event_processor_observer_mac.h"

#include "base/observer_list.h"

namespace content {

ScopedNotifyNativeEventProcessorObserver::
    ScopedNotifyNativeEventProcessorObserver(
        base::ObserverList<NativeEventProcessorObserver>::Unchecked*
            observer_list,
        NSEvent* event)
    : observer_list_(observer_list), event_(event) {
  for (auto& observer : *observer_list_)
    observer.WillRunNativeEvent((__bridge const void*)event_);
}

ScopedNotifyNativeEventProcessorObserver::
    ~ScopedNotifyNativeEventProcessorObserver() {
  for (auto& obs : *observer_list_) {
    obs.DidRunNativeEvent((__bridge const void*)event_);
  }
}

}  // namespace content
