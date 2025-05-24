// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_ACCESSIBILITY_CONTROLLER_H_
#define CONTENT_WEB_TEST_RENDERER_ACCESSIBILITY_CONTROLLER_H_

#include <vector>

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/web_test/renderer/web_ax_object_proxy.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "ui/accessibility/ax_event_intent.h"
#include "v8/include/v8.h"

namespace blink {

class WebAXContext;
class WebLocalFrame;
class WebString;
class WebView;

}  // namespace blink

namespace content {

class WebFrameTestProxy;

class AccessibilityController {
 public:
  explicit AccessibilityController(WebFrameTestProxy* web_frame_test_proxy);

  AccessibilityController(const AccessibilityController&) = delete;
  AccessibilityController& operator=(const AccessibilityController&) = delete;

  ~AccessibilityController();

  void Reset();
  void Install(blink::WebLocalFrame* frame);
  bool ShouldLogAccessibilityEvents();
  void NotificationReceived(
      blink::WebLocalFrame* frame,
      const blink::WebAXObject& target,
      const std::string& notification_name,
      const std::vector<ui::AXEventIntent>& event_intents);
  void PostNotification(const blink::WebAXObject& target,
                        const std::string& notification_name,
                        const std::vector<ui::AXEventIntent>& event_intents);

  void Remove(unsigned axid);

 private:
  friend class AccessibilityControllerBindings;

  // Bound methods and properties
  void LogAccessibilityEvents();
  void SetNotificationListener(v8::Local<v8::Function> callback);
  void UnsetNotificationListener();
  v8::Local<v8::Object> FocusedElement();
  v8::Local<v8::Object> RootElement();
  v8::Local<v8::Object> AccessibleElementById(const std::string& id);
  bool IsInstalled() { return elements_ != nullptr && ax_context_ != nullptr; }

  v8::Local<v8::Object> FindAccessibleElementByIdRecursive(
      const blink::WebAXObject&,
      const blink::WebString& id);

  blink::WebAXObject GetAccessibilityObjectForMainFrame() const;

  // If true, will log all accessibility notifications.
  bool log_accessibility_events_;

  std::unique_ptr<WebAXObjectProxyList> elements_;

  v8::Persistent<v8::Function> notification_callback_;

  blink::WebView* web_view() const;
  raw_ptr<WebFrameTestProxy> web_frame_test_proxy_;

  std::unique_ptr<blink::WebAXContext> ax_context_;

  base::WeakPtrFactory<AccessibilityController> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_ACCESSIBILITY_CONTROLLER_H_
