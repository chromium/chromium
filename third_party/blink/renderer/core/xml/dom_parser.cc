/*
 *  Copyright (C) 2003, 2006, 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA
 */

#include "third_party/blink/renderer/core/xml/dom_parser.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_parse_from_string_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

Document* DOMParser::parseFromString(const String& str,
                                     const String& type,
                                     const ParseFromStringOptions* options) {
  Document* doc = DocumentInit::Create()
                      .WithURL(window_->Url())
                      .WithTypeFrom(type)
                      .WithExecutionContext(window_)
                      .WithAgent(*window_->GetAgent())
                      .CreateDocument();
  bool include_shadow_roots =
      options->hasIncludeShadowRoots() && options->includeShadowRoots();
  doc->setAllowDeclarativeShadowRoots(include_shadow_roots);
  doc->CountUse(mojom::blink::WebFeature::kParseFromString);
  if (include_shadow_roots) {
    doc->CountUse(mojom::blink::WebFeature::kParseFromStringIncludeShadows);
  }
  doc->SetContent(str);
  doc->SetMimeType(AtomicString(type));
  return doc;
}

DOMParser::DOMParser(ScriptState* script_state)
    : window_(LocalDOMWindow::From(script_state)) {}

void DOMParser::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
