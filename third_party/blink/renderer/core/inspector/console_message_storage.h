// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_CONSOLE_MESSAGE_STORAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_CONSOLE_MESSAGE_STORAGE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ConsoleMessage;
class ExecutionContext;

class CORE_EXPORT ConsoleMessageStorage
    : public GarbageCollected<ConsoleMessageStorage> {
 public:
  ConsoleMessageStorage();

  // If |discard_duplicates| is set, the message will only be added if no
  // console message with the same text has exists in |messages_|. Returns
  // whether the given message was actually added.
  bool AddConsoleMessage(ExecutionContext*,
                         ConsoleMessage*,
                         bool discard_duplicates = false);
  void Clear();
  wtf_size_t size() const;
  ConsoleMessage* at(wtf_size_t index) const;
  int ExpiredCount() const;

  void Trace(blink::Visitor*);

 private:
  int expired_count_;
  HeapDeque<Member<ConsoleMessage>> messages_;

  DISALLOW_COPY_AND_ASSIGN(ConsoleMessageStorage);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_INSPECTOR_CONSOLE_MESSAGE_STORAGE_H_
