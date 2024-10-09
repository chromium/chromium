// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/background_tracing_helper.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/hash/md5.h"
#include "base/numerics/byte_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_split.h"
#include "base/trace_event/named_trigger.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/performance_mark.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/number_parsing_options.h"
#include "third_party/blink/renderer/platform/wtf/text/string_operators.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "url/url_constants.h"

namespace blink {

namespace {

// Converts `chars` to a 1-8 character hash. If successful the parsed hash is
// returned.
std::optional<uint32_t> ConvertToHashInteger(std::string_view chars) {
  // Fail if the hash string is too long or empty.
  if (chars.size() == 0 || chars.size() > 8) {
    return std::nullopt;
  }
  for (auto c : chars) {
    if (!IsASCIIHexDigit(c)) {
      return std::nullopt;
    }
  }
  return WTF::HexCharactersToUInt(base::as_byte_span(chars),
                                  WTF::NumberParsingOptions(), nullptr);
}

static constexpr char kTriggerPrefix[] = "trigger:";

bool MarkNameIsTrigger(StringView mark_name) {
  return StringView(mark_name, 0, std::size(kTriggerPrefix) - 1) ==
         kTriggerPrefix;
}

std::string GenerateFullTrigger(std::string_view site,
                                std::string_view mark_name) {
  return base::StrCat({site, "-", mark_name});
}

BackgroundTracingHelper::SiteHashSet MakeSiteHashSet() {
  // Do nothing if the feature is not enabled.
  if (!base::FeatureList::IsEnabled(
          features::kBackgroundTracingPerformanceMark)) {
    return {};
  }
  // Get the allow-list from the Finch configuration.
  std::string allow_list =
      features::kBackgroundTracingPerformanceMark_AllowList.Get();

  // Parse the allow-list. Silently ignoring malformed configuration data simply
  // means the feature will be disabled when this occurs.
  return BackgroundTracingHelper::ParsePerformanceMarkSiteHashes(allow_list);
}

}  // namespace

BackgroundTracingHelper::BackgroundTracingHelper(ExecutionContext* context) {
  // Used to configure a per-origin allowlist of performance.mark events that
  // are permitted to be included in background traces. See crbug.com/1181774.

  // If there's no allow-list, then bail early.
  if (GetSiteHashSet().empty()) {
    return;
  }

  // Only support http and https origins to actual remote servers.
  auto* origin = context->GetSecurityOrigin();
  if (origin->IsLocal() || origin->IsOpaque() || origin->IsLocalhost())
    return;
  if (CommonSchemeRegistry::IsExtensionScheme(origin->Protocol().Ascii()) &&
      origin->Protocol() != url::kHttpScheme &&
      origin->Protocol() != url::kHttpsScheme) {
    return;
  }

  // Get the hash of the domain in an encoded format (friendly for converting to
  // ASCII, and matching the format in which URLs will be encoded prior to
  // hashing in the Finch list).
  String this_site = EncodeWithURLEscapeSequences(origin->Domain());
  std::string this_site_ascii = this_site.Ascii();
  uint32_t this_site_hash = MD5Hash32(this_site_ascii);

  // We only need the site information if it's allowed by the allow list.
  if (base::Contains(GetSiteHashSet(), this_site_hash)) {
    site_ = this_site_ascii;
    site_hash_ = this_site_hash;
  }

  // Extract a unique ID for the ExecutionContext, using the UnguessableToken
  // associated with it. This squishes the 128 bits of token down into a 32-bit
  // ID.
  auto token = context->GetExecutionContextToken();
  uint64_t merged = token.value().GetHighForSerialization() ^
                    token.value().GetLowForSerialization();
  execution_context_id_ = static_cast<uint32_t>(merged & 0xffffffff) ^
                          static_cast<uint32_t>((merged >> 32) & 0xffffffff);
}

BackgroundTracingHelper::~BackgroundTracingHelper() = default;

void BackgroundTracingHelper::MaybeEmitBackgroundTracingPerformanceMarkEvent(
    const PerformanceMark& mark) {
  if (site_.empty()) {
    return;
  }

  // Parse the mark and the numerical suffix, if any.
  if (!MarkNameIsTrigger(mark.name())) {
    return;
  }
  auto mark_and_id = SplitMarkNameAndId(mark.name());
  std::string mark_name = mark_and_id.first.ToString().Ascii();
  uint32_t mark_hash = MD5Hash32(mark_name);

  // Emit the trace events. We emit hashes and strings to facilitate local trace
  // consumption. However, the strings will be stripped and only the hashes
  // shipped externally.

  auto event_lambda = [&](perfetto::EventContext ctx) {
    auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
    auto* data = event->set_chrome_hashed_performance_mark();
    data->set_site_hash(site_hash_);
    data->set_site(site_);
    data->set_mark_hash(mark_hash);
    data->set_mark(mark_name);
    data->set_execution_context_id(execution_context_id_);
    if (mark_and_id.second.has_value()) {
      data->set_sequence_number(*mark_and_id.second);
    }
  };

  // For additional context, also emit a paired event marking *when* the
  // performance.mark was actually created.
  TRACE_EVENT_INSTANT("blink", "performance.mark.created", event_lambda);

  // Emit an event with the actual timestamp associated with the mark.
  TRACE_EVENT_INSTANT("blink", "performance.mark", mark.UnsafeTimeForTraces(),
                      event_lambda);

  base::trace_event::EmitNamedTrigger(GenerateFullTrigger(site_, mark_name),
                                      mark_and_id.second);
}

void BackgroundTracingHelper::Trace(Visitor*) const {}

// static
const BackgroundTracingHelper::SiteHashSet&
BackgroundTracingHelper::GetSiteHashSet() {
  // This needs to be thread-safe because performance.mark is supported by both
  // windows and workers.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(SiteHashSet, site_hash_set_,
                                  (MakeSiteHashSet()));
  return site_hash_set_;
}

// static
size_t BackgroundTracingHelper::GetIdSuffixPos(StringView string) {
  // Extract any trailing integers.
  size_t cursor = string.length();
  while (cursor > 0) {
    char c = string[cursor - 1];
    if (c < '0' || c > '9')
      break;
    --cursor;
  }

  // A valid suffix must have 1 or more integers.
  if (cursor == string.length()) {
    return 0;
  }

  // A valid suffix must be preceded by an underscore and at least one prefix
  // character.
  if (cursor < 2)
    return 0;

  // A valid suffix must be preceded by an underscore.
  if (string[cursor - 1] != '_')
    return 0;

  // Return the location of the underscore.
  return cursor - 1;
}

std::pair<StringView, std::optional<uint32_t>>
BackgroundTracingHelper::SplitMarkNameAndId(StringView mark_name) {
  DCHECK(MarkNameIsTrigger(mark_name));
  // Extract a sequence number suffix, if it exists.
  mark_name = StringView(mark_name, std::size(kTriggerPrefix) - 1);
  size_t sequence_number_pos = GetIdSuffixPos(mark_name);
  if (sequence_number_pos == 0) {
    return std::make_pair(mark_name, std::nullopt);
  }
  auto suffix = StringView(mark_name, sequence_number_pos + 1);
  mark_name = StringView(mark_name, 0, sequence_number_pos);
  bool result = false;
  int seq_num =
      WTF::CharactersToInt(suffix, WTF::NumberParsingOptions(), &result);
  if (!result) {
    return std::make_pair(mark_name, std::nullopt);
  }
  return std::make_pair(mark_name, seq_num);
}

// static
uint32_t BackgroundTracingHelper::MD5Hash32(std::string_view string) {
  base::MD5Digest digest;
  base::MD5Sum(base::as_byte_span(string), &digest);
  return base::U32FromBigEndian(base::span(digest.a).first<4u>());
}

// static
BackgroundTracingHelper::SiteHashSet
BackgroundTracingHelper::ParsePerformanceMarkSiteHashes(
    std::string_view allow_list) {
  SiteHashSet allow_listed_hashes;
  auto hashes = base::SplitStringPiece(allow_list, ",", base::TRIM_WHITESPACE,
                                       base::SPLIT_WANT_NONEMPTY);
  for (auto& hash_str : hashes) {
    auto hash = ConvertToHashInteger(hash_str);
    if (!hash.has_value()) {
      return {};
    }
    allow_listed_hashes.insert(*hash);
  }
  return allow_listed_hashes;
}

}  // namespace blink
