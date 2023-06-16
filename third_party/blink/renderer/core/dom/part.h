// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/part_root.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class Node;

// Implementation of the Part class, which is part of the DOM Parts API.
// This is the base class for all Part types, and it does not have a JS-public
// constructor. The Part class holds a reference to its root, which is never
// nullptr.
class CORE_EXPORT Part : public PartRoot {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~Part() override = default;

  void Trace(Visitor* visitor) const override;
  virtual bool IsValid() = 0;
  virtual Node* RelevantNode() const = 0;

  // Part API
  PartRoot* root() const { return root_; }
  // TODO(1453291) Populate metadata_.
  Vector<String>& metadata() { return metadata_; }
  void disconnect();

 protected:
  explicit Part(PartRoot& root);
  bool IsPart() const override { return true; }

 private:
  Member<PartRoot> root_;
  Vector<String> metadata_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PART_H_
