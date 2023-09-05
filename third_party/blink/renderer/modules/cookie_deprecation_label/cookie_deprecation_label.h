// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_H_

#include "third_party/blink/public/mojom/cookie_deprecation_label/cookie_deprecation_label.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Navigator;
class ScriptState;

class CookieDeprecationLabel : public ScriptWrappable,
                               public Supplement<Navigator> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // Web exposed as navigator.cookieDeprecationLabel
  static CookieDeprecationLabel* cookieDeprecationLabel(Navigator& navigator);

  explicit CookieDeprecationLabel(Navigator& navigator);
  ~CookieDeprecationLabel() override;

  // Web exposed function defined in the IDL file.
  ScriptPromise getValue(ScriptState* script_state);

  void Trace(Visitor*) const override;

 private:
  mojom::blink::CookieDeprecationLabelDocumentService* GetDocumentService(
      ScriptState* script_state);

  // Created lazily.
  HeapMojoRemote<mojom::blink::CookieDeprecationLabelDocumentService>
      label_document_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_COOKIE_DEPRECATION_LABEL_COOKIE_DEPRECATION_LABEL_H_
