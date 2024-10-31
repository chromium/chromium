// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/prefetch_matches.h"

#include <array>
#include <functional>
#include <iomanip>
#include <ios>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/stack_allocated.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "build/buildflag.h"
#include "net/base/load_flags_to_string.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/stringify_enum.h"

namespace network {

namespace {

// A macro to run the DO_FIELD() macro over every member of the ResourceRequest
// struct. This list of fields must be complete so that we can verify at compile
// time that it matches the struct. The optional argument can be
// used to place a comma after every field but the last one, ie.
// DO_FIELD_FOR_ALL_FIELDS(COMMA).
#define COMMA ,

// clang-format off

#define DO_FIELD_FOR_ALL_FIELDS(...)                           \
  DO_FIELD(method) __VA_ARGS__                                 \
  DO_FIELD(url) __VA_ARGS__                                    \
  DO_FIELD(site_for_cookies) __VA_ARGS__                       \
  DO_FIELD(update_first_party_url_on_redirect) __VA_ARGS__     \
  DO_FIELD(request_initiator) __VA_ARGS__                      \
  DO_FIELD(isolated_world_origin) __VA_ARGS__                  \
  DO_FIELD(navigation_redirect_chain) __VA_ARGS__              \
  DO_FIELD(referrer) __VA_ARGS__                               \
  DO_FIELD(referrer_policy) __VA_ARGS__                        \
  DO_FIELD(headers) __VA_ARGS__                                \
  DO_FIELD(cors_exempt_headers) __VA_ARGS__                    \
  DO_FIELD(load_flags) __VA_ARGS__                             \
  DO_FIELD(resource_type) __VA_ARGS__                          \
  DO_FIELD(priority) __VA_ARGS__                               \
  DO_FIELD(priority_incremental) __VA_ARGS__                   \
  DO_FIELD(cors_preflight_policy) __VA_ARGS__                  \
  DO_FIELD(originated_from_service_worker) __VA_ARGS__         \
  DO_FIELD(skip_service_worker) __VA_ARGS__                    \
  DO_FIELD(mode) __VA_ARGS__                                   \
  DO_FIELD(required_ip_address_space) __VA_ARGS__              \
  DO_FIELD(credentials_mode) __VA_ARGS__                       \
  DO_FIELD(redirect_mode) __VA_ARGS__                          \
  DO_FIELD(fetch_integrity) __VA_ARGS__                        \
  DO_FIELD(destination) __VA_ARGS__                            \
  DO_FIELD(original_destination) __VA_ARGS__                   \
  DO_FIELD(request_body) __VA_ARGS__                           \
  DO_FIELD(keepalive) __VA_ARGS__                              \
  DO_FIELD(browsing_topics) __VA_ARGS__                        \
  DO_FIELD(ad_auction_headers) __VA_ARGS__                     \
  DO_FIELD(shared_storage_writable_eligible) __VA_ARGS__       \
  DO_FIELD(has_user_gesture) __VA_ARGS__                       \
  DO_FIELD(enable_load_timing) __VA_ARGS__                     \
  DO_FIELD(enable_upload_progress) __VA_ARGS__                 \
  DO_FIELD(do_not_prompt_for_login) __VA_ARGS__                \
  DO_FIELD(is_outermost_main_frame) __VA_ARGS__                \
  DO_FIELD(transition_type) __VA_ARGS__                        \
  DO_FIELD(previews_state) __VA_ARGS__                         \
  DO_FIELD(upgrade_if_insecure) __VA_ARGS__                    \
  DO_FIELD(is_revalidating) __VA_ARGS__                        \
  DO_FIELD(throttling_profile_id) __VA_ARGS__                  \
  DO_FIELD(custom_proxy_pre_cache_headers) __VA_ARGS__         \
  DO_FIELD(custom_proxy_post_cache_headers) __VA_ARGS__        \
  DO_FIELD(fetch_window_id) __VA_ARGS__                        \
  DO_FIELD(devtools_request_id) __VA_ARGS__                    \
  DO_FIELD(devtools_stack_id) __VA_ARGS__                      \
  DO_FIELD(is_fetch_like_api) __VA_ARGS__                      \
  DO_FIELD(is_fetch_later_api) __VA_ARGS__                     \
  DO_FIELD(is_favicon) __VA_ARGS__                             \
  DO_FIELD(recursive_prefetch_token) __VA_ARGS__               \
  DO_FIELD(trusted_params) __VA_ARGS__                         \
  DO_FIELD(trust_token_params) __VA_ARGS__                     \
  DO_FIELD(web_bundle_token_params) __VA_ARGS__                \
  DO_FIELD(devtools_accepted_stream_types) __VA_ARGS__         \
  DO_FIELD(net_log_create_info) __VA_ARGS__                    \
  DO_FIELD(net_log_reference_info) __VA_ARGS__                 \
  DO_FIELD(target_ip_address_space) __VA_ARGS__                \
  DO_FIELD(storage_access_api_status) __VA_ARGS__              \
  DO_FIELD(attribution_reporting_support) __VA_ARGS__          \
  DO_FIELD(attribution_reporting_eligibility) __VA_ARGS__      \
  DO_FIELD(shared_dictionary_writer_enabled) __VA_ARGS__       \
  DO_FIELD(attribution_reporting_src_token) __VA_ARGS__        \
  DO_FIELD(is_ad_tagged) __VA_ARGS__                           \
  DO_FIELD(prefetch_token) __VA_ARGS__                         \
  DO_FIELD(socket_tag)

// clang-format on

constexpr bool FieldCountIsCorrect(const ResourceRequest& request) {
  if ((false)) {
    // This will fail to compile if the number of fields in ResourceRequest is
    // different from the number this code knows about. It doesn't actually need
    // to be executed. The double extra parenthesis around "(false)" indicate to
    // the compiler that this code is intentionally unreachable.
#define DO_FIELD(name) unused_##name
    [[maybe_unused]] const auto& [DO_FIELD_FOR_ALL_FIELDS(COMMA)] = request;
#undef DO_FIELD
  }
  return true;
}

enum class Fields {
#define DO_FIELD(name) k##name,
  DO_FIELD_FOR_ALL_FIELDS()
#undef DO_FIELD
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. Fields that have been removed from
// ResourceRequest should have "Obsolete" added to their names. The `kUnknown`
// value is used for fields that do not have an entry in the mapping below. If
// it shows up in UMA then this enum needs to be updated to include the new
// field(s). Fields that are not used for matching do not need to be included in
// this enum.
//
// LINT.IfChange(FieldsForUma)
enum class FieldsForUma {
  kUnknown = 0,
  kMethod = 1,
  kUrl = 2,
  kSiteForCookies = 3,
  kUpdateFirstPartyUrlOnRedirect = 4,
  kRequestInitiator = 5,
  kIsolatedWorldOrigin = 6,
  kNavigationRedirectChain = 7,
  kReferrer = 8,
  kReferrerPolicy = 9,
  kHeaders = 10,
  kCorsExemptHeaders = 11,
  kLoadFlags = 12,
  kResourceType = 13,
  kPriority = 14,
  kPriorityIncremental = 15,
  kCorsPreflightPolicy = 16,
  kOriginatedFromServiceWorker = 17,
  kSkipServiceWorker = 18,
  kMode = 19,
  kRequiredIpAddressSpace = 20,
  kCredentialsMode = 21,
  kRedirectMode = 22,
  kFetchIntegrity = 23,
  kDestination = 24,
  kOriginalDestination = 25,
  kRequestBody = 26,
  kKeepalive = 27,
  kBrowsingTopics = 28,
  kAdAuctionHeaders = 29,
  kSharedStorageWritableEligible = 30,
  kHasUserGesture = 31,
  kEnableLoadTiming = 32,
  kEnableUploadProgress = 33,
  kDoNotPromptForLogin = 34,
  kIsOutermostMainFrame = 35,
  kTransitionType = 36,
  kPreviewsState = 37,
  kUpgradeIfInsecure = 38,
  kIsRevalidating = 39,
  kThrottlingProfileId = 40,
  kCustomProxyPreCacheHeaders = 41,
  kCustomProxyPostCacheHeaders = 42,
  kFetchWindowId = 43,
  kDevtoolsRequestId = 44,
  kDevtoolsStackId = 45,
  kIsFetchLikeApi = 46,
  kIsFetchLaterApi = 47,
  kIsFavicon = 48,
  kRecursivePrefetchToken = 49,
  kTrustedParams = 50,
  kTrustTokenParams = 51,
  kWebBundleTokenParams = 52,
  kDevtoolsAcceptedStreamTypes = 53,
  kNetLogCreateInfo = 54,
  kNetLogReferenceInfo = 55,
  kTargetIpAddressSpace = 56,
  kStorageAccessApiStatus = 57,
  kAttributionReportingSupport = 58,
  kAttributionReportingEligibility = 59,
  kSharedDictionaryWriterEnabled = 60,
  kAttributionReportingSrcToken = 61,
  kIsAdTagged = 62,
  kMaxValue = kIsAdTagged,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/network/enums.xml:PrefetchMatchesResourceRequestField)

constexpr auto kUmaEnumMap = base::MakeFixedFlatMap<Fields, FieldsForUma>({
    {Fields::kmethod, FieldsForUma::kMethod},
    {Fields::kurl, FieldsForUma::kUrl},
    {Fields::ksite_for_cookies, FieldsForUma::kSiteForCookies},
    {Fields::kupdate_first_party_url_on_redirect,
     FieldsForUma::kUpdateFirstPartyUrlOnRedirect},
    {Fields::krequest_initiator, FieldsForUma::kRequestInitiator},
    {Fields::kisolated_world_origin, FieldsForUma::kIsolatedWorldOrigin},
    {Fields::knavigation_redirect_chain,
     FieldsForUma::kNavigationRedirectChain},
    {Fields::kreferrer, FieldsForUma::kReferrer},
    {Fields::kreferrer_policy, FieldsForUma::kReferrerPolicy},
    {Fields::kheaders, FieldsForUma::kHeaders},
    {Fields::kcors_exempt_headers, FieldsForUma::kCorsExemptHeaders},
    {Fields::kload_flags, FieldsForUma::kLoadFlags},
    {Fields::kresource_type, FieldsForUma::kResourceType},
    {Fields::kpriority, FieldsForUma::kPriority},
    {Fields::kpriority_incremental, FieldsForUma::kPriorityIncremental},
    {Fields::kcors_preflight_policy, FieldsForUma::kCorsPreflightPolicy},
    {Fields::koriginated_from_service_worker,
     FieldsForUma::kOriginatedFromServiceWorker},
    {Fields::kskip_service_worker, FieldsForUma::kSkipServiceWorker},
    {Fields::kmode, FieldsForUma::kMode},
    {Fields::krequired_ip_address_space, FieldsForUma::kRequiredIpAddressSpace},
    {Fields::kcredentials_mode, FieldsForUma::kCredentialsMode},
    {Fields::kredirect_mode, FieldsForUma::kRedirectMode},
    {Fields::kfetch_integrity, FieldsForUma::kFetchIntegrity},
    {Fields::kdestination, FieldsForUma::kDestination},
    {Fields::koriginal_destination, FieldsForUma::kOriginalDestination},
    {Fields::krequest_body, FieldsForUma::kRequestBody},
    {Fields::kkeepalive, FieldsForUma::kKeepalive},
    {Fields::kbrowsing_topics, FieldsForUma::kBrowsingTopics},
    {Fields::kad_auction_headers, FieldsForUma::kAdAuctionHeaders},
    {Fields::kshared_storage_writable_eligible,
     FieldsForUma::kSharedStorageWritableEligible},
    {Fields::khas_user_gesture, FieldsForUma::kHasUserGesture},
    {Fields::kenable_load_timing, FieldsForUma::kEnableLoadTiming},
    {Fields::kenable_upload_progress, FieldsForUma::kEnableUploadProgress},
    {Fields::kdo_not_prompt_for_login, FieldsForUma::kDoNotPromptForLogin},
    {Fields::kis_outermost_main_frame, FieldsForUma::kIsOutermostMainFrame},
    {Fields::ktransition_type, FieldsForUma::kTransitionType},
    {Fields::kpreviews_state, FieldsForUma::kPreviewsState},
    {Fields::kupgrade_if_insecure, FieldsForUma::kUpgradeIfInsecure},
    {Fields::kis_revalidating, FieldsForUma::kIsRevalidating},
    {Fields::kthrottling_profile_id, FieldsForUma::kThrottlingProfileId},
    {Fields::kcustom_proxy_pre_cache_headers,
     FieldsForUma::kCustomProxyPreCacheHeaders},
    {Fields::kcustom_proxy_post_cache_headers,
     FieldsForUma::kCustomProxyPostCacheHeaders},
    {Fields::kfetch_window_id, FieldsForUma::kFetchWindowId},
    {Fields::kdevtools_request_id, FieldsForUma::kDevtoolsRequestId},
    {Fields::kdevtools_stack_id, FieldsForUma::kDevtoolsStackId},
    {Fields::kis_fetch_like_api, FieldsForUma::kIsFetchLikeApi},
    {Fields::kis_fetch_later_api, FieldsForUma::kIsFetchLaterApi},
    {Fields::kis_favicon, FieldsForUma::kIsFavicon},
    {Fields::krecursive_prefetch_token, FieldsForUma::kRecursivePrefetchToken},
    {Fields::ktrust_token_params, FieldsForUma::kTrustTokenParams},
    {Fields::kweb_bundle_token_params, FieldsForUma::kWebBundleTokenParams},
    {Fields::kdevtools_accepted_stream_types,
     FieldsForUma::kDevtoolsAcceptedStreamTypes},
    {Fields::knet_log_create_info, FieldsForUma::kNetLogCreateInfo},
    {Fields::knet_log_reference_info, FieldsForUma::kNetLogReferenceInfo},
    {Fields::ktarget_ip_address_space, FieldsForUma::kTargetIpAddressSpace},
    {Fields::kstorage_access_api_status, FieldsForUma::kStorageAccessApiStatus},
    {Fields::kattribution_reporting_support,
     FieldsForUma::kAttributionReportingSupport},
    {Fields::kattribution_reporting_eligibility,
     FieldsForUma::kAttributionReportingEligibility},
    {Fields::kshared_dictionary_writer_enabled,
     FieldsForUma::kSharedDictionaryWriterEnabled},
    {Fields::kattribution_reporting_src_token,
     FieldsForUma::kAttributionReportingSrcToken},
    {Fields::kis_ad_tagged, FieldsForUma::kIsAdTagged},
});

// Fields that should be completely ignored for the purposes of matching should
// be added to this array.
constexpr std::array kIgnoredFields = {
    // TODO(crbug.com/342445996): Dynamically adjust the priority of the request
    // once the real request arrives.
    Fields::kpriority,
    Fields::kpriority_incremental,

    // Prefefches can't prompt for login, but render processes generally allow
    // it.
    // TODO(crbug.com/342445996): Is this really okay? What's the right answer
    // here?
    Fields::kdo_not_prompt_for_login,

    // TODO(crbug.com/342445996): Figure out how to plumb the
    // throttling_profile_id through to the prefetch machinery so that
    // prefetches will be throttled correctly. Then remove this.
    Fields::kthrottling_profile_id,

    // fetch_window_id is used to associate auth/client certificate UI with the
    // correct window. Prefetches should never display UI, so don't need this.
    Fields::kfetch_window_id,

    // TODO(crbug.com/342445996): Wire up devtools to prefetches somehow.
    Fields::kdevtools_request_id,
    Fields::kdevtools_stack_id,
    // trusted_params are used by the prefetch machinery, but are prohibited
    // from being sent by a render process, so won't match.
    Fields::ktrusted_params,

    // These are not expected to match.
    Fields::knet_log_create_info,
    Fields::knet_log_reference_info,

    // Currently ignored.
    Fields::kprefetch_token,

    // It doesn't matter if they match.
    Fields::ksocket_tag,
};

// These headers are completely ignored for the purposes of matching when they
// appear in the `headers` field.
constexpr auto kIgnoredHeaders = base::MakeFixedFlatSet<std::string_view>({
    "purpose",
    "sec-purpose",
});
using IgnoredHeadersType = decltype(kIgnoredHeaders);

bool MatchHeadersWithExceptions(const net::HttpRequestHeaders& prefetch_headers,
                                const net::HttpRequestHeaders& real_headers,
                                const IgnoredHeadersType& ignored_headers) {
  std::unordered_map<std::string, std::string_view> lowered_prefetch_headers;
  const net::HttpRequestHeaders::HeaderVector& prefetch_headers_vector =
      prefetch_headers.GetHeaderVector();
  lowered_prefetch_headers.reserve(prefetch_headers_vector.size());
  for (const auto& keyvalue : prefetch_headers_vector) {
    std::string lowered_key = base::ToLowerASCII(keyvalue.key);
    if (ignored_headers.contains(lowered_key)) {
      continue;
    }
    auto [_, inserted] = lowered_prefetch_headers.emplace(
        std::move(lowered_key), keyvalue.value);
    CHECK(inserted) << "There should be no duplicate header keys in an "
                       "HttpRequestHeaders object, and certainly not '"
                    << lowered_key << "'";
  }
  const net::HttpRequestHeaders::HeaderVector& real_headers_vector =
      real_headers.GetHeaderVector();
  size_t real_header_count = 0;
  for (const auto& keyvalue : real_headers_vector) {
    std::string lowered_key = base::ToLowerASCII(keyvalue.key);
    if (ignored_headers.contains(lowered_key)) {
      continue;
    }
    ++real_header_count;
    auto it = lowered_prefetch_headers.find(lowered_key);
    if (it == lowered_prefetch_headers.end()) {
      return false;
    }
    if (it->second != keyvalue.value) {
      return false;
    }
  }
  return real_header_count == lowered_prefetch_headers.size();
}

// MatchByType() can be overloaded to provide special behavior for specific
// types. This will apply to all fields of this type that do not have a
// specialization of FieldMatcher.

// This is the generic implementation that uses operator==.
template <typename T>
bool MatchByType(const T& prefetch_value, const T& real_value) {
  return prefetch_value == real_value;
}

// SiteForCookies does not have operator==.
bool MatchByType(const net::SiteForCookies& prefetch_value,
                 const net::SiteForCookies& real_value) {
  return prefetch_value.IsEquivalent(real_value);
}

bool MatchByType(const net::HttpRequestHeaders& prefetch_headers,
                 const net::HttpRequestHeaders& real_headers) {
  return MatchHeadersWithExceptions(prefetch_headers, real_headers, {});
}

bool MatchByType(
    const std::optional<ResourceRequest::WebBundleTokenParams>& prefetch_params,
    const std::optional<ResourceRequest::WebBundleTokenParams>& real_params) {
  if (prefetch_params.has_value() != real_params.has_value()) {
    return false;
  }
  if (!prefetch_params.has_value()) {
    return true;
  }
  // Neither `handle` nor `render_process_id` would be expected to match anyway.
  return prefetch_params->bundle_url == real_params->bundle_url &&
         prefetch_params->token == real_params->token;
}

bool MatchByType(const DataElementBytes& prefetch_value,
                 const DataElementBytes& real_value) {
  return prefetch_value.bytes() == real_value.bytes();
}

bool MatchByType(const DataElementFile& prefetch_value,
                 const DataElementFile& real_value) {
  return prefetch_value.path() == real_value.path() &&
         prefetch_value.offset() == real_value.offset() &&
         prefetch_value.length() == real_value.length() &&
         prefetch_value.expected_modification_time() ==
             real_value.expected_modification_time();
}

bool MatchByType(const DataElement& prefetch_value,
                 const DataElement& real_value) {
  using Tag = DataElement::Tag;
  const Tag prefetch_tag = prefetch_value.type();
  const Tag real_tag = real_value.type();
  if (prefetch_tag != real_tag) {
    return false;
  }
  switch (real_tag) {
    case Tag::kBytes:
      return MatchByType(prefetch_value.As<DataElementBytes>(),
                         real_value.As<DataElementBytes>());
    case Tag::kFile:
      return MatchByType(prefetch_value.As<DataElementFile>(),
                         real_value.As<DataElementFile>());
    default:
      // For now, every other type just returns false. If we needed to support
      // DataElementDataPipe or DataElementChunkedDataPipe we'd need to somehow
      // make this function asynchronous.
      return false;
  }
}

bool MatchByType(const ResourceRequestBody& prefetch_value,
                 const ResourceRequestBody& real_value) {
  const auto& prefetch_elements = *prefetch_value.elements();
  const auto& real_elements = *real_value.elements();
  if (prefetch_elements.size() != real_elements.size()) {
    return false;
  }
  for (size_t i = 0; i < prefetch_elements.size(); ++i) {
    if (!MatchByType(prefetch_elements[i], real_elements[i])) {
      return false;
    }
  }
  return true;
}

// Unwrap a scoped_refptr to perform a match on the contained objects.
template <typename T>
bool MatchByType(const scoped_refptr<T>& prefetch_value,
                 const scoped_refptr<T>& real_value) {
  if (prefetch_value == nullptr && real_value == nullptr) {
    return true;
  }
  if (prefetch_value == nullptr || real_value == nullptr) {
    // Since both of them aren't null, one of them must be null and the other
    // not, so they don't match.
    return false;
  }
  return MatchByType(*prefetch_value, *real_value);
}

// FieldMatcher is the configuration point for matches whose behaviour varies
// depending on which field is involved. Specializations should be created for
// fields that need specific behavior.

// This is the general implementation of FieldMatcher which doesn't care what
// field it is looking at, and only compares based on the type.
template <Fields f>
struct FieldMatcher {
  template <typename T>
  static bool Match(const T& prefetch_value, const T& real_value) {
    // Immediately dispatch to a function that doesn't care about the field to
    // encourage the compiler to reduce code size.
    return MatchByType(prefetch_value, real_value);
  }
};

template <Fields f>
constexpr bool kFieldIsIgnored = base::Contains(kIgnoredFields, f);

// This is the implementation of FieldMatcher that completely ignores the
// contents of the field. Fields which should use this implementation should be
// added to `kIgnoredFields` above.
template <Fields f>
  requires kFieldIsIgnored<f>
struct FieldMatcher<f> {
  template <typename T>
  static bool Match(const T& prefetch_request, const T& real_request) {
    return true;
  }
};

// We ignore some differences in the `headers` field.
template <>
struct FieldMatcher<Fields::kheaders> {
  static bool Match(const net::HttpRequestHeaders& prefetch_headers,
                    const net::HttpRequestHeaders& real_headers) {
    return MatchHeadersWithExceptions(prefetch_headers, real_headers,
                                      kIgnoredHeaders);
  }
};

void LogMismatchToUma(Fields field) {
  auto it = kUmaEnumMap.find(field);
  FieldsForUma uma_field = FieldsForUma::kUnknown;
  if (it != kUmaEnumMap.end()) {
    uma_field = it->second;
  }
  // TODO(ricea): This can be switched to the function version whem mismatches
  // become sufficiently rare that the overhead doesn't matter.
  UMA_HISTOGRAM_ENUMERATION("Network.PrefetchMatches.FirstMismatch", uma_field);
}

void PrintSpanifiedObject(std::ostream& os, base::span<uint8_t> object) {
  os << object.size() << "-byte-object<";
  size_t counter = 0;
  for (uint8_t byte : object) {
    auto oldflags = os.flags();
    os << '<' << std::hex << std::setfill('0') << std::setw(2) << uint32_t{byte}
       << '>';
    os.flags(oldflags);
    ++counter;
    if (counter < object.size()) {
      if (counter % 8 == 0) {
        os << ' ';
      } else {
        os << '-';
      }
    }
  }
  os << '>';
}

template <typename T>
void PrintAsBinary(std::ostream& os, const T& value) {
  constexpr size_t kSize = sizeof(T);
  // Print the bytes. This is inspired by GTest's handling of unprintable
  // values.
  std::array<uint8_t, kSize> bytes;
  // memcpy is approved by the C++ standard for type-punning to bytes.
  base::span(bytes).copy_from(base::byte_span_from_ref(value));
  PrintSpanifiedObject(os, bytes);
}

// CustomPrinter() is a customization point for types that need custom handling
// to print the values.

// This version enables printing anything that has a suitable ToString()
// method.
template <typename T>
  requires requires(std::ostream& os, const T& value) {
    os << value.ToString();
  }
void CustomPrinter(std::ostream& os, const T& value) {
  os << value.ToString();
}

// This version enables printing anything that has a suitable ToDebugString()
// method.
template <typename T>
  requires requires(std::ostream& os, const T& value) {
    os << value.ToDebugString();
  }
void CustomPrinter(std::ostream& os, const T& value) {
  os << value.ToDebugString();
}

// Print an enum.
template <typename T>
  requires std::is_enum_v<T>
void CustomPrinter(std::ostream& os, const T& value) {
  StreamEnumValueTo(os, value);
}

// The printers for std::vector and std::optional can recursively call into
// other printers, so they need to go last. They also need to be preclared.

template <typename T>
void CustomPrinter(std::ostream& os, const std::vector<T>& values);
template <typename T>
void CustomPrinter(std::ostream& os, const std::optional<T>& value);

// Print a std::vector of something using a CustomPrinter if available.
template <typename T>
void CustomPrinter(std::ostream& os, const std::vector<T>& values) {
  os << '{';
  for (size_t i = 0; i < values.size(); ++i) {
    if constexpr (requires { CustomPrinter(os, values[i]); }) {
      CustomPrinter(os, values[i]);
    } else {
      os << values[i];
    }
    if (i < values.size() - 1) {
      os << ", ";
    }
  }
  os << '}';
}

// Print something wrapped in std::optional that otherwise would be printable.
template <typename T>
void CustomPrinter(std::ostream& os, const std::optional<T>& value) {
  if (value.has_value()) {
    const T& inner_value = value.value();
    if constexpr (requires { CustomPrinter(os, inner_value); }) {
      CustomPrinter(os, inner_value);
    } else if constexpr (requires { os << inner_value; }) {
      os << inner_value;
    } else {
      PrintAsBinary(os, inner_value);
    }
  } else {
    os << "std::nullopt";
  }
}

// MakeStreamable makes it possible to stream a value of any type (except void)
// to a stream. It does this by using a CustomPrinter() if one is available, and
// falling back to printing bytes if all else fails.
template <typename T>
class MakeStreamable final {
  STACK_ALLOCATED();

 public:
  explicit MakeStreamable(const T& value) : value_(value) {}

  MakeStreamable(const MakeStreamable&) = delete;
  MakeStreamable& operator=(const MakeStreamable&) = delete;

 private:
  friend std::ostream& operator<<(std::ostream& os, MakeStreamable<T>&& me) {
    const T& value = me.value_;
    if constexpr (requires { CustomPrinter(os, value); }) {
      CustomPrinter(os, value);
    } else if constexpr (requires { os << value; }) {
      os << value;
    } else {
      PrintAsBinary(os, value);
    }
    return os;
  }

  const T& value_;
};

template <typename T>
void LogMismatchVerbosely(const T& prefetch_value,
                          const T& real_value,
                          std::string_view name) {
  DVLOG(1) << "Mismatch between prefetched\nResourceRequest and real "
           << "ResourceRequest:\n"
           << std::boolalpha << "prefetch_request." << name << " = "
           << MakeStreamable(prefetch_value) << "\n    real_request." << name
           << " = " << MakeStreamable(real_value) << "\n";
}

// A special version of LogMismatchVerbosely just for load_flags. At this time
// we don't need a generalised method to change the logging per-field, so just
// overload for this specific case.
void LogMismatchVerbosely(int prefetch_value,
                          int real_value,
                          std::string_view name) {
  if (!DCHECK_IS_ON()) {
    return;
  }
  if (name == "load_flags") {
    DVLOG(1) << "Mismatch between prefetched\nResourceRequest and real "
             << "ResourceRequest:\nprefetch_request.load_flags = "
             << net::LoadFlagsToString(prefetch_value)
             << "\n    real_request.load_flags = "
             << net::LoadFlagsToString(real_value) << "\n ";
  } else {
    DVLOG(1) << "Mismatch between prefetched\nResourceRequest and real "
             << "ResourceRequest:\nprefetch_request." << name << " = "
             << prefetch_value << "\n    real_request." << name << " = "
             << real_value << "\n";
  }
}

}  // namespace

bool PrefetchMatches(const ResourceRequest& prefetch_request,
                     const ResourceRequest& real_request) {
  CHECK(!real_request.trusted_params.has_value());

  CHECK(FieldCountIsCorrect(prefetch_request));

  bool all_fields_match = true;
  using enum Fields;

#define DO_FIELD(name)                                                     \
  if (!FieldMatcher<k##name>::Match(prefetch_request.name,                 \
                                    real_request.name)) {                  \
    if (all_fields_match) {                                                \
      LogMismatchToUma(k##name);                                           \
      all_fields_match = false;                                            \
    }                                                                      \
    LogMismatchVerbosely(prefetch_request.name, real_request.name, #name); \
  }

  DO_FIELD_FOR_ALL_FIELDS()

#undef DO_FIELD

  UMA_HISTOGRAM_BOOLEAN("Network.PrefetchMatches.Result", all_fields_match);
  return all_fields_match;
}

}  // namespace network
