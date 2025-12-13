// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_SANITIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_SANITIZER_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_sanitizer_presets.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer_names.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class Element;
class ExceptionState;
class Node;
class QualifiedName;
class SanitizerConfig;
class SanitizerElementNamespace;
class V8UnionSanitizerConfigOrSanitizerPresets;
class V8UnionSanitizerAttributeNamespaceOrString;
class V8UnionSanitizerElementNamespaceWithAttributesOrString;
class V8UnionSanitizerElementNamespaceOrString;

class CORE_EXPORT Sanitizer final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Called by WebIDL for Sanitizer constructor, new Sanitizer(xxx).
  static Sanitizer* Create(const V8UnionSanitizerConfigOrSanitizerPresets*,
                           ExceptionState&);

  // Called by Sanitizer API, to implement setHTML / setHTMLUnsafe & friends.
  static Sanitizer* Create(const SanitizerConfig*, bool safe, ExceptionState&);
  static Sanitizer* Create(const V8SanitizerPresets::Enum, ExceptionState&);

  static Sanitizer* CreateEmpty();

  Sanitizer() = default;
  ~Sanitizer() override = default;
  Sanitizer(const Sanitizer&) = delete;  // Use MakeGarbageCollected + setFrom.

  // This constructor is meant to be used by generated code to build up the
  // Sanitizer builtins. You probably don't want to use it in regular code.
  Sanitizer(std::unique_ptr<SanitizerNameSet>,
            std::unique_ptr<SanitizerNameSet>,
            std::unique_ptr<SanitizerNameSet>,
            std::unique_ptr<SanitizerNameSet>,
            std::unique_ptr<SanitizerNameSet>,
            SanitizerNameMap,
            SanitizerNameMap,
            bool,
            bool);

  // API methods:
  bool allowElement(
      const V8UnionSanitizerElementNamespaceWithAttributesOrString*);
  bool removeElement(const V8UnionSanitizerElementNamespaceOrString*);
  bool replaceElementWithChildren(
      const V8UnionSanitizerElementNamespaceOrString*);
  bool allowAttribute(const V8UnionSanitizerAttributeNamespaceOrString*);
  bool removeAttribute(const V8UnionSanitizerAttributeNamespaceOrString*);
  void setComments(bool);
  void setDataAttributes(bool);
  void removeUnsafe();
  SanitizerConfig* get() const;

  // Internal versions of API methods that use Blink types (rather than IDL):
  bool AllowElement(const QualifiedName&,
                    SanitizerNameSet* allow_attrs = nullptr,
                    SanitizerNameSet* remove_attrs = nullptr);
  bool RemoveElement(const QualifiedName&);
  bool ReplaceElement(const QualifiedName&);
  bool AllowAttribute(const QualifiedName&);
  bool RemoveAttribute(const QualifiedName&);

  // The core methods (not directly exposed to the API): Recursively sanitize
  // the node according to the current config.
  void SanitizeSafe(Node* node) const;
  void SanitizeUnsafe(Node* node) const;

  // Unit test support:
  const SanitizerNameSet* AllowElements() const {
    return allow_elements_.get();
  }
  const SanitizerNameSet* RemoveElements() const {
    return remove_elements_.get();
  }
  const SanitizerNameSet* ReplaceElements() const {
    return replace_elements_.get();
  }
  const SanitizerNameSet* AllowAttrs() const { return allow_attrs_.get(); }
  const SanitizerNameSet* RemoveAttrs() const { return remove_attrs_.get(); }
  const SanitizerNameMap& AllowAttrsPerElement() const {
    return allow_attrs_per_element_;
  }
  const SanitizerNameMap& RemoveAttrsPerElement() const {
    return remove_attrs_per_element_;
  }
  bool AllowDataAttrs() const {
    return data_attrs_ == SanitizerBoolWithAbsence::kTrue;
  }
  bool AllowComments() const {
    return comments_ == SanitizerBoolWithAbsence::kTrue;
  }

 private:
  enum class SanitizerBoolWithAbsence { kAbsent, kTrue, kFalse };

  // Helper methods for SanitizeSafe/Unsafe:
  void Sanitize(Node* node, bool safe) const;
  void SanitizeElement(Element* element) const;
  void SanitizeJavascriptNavigationAttributes(Element* element,
                                              bool safe) const;
  void SanitizeTemplate(Node* node, bool safe) const;

  // Helper for Create: Convert from IDL representation to internal.
  bool setFrom(const SanitizerConfig*, bool safe);
  // Helper for constructors: Copy from other Sanitizer.
  void setFrom(const Sanitizer&);

  // Helpers for get(): Convert from internal to IDL representation.
  QualifiedName getFrom(const String& name, const String& namespaceURI) const;
  QualifiedName getFrom(const SanitizerElementNamespace*) const;
  QualifiedName getFrom(
      const V8UnionSanitizerElementNamespaceWithAttributesOrString*) const;
  QualifiedName getFrom(const V8UnionSanitizerElementNamespaceOrString*) const;
  QualifiedName getFrom(
      const V8UnionSanitizerAttributeNamespaceOrString*) const;

  // Check configuration validity.
  bool isValid() const;

  // These members are the Blink-representation of SanitizerConfig, and the core
  // data structure(s) for Sanitizer. We'll use the Blink-specific types,
  // specifically HashSet and QualifiedName, at the expense of additional work
  // when converting from or to IDL types.
  //
  // The spec makes copious use of presence or absence of data members. We'll
  // represent these as follows:
  // - Name sets: Wrap them in unique_ptr. null pointer => absent member.
  // - Per element attributes: Use a map. No entry in map => absent member.
  // - Boolean items: Tri-state enum.
  std::unique_ptr<SanitizerNameSet> allow_elements_;
  std::unique_ptr<SanitizerNameSet> remove_elements_;
  std::unique_ptr<SanitizerNameSet> replace_elements_;
  std::unique_ptr<SanitizerNameSet> allow_attrs_;
  std::unique_ptr<SanitizerNameSet> remove_attrs_;
  SanitizerNameMap allow_attrs_per_element_;
  SanitizerNameMap remove_attrs_per_element_;
  SanitizerBoolWithAbsence data_attrs_;
  SanitizerBoolWithAbsence comments_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_SANITIZER_H_
