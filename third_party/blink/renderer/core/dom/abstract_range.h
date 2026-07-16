// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ABSTRACT_RANGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ABSTRACT_RANGE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class Document;
class Node;

class CORE_EXPORT AbstractRange : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  virtual unsigned startOffset() const = 0;
  virtual unsigned endOffset() const = 0;
  virtual bool collapsed() const = 0;

  // Exposed on AbstractRange only when LegacyAbstractRange is on (i.e. when
  // OpaqueRange is off). Range/StaticRange override via NodeRange; OpaqueRange
  // inherits the nullptr default.
  // TODO(crbug.com/421421332): Move these down to NodeRange when the
  // OpaqueRange flag is cleaned up.
  virtual Node* startContainer() const { return nullptr; }
  virtual Node* endContainer() const { return nullptr; }

  static bool HasDifferentRootContainer(Node* start_root_container,
                                        Node* end_root_container);
  static unsigned LengthOfContents(const Node*);
  virtual bool IsStaticRange() const = 0;
  virtual bool IsOpaqueRange() const { return false; }
  virtual Document& OwnerDocument() const = 0;

 protected:
  AbstractRange();
  ~AbstractRange() override;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_ABSTRACT_RANGE_H_
