// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_item.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Navigator;
class ScriptState;
class ClipboardUnsanitizedFormats;

class Clipboard : public EventTarget, public Supplement<Navigator> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];
  static Clipboard* clipboard(Navigator&);
  explicit Clipboard(Navigator&);

  Clipboard(const Clipboard&) = delete;
  Clipboard& operator=(const Clipboard&) = delete;

  ScriptPromise read(ScriptState*,
                     ClipboardUnsanitizedFormats* formats = nullptr);
  ScriptPromise readText(ScriptState*);

  ScriptPromise write(ScriptState*, const HeapVector<Member<ClipboardItem>>&);
  ScriptPromise writeText(ScriptState*, const String&);

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  static String ParseWebCustomFormat(const String& format);

  void Trace(Visitor*) const override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_H_
