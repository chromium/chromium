// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PERSISTENT_WIDGETS_HTML_PERSISTENT_WIDGET_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PERSISTENT_WIDGETS_HTML_PERSISTENT_WIDGET_ELEMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

class ExceptionState;
class ScriptObject;
class ScriptValue;
class WindowPostMessageOptions;

class CORE_EXPORT HTMLPersistentWidgetElement : public HTMLFrameOwnerElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLPersistentWidgetElement(Document& document);

  // HTMLFrameOwnerElement overrides:
  FrameOwnerElementType OwnerType() const override {
    // TODO(crbug.com/537818859): Replace this with new FrameOwnerElementType
    // for persistent widgets.
    NOTREACHED();
  }
  network::ParsedPermissionsPolicy ConstructContainerPolicy() const override {
    return network::ParsedPermissionsPolicy();
  }

  // Element overrides:
  void ParseAttribute(const AttributeModificationParams& params) override;
  bool IsURLAttribute(const Attribute&) const override;

  void postMessage(const ScriptValue& message,
                   const String& target_origin,
                   ExceptionState& exception_state);
  void postMessage(const ScriptValue& message,
                   const WindowPostMessageOptions* options,
                   ExceptionState& exception_state);

  void Trace(Visitor* visitor) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PERSISTENT_WIDGETS_HTML_PERSISTENT_WIDGET_ELEMENT_H_
