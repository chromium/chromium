/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_DOCUMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_DOCUMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"

namespace blink {

class CORE_EXPORT HTMLDocument : public Document {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit HTMLDocument(
      const DocumentInit&,
      DocumentClassFlags extended_document_classes = DocumentClassFlags());
  ~HTMLDocument() override;

  static HTMLDocument* CreateForTest(ExecutionContext& execution_context);

  void AddNamedItem(const AtomicString& name);
  void RemoveNamedItem(const AtomicString& name);
  bool HasNamedItem(const AtomicString& name);
  bool HasAnyNamedItems() const { return !named_items_in_order_.empty(); }
  // The current named items, in the order in which they were (first) added.
  const LinkedHashSet<AtomicString>& NamedItems() const {
    return named_items_in_order_;
  }

  static bool IsCaseSensitiveAttribute(const QualifiedName&);

  Document* CloneDocumentWithoutChildren() const final;

  [[nodiscard]] v8::Local<v8::Object> AssociateWithWrapper(
      v8::Isolate*,
      const WrapperTypeInfo*,
      v8::Local<v8::Object> wrapper) override;

 private:
  HashCountedSet<AtomicString> named_item_counts_;
  // The keys of `named_item_counts_`, in the order in which they were added.
  // The named property accessors are installed on the document wrapper in this
  // order, which is observable (e.g. via Object.getOwnPropertyNames()). It
  // matches the order in which they'd have been installed if the wrapper had
  // existed when they were added.
  // TODO(crbug.com/587541566): The spec requires tree order, which this is not
  // (e.g. for named elements inserted before existing ones).
  LinkedHashSet<AtomicString> named_items_in_order_;
};

inline bool HTMLDocument::HasNamedItem(const AtomicString& name) {
  return named_item_counts_.Contains(name);
}

template <>
struct DowncastTraits<HTMLDocument> {
  static bool AllowFrom(const Document& document) {
    return document.IsHTMLDocument();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_DOCUMENT_H_
