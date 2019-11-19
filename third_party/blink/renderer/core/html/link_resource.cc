/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/link_resource.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_link_element.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

LinkResource::LinkResource(HTMLLinkElement* owner) : owner_(owner) {
  DCHECK(owner_);
}

LinkResource::~LinkResource() = default;

bool LinkResource::ShouldLoadResource() const {
  return GetDocument().GetFrame() || GetDocument().ImportsController();
}

LocalFrame* LinkResource::LoadingFrame() const {
  return owner_->GetDocument().MasterDocument().GetFrame();
}

Document& LinkResource::GetDocument() {
  return owner_->GetDocument();
}

const Document& LinkResource::GetDocument() const {
  return owner_->GetDocument();
}

WTF::TextEncoding LinkResource::GetCharset() const {
  AtomicString charset = owner_->FastGetAttribute(html_names::kCharsetAttr);
  if (charset.IsEmpty() && GetDocument().GetFrame())
    return GetDocument().Encoding();
  return WTF::TextEncoding(charset);
}

void LinkResource::Trace(Visitor* visitor) {
  visitor->Trace(owner_);
}

}  // namespace blink
