// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"

namespace blink {

// Construction of WrapperTypeInfo may require non-trivial initialization due
// to cross-component address resolution in order to load the pointer to the
// parent interface's WrapperTypeInfo.  We ignore this issue because the issue
// happens only on component builds and the official release builds
// (statically-linked builds) are never affected by this issue.
#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

const WrapperTypeInfo DOMArrayBufferView::wrapper_type_info_body_{
    gin::kEmbedderBlink,
    nullptr,
    nullptr,
    "ArrayBufferView",
    nullptr,
    kDOMWrappersTag,
    kDOMWrappersTag,
    WrapperTypeInfo::kWrapperTypeObjectPrototype,
    WrapperTypeInfo::kObjectClassId,
    WrapperTypeInfo::kNotInheritFromActiveScriptWrappable,
    WrapperTypeInfo::kIdlBufferSourceType,
};

const WrapperTypeInfo& DOMArrayBufferView::wrapper_type_info_ =
    DOMArrayBufferView::wrapper_type_info_body_;

#if defined(COMPONENT_BUILD) && defined(WIN32) && defined(__clang__)
#pragma clang diagnostic pop
#endif

}  // namespace blink
