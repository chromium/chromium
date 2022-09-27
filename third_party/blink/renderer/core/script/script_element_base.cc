// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/script/script_element_base.h"

#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/svg/svg_script_element.h"

namespace blink {

ScriptLoader* ScriptLoaderFromElement(Element* element) {
  ScriptLoader* script_loader = nullptr;
  if (auto* html_script = DynamicTo<HTMLScriptElement>(*element))
    script_loader = html_script->Loader();
  else if (auto* svg_script = DynamicTo<SVGScriptElement>(*element))
    script_loader = svg_script->Loader();
  DCHECK(script_loader);
  return script_loader;
}

ScriptLoader* ScriptElementBase::InitializeScriptLoader(
    CreateElementFlags flags) {
  return MakeGarbageCollected<ScriptLoader>(this, flags);
}

}  // namespace blink
