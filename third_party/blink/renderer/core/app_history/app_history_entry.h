// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_ENTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_ENTRY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class HistoryItem;
class ScriptValue;

class CORE_EXPORT AppHistoryEntry final : public EventTargetWithInlineData,
                                          public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  AppHistoryEntry(ExecutionContext*, HistoryItem*);
  ~AppHistoryEntry() final = default;

  String key() const;
  String id() const;
  KURL url();
  int64_t index();
  bool sameDocument() const;

  ScriptValue getState() const;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(dispose, kDispose)

  HistoryItem* GetItem() { return item_; }

  // EventTargetWithInlineData overrides:
  const AtomicString& InterfaceName() const final;
  ExecutionContext* GetExecutionContext() const final {
    return ExecutionContextClient::GetExecutionContext();
  }

  void Trace(Visitor*) const final;

 private:
  Member<HistoryItem> item_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_ENTRY_H_
