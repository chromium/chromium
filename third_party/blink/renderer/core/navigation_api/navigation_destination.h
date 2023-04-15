// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_DESTINATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_DESTINATION_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/navigation_api/navigation_history_entry.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class CORE_EXPORT NavigationDestination final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  NavigationDestination(const KURL& url,
                        bool same_document,
                        SerializedScriptValue* state)
      : url_(url), same_document_(same_document), state_(state) {}
  ~NavigationDestination() final = default;

  void SetDestinationEntry(NavigationHistoryEntry* entry) { entry_ = entry; }

  String key() const { return entry_ ? entry_->key() : String(); }
  String id() const { return entry_ ? entry_->id() : String(); }
  const KURL& url() const { return url_; }
  int64_t index() const { return entry_ ? entry_->index() : -1; }
  bool sameDocument() const { return same_document_; }
  ScriptValue getState(ScriptState* script_state) {
    v8::Isolate* isolate = script_state->GetIsolate();
    return state_ ? ScriptValue(isolate, state_->Deserialize(isolate))
                  : ScriptValue();
  }

  void Trace(Visitor* visitor) const override {
    ScriptWrappable::Trace(visitor);
    visitor->Trace(entry_);
  }

 private:
  KURL url_;
  bool same_document_;
  scoped_refptr<SerializedScriptValue> state_;
  Member<NavigationHistoryEntry> entry_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_NAVIGATION_API_NAVIGATION_DESTINATION_H_
