// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_API_NAVIGATION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_API_NAVIGATION_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class AppHistory;
class AppHistoryEntry;
class AppHistoryResult;
class AppHistoryNavigationOptions;
class ScriptState;
class ScriptPromiseResolver;
class SerializedScriptValue;

class AppHistoryApiNavigation final
    : public GarbageCollected<AppHistoryApiNavigation> {
 public:
  AppHistoryApiNavigation(ScriptState*,
                          AppHistory*,
                          AppHistoryNavigationOptions*,
                          const String& key,
                          scoped_refptr<SerializedScriptValue> state = nullptr);

  void NotifyAboutTheCommittedToEntry(AppHistoryEntry*);
  void ResolveFinishedPromise();
  void RejectFinishedPromise(const ScriptValue& value);
  void CleanupForCrossDocument();

  // Note: even though this returns the same AppHistoryResult every time, the
  // bindings layer will create a new JS object for each distinct AppHistory
  // method call, so we still match the specified semantics.
  AppHistoryResult* GetAppHistoryResult() const { return result_; }

  const ScriptValue& GetInfo() const { return info_; }
  const String& GetKey() const { return key_; }

  SerializedScriptValue* TakeSerializedState() {
    return serialized_state_.release();
  }
  SerializedScriptValue* GetSerializedState() const {
    return serialized_state_.get();
  }

  void Trace(Visitor* visitor) const;

 private:
  scoped_refptr<SerializedScriptValue> serialized_state_;
  ScriptValue info_;
  String key_;
  Member<AppHistory> app_history_;
  Member<AppHistoryEntry> committed_to_entry_;
  Member<ScriptPromiseResolver> committed_resolver_;
  Member<ScriptPromiseResolver> finished_resolver_;
  Member<AppHistoryResult> result_;

  // In same-document traversal cases ResolveFinishedPromise() can be called
  // before NotifyAboutTheCommittedToEntry(). This tracks that, to let us ensure
  // NotifyAboutTheCommittedToEntry() can also resolve the finished promise.
  bool did_finish_before_commit_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_APP_HISTORY_APP_HISTORY_API_NAVIGATION_H_
