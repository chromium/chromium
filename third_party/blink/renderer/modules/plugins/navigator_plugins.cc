// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/plugins/navigator_plugins.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/modules/plugins/dom_mime_type.h"
#include "third_party/blink/renderer/modules/plugins/dom_mime_type_array.h"
#include "third_party/blink/renderer/modules/plugins/dom_plugin_array.h"

namespace blink {

NavigatorPlugins::NavigatorPlugins(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

// static
NavigatorPlugins& NavigatorPlugins::From(Navigator& navigator) {
  NavigatorPlugins* supplement = ToNavigatorPlugins(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorPlugins>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
NavigatorPlugins* NavigatorPlugins::ToNavigatorPlugins(Navigator& navigator) {
  return Supplement<Navigator>::From<NavigatorPlugins>(navigator);
}

// static
const char NavigatorPlugins::kSupplementName[] = "NavigatorPlugins";

// static
DOMPluginArray* NavigatorPlugins::plugins(Navigator& navigator) {
  return NavigatorPlugins::From(navigator).plugins(navigator.DomWindow());
}

// static
DOMMimeTypeArray* NavigatorPlugins::mimeTypes(Navigator& navigator) {
  return NavigatorPlugins::From(navigator).mimeTypes(navigator.DomWindow());
}

// static
bool NavigatorPlugins::pdfViewerEnabled(Navigator& navigator) {
  return NavigatorPlugins::From(navigator).pdfViewerEnabled(
      navigator.DomWindow());
}

// static
bool NavigatorPlugins::javaEnabled(Navigator& navigator) {
  return false;
}

DOMPluginArray* NavigatorPlugins::plugins(LocalDOMWindow* window) const {
  if (!plugins_) {
    plugins_ = MakeGarbageCollected<DOMPluginArray>(window);
  }

  DOMPluginArray* result = plugins_.Get();
  return result;
}

DOMMimeTypeArray* NavigatorPlugins::mimeTypes(LocalDOMWindow* window) const {
  if (!mime_types_) {
    mime_types_ = MakeGarbageCollected<DOMMimeTypeArray>(window);
  }
  return mime_types_.Get();
}

bool NavigatorPlugins::pdfViewerEnabled(LocalDOMWindow* window) const {
  return plugins(window)->IsPdfViewerAvailable();
}

void NavigatorPlugins::Trace(Visitor* visitor) const {
  visitor->Trace(plugins_);
  visitor->Trace(mime_types_);
  Supplement<Navigator>::Trace(visitor);
}

}  // namespace blink
