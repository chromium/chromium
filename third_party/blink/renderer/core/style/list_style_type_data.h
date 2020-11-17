// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_LIST_STYLE_TYPE_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_LIST_STYLE_TYPE_DATA_H_

#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class TreeScope;

class ListStyleTypeData final : public GarbageCollected<ListStyleTypeData> {
 public:
  ~ListStyleTypeData() = default;
  void Trace(Visitor*) const;

  enum class Type { kCounterStyle, kString };

  ListStyleTypeData(Type type,
                    AtomicString name_or_string_value,
                    const TreeScope* tree_scope)
      : type_(type),
        name_or_string_value_(name_or_string_value),
        tree_scope_(tree_scope) {}

  static ListStyleTypeData* CreateString(const AtomicString&);
  static ListStyleTypeData* CreateCounterStyle(const AtomicString&,
                                               const TreeScope*);

  bool operator==(const ListStyleTypeData& other) const {
    return type_ == other.type_ &&
           name_or_string_value_ == other.name_or_string_value_ &&
           tree_scope_ == other.tree_scope_;
  }
  bool operator!=(const ListStyleTypeData& other) const {
    return !operator==(other);
  }

  bool IsCounterStyle() const { return type_ == Type::kCounterStyle; }
  bool IsString() const { return type_ == Type::kString; }

  AtomicString GetCounterStyleName() const {
    DCHECK_EQ(Type::kCounterStyle, type_);
    return name_or_string_value_;
  }

  AtomicString GetStringValue() const {
    DCHECK_EQ(Type::kString, type_);
    return name_or_string_value_;
  }

  EListStyleType ToDeprecatedListStyleTypeEnum() const;

 private:
  Type type_;
  AtomicString name_or_string_value_;

  // The tree scope for looking up the custom counter style name
  Member<const TreeScope> tree_scope_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_LIST_STYLE_TYPE_DATA_H_
