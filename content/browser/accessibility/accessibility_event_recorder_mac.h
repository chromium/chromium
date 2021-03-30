// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_MAC_H_

#include "content/browser/accessibility/accessibility_event_recorder.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"

@class BrowserAccessibilityCocoa;

namespace content {

// Implementation of AccessibilityEventRecorder that uses AXObserver to
// watch for NSAccessibility events.
class CONTENT_EXPORT AccessibilityEventRecorderMac
    : public AccessibilityEventRecorder {
 public:
  AccessibilityEventRecorderMac(BrowserAccessibilityManager* manager,
                                base::ProcessId pid,
                                const AXTreeSelector& selector);
  ~AccessibilityEventRecorderMac() override;

  // Callback executed every time we receive an event notification.
  void EventReceived(AXUIElementRef element,
                     CFStringRef notification,
                     CFDictionaryRef user_info);
  static std::string SerializeTextSelectionChangedProperties(
      CFDictionaryRef user_info);

 private:
  // Add one notification to the list of notifications monitored by our
  // observer.
  void AddNotification(NSString* notification);

  // Convenience function to get the value of an AX attribute from
  // an AXUIElementRef as a string.
  std::string GetAXAttributeValue(AXUIElementRef element,
                                  NSString* attribute_name);

  // The AXUIElement for the Chrome application.
  base::ScopedCFTypeRef<AXUIElementRef> application_;

  // The AXObserver we use to monitor AX notifications.
  base::ScopedCFTypeRef<AXObserverRef> observer_ref_;
  CFRunLoopSourceRef observer_run_loop_source_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityEventRecorderMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_MAC_H_
