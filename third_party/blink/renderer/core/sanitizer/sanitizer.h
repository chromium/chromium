// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_SANITIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_SANITIZER_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class SanitizerConfig;
class V8UnionSanitizerAttributeNamespaceOrString;
class V8UnionSanitizerElementNamespaceWithAttributesOrString;
class V8UnionSanitizerElementNamespaceOrString;

class Sanitizer final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Called by JS constructor, new Sanitizer(config).
  static Sanitizer* Create(ExecutionContext*,
                           const SanitizerConfig*,
                           ExceptionState&);
  Sanitizer() = default;
  ~Sanitizer() override = default;

  // API methods:
  void allowElement(
      const V8UnionSanitizerElementNamespaceWithAttributesOrString*);
  void removeElement(const V8UnionSanitizerElementNamespaceOrString*);
  void replaceWithChildrenElement(
      const V8UnionSanitizerElementNamespaceOrString*);
  void allowAttribute(const V8UnionSanitizerAttributeNamespaceOrString*);
  void removeAttribute(const V8UnionSanitizerAttributeNamespaceOrString*);
  void setComments(bool);
  void setDataAttributes(bool);
  void removeUnsafe();
  SanitizerConfig* get() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_SANITIZER_H_
