// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/font_access/navigator_fonts.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/modules/font_access/font_manager.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

template <typename T>
class NavigatorFontsImpl final : public GarbageCollected<NavigatorFontsImpl<T>>,
                                 public Supplement<T>,
                                 public NameClient {
 public:
  static const char kSupplementName[];

  static NavigatorFontsImpl& From(T& navigator) {
    NavigatorFontsImpl* supplement = static_cast<NavigatorFontsImpl*>(
        Supplement<T>::template From<NavigatorFontsImpl>(navigator));
    if (!supplement) {
      supplement = MakeGarbageCollected<NavigatorFontsImpl>(navigator);
      Supplement<T>::ProvideTo(navigator, supplement);
    }
    return *supplement;
  }

  explicit NavigatorFontsImpl(T& navigator) : Supplement<T>(navigator) {}

  FontManager* GetFontManager(ExecutionContext* context) const {
    if (!font_manager_) {
      font_manager_ = MakeGarbageCollected<FontManager>(context);
    }
    return font_manager_.Get();
  }

  void Trace(blink::Visitor* visitor) const override {
    visitor->Trace(font_manager_);
    Supplement<T>::Trace(visitor);
  }

  const char* NameInHeapSnapshot() const override {
    return "NavigatorFontsImpl";
  }

 private:
  mutable Member<FontManager> font_manager_;
};

// static
template <typename T>
const char NavigatorFontsImpl<T>::kSupplementName[] = "NavigatorFontsImpl";

}  // namespace

FontManager* NavigatorFonts::fonts(ScriptState* script_state,
                                   Navigator& navigator,
                                   ExceptionState& exception_state) {
  DCHECK(ExecutionContext::From(script_state)->IsContextThread());
  ExecutionContext* context = ExecutionContext::From(script_state);
  return NavigatorFontsImpl<Navigator>::From(navigator).GetFontManager(context);
}

}  // namespace blink
