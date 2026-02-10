// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_credential_element.h"

#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

HTMLCredentialElement::HTMLCredentialElement(Document& document)
    : HTMLElement(html_names::kCredentialTag, document) {}

}  // namespace blink
