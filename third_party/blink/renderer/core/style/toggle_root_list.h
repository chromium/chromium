// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_ROOT_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_ROOT_LIST_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/style/toggle_root.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

typedef WTF::Vector<ToggleRoot, 1> ToggleRootVector;

class ToggleRootList : public RefCounted<ToggleRootList> {
 public:
  static scoped_refptr<ToggleRootList> Create() {
    return base::AdoptRef(new ToggleRootList());
  }

  bool operator==(const ToggleRootList& other) const {
    return roots_ == other.roots_;
  }
  bool operator!=(const ToggleRootList& other) const {
    return !(*this == other);
  }

  void Append(ToggleRoot&& root) { roots_.push_back(std::move(root)); }

  const ToggleRootVector& Roots() const { return roots_; }

 private:
  ToggleRootVector roots_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_ROOT_LIST_H_
