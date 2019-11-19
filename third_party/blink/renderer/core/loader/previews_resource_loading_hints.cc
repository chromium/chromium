// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/previews_resource_loading_hints.h"

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

String GetConsoleLogStringForBlockedLoad(const KURL& url) {
  return "[Intervention] Non-critical resource " + url.GetString() +
         " is blocked due to page load being slow. Learn more at "
         "https://www.chromestatus.com/feature/4510564810227712.";
}

}  // namespace

// static
PreviewsResourceLoadingHints* PreviewsResourceLoadingHints::Create(
    ExecutionContext& execution_context,
    int64_t ukm_source_id,
    const WebVector<WebString>& subresource_patterns_to_block) {
  return MakeGarbageCollected<PreviewsResourceLoadingHints>(
      &execution_context, ukm_source_id, subresource_patterns_to_block);
}

// static
PreviewsResourceLoadingHints*
PreviewsResourceLoadingHints::CreateFromLoadingHintsProvider(
    ExecutionContext& execution_context,
    std::unique_ptr<WebLoadingHintsProvider> loading_hints_provider) {
  WebVector<WebString> subresource_patterns_to_block;
  for (const auto& pattern :
       loading_hints_provider->subresource_patterns_to_block) {
    // |pattern| is guaranteed to be ascii.
    subresource_patterns_to_block.emplace_back(pattern);
  }

  return MakeGarbageCollected<PreviewsResourceLoadingHints>(
      &execution_context, loading_hints_provider->ukm_source_id,
      subresource_patterns_to_block);
}

PreviewsResourceLoadingHints::PreviewsResourceLoadingHints(
    ExecutionContext* execution_context,
    int64_t ukm_source_id,
    const WebVector<WebString>& subresource_patterns_to_block)
    : execution_context_(execution_context),
      ukm_source_id_(ukm_source_id),
      subresource_patterns_to_block_(subresource_patterns_to_block) {
  DCHECK_NE(ukm::kInvalidSourceId, ukm_source_id_);

  subresource_patterns_to_block_usage_.Fill(
      false,
      static_cast<WTF::wtf_size_t>(subresource_patterns_to_block.size()));
  blocked_resource_load_priority_counts_.Fill(
      0, static_cast<int>(ResourceLoadPriority::kHighest) + 1);

  // Populate which specific resource types are eligible for blocking.
  // Certain resource types are blocked by default since their blocking
  // is currently verified by the server verification pipeline. Note that
  // the blocking of these resource types can be overridden using field trial.
  block_resource_type_[static_cast<int>(ResourceType::kCSSStyleSheet)] = true;
  block_resource_type_[static_cast<int>(ResourceType::kScript)] = true;
  block_resource_type_[static_cast<int>(ResourceType::kRaw)] = true;
  for (int i = 0; i < static_cast<int>(ResourceType::kMaxValue) + 1; ++i) {
    // Parameter names are of format: "block_resource_type_%d". The value
    // should be either "true" or "false".
    block_resource_type_[i] = base::GetFieldTrialParamByFeatureAsBool(
        features::kPreviewsResourceLoadingHintsSpecificResourceTypes,
        String::Format("block_resource_type_%d", i).Ascii(),
        block_resource_type_[i]);
  }

  // Ensure that the ResourceType enums have not changed. These should not be
  // changed since the resource type integer values are used as field trial
  // params.
  static_assert(static_cast<int>(ResourceType::kImage) == 1 &&
                    static_cast<int>(ResourceType::kCSSStyleSheet) == 2 &&
                    static_cast<int>(ResourceType::kScript) == 3,
                "ResourceType enums can't be changed");
}

PreviewsResourceLoadingHints::~PreviewsResourceLoadingHints() = default;

bool PreviewsResourceLoadingHints::AllowLoad(
    ResourceType type,
    const KURL& resource_url,
    ResourceLoadPriority resource_load_priority) const {
  if (!resource_url.ProtocolIsInHTTPFamily())
    return true;

  if (!block_resource_type_[static_cast<int>(type)])
    return true;

  WTF::String resource_url_string = resource_url.GetString();
  resource_url_string = resource_url_string.Left(resource_url.PathEnd());
  bool allow_load = true;

  int pattern_index = 0;
  for (const WTF::String& subresource_pattern :
       subresource_patterns_to_block_) {
    // TODO(tbansal): https://crbug.com/856247. Add support for wildcard
    // matching.
    if (resource_url_string.Find(subresource_pattern) != kNotFound) {
      allow_load = false;
      subresource_patterns_to_block_usage_[pattern_index] = true;
      blocked_resource_load_priority_counts_[static_cast<int>(
          resource_load_priority)]++;
      break;
    }
    pattern_index++;
  }

  UMA_HISTOGRAM_BOOLEAN("ResourceLoadingHints.ResourceLoadingBlocked",
                        !allow_load);
  if (!allow_load) {
    ReportBlockedLoading(resource_url);
    UMA_HISTOGRAM_ENUMERATION(
        "ResourceLoadingHints.ResourceLoadingBlocked.ResourceLoadPriority."
        "Blocked",
        resource_load_priority,
        static_cast<int>(ResourceLoadPriority::kHighest) + 1);
  } else {
    UMA_HISTOGRAM_ENUMERATION(
        "ResourceLoadingHints.ResourceLoadingBlocked.ResourceLoadPriority."
        "Allowed",
        resource_load_priority,
        static_cast<int>(ResourceLoadPriority::kHighest) + 1);
  }
  return allow_load;
}

void PreviewsResourceLoadingHints::ReportBlockedLoading(
    const KURL& resource_url) const {
  execution_context_->AddConsoleMessage(ConsoleMessage::Create(
      mojom::ConsoleMessageSource::kOther, mojom::ConsoleMessageLevel::kWarning,
      GetConsoleLogStringForBlockedLoad(resource_url)));
}

void PreviewsResourceLoadingHints::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
}

void PreviewsResourceLoadingHints::RecordUKM(
    ukm::UkmRecorder* ukm_recorder) const {
  DCHECK(ukm_recorder);

  size_t patterns_to_block_used_count = 0;
  for (bool pattern_used : subresource_patterns_to_block_usage_) {
    if (pattern_used) {
      patterns_to_block_used_count++;
    }
  }

  ukm::builders::PreviewsResourceLoadingHints(ukm_source_id_)
      .Setpatterns_to_block_total(subresource_patterns_to_block_.size())
      .Setpatterns_to_block_used(patterns_to_block_used_count)
      .Setblocked_very_low_priority(
          blocked_resource_load_priority_counts_[static_cast<int>(
              ResourceLoadPriority::kVeryLow)])
      .Setblocked_low_priority(
          blocked_resource_load_priority_counts_[static_cast<int>(
              ResourceLoadPriority::kLow)])
      .Setblocked_medium_priority(
          blocked_resource_load_priority_counts_[static_cast<int>(
              ResourceLoadPriority::kMedium)])
      .Setblocked_high_priority(
          blocked_resource_load_priority_counts_[static_cast<int>(
              ResourceLoadPriority::kHigh)])
      .Setblocked_very_high_priority(
          blocked_resource_load_priority_counts_[static_cast<int>(
              ResourceLoadPriority::kVeryHigh)])
      .Record(ukm_recorder);
}

}  // namespace blink
