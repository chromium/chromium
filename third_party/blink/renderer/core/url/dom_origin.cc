// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url/dom_origin.h"

#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// Static `::Create()` methods:
DOMOrigin* DOMOrigin::Create() {
  return MakeGarbageCollected<DOMOrigin>(SecurityOrigin::CreateUniqueOpaque());
}

// static
DOMOrigin* DOMOrigin::parse(const String& value) {
  scoped_refptr<const SecurityOrigin> security_origin =
      SecurityOrigin::CreateFromString(value);

  // SecurityOrigin::CreateFromString will accept a wide variety of inputs, as
  // it routes things through URL parsing before minting an origin out of the
  // result. We'd like to ensure that the web-facing API requires a properly
  // serialized origin, so we check here to verify that the value we provided
  // matches the serialization of the SecurityOrigin we received.
  if (security_origin->ToString() != value) {
    return nullptr;
  }
  return MakeGarbageCollected<DOMOrigin>(std::move(security_origin));
}

// static
DOMOrigin* DOMOrigin::fromURL(const String& serialized_url) {
  KURL url(serialized_url);
  if (!url.IsValid()) {
    return nullptr;
  }
  return MakeGarbageCollected<DOMOrigin>(SecurityOrigin::Create(url));
}

DOMOrigin* DOMOrigin::Create(const String& value,
                             ExceptionState& exception_state) {
  if (DOMOrigin* dom_origin = DOMOrigin::parse(value)) {
    return dom_origin;
  }

  exception_state.ThrowTypeError("Invalid serialized origin");
  return nullptr;
}

// Constructor
DOMOrigin::DOMOrigin(scoped_refptr<const SecurityOrigin> origin)
    : origin_(std::move(origin)) {}

// Destructor
DOMOrigin::~DOMOrigin() = default;


bool DOMOrigin::opaque() const {
  return origin_->IsOpaque();
}

String DOMOrigin::toJSON() const {
  return origin_->ToString();
}

bool DOMOrigin::isSameOrigin(const DOMOrigin* other) const {
  return origin_->IsSameOriginWith(other->origin_.get());
}

bool DOMOrigin::isSameSite(const DOMOrigin* other) const {
  return origin_->IsSameSiteWith(other->origin_.get());
}

void DOMOrigin::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
