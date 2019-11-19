// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_INVALIDATION_ROOT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_INVALIDATION_ROOT_H_

#include "third_party/blink/renderer/core/css/style_traversal_root.h"

namespace blink {

class CORE_EXPORT StyleInvalidationRoot : public StyleTraversalRoot {
  DISALLOW_NEW();

 public:
  Element* RootElement() const;

 private:
#if DCHECK_IS_ON()
  ContainerNode* Parent(const Node& node) const final;
#endif  // DCHECK_IS_ON()
  bool IsDirty(const Node& node) const final;
  void RootRemoved(ContainerNode& parent) final;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_STYLE_INVALIDATION_ROOT_H_
