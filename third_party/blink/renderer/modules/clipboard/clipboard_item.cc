// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_item.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace blink {

// static
ClipboardItem* ClipboardItem::Create(
    const HeapVector<std::pair<String, ScriptPromise<Blob>>>& representations,
    ExceptionState& exception_state) {
  // Check that incoming dictionary isn't empty. If it is, it's possible that
  // Javascript bindings implicitly converted an Object (like a
  // ScriptPromise<Blob>) into {}, an empty dictionary.
  if (!representations.size()) {
    exception_state.ThrowTypeError("Empty dictionary argument");
    return nullptr;
  }
  return MakeGarbageCollected<ClipboardItem>(representations);
}

ClipboardItem::ClipboardItem(
    const HeapVector<std::pair<String, ScriptPromise<Blob>>>& representations) {
  for (const auto& representation : representations) {
    String web_custom_format =
        Clipboard::ParseWebCustomFormat(representation.first);
    if (web_custom_format.empty()) {
      // Any arbitrary type can be added to ClipboardItem, but there may not be
      // any read/write support for that type.
      // TODO(caseq,japhet): we can't pass typed promises from bindings yet, but
      // when we can, the type cast below should go away.
      representations_.emplace_back(
          representation.first,
          static_cast<const ScriptPromise<Blob>&>(representation.second));
    } else {
      // Types with "web " prefix are special, so we do some level of MIME type
      // parsing here to get a valid web custom format type.
      // We want to ensure that the string after removing the "web " prefix is
      // a valid MIME type.
      // e.g. "web text/html" is a web custom MIME type & "text/html" is a
      // well-known MIME type. Removing the "web " prefix makes it hard to
      // differentiate between the two.
      // TODO(caseq,japhet): we can't pass typed promises from bindings yet, but
      // when we can, the type cast below should go away.
      String web_custom_format_string =
          String::Format("%s%s", ui::kWebClipboardFormatPrefix,
                         web_custom_format.Utf8().c_str());
      representations_.emplace_back(
          web_custom_format_string,
          static_cast<const ScriptPromise<Blob>&>(representation.second));
      custom_format_types_.push_back(web_custom_format_string);
    }
  }
}

Vector<String> ClipboardItem::types() const {
  Vector<String> types;
  types.ReserveInitialCapacity(representations_.size());
  for (const auto& item : representations_) {
    types.push_back(item.first);
  }
  return types;
}

ScriptPromise<Blob> ClipboardItem::getType(
    ScriptState* script_state,
    const String& type,
    ExceptionState& exception_state) const {
  for (const auto& item : representations_) {
    if (type == item.first)
      return item.second;
  }

  exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                    "The type was not found");
  return ScriptPromise<Blob>();
}

// static
bool ClipboardItem::supports(const String& type) {
  if (type.length() >= mojom::blink::ClipboardHost::kMaxFormatSize) {
    return false;
  }

  if (!Clipboard::ParseWebCustomFormat(type).empty()) {
    return true;
  }

  // TODO(https://crbug.com/1029857): Add support for other types.
  return type == kMimeTypeImagePng || type == kMimeTypeTextPlain ||
         type == kMimeTypeTextHTML || type == kMimeTypeImageSvg;
}

void ClipboardItem::Trace(Visitor* visitor) const {
  visitor->Trace(representations_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
