// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_

#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/sanitizer_api/sanitizer_config_impl.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ContainerNode;
class Document;
class DocumentFragment;
class Element;
class ExceptionState;
class ExecutionContext;
class Node;
class LocalDOMWindow;
class SanitizerConfig;
class ScriptState;

class MODULES_EXPORT Sanitizer final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Sanitizer* Create(ExecutionContext*,
                           const SanitizerConfig*,
                           ExceptionState&);
  Sanitizer(ExecutionContext*, const SanitizerConfig*);
  ~Sanitizer() override;

  DocumentFragment* sanitize(ScriptState* script_state,
                             const V8SanitizerInput* input,
                             ExceptionState& exception_state);
  Element* sanitizeFor(ScriptState* script_state,
                       const String& element,
                       const String& markup,
                       ExceptionState& exception_state);

  SanitizerConfig* getConfiguration() const;
  static SanitizerConfig* getDefaultConfiguration();

  // Get a (shared) Sanitizer instance with the default configuration.
  static Sanitizer* getDefaultInstance();

  // Implementation of ElementSanitizer::SetHTML, so that we have
  // all the sanitizer logic in one place.
  void ElementSetHTML(ScriptState* script_state,
                      Element& element,
                      const String& markup,
                      ExceptionState& exception_state);

  void Trace(Visitor*) const override;

 private:
  Node* DropNode(Node*, ContainerNode*);
  Node* BlockElement(Element*, ContainerNode*, ExceptionState&);
  Node* KeepElement(Element*, ContainerNode*, LocalDOMWindow*);

  DocumentFragment* PrepareFragment(LocalDOMWindow* window,
                                    ScriptState* script_state,
                                    const V8SanitizerInput* input,
                                    ExceptionState& exception_state);
  void DoSanitizing(ContainerNode*, LocalDOMWindow*, ExceptionState&);

  SanitizerConfigImpl config_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SANITIZER_API_SANITIZER_H_
