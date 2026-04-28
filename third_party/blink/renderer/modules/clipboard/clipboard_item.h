// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_ITEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_ITEM_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "base/metrics/single_sample_metrics.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_blob_string.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/clipboard/clipboard_reader.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"

namespace blink {

class Blob;
class ScriptState;
class ExecutionContext;
template <typename IDLType>
class ScriptPromiseResolver;

// A `ClipboardItem` holds data that was read from or will be written to the
// system clipboard. Spec:
// https://w3c.github.io/clipboard-apis/#clipboard-item-interface
class MODULES_EXPORT ClipboardItem final
    : public ScriptWrappable,
      public ExecutionContextLifecycleObserver,
      public ClipboardReaderResultHandler {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // `kEager`: created by web author for clipboard write.
  // `kLazy`: created by `clipboard.read()` for lazy read.
  enum class AccessMode {
    kEager,
    kLazy,
  };

  // Creates a `ClipboardItem` containing `representations`.
  // If `representations` is empty, writes error info to `exception_state` and
  // returns nullptr.
  static ClipboardItem* Create(
      const HeapVector<
          std::pair<String, MemberScriptPromise<V8UnionBlobOrString>>>&
          representations,
      ExceptionState& exception_state);

  // Constructs a `ClipboardItem` instance from `representations`.
  // Parses the web custom MIME types and stores them in `custom_format_types_`
  // and `representations_`.
  // If an empty `ClipboardItem` is a valid use-case, use the constructor
  // directly, else use `Create` method.
  explicit ClipboardItem(
      const HeapVector<
          std::pair<String, MemberScriptPromise<V8UnionBlobOrString>>>&
          representations,
      std::optional<absl::uint128> sequence_number = std::nullopt);
  explicit ClipboardItem(const HeapVector<String>& mime_types,
                         std::optional<absl::uint128> sequence_number,
                         ExecutionContext* execution_context,
                         bool sanitize_html_for_lazy_read,
                         AccessMode access_mode);

  // Returns the MIME types contained in the `ClipboardItem`.
  // Spec: https://w3c.github.io/clipboard-apis/#dom-clipboarditem-types
  Vector<String> types() const;

  // Retrieves the data of a specific `type` from the `ClipboardItem` and
  // returns a promise resolved to that data.
  // `script_state`: The script state in which the promise will be resolved.
  // `type`: The MIME type or a custom MIME type with a "web " prefix of data to
  // retrieve. `exception_state`: The exception state to be updated if an error
  // occurs. Spec:
  // https://w3c.github.io/clipboard-apis/#dom-clipboarditem-gettype
  ScriptPromise<Blob> getType(ScriptState* script_state,
                              const String& type,
                              ExceptionState& exception_state);

  // Checks if a particular MIME type is supported by the Async Clipboard API.
  // `type` refers to a MIME type or a custom MIME type with a "web " prefix.
  // Spec: https://w3c.github.io/clipboard-apis/#dom-clipboarditem-supports
  static bool supports(const String& type);

  const HeapVector<std::pair<String, MemberScriptPromise<V8UnionBlobOrString>>>&
  GetRepresentations() const {
    return representations_;
  }

  // Returns the custom formats that have a "web " prefix.
  const Vector<String>& CustomFormats() const { return custom_format_types_; }
  // ScriptWrappable
  void Trace(Visitor*) const override;

  void OnRead(Blob* blob, const String& mime_type) override;
  ExecutionContext* GetExecutionContext() const override {
    return ExecutionContextLifecycleObserver::GetExecutionContext();
  }
  LocalFrame* GetLocalFrame() const override;
  SystemClipboard* GetSystemClipboard() const;

 private:
  // ExecutionContextLifecycleObserver
  void ContextDestroyed() override;

  void ReadRepresentationFromClipboardReader(const String& format);
  void ResolveFormatData(const String& mime_type, Blob* blob);
  bool HasClipboardChangedSinceClipboardRead();
  // Map of MIME type to `ScriptPromiseResolver` for the promise returned by
  // `getType()`.
  HeapHashMap<String, Member<ScriptPromiseResolver<Blob>>>
      representations_with_resolvers_;
  // Stores built-in and web custom MIME types.
  HeapVector<std::pair<String, MemberScriptPromise<V8UnionBlobOrString>>>
      representations_;
  // Stores the MIME types available for lazy-read `ClipboardItem`s.
  Vector<String> mime_types_;
  // The vector of custom MIME types that have a "web " prefix.
  Vector<String> custom_format_types_;
  // Use std::optional to distinguish "not yet set" from any valid sequence
  // number (0 is a valid clipboard sequence number).
  std::optional<absl::uint128> sequence_number_;
  // Whether data is fetched from the clipboard on demand via `getType()`.
  AccessMode access_mode_ = AccessMode::kEager;
  // Whether HTML data should be sanitized when reading lazily.
  bool sanitize_html_for_lazy_read_ = true;
  // Cumulative size of blobs resolved via getType() for single read call.
  uint64_t total_lazy_read_blob_size_ = 0;
  // SingleSampleMetrics for lazy-read histograms. These accumulate values via
  // SetSample() during blob resolution and emit a single sample at destruction,
  // avoiding the need to record in ContextDestroyed() or pre-finalizers.
  std::unique_ptr<base::SingleSampleMetric> formats_never_read_metric_;
  std::unique_ptr<base::SingleSampleMetric> total_blob_size_kb_metric_;
  // Tracks the last `getType()` call time per MIME type for telemetry.
  HashMap<String, base::TimeTicks> last_get_type_calls_;
  // The time this `ClipboardItem` was created, used for telemetry.
  base::TimeTicks creation_time_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_ITEM_H_
