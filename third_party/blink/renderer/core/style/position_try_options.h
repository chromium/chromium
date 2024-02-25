// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_POSITION_TRY_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_POSITION_TRY_OPTIONS_H_

#include "third_party/blink/renderer/core/style/scoped_css_name.h"

namespace blink {

enum class TryTactic {
  kNone = 0,
  kFlipBlock = 1 << 1,
  kFlipInline = 1 << 2,
  kFlipStart = 1 << 3,
};

using TryTacticFlags = unsigned;

class CORE_EXPORT PositionTryOption {
  DISALLOW_NEW();

 public:
  explicit PositionTryOption(TryTacticFlags tactic) : tactic_(tactic) {}
  explicit PositionTryOption(const ScopedCSSName* name)
      : position_try_name_(name) {}

  bool HasTryTactic() const {
    return tactic_ != static_cast<TryTacticFlags>(TryTactic::kNone);
  }
  TryTacticFlags GetTryTactic() const { return tactic_; }
  const ScopedCSSName* GetPositionTryName() const { return position_try_name_; }

  bool operator==(const PositionTryOption& other) const;

  void Trace(Visitor* visitor) const;

 private:
  Member<const ScopedCSSName> position_try_name_;
  TryTacticFlags tactic_ = static_cast<TryTacticFlags>(TryTactic::kNone);
};

class CORE_EXPORT PositionTryOptions
    : public GarbageCollected<PositionTryOptions> {
 public:
  PositionTryOptions(HeapVector<PositionTryOption> options)
      : options_(std::move(options)) {}

  const HeapVector<PositionTryOption>& GetOptions() const { return options_; }
  bool operator==(const PositionTryOptions& other) const;
  void Trace(Visitor* visitor) const;

 private:
  HeapVector<PositionTryOption> options_;
};

}  // namespace blink

namespace WTF {

template <>
struct VectorTraits<blink::PositionTryOption>
    : VectorTraitsBase<blink::PositionTryOption> {
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
  static const bool kCanTraceConcurrently = true;
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_POSITION_TRY_OPTIONS_H_
