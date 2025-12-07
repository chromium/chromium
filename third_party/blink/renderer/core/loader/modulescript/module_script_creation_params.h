// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_CREATION_PARAMS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_CREATION_PARAMS_H_

#include "base/check_op.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8-callbacks.h"

namespace blink {

typedef v8::ModuleImportPhase ModuleImportPhase;

// Spec module types. Return value of
// "https://html.spec.whatwg.org/#module-type-from-module-request".
enum class ModuleType { kInvalid, kJavaScriptOrWasm, kJSON, kCSS };

// Non-standard internal module types. The spec defines javascript-or-wasm
// as the module type for module requests without an explicit type and uses
// the MIME type to disambiguate between JavaScript and Wasm modules.
// We don't pass the MIME type to `ModuleScript`, instead, we resolve the
// kJavascriptOrWasm type to kJavaScript or kWasm before calling the
// ModuleScriptCreationParams constructor.
enum class ResolvedModuleType { kJSON, kCSS, kJavaScript, kWasm };

class CORE_EXPORT ModuleScriptCreationParams {
  DISALLOW_NEW();

 public:
  ModuleScriptCreationParams(
      const KURL& source_url,
      const KURL& base_url,
      ScriptSourceLocationType source_location_type,
      const ResolvedModuleType module_type,
      std::variant<ParkableString, base::HeapArray<uint8_t>>&& source,
      CachedMetadataHandler* cache_handler,
      network::mojom::ReferrerPolicy response_referrer_policy,
      const String& source_map_url,
      ScriptStreamer* script_streamer = nullptr,
      ScriptStreamer::NotStreamingReason not_streaming_reason =
          ScriptStreamer::NotStreamingReason::kStreamingDisabled,
      ModuleImportPhase import_phase = ModuleImportPhase::kEvaluation)
      : source_url_(source_url),
        base_url_(base_url),
        source_location_type_(source_location_type),
        module_type_(module_type),
        source_(std::move(source)),
        cache_handler_(cache_handler),
        response_referrer_policy_(response_referrer_policy),
        source_map_url_(source_map_url),
        script_streamer_(script_streamer),
        not_streaming_reason_(not_streaming_reason),
        import_phase_(import_phase) {
    DCHECK(source_location_type == ScriptSourceLocationType::kExternalFile ||
           source_location_type == ScriptSourceLocationType::kInline);
    // https://html.spec.whatwg.org/multipage/webappapis.html#concept-script-base-url
    if (source_location_type == ScriptSourceLocationType::kExternalFile) {
      DCHECK_EQ(source_url, base_url);
    }
    DCHECK_EQ(
        !script_streamer,
        not_streaming_reason != ScriptStreamer::NotStreamingReason::kInvalid);
  }

  ~ModuleScriptCreationParams() = default;

  // Move-only. The move constructor is also deleted because it's not used and
  // due to const members.
  ModuleScriptCreationParams(const ModuleScriptCreationParams&) = delete;
  ModuleScriptCreationParams& operator=(const ModuleScriptCreationParams&) =
      delete;
  ModuleScriptCreationParams(ModuleScriptCreationParams&&) = default;
  ModuleScriptCreationParams& operator=(ModuleScriptCreationParams&&) = delete;

  ModuleScriptCreationParams IsolatedCopy() const {
    // `script_streamer_` and `cache_handler_` are intentionally cleared since
    // they cannot be passed across threads. This only disables script
    // streaming and caching on worklet top-level scripts, where the
    // ModuleScriptCreationParams is passed across threads.
    return ModuleScriptCreationParams(
        SourceURL(), BaseURL(), source_location_type_, module_type_,
        CopySource(), /*cache_handler=*/nullptr, response_referrer_policy_,
        source_map_url_, /*script_streamer=*/nullptr,
        ScriptStreamer::NotStreamingReason::kStreamingDisabled, import_phase_);
  }

  ResolvedModuleType GetModuleType() const { return module_type_; }
  ModuleImportPhase GetModuleImportPhase() const { return import_phase_; }

  const KURL& SourceURL() const { return source_url_; }
  const KURL& BaseURL() const { return base_url_; }
  const String& SourceMapURL() const { return source_map_url_; }

  const ParkableString& GetSourceText() const {
    CHECK_NE(module_type_, ResolvedModuleType::kWasm);
    return std::get<ParkableString>(source_);
  }

  const base::HeapArray<uint8_t>& GetWasmSource() const {
    CHECK_EQ(module_type_, ResolvedModuleType::kWasm);
    return std::get<base::HeapArray<uint8_t>>(source_);
  }

  ScriptSourceLocationType SourceLocationType() const {
    return source_location_type_;
  }

  ModuleScriptCreationParams CopyWithClearedSourceText() const {
    return ModuleScriptCreationParams(
        source_url_, base_url_, source_location_type_, module_type_,
        ParkableString(), /*cache_handler=*/nullptr, response_referrer_policy_,
        source_map_url_, /*script_streamer=*/nullptr,
        ScriptStreamer::NotStreamingReason::kStreamingDisabled, import_phase_);
  }

  CachedMetadataHandler* CacheHandler() const { return cache_handler_; }

  bool IsSafeToSendToAnotherThread() const {
    return !script_streamer_ && !cache_handler_;
  }

  ScriptStreamer* GetScriptStreamer() const { return script_streamer_; }
  ScriptStreamer::NotStreamingReason NotStreamingReason() const {
    return not_streaming_reason_;
  }

  static String ModuleTypeToString(const ModuleType module_type);

  network::mojom::ReferrerPolicy ResponseReferrerPolicy() const {
    return response_referrer_policy_;
  }

 private:
  std::variant<ParkableString, base::HeapArray<uint8_t>> CopySource() const;

  const KURL source_url_;
  const KURL base_url_;
  const ScriptSourceLocationType source_location_type_;
  const ResolvedModuleType module_type_;

  // For Wasm modules the wire bytes are passed directly to the compiler.
  // Otherwise, decoded text is stored as ParkableString.
  // Cannot be const to support the move constructor.
  // TODO(https://crbug.com/42204365): Wrap this and the module type in a
  // new class.
  std::variant<ParkableString, base::HeapArray<uint8_t>> source_;

  // |cache_handler_| is cleared when crossing thread boundaries.
  Persistent<CachedMetadataHandler> cache_handler_;

  // This is the referrer policy specified in the `Referrer-Policy` header on
  // the response associated with the module script that `this` represents. This
  // will always be `kDefault` if there is no referrer policy sent in the
  // response. Consumers of this policy are responsible for detecting this.
  const network::mojom::ReferrerPolicy response_referrer_policy_;

  // |source_map_url_| as provided by the response header.
  const String source_map_url_;

  // |script_streamer_| is cleared when crossing thread boundaries.
  Persistent<ScriptStreamer> script_streamer_;
  const ScriptStreamer::NotStreamingReason not_streaming_reason_;

  const ModuleImportPhase import_phase_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_MODULESCRIPT_MODULE_SCRIPT_CREATION_PARAMS_H_
