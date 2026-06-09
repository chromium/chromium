// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_item.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/single_sample_metrics.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "ui/base/clipboard/clipboard_constants.h"

namespace blink {

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

ClipboardItem::ClipboardItem(const HeapVector<String>& mime_types,
                             std::optional<absl::uint128> sequence_number,
                             ExecutionContext* execution_context,
                             bool sanitize_html_for_lazy_read,
                             AccessMode access_mode)
    : ExecutionContextLifecycleObserver(execution_context),
      sequence_number_(sequence_number),
      access_mode_(access_mode),
      sanitize_html_for_lazy_read_(sanitize_html_for_lazy_read) {
  CHECK(
      RuntimeEnabledFeatures::ReadClipboardDataOnClipboardItemGetTypeEnabled());
  for (const auto& mime_type : mime_types) {
    String web_custom_format = Clipboard::ParseWebCustomFormat(mime_type);
    if (web_custom_format.empty()) {
      mime_types_.emplace_back(mime_type);
    } else {
      String web_custom_format_string =
          StrCat({ui::kWebClipboardFormatPrefix, web_custom_format});
      mime_types_.emplace_back(web_custom_format_string);
      custom_format_types_.push_back(web_custom_format_string);
    }
  }
  if (access_mode_ == AccessMode::kLazy) {
    formats_never_read_metric_ =
        base::SingleSampleMetricsFactory::Get()->CreateCustomCountsMetric(
            "Blink.Clipboard.LazyRead.FormatsNeverRead", 1, 100, 50);
    formats_never_read_metric_->SetSample(
        static_cast<int>(mime_types_.size()));
    total_blob_size_kb_metric_ =
        base::SingleSampleMetricsFactory::Get()->CreateCustomCountsMetric(
            "Blink.Clipboard.LazyRead.TotalBlobSizeKB", 1, 10000, 50);
    total_blob_size_kb_metric_->SetSample(0);
  }
}

ClipboardItem::ClipboardItem(
    const HeapVector<
        std::pair<String, MemberScriptPromise<V8UnionBlobOrString>>>&
        representations,
    std::optional<absl::uint128> sequence_number)
    : ExecutionContextLifecycleObserver(nullptr),
      sequence_number_(sequence_number) {
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
          StrCat({ui::kWebClipboardFormatPrefix, web_custom_format});
      representations_.emplace_back(web_custom_format_string,
                                    representation.second);
      custom_format_types_.push_back(web_custom_format_string);
    }
  }
}

Vector<String> ClipboardItem::types() const {
  Vector<String> types;
  if (access_mode_ == AccessMode::kLazy) {
    CHECK(RuntimeEnabledFeatures::
              ReadClipboardDataOnClipboardItemGetTypeEnabled());
    types.ReserveInitialCapacity(mime_types_.size());
    for (const auto& item : mime_types_) {
      types.push_back(item);
    }

  } else {
    types.ReserveInitialCapacity(representations_.size());
    for (const auto& item : representations_) {
      types.push_back(item.first);
    }
  }
  return types;
}

void ClipboardItem::ResolveFormatData(const String& mime_type, Blob* blob) {
  CHECK(
      RuntimeEnabledFeatures::ReadClipboardDataOnClipboardItemGetTypeEnabled());

  if (representations_with_resolvers_.find(mime_type) ==
      representations_with_resolvers_.end()) {
    return;
  }
  const bool clipboard_changed = HasClipboardChangedSinceClipboardRead();
  base::UmaHistogramBoolean("Blink.Clipboard.LazyRead.GetTypeRejected",
                            clipboard_changed);
  if (clipboard_changed) {
    representations_with_resolvers_.at(mime_type)->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Clipboard data has changed"));
    return;
  }

  base::UmaHistogramBoolean("Blink.Clipboard.LazyRead.NullBlobResolved", !blob);
  if (!blob) {
    representations_with_resolvers_.at(mime_type)->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kInvalidStateError,
                                           "Failed to read clipboard data."));
    return;
  }

  total_lazy_read_blob_size_ += blob->size();
  representations_with_resolvers_.at(mime_type)->Resolve(blob);

  int formats_never_requested =
      static_cast<int>(mime_types_.size()) -
      static_cast<int>(representations_with_resolvers_.size());
  if (formats_never_read_metric_ && formats_never_requested >= 0) {
    formats_never_read_metric_->SetSample(formats_never_requested);
  }
  if (total_blob_size_kb_metric_) {
    total_blob_size_kb_metric_->SetSample(
        static_cast<int>(total_lazy_read_blob_size_ / 1024));
  }
}

ScriptPromise<Blob> ClipboardItem::getType(ScriptState* script_state,
                                           const String& type,
                                           ExceptionState& exception_state) {
  if (access_mode_ != AccessMode::kLazy) {
    for (const auto& item : representations_) {
      if (type == item.first) {
        return item.second.Unwrap().Then(
            script_state,
            MakeGarbageCollected<UnionToBlobResolverFunction>(type));
      }
    }
    // For non-lazy ClipboardItems, if type wasn't found above, reject.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                      "The type was not found");
    return ScriptPromise<Blob>();
  }

  CHECK(
      RuntimeEnabledFeatures::ReadClipboardDataOnClipboardItemGetTypeEnabled());

  if (!GetExecutionContext()) {
    return ScriptPromise<Blob>();
  }

  const bool clipboard_changed = HasClipboardChangedSinceClipboardRead();
  base::UmaHistogramBoolean("Blink.Clipboard.LazyRead.GetTypeRejected",
                            clipboard_changed);
  if (clipboard_changed) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Clipboard data has changed");
    return ScriptPromise<Blob>();
  }

  const bool has_cached_resolver = representations_with_resolvers_.find(type) !=
                                   representations_with_resolvers_.end();
  const bool supported_lazy_type = mime_types_.Contains(type);

  // Return cached promise if we've already started reading this type. This
  // ensures multiple getType() calls for the same type share the same promise.
  if (has_cached_resolver) {
    return representations_with_resolvers_.at(type)->Promise();
  }

  if (supported_lazy_type) {
    // Create the promise resolver first, then store it
    auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<Blob>>(
        script_state, exception_state.GetContext());
    representations_with_resolvers_.insert(type, resolver);
    ReadRepresentationFromClipboardReader(type);
    return representations_with_resolvers_.at(type)->Promise();
  }

  // If we get here, the type was not found
  exception_state.ThrowDOMException(DOMExceptionCode::kNotFoundError,
                                    "The type was not found");
  return ScriptPromise<Blob>();
}

void ClipboardItem::ReadRepresentationFromClipboardReader(
    const String& format) {
  CHECK(
      RuntimeEnabledFeatures::ReadClipboardDataOnClipboardItemGetTypeEnabled());
  SystemClipboard* system_clipboard = GetSystemClipboard();
  if (!system_clipboard) {
    ResolveFormatData(format, nullptr);
    return;
  }
  ClipboardReader* clipboard_reader = ClipboardReader::Create(
      system_clipboard, format, this, sanitize_html_for_lazy_read_);
  if (!clipboard_reader) {
    ResolveFormatData(format, nullptr);
    return;
  }
  clipboard_reader->Read();
}

bool ClipboardItem::HasClipboardChangedSinceClipboardRead() {
  // If sequence_number_ was never initialized, we can't verify if clipboard
  // changed, so conservatively return true.
  if (!sequence_number_.has_value()) {
    return true;
  }

  SystemClipboard* system_clipboard = GetSystemClipboard();
  if (!system_clipboard) {
    return true;
  }

  return system_clipboard->SequenceNumber() != sequence_number_.value();
}

LocalFrame* ClipboardItem::GetLocalFrame() const {
  LocalDOMWindow* window = DynamicTo<LocalDOMWindow>(GetExecutionContext());
  if (!window) {
    return nullptr;
  }
  return window->GetFrame();
}

SystemClipboard* ClipboardItem::GetSystemClipboard() const {
  LocalFrame* frame = GetLocalFrame();
  if (!frame) {
    return nullptr;
  }
  return frame->GetSystemClipboard();
}

void ClipboardItem::OnRead(Blob* blob, const String& mime_type) {
  ResolveFormatData(mime_type, blob);
}

void ClipboardItem::ContextDestroyed() {
  // Flush SingleSampleMetrics to emit their final values now rather than
  // waiting for GC. The values were pre-computed via SetSample() during
  // ResolveFormatData(), so no GC-managed state is accessed here.
  formats_never_read_metric_.reset();
  total_blob_size_kb_metric_.reset();

  DOMException* detached_error = MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNotAllowedError, "Document detached.");
  for (auto& entry : representations_with_resolvers_) {
    entry.value->Reject(detached_error);
  }
  representations_with_resolvers_.clear();
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
  visitor->Trace(representations_with_resolvers_);
  ClipboardReaderResultHandler::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
