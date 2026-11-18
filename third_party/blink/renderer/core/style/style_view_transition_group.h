// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_VIEW_TRANSITION_GROUP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_VIEW_TRANSITION_GROUP_H_

#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/core/style/style_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

class StyleViewTransitionGroup {
  DISALLOW_NEW();

 public:
  enum class GroupType { kNormal, kCustom, kNearest, kContain };

  bool IsNearest() const { return type_ == GroupType::kNearest; }
  bool IsCustom() const { return type_ == GroupType::kCustom; }
  bool IsNormal() const { return type_ == GroupType::kNormal; }
  bool IsContain() const { return type_ == GroupType::kContain; }

  static StyleViewTransitionGroup Nearest(
      const TreeScope* tree_scope = nullptr) {
    return StyleViewTransitionGroup(GroupType::kNearest, tree_scope);
  }
  static StyleViewTransitionGroup Normal() {
    return StyleViewTransitionGroup(GroupType::kNormal, nullptr);
  }
  static StyleViewTransitionGroup Contain(
      const TreeScope* tree_scope = nullptr) {
    return StyleViewTransitionGroup(GroupType::kContain, tree_scope);
  }
  static StyleViewTransitionGroup Create(
      const AtomicString& name,
      const TreeScope* tree_scope = nullptr) {
    CHECK(name);
    CHECK_NE(name, "nearest");
    CHECK_NE(name, "normal");
    CHECK_NE(name, "contain");
    return StyleViewTransitionGroup(name, tree_scope);
  }

  AtomicString CustomName() const {
    CHECK_EQ(type_, GroupType::kCustom);
    return custom_name_;
  }

  const TreeScope* GetTreeScope() const { return tree_scope_.Get(); }

  bool operator==(const StyleViewTransitionGroup& other) const {
    return type_ == other.type_ && custom_name_ == other.custom_name_ &&
           tree_scope_ == other.tree_scope_;
  }

  void Trace(Visitor* visitor) const { visitor->Trace(tree_scope_); }

 private:
  StyleViewTransitionGroup(const AtomicString& custom_name,
                           const TreeScope* tree_scope)
      : type_(GroupType::kCustom),
        custom_name_(custom_name),
        tree_scope_(tree_scope) {}

  StyleViewTransitionGroup(GroupType type, const TreeScope* tree_scope)
      : type_(type), tree_scope_(tree_scope) {}
  GroupType type_;
  AtomicString custom_name_;
  WeakMember<const TreeScope> tree_scope_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_VIEW_TRANSITION_GROUP_H_
