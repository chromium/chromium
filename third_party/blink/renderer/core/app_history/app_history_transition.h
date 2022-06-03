// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_TRANSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_TRANSITION_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class AppHistoryEntry;

class CORE_EXPORT AppHistoryTransition final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AppHistoryTransition(const String& navigation_type, AppHistoryEntry* from);
  ~AppHistoryTransition() final = default;

  const String& navigationType() const { return navigation_type_; }
  AppHistoryEntry* from() const { return from_; }

  void Trace(Visitor*) const final;

 private:
  String navigation_type_;
  Member<AppHistoryEntry> from_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_TRANSITION_H_
