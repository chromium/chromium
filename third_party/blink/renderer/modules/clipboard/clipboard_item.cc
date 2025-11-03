// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_item.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace blink {

// The time threshold to consider an operation as "delayed" for UseCounter
// purposes.
constexpr base::TimeDelta kClipboardOperation5SecDelay = base::Seconds(5);
constexpr base::TimeDelta kClipboardOperation1MinDelay = base::Minutes(1);
constexpr base::TimeDelta kClipboardOperation10MinDelay = base::Minutes(10);

class UnionToBlobResolverFunction final
    : public ThenCallable<V8UnionBlobOrString,
                          UnionToBlobResolverFunction,
                          Blob> {
 public:
  explicit UnionToBlobResolverFunction(const String& mime_type)
      : mime_type_(mime_type) {}

  Blob* React(ScriptState* script_state, V8UnionBlobOrString* union_value) {
    if (union_value->IsBlob()) {
      return union_value->GetAsBlob();
    } else if (union_value->IsString()) {
      // ClipboardItem::getType() returns a Blob, so we need to convert the
      // string to a Blob here.
      StringUtf8Adaptor utf8_text(union_value->GetAsString());
      return Blob::Create(base::as_byte_span(utf8_text), mime_type_);
    }
    return nullptr;
  }

 private:
  String mime_type_;
};

// static
ClipboardItem* ClipboardItem::Create(
    const HeapVector<
        std::pair<String, MemberScriptPromise<V8UnionBlobOrString>>>&
        representations,
    ExceptionState& exception_state) {
  // Check that incoming dictionary isn't empty. If it is, it's possible that
  // Javascript bindings implicitly converted an Object (like a
  // ScriptPromise<V8UnionBlobOrString>) into {}, an empty dictionary.
  if (!representations.size()) {
    exception_state.ThrowTypeError("Empty dictionary argument");
    return nullptr;
  }
  return MakeGarbageCollected<ClipboardItem>(representations);
}

ClipboardItem::ClipboardItem(
    const HeapVector<
        std::pair<String, MemberScriptPromise<V8UnionBlobOrString>>>&
        representations,
    absl::uint128 sequence_number)
    : sequence_number_(sequence_number),
      creation_time_(base::TimeTicks::Now()) {
  for (const auto& representation : representations) {
    String web_custom_format =
        Clipboard::ParseWebCustomFormat(representation.first);
    if (web_custom_format.empty()) {
      // Any arbitrary type can be added to ClipboardItem, but there may not be
      // any read/write support for that type.
      // TODO(caseq,japhet): we can't pass typed promises from bindings yet, but
      // when we can, the type cast below should go away.
      representations_.emplace_back(representation.first,
                                    representation.second);
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
      representations_.emplace_back(web_custom_format_string,
                                    representation.second);
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

ScriptPromise<Blob> ClipboardItem::getType(ScriptState* script_state,
                                           const String& type,
                                           ExceptionState& exception_state) {
  for (const auto& item : representations_) {
    if (type == item.first) {
      if (RuntimeEnabledFeatures::ClipboardItemGetTypeCounterEnabled()) {
        CaptureTelemetry(ExecutionContext::From(script_state), type);
      }

      return item.second.Unwrap().Then(
          script_state,
          MakeGarbageCollected<UnionToBlobResolverFunction>(type));
    }
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
  return type == ui::kMimeTypePng || type == ui::kMimeTypePlainText ||
         type == ui::kMimeTypeHtml || type == ui::kMimeTypeSvg;
}

void ClipboardItem::Trace(Visitor* visitor) const {
  visitor->Trace(representations_);
  ScriptWrappable::Trace(visitor);
}

void ClipboardItem::CaptureTelemetry(ExecutionContext* context,
                                     const String& type) {
  if (!context) {
    return;
  }
  LocalDOMWindow& window = *To<LocalDOMWindow>(context);
  SystemClipboard* system_clipboard =
      window.GetFrame() ? window.GetFrame()->GetSystemClipboard() : nullptr;
  if (system_clipboard) {
    absl::uint128 seqno = system_clipboard->SequenceNumber();
    if (seqno != sequence_number_) {
      // Case 1: Clipboard changed between read() and getType()
      UseCounter::Count(context,
                        WebFeature::kClipboardChangedBetweenReadAndGetType);

      // Case 2: Clipboard changed between two getType() calls
      if (!last_get_type_calls_.empty()) {
        UseCounter::Count(context,
                          WebFeature::kClipboardChangedBetweenGetTypes);
      }
    }
  }
  // Case 3: Time difference between read() and getType() calls is more
  // than threshold
  const base::TimeTicks current_time = base::TimeTicks::Now();
  const base::TimeDelta time_diff = current_time - creation_time_;
  if (time_diff >= kClipboardOperation5SecDelay &&
      time_diff < kClipboardOperation1MinDelay) {
    UseCounter::Count(
        context,
        WebFeature::kClipboardReadAndGetTypeTimeDiffIsBetween5SecAnd1Min);
  } else if (time_diff >= kClipboardOperation1MinDelay &&
             time_diff < kClipboardOperation10MinDelay) {
    UseCounter::Count(
        context,
        WebFeature::kClipboardReadAndGetTypeTimeDiffIsBetween1MinAnd10Min);
  } else if (time_diff > kClipboardOperation10MinDelay) {
    UseCounter::Count(
        context, WebFeature::kClipboardReadAndGetTypeTimeDiffIsMoreThan10Min);
  }

  // Case 4: Time difference between two getType() calls for the same
  // types is more than threshold
  auto it = last_get_type_calls_.find(type);
  if (it != last_get_type_calls_.end()) {
    const base::TimeDelta type_time_diff = current_time - it->value;
    if (type_time_diff >= kClipboardOperation5SecDelay &&
        type_time_diff < kClipboardOperation1MinDelay) {
      UseCounter::Count(
          context,
          WebFeature::kClipboardGetTypeTimeDiffOfSameTypeIsBetween5SecAnd1Min);
    } else if (type_time_diff >= kClipboardOperation1MinDelay &&
               type_time_diff < kClipboardOperation10MinDelay) {
      UseCounter::Count(
          context,
          WebFeature::kClipboardGetTypeTimeDiffOfSameTypeIsBetween1MinAnd10Min);
    } else if (type_time_diff > kClipboardOperation10MinDelay) {
      UseCounter::Count(
          context,
          WebFeature::kClipboardGetTypeTimeDiffOfSameTypeIsMoreThan10Min);
    }
  } else {
    // Update the last call time for this type
    last_get_type_calls_.Set(type, current_time);
  }

  if (!window.document()->hasFocus()) {
    UseCounter::Count(context, WebFeature::kClipboardGetTypeWindowNotInFocus);
  }
}

}  // namespace blink
