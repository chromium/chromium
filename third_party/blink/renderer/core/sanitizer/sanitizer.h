// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_SANITIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_SANITIZER_H_

#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class QualifiedName;
class SanitizerConfig;
class SanitizerElementNamespace;
class V8UnionSanitizerAttributeNamespaceOrString;
class V8UnionSanitizerElementNamespaceWithAttributesOrString;
class V8UnionSanitizerElementNamespaceOrString;

class CORE_EXPORT Sanitizer final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Called by JS constructor, new Sanitizer(config).
  static Sanitizer* Create(ExecutionContext*,
                           const SanitizerConfig*,
                           ExceptionState&);
  Sanitizer() = default;
  ~Sanitizer() override = default;

  // This constructor is meant to be used by generated code to build up the
  // Sanitizer builtins. You probably don't want to use it in regular code.
  Sanitizer(HashSet<QualifiedName>,
            HashSet<QualifiedName>,
            HashSet<QualifiedName>,
            HashSet<QualifiedName>,
            HashSet<QualifiedName>,
            bool,
            bool);

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

  // Internal versions of API methods that use Blink types (rather than IDL):
  void AllowElement(const QualifiedName&);
  void RemoveElement(const QualifiedName&);
  void ReplaceElement(const QualifiedName&);
  void AllowAttribute(const QualifiedName&);
  void RemoveAttribute(const QualifiedName&);

  // Accessors, mainly for testing:
  const HashSet<QualifiedName>& allow_elements() const {
    return allow_elements_;
  }
  const HashSet<QualifiedName>& remove_elements() const {
    return remove_elements_;
  }
  const HashSet<QualifiedName>& replace_elements() const {
    return replace_elements_;
  }
  const HashSet<QualifiedName>& allow_attrs() const { return allow_attrs_; }
  const HashSet<QualifiedName>& remove_attrs() const { return remove_attrs_; }
  const HashMap<QualifiedName, HashSet<QualifiedName>>&
  allow_attrs_per_element() const {
    return allow_attrs_per_element_;
  }
  const HashMap<QualifiedName, HashSet<QualifiedName>>&
  remove_attrs_per_element() const {
    return remove_attrs_per_element_;
  }
  bool allow_data_attrs() const { return allow_data_attrs_; }
  bool allow_comments() const { return allow_comments_; }

 private:
  // Helper for copy constructor and Create: Convert from IDL representation
  // to internal.
  bool setFrom(const SanitizerConfig*);

  // Helpers for get(): Convert from internal to IDL representation.
  QualifiedName getFrom(const String& name, const String& namespaceURI) const;
  QualifiedName getFrom(const SanitizerElementNamespace*) const;
  QualifiedName getFrom(
      const V8UnionSanitizerElementNamespaceWithAttributesOrString*) const;
  QualifiedName getFrom(const V8UnionSanitizerElementNamespaceOrString*) const;
  QualifiedName getFrom(
      const V8UnionSanitizerAttributeNamespaceOrString*) const;

  // These members are Blink-representation of SanitizerConfig, and the core
  // data structure(s) for Sanitizer. We'll try to keep them simple (sets and
  // maps), and to use QualifiedName, since that's an efficient-to-compare
  // QName implementation.
  HashSet<QualifiedName> allow_elements_;
  HashSet<QualifiedName> remove_elements_;
  HashSet<QualifiedName> replace_elements_;
  HashSet<QualifiedName> allow_attrs_;
  HashSet<QualifiedName> remove_attrs_;
  HashMap<QualifiedName, HashSet<QualifiedName>> allow_attrs_per_element_;
  HashMap<QualifiedName, HashSet<QualifiedName>> remove_attrs_per_element_;
  bool allow_data_attrs_;
  bool allow_comments_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SANITIZER_SANITIZER_H_
