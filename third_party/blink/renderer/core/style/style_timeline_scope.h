// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_TIMELINE_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_TIMELINE_SCOPE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

// https://drafts.csswg.org/scroll-animations-1/#timeline-scope
class CORE_EXPORT StyleTimelineScope {
 public:
  enum class Type { kNone, kAll, kNames };

  StyleTimelineScope() = default;
  StyleTimelineScope(Type type, Vector<AtomicString> names)
      : type_(type), names_(std::move(names)) {}

  bool operator==(const StyleTimelineScope& o) const {
    return type_ == o.type_ && names_ == o.names_;
  }

  bool IsNone() const { return type_ == Type::kNone; }
  bool IsAll() const { return type_ == Type::kAll; }

  const Vector<AtomicString>& Names() const { return names_; }

 private:
  Type type_ = Type::kNone;
  Vector<AtomicString> names_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_TIMELINE_SCOPE_H_
