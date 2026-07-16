// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_INSTALL_RESULT_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_INSTALL_RESULT_EVENT_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_install_result.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/event.h"

namespace blink {

class InstallResultEventInit;

// TODO(crbug.com/535263061): Move the <install> element's event and related
// files into a dedicated folder.
class CORE_EXPORT InstallResultEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static InstallResultEvent* Create(const AtomicString& type,
                                    const InstallResultEventInit* initializer) {
    return MakeGarbageCollected<InstallResultEvent>(type, initializer);
  }

  InstallResultEvent(const AtomicString& type,
                     const InstallResultEventInit* initializer);
  ~InstallResultEvent() override;

  V8InstallResult result() const { return V8InstallResult(result_); }

  const AtomicString& InterfaceName() const override;

  void Trace(Visitor*) const override;

 private:
  V8InstallResult::Enum result_ = V8InstallResult::Enum::kAborted;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_INSTALL_RESULT_EVENT_H_
