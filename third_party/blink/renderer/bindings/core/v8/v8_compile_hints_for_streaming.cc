// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_for_streaming.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/v8_compile_hints_histograms.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_compile_hints_producer.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_local_compile_hints_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata.h"

namespace blink::v8_compile_hints {
namespace {

static bool LocalCompileHintsEnabled() {
  return base::FeatureList::IsEnabled(features::kLocalCompileHints);
}

}  // namespace

CompileHintsForStreaming::Builder::Builder(
    V8CrowdsourcedCompileHintsProducer* crowdsourced_compile_hints_producer,
    V8CrowdsourcedCompileHintsConsumer* crowdsourced_compile_hints_consumer,
    const KURL& resource_url)
    : might_generate_crowdsourced_compile_hints_(
          crowdsourced_compile_hints_producer &&
          crowdsourced_compile_hints_producer->MightGenerateData()),
      crowdsourced_compile_hint_callback_data_(
          (!might_generate_crowdsourced_compile_hints_ &&
           crowdsourced_compile_hints_consumer &&
           crowdsourced_compile_hints_consumer->HasData())
              ? crowdsourced_compile_hints_consumer->GetDataWithScriptNameHash(
                    ScriptNameHash(resource_url))
              : nullptr) {}

std::unique_ptr<CompileHintsForStreaming>
CompileHintsForStreaming::Builder::Build(
    scoped_refptr<CachedMetadata> cached_metadata) && {
  if (might_generate_crowdsourced_compile_hints_) {
    return std::make_unique<CompileHintsForStreaming>(base::PassKey<Builder>());
  }
  // We can only consume local or crowdsourced compile hints, but
  // not both at the same time. If the page has crowdsourced compile hints,
  // we won't generate local compile hints, so won't ever have them.
  // We'd only have both local and crowdsourced compile hints available in
  // special cases, e.g., if crowdsourced compile hints were temporarily
  // unavailable, we generated local compile hints, and during the next page
  // load we have both available.

  // TODO(40286622): Enable using crowdsourced compile hints and
  // augmenting them with local compile hints. 1) Enable consuming compile hints
  // and at the same time, producing compile hints for functions which were
  // still lazy and 2) enable consuming both kind of compile hints at the same
  // time.
  if (crowdsourced_compile_hint_callback_data_) {
    return std::make_unique<CompileHintsForStreaming>(
        std::move(crowdsourced_compile_hint_callback_data_),
        base::PassKey<Builder>());
  }
  if (LocalCompileHintsEnabled() && cached_metadata) {
    auto local_compile_hints_consumer =
        std::make_unique<v8_compile_hints::V8LocalCompileHintsConsumer>(
            cached_metadata.get());
    if (local_compile_hints_consumer->IsRejected()) {
      base::UmaHistogramEnumeration(kStatusHistogram,
                                    Status::kNoCompileHintsStreaming);
      return nullptr;
    }
    // TODO(40286622): It's not clear what we should do if the resource is
    // not hot but we have compile hints. 1) Consume compile hints and
    // produce new ones (currently not possible in the API) and combine both
    // compile hints. 2) Ignore existing compile hints (we're anyway not
    // creating the code cache yet) and produce new ones.
    return std::make_unique<CompileHintsForStreaming>(
        std::move(local_compile_hints_consumer), base::PassKey<Builder>());
  }
  if (LocalCompileHintsEnabled()) {
    // For producing a local compile hints.
    return std::make_unique<CompileHintsForStreaming>(base::PassKey<Builder>());
  }
  base::UmaHistogramEnumeration(kStatusHistogram,
                                Status::kNoCompileHintsStreaming);
  return nullptr;
}

CompileHintsForStreaming::CompileHintsForStreaming(base::PassKey<Builder>)
    : compile_options_(v8::ScriptCompiler::kProduceCompileHints) {
  base::UmaHistogramEnumeration(kStatusHistogram,
                                Status::kProduceCompileHintsStreaming);
}

CompileHintsForStreaming::CompileHintsForStreaming(
    std::unique_ptr<V8LocalCompileHintsConsumer> local_compile_hints_consumer,
    base::PassKey<Builder>)
    : compile_options_(v8::ScriptCompiler::kConsumeCompileHints),
      local_compile_hints_consumer_(std::move(local_compile_hints_consumer)) {
  base::UmaHistogramEnumeration(kStatusHistogram,
                                Status::kConsumeLocalCompileHintsStreaming);
}

CompileHintsForStreaming::CompileHintsForStreaming(
    std::unique_ptr<V8CrowdsourcedCompileHintsConsumer::DataAndScriptNameHash>
        crowdsourced_compile_hint_callback_data,
    base::PassKey<Builder>)
    : compile_options_(v8::ScriptCompiler::kConsumeCompileHints),
      crowdsourced_compile_hint_callback_data_(
          std::move(crowdsourced_compile_hint_callback_data)) {
  base::UmaHistogramEnumeration(
      kStatusHistogram, Status::kConsumeCrowdsourcedCompileHintsStreaming);
}

v8::CompileHintCallback CompileHintsForStreaming::GetCompileHintCallback()
    const {
  if (local_compile_hints_consumer_) {
    return V8LocalCompileHintsConsumer::GetCompileHint;
  }
  if (crowdsourced_compile_hint_callback_data_) {
    return &V8CrowdsourcedCompileHintsConsumer::CompileHintCallback;
  }
  return nullptr;
}

void* CompileHintsForStreaming::GetCompileHintCallbackData() const {
  if (local_compile_hints_consumer_) {
    return local_compile_hints_consumer_.get();
  }
  if (crowdsourced_compile_hint_callback_data_) {
    return crowdsourced_compile_hint_callback_data_.get();
  }
  return nullptr;
}

V8LocalCompileHintsConsumer*
CompileHintsForStreaming::GetV8LocalCompileHintsConsumerForTest() const {
  return local_compile_hints_consumer_.get();
}

}  // namespace blink::v8_compile_hints
