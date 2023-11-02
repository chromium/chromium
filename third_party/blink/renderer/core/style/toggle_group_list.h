// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_GROUP_LIST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_GROUP_LIST_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/style/toggle_group.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

using ToggleGroupVector = WTF::Vector<ToggleGroup, 1>;

class ToggleGroupList : public RefCounted<ToggleGroupList> {
 public:
  static scoped_refptr<ToggleGroupList> Create() {
    return base::AdoptRef(new ToggleGroupList());
  }

  bool operator==(const ToggleGroupList& other) const {
    return groups_ == other.groups_;
  }
  bool operator!=(const ToggleGroupList& other) const {
    return !(*this == other);
  }

  void Append(ToggleGroup&& group) { groups_.push_back(std::move(group)); }

  const ToggleGroupVector& Groups() const { return groups_; }

 private:
  ToggleGroupVector groups_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_TOGGLE_GROUP_LIST_H_
