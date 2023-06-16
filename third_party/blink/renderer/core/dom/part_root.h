// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_ROOT_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_part_root.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class Document;
class DocumentPartRoot;
class Part;

// Implementation of the PartRoot class, which is part of the DOM Parts API.
// PartRoot is the base of the class hierarchy.
class CORE_EXPORT PartRoot : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~PartRoot() override = default;

  void Trace(Visitor* visitor) const override;

  // Adds a new part to this PartRoot's collection of maintained parts.
  void AddPart(Part& new_part);
  void RemovePart(Part& part);
  virtual String ToString() const = 0;
  // Both DocumentPartRoot and ChildNodePart can have contained parts, while
  // NodePart cannot. However, due to the class hierarchy, NodePart is a
  // PartRoot, so this method is used to detect which PartRoots can actually
  // have contained parts.
  virtual bool SupportsContainedParts() const { return false; }

  // PartRoot API
  HeapVector<Member<Part>> getParts();
  // TODO(crbug.com/1453291) Implement this method.
  PartRoot* clone() const { return nullptr; }

 protected:
  PartRoot() = default;
  virtual bool IsPart() const { return false; }
  virtual bool IsDocumentPartRoot() const { return false; }
  virtual Document* GetDocument() const = 0;

 private:
  DocumentPartRoot* GetDocumentPartRoot();
  HeapVector<Member<Part>> RebuildPartsList();

  HeapVector<Member<Part>> parts_unordered_;
  HeapVector<Member<Part>> cached_ordered_parts_;
  bool cached_parts_list_dirty_{false};
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const PartRoot&);
CORE_EXPORT std::ostream& operator<<(std::ostream&, const PartRoot*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_ROOT_H_
