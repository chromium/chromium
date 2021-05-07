// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_H_

#include "base/macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_item.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class ClipboardItemOptions;
class Navigator;
class ScriptState;

class Clipboard : public EventTargetWithInlineData,
                  public Supplement<Navigator> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static Clipboard* clipboard(Navigator&);
  explicit Clipboard(Navigator&);

  ScriptPromise read(ScriptState*);
  ScriptPromise read(ScriptState*, ClipboardItemOptions*);
  ScriptPromise readText(ScriptState*);

  ScriptPromise write(ScriptState*, const HeapVector<Member<ClipboardItem>>&);
  ScriptPromise writeText(ScriptState*, const String&);

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  void Trace(Visitor*) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(Clipboard);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_H_
