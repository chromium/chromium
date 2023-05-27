// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_topics/header_util.h"

#include "base/strings/strcat.h"
#include "components/browsing_topics/common/semantic_tree.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/page.h"
#include "content/public/common/content_client.h"
#include "net/http/http_request_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"

namespace content {

namespace {

// These numbers must align with the structured header's internal
// implementation.
// For ' ' character.
constexpr int kInnerListItemsSeparatorLength = 1;
// For ';' and '=' characters.
constexpr int kParamsSeparatorLength = 2;

constexpr int kParamKeyLength = 1;
constexpr int kTopicMaxLength = 3;

// Example: chrome.1:1:10
constexpr int kVersionMaxLength = 13;

static_assert(blink::features::kBrowsingTopicsConfigVersionDefault < 10,
              "Topics config version should not exceed 1 digit, or "
              "`kVersionMaxLength` should be updated accordingly.");

static_assert(blink::features::kBrowsingTopicsTaxonomyVersionDefault < 10,
              "Topics taxonomy version should not exceed 1 digit, or "
              "`kVersionMaxLength` should be updated accordingly.");

static_assert(browsing_topics::SemanticTree::kNumTopics < 1000,
              "Total number of topics (i.e. max topic ID) should not exceed 3 "
              "digits, or `kTopicMaxLength` should be updated accordingly.");

}  // namespace

const char kBrowsingTopicsRequestHeaderKey[] = "Sec-Browsing-Topics";

net::structured_headers::ParameterizedItem CreateParameterizedTopic(
    const blink::mojom::EpochTopicPtr& topic,
    bool skip_version,
    int& serialized_inner_list_length) {
  if (skip_version) {
    return net::structured_headers::ParameterizedItem(
        net::structured_headers::Item(static_cast<int64_t>(topic->topic)), {});
  }

  serialized_inner_list_length +=
      kParamsSeparatorLength + kParamKeyLength + topic->version.size();

  return net::structured_headers::ParameterizedItem(
      net::structured_headers::Item(static_cast<int64_t>(topic->topic)),
      {{"v", net::structured_headers::Item(
                 topic->version, net::structured_headers::Item::kTokenType)}});
}

std::string DeriveTopicsHeaderValue(
    const std::vector<blink::mojom::EpochTopicPtr>& topics,
    int num_versions_in_epochs) {
  std::vector<net::structured_headers::ParameterizedItem> header_list;

  // Manually derive the length of the inner topics list in serialized format.
  int serialized_inner_list_length = 0;

  absl::optional<std::string> last_version;
  for (auto& topic : topics) {
    int topic_digits_count = base::NumberToString(topic->topic).size();
    CHECK_LE(topic_digits_count, kTopicMaxLength);

    serialized_inner_list_length += topic_digits_count;
    if (!header_list.empty()) {
      serialized_inner_list_length += kInnerListItemsSeparatorLength;
    }

    bool skip_version =
        (last_version && last_version.value() == topic->version);

    header_list.push_back(CreateParameterizedTopic(
        topic, skip_version, serialized_inner_list_length));
    last_version = topic->version;
  }

  int max_number_of_epochs =
      blink::features::kBrowsingTopicsNumberOfEpochsToExpose.Get();
  CHECK_LE(num_versions_in_epochs, max_number_of_epochs);
  CHECK_GT(max_number_of_epochs, 0);

  // If there are no valid epochs, use the `max_padding_length` for epochs
  // having same versions.
  if (num_versions_in_epochs == 0) {
    num_versions_in_epochs = 1;
  }

  int max_padding_length =
      max_number_of_epochs * kTopicMaxLength +
      num_versions_in_epochs *
          (kVersionMaxLength + kParamsSeparatorLength + kParamKeyLength) +
      (max_number_of_epochs - 1) * kInnerListItemsSeparatorLength;

  // If `serialized_inner_list_length` is greater than
  // `max_padding_length`, keep the header as-is and don't pad
  // anything. This could happen in the following situations:
  // - We have a newer config/taxonomoy version from Finch and haven't updated
  //   the default parameter values.
  // - A newer model is used on an old Chrome version.
  // - There was a race between call to get `topics` and the call to get the
  //   `num_versions_in_epochs`. This should be extremely unlikely to occur.
  int padding_length = (serialized_inner_list_length <= max_padding_length)
                           ? (max_padding_length - serialized_inner_list_length)
                           : 0;
  std::string padded_token =
      base::StrCat({"P", std::string(padding_length, '0')});

  std::vector<net::structured_headers::DictionaryMember> header_dict;
  header_dict.emplace_back("t", net::structured_headers::ParameterizedMember(
                                    std::move(header_list), {}));
  header_dict.emplace_back(
      "p", net::structured_headers::ParameterizedMember(
               net::structured_headers::Item(
                   padded_token, net::structured_headers::Item::kTokenType),
               {}));

  absl::optional<std::string> serialized_header =
      net::structured_headers::SerializeDictionary(
          net::structured_headers::Dictionary(std::move(header_dict)));
  CHECK(serialized_header);

  return *serialized_header;
}

void HandleTopicsEligibleResponse(
    const network::mojom::ParsedHeadersPtr& parsed_headers,
    const url::Origin& caller_origin,
    RenderFrameHost& request_initiator_frame,
    browsing_topics::ApiCallerSource caller_source) {
  DCHECK(caller_source == browsing_topics::ApiCallerSource::kFetch ||
         caller_source == browsing_topics::ApiCallerSource::kIframeAttribute);

  if (!parsed_headers || !parsed_headers->observe_browsing_topics) {
    return;
  }

  // Check the page's IsPrimary() status again in case it has changed since the
  // request time.
  if (!request_initiator_frame.GetPage().IsPrimary())
    return;

  // TODO(crbug.com/1244137): IsPrimary() doesn't actually detect portals yet.
  // Remove this when it does.
  if (!static_cast<const RenderFrameHostImpl*>(
           request_initiator_frame.GetMainFrame())
           ->IsOutermostMainFrame()) {
    return;
  }

  // Store the observation.
  std::vector<blink::mojom::EpochTopicPtr> topics;
  GetContentClient()->browser()->HandleTopicsWebApi(
      caller_origin, request_initiator_frame.GetMainFrame(), caller_source,
      /*get_topics=*/false,
      /*observe=*/true, topics);
}

}  // namespace content
