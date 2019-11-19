// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_TYPE_POLICY_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_TYPE_POLICY_FACTORY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ExceptionState;
class ScriptState;
class ScriptValue;
class TrustedHTML;
class TrustedTypePolicy;
class TrustedTypePolicyOptions;

class CORE_EXPORT TrustedTypePolicyFactory final : public ScriptWrappable,
                                                   public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(TrustedTypePolicyFactory);

 public:
  explicit TrustedTypePolicyFactory(ExecutionContext*);

  // TrustedTypePolicyFactory.idl
  TrustedTypePolicy* createPolicy(const String&,
                                  const TrustedTypePolicyOptions*,
                                  ExceptionState&);

  TrustedTypePolicy* defaultPolicy() const;

  Vector<String> getPolicyNames() const;

  bool isHTML(ScriptState*, const ScriptValue&);
  bool isScript(ScriptState*, const ScriptValue&);
  bool isScriptURL(ScriptState*, const ScriptValue&);
  bool isURL(ScriptState*, const ScriptValue&);

  TrustedHTML* emptyHTML() const;

  String getPropertyType(const String& tagName,
                         const String& propertyName) const;
  String getPropertyType(const String& tagName,
                         const String& propertyName,
                         const String& elementNS) const;
  String getAttributeType(const String& tagName,
                          const String& attributeName) const;
  String getAttributeType(const String& tagName,
                          const String& attributeName,
                          const String& tagNS) const;
  String getAttributeType(const String& tagName,
                          const String& attributeName,
                          const String& tagNS,
                          const String& attributeNS) const;
  ScriptValue getTypeMapping(ScriptState*) const;
  ScriptValue getTypeMapping(ScriptState*, const String& ns) const;

  // Count whether a Trusted Type error occured during DOM operations.
  // (We aggregate this here to get a count per document, so that we can
  //  relate it to the total number of TT enabled documents.)
  void CountTrustedTypeAssignmentError();

  void Trace(blink::Visitor*) override;

 private:
  const WrapperTypeInfo* GetWrapperTypeInfoFromScriptValue(ScriptState*,
                                                           const ScriptValue&);

  Member<TrustedHTML> empty_html_;
  HeapHashMap<String, Member<TrustedTypePolicy>> policy_map_;

  bool hadAssignmentError = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_TRUSTED_TYPE_POLICY_FACTORY_H_
