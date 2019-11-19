/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCREEN_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCREEN_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "ui/display/mojom/display.mojom-blink.h"

namespace blink {

class LocalFrame;

class CORE_EXPORT Screen final : public ScriptWrappable,
                                 public DOMWindowClient,
                                 public Supplementable<Screen> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Screen);

 public:
  explicit Screen(LocalFrame*);

  int height() const;
  int width() const;
  unsigned colorDepth() const;
  unsigned pixelDepth() const;
  int availLeft() const;
  int availTop() const;
  int availHeight() const;
  int availWidth() const;

  void Trace(blink::Visitor*) override;

  // Proposed extensions to the Screen interface.
  // https://github.com/spark008/screen-enumeration/blob/master/EXPLAINER.md
  // TODO(msw): Resolve different info sources, caching, and lifetimes.
  Screen(display::mojom::blink::DisplayPtr display, bool primary);
  int left() const;
  int top() const;
  bool internal() const;
  bool primary() const;
  float scaleFactor() const;
  const String name() const;

 private:
  // A static snapshot of the display's information, provided upon construction.
  // This member is only valid for Screen objects obtained via the experimental
  // Screen Enumeration API.
  const display::mojom::blink::DisplayPtr display_;
  // True if this is the primary screen of the operating system; it is a static
  // value provided upon construction. This member is only valid for Screen
  // objects obtained via the experimental Screen Enumeration API.
  const base::Optional<bool> primary_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SCREEN_H_
