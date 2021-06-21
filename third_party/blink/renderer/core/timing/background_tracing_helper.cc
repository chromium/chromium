// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/background_tracing_helper.h"

#include "base/cxx17_backports.h"
#include "base/feature_list.h"
#include "base/hash/md5.h"
#include "base/sys_byteorder.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/performance_mark.h"
#include "third_party/blink/renderer/platform/instrumentation/resource_coordinator/renderer_resource_coordinator.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/number_parsing_options.h"
#include "third_party/blink/renderer/platform/wtf/text/string_operators.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "url/url_constants.h"

namespace blink {

namespace {

enum TerminationCondition {
  kMustHaveTerminator,
  kMayHaveTerminator,
};

// Consumes a 1-8 byte hash from the given string, and the terminator if one is
// encountered. On success, the parsed hash is output via the optional |hash|
// and the new position of the cursor is returned. On failure, nullptr is
// returned.
const char* ParseHash(const char* begin,
                      const char* end,
                      uint32_t& hash,
                      TerminationCondition termination,
                      char valid_terminator0,
                      char valid_terminator1 = 0,
                      char valid_terminator2 = 0) {
  DCHECK(begin);
  DCHECK(end);
  DCHECK_LE(begin, end);
  DCHECK_NE(valid_terminator0, 0);

  const char* cur = begin;
  while (cur < end) {
    // Stop when a terminator is encountered.
    if (*cur == valid_terminator0)
      break;
    if (valid_terminator1 != 0 && *cur == valid_terminator1)
      break;
    if (valid_terminator2 != 0 && *cur == valid_terminator2)
      break;
    // Stop if any invalid characters are encountered.
    if (!IsASCIIHexDigit(*cur))
      return nullptr;
    ++cur;
    // Stop if the hash string is too long.
    if (cur - begin > 8)
      return nullptr;
  }

  // Stop if the hash is empty.
  if (cur == begin)
    return nullptr;

  // Enforce mandatory terminator characters.
  if (termination == kMustHaveTerminator && cur == end)
    return nullptr;

  // At this point we've successfully consumed a hash, so parse it.
  bool parsed = false;
  hash = WTF::HexCharactersToUInt(reinterpret_cast<const unsigned char*>(begin),
                                  cur - begin, WTF::NumberParsingOptions::kNone,
                                  &parsed);
  DCHECK(parsed);

  // If there's a terminator, advance past it.
  if (cur < end)
    ++cur;

  // Finally, return the advanced cursor.
  return cur;
}

static constexpr char kTriggerPrefix[] = "trigger:";

bool MarkNameIsTrigger(const String& mark_name) {
  return mark_name.StartsWith(kTriggerPrefix);
}

String GenerateFullTrigger(const String& site, const String& mark_name) {
  DCHECK(MarkNameIsTrigger(mark_name));
  return site + "-" + mark_name.Substring(base::size(kTriggerPrefix) - 1);
}

}  // namespace

// A thin wrapper around a SiteMarkHashMap that populates it at construction by
// parsing the relevant Finch parameters.
struct BackgroundTracingHelper::SiteMarkHashMapContainer {
  SiteMarkHashMapContainer();
  ~SiteMarkHashMapContainer() = default;

  SiteMarkHashMap site_mark_hash_map;
};

BackgroundTracingHelper::SiteMarkHashMapContainer::SiteMarkHashMapContainer() {
  // Do nothing if the feature is not enabled.
  if (!base::FeatureList::IsEnabled(
          features::kBackgroundTracingPerformanceMark))
    return;

  // Get the allow-list from the Finch configuration.
  std::string allow_list =
      features::kBackgroundTracingPerformanceMark_AllowList.Get();

  // Parse the allow-list. Silently ignoring malformed configuration data simply
  // means the feature will be disabled when this occurs.
  BackgroundTracingHelper::ParseBackgroundTracingPerformanceMarkHashes(
      allow_list, site_mark_hash_map);
}

BackgroundTracingHelper::BackgroundTracingHelper(ExecutionContext* context) {
  // Used to configure a per-origin allowlist of performance.mark events that
  // are permitted to be included in background traces. See crbug.com/1181774.

  // If there's no allow-list, then bail early.
  if (GetSiteMarkHashMap().IsEmpty())
    return;

  // Only support http and https origins to actual remote servers.
  auto* origin = context->GetSecurityOrigin();
  if (origin->IsLocal() || origin->IsOpaque() || origin->IsLocalhost())
    return;
  if (origin->Protocol() != url::kHttpScheme &&
      origin->Protocol() != url::kHttpsScheme) {
    return;
  }

  // Get the hash of the domain in an encoded format (friendly for converting to
  // ASCII, and matching the format in which URLs will be encoded prior to
  // hashing in the Finch list).
  String this_site = EncodeWithURLEscapeSequences(origin->Domain());
  uint32_t this_site_hash = MD5Hash32(this_site.Ascii());

  // Get the allow-list for this site, if there is one.
  mark_hashes_ = GetMarkHashSetForSiteHash(this_site_hash);

  // We only need the site information if there's actually a set of mark hashes.
  if (mark_hashes_) {
    site_ = this_site;
    site_hash_ = this_site_hash;
  }
}

BackgroundTracingHelper::~BackgroundTracingHelper() = default;

void BackgroundTracingHelper::MaybeEmitBackgroundTracingPerformanceMarkEvent(
    const PerformanceMark& mark) {
  if (!mark_hashes_)
    return;

  // Get the hashed mark name.
  const String& mark_name = mark.name();
  std::string mark_name_ascii = mark_name.Ascii();
  uint32_t mark_hash = MD5Hash32(mark_name_ascii);

  // See if the mark hash is in the permitted list.
  if (!mark_hashes_->Contains(mark_hash))
    return;

  // Emit the trace events. We emit hashes and strings to facilitate local trace
  // consumption. However, the strings will be stripped and only the hashes
  // shipped externally.

  auto event_lambda = [&](perfetto::EventContext ctx) {
    auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
    auto* data = event->set_chrome_hashed_performance_mark();
    data->set_site_hash(site_hash_);
    data->set_site(site_.Ascii());
    data->set_mark_hash(mark_hash);
    data->set_mark(mark_name_ascii);
  };

  // For additional context, also emit a paired event marking *when* the
  // performance.mark was actually created.
  TRACE_EVENT_INSTANT("blink", "performance.mark.created", event_lambda);

  // Emit an event with the actual timestamp associated with the mark.
  TRACE_EVENT_INSTANT("blink", "performance.mark", mark.UnsafeTimeForTraces(),
                      event_lambda);

  // If this is a slow-reports trigger then fire it.
  if (MarkNameIsTrigger(mark_name)) {
    RendererResourceCoordinator::Get()->FireBackgroundTracingTrigger(
        GenerateFullTrigger(site_, mark_name));
  }
}

void BackgroundTracingHelper::Trace(Visitor*) const {}

// static
const BackgroundTracingHelper::SiteMarkHashMap&
BackgroundTracingHelper::GetSiteMarkHashMap() {
  // This needs to be thread-safe because performance.mark is supported by both
  // windows and workers.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(SiteMarkHashMapContainer,
                                  site_mark_hash_map_container, ());
  return site_mark_hash_map_container.site_mark_hash_map;
}

// static
const BackgroundTracingHelper::MarkHashSet*
BackgroundTracingHelper::GetMarkHashSetForSiteHash(uint32_t site_hash) {
  const SiteMarkHashMap& site_mark_hash_map = GetSiteMarkHashMap();
  auto it = site_mark_hash_map.find(site_hash);
  if (it == site_mark_hash_map.end())
    return nullptr;
  return &(it->value);
}

// static
uint32_t BackgroundTracingHelper::MD5Hash32(base::StringPiece string) {
  base::MD5Digest digest;
  base::MD5Sum(string.data(), string.size(), &digest);
  uint32_t value;
  DCHECK_GE(sizeof(digest.a), sizeof(value));
  memcpy(&value, digest.a, sizeof(value));
  return base::NetToHost32(value);
}

// static
bool BackgroundTracingHelper::ParseBackgroundTracingPerformanceMarkHashes(
    base::StringPiece allow_list,
    SiteMarkHashMap& allow_listed_hashes) {
  // We parse into this temporary structure, and move into the output on
  // success.
  SiteMarkHashMap parsed_allow_listed_hashes;

  // The format is:
  //
  //   sitehash0=markhash0,...,markhashn;sitehash1=markhash0,...,markhashn
  //
  // where each hash is a 32-bit hex hash. We also allow commas to be replaced
  // with underscores so that they can be easily specified via the
  // --enable-features command-line.
  const char* cur = allow_list.data();
  const char* end = allow_list.data() + allow_list.size();
  while (cur < end) {
    // Parse a site hash.
    uint32_t site_hash = 0;
    cur = ParseHash(cur, end, site_hash, kMustHaveTerminator, '=');
    if (!cur)
      return false;

    // The site hash must be unique.
    if (parsed_allow_listed_hashes.Contains(site_hash))
      return false;

    // Parse the mark hashes.
    MarkHashSet parsed_mark_hashes;
    while (true) {
      // At least a single mark hash entry is expected per site hash.
      uint32_t mark_hash = 0;
      cur = ParseHash(cur, end, mark_hash, kMayHaveTerminator, ',', ';', '_');
      if (!cur)
        return false;

      // Duplicate entries are an error.
      auto result = parsed_mark_hashes.insert(mark_hash);
      if (!result.is_new_entry)
        return false;

      // We're done processing the current list of mark hashes if there's no
      // data left to consume, or if the terminator was a ';'.
      if (cur == end || cur[-1] == ';')
        break;
    }

    auto result = parsed_allow_listed_hashes.insert(
        site_hash, std::move(parsed_mark_hashes));
    // We guaranteed uniqueness of insertion by checking for the |site_hash|
    // before parsing the mark hashes.
    DCHECK(result.is_new_entry);
  }

  // Getting here means we successfully parsed the whole list.
  allow_listed_hashes = std::move(parsed_allow_listed_hashes);
  return true;
}

}  // namespace blink
