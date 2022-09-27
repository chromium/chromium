// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_item.h"

#include "net/base/mime_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace blink {

// static
ClipboardItem* ClipboardItem::Create(
    const HeapVector<std::pair<String, ScriptPromise>>& items,
    ExceptionState& exception_state) {
  // Check that incoming dictionary isn't empty. If it is, it's possible that
  // Javascript bindings implicitly converted an Object (like a ScriptPromise)
  // into {}, an empty dictionary.
  if (!items.size()) {
    exception_state.ThrowTypeError("Empty dictionary argument");
    return nullptr;
  }
  return MakeGarbageCollected<ClipboardItem>(items);
}

ClipboardItem::ClipboardItem(
    const HeapVector<std::pair<String, ScriptPromise>>& items) {
  DCHECK(items.size());
  for (const auto& item : items) {
    String web_custom_format = Clipboard::ParseWebCustomFormat(item.first);
    if (!web_custom_format.empty()) {
      // Types with "web " prefix are special, so we do some level of MIME type
      // parsing here to get a valid web custom format type.
      // We want to ensure that the string after removing the "web " prefix is
      // a valid MIME type.
      // e.g. "web text/html" is a web custom MIME type & "text/html" is a
      // well-known MIME type. Removing the "web " prefix makes it hard to
      // differentiate between the two.
      std::string web_top_level_mime_type;
      std::string web_mime_sub_type;
      if (net::ParseMimeTypeWithoutParameter(web_custom_format.Utf8(),
                                             &web_top_level_mime_type,
                                             &web_mime_sub_type)) {
        String web_custom_format_string = String::Format(
            "%s%s/%s", ui::kWebClipboardFormatPrefix,
            web_top_level_mime_type.c_str(), web_mime_sub_type.c_str());
        items_.emplace_back(web_custom_format_string, item.second);
        custom_format_items_.push_back(web_custom_format_string);
        continue;
      }
    }
    // Any arbitrary type can be added to ClipboardItem, but there may not be
    // any read/write support for that type.
    items_.push_back(item);
  }
}

Vector<String> ClipboardItem::types() const {
  Vector<String> types;
  types.ReserveInitialCapacity(items_.size());
  for (const auto& item : items_) {
    types.push_back(item.first);
  }
  return types;
}

ScriptPromise ClipboardItem::getType(ScriptState* script_state,
                                     const String& type,
                                     ExceptionState& exception_state) const {
  for (const auto& item : items_) {
    if (type == item.first)
      return item.second;
  }

  exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                    "The type was not found");
  return ScriptPromise();
}

void ClipboardItem::Trace(Visitor* visitor) const {
  visitor->Trace(items_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
