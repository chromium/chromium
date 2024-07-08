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

// The max number of digits in a topic. As new taxonomies are introduced and old
// topics are expired, the expectation is this value will gradually grow.
constexpr int kTopicMaxLength = 3;

// The number of characters in a version string, e.g., chrome.1:1:10. This will
// grow as versions require more digits.
constexpr int kVersionMaxLength = 13;

static_assert(browsing_topics::ConfigVersion::kMaxValue < 10,
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

std::string DeriveTopicsHeaderValue(
    const std::vector<blink::mojom::EpochTopicPtr>& topics,
    int num_versions_in_epochs) {
  net::structured_headers::List header_list;
  std::optional<std::string> last_version;
  std::vector<net::structured_headers::ParameterizedItem> cur_topics;

  // Build up the header without the padding parameter.
  for (auto& topic : topics) {
    bool new_version =
        (!last_version.has_value() || last_version.value() != topic->version);
    if (new_version) {
      if (cur_topics.size() > 0) {
        CHECK(last_version.has_value());
        header_list.push_back(net::structured_headers::ParameterizedMember(
            cur_topics,
            {{"v",
              net::structured_headers::Item(
                  *last_version, net::structured_headers::Item::kTokenType)}}));
        cur_topics.clear();
      }
      last_version = topic->version;
    }
    cur_topics.push_back(net::structured_headers::ParameterizedItem(
        net::structured_headers::Item(static_cast<int64_t>(topic->topic)), {}));
  }

  if (cur_topics.size() > 0) {
    CHECK(last_version.has_value());
    header_list.push_back(net::structured_headers::ParameterizedMember(
        cur_topics, {{"v", net::structured_headers::Item(
                               *last_version,
                               net::structured_headers::Item::kTokenType)}}));
  }

  // The header is now complete, except for padding. We want the header to be of
  // fixed size for the given number of versions in the list, so we add padding
  // to make that happen.

  // When adding padding, we'll always have at least 1 version.
  if (num_versions_in_epochs == 0) {
    num_versions_in_epochs = 1;
  }

  // The number of topics that should be in the padded response.
  int max_number_of_epochs =
      blink::features::kBrowsingTopicsNumberOfEpochsToExpose.Get();
  CHECK_LE(num_versions_in_epochs, max_number_of_epochs);
  CHECK_GT(max_number_of_epochs, 0);

  // The padded length of the header given the number of versions.
  // Example header: Sec-Browsing-Topics: (100 200);v=chrome.1:1:2,
  // (300);v=chrome.1:1:4, ();p=P00
  int max_length =
      max_number_of_epochs * kTopicMaxLength +  // length of three topics
      max_number_of_epochs -
      num_versions_in_epochs +      // spaces between topics in a list
      num_versions_in_epochs * 5 +  // '();v='
      num_versions_in_epochs *
          kVersionMaxLength +            // max length of the versions
      (num_versions_in_epochs - 1) * 2;  // the ', ' between topic lists

  // Add the bytes for the ", " between the last list and the padding list in
  // the event that there are no topics.
  if (header_list.size() == 0) {
    max_length += 2;
  }

  // How many bytes of padding do we need to add?
  int padding_needed =
      header_list.size() > 0
          ? max_length -
                net::structured_headers::SerializeList(header_list)->length()
          : max_length;

  // The padding should generally be >= 0. It can be negative in certain
  // circumstances and we need to handle that here. It can be negative if a new
  // version is rolled out via finch (e.g., model or taxonomy) that uses an
  // extra digit in its number but the binary hasn't been updated to handle the
  // extra digit yet. It could also happen if there is a race between getting
  // topics and getting the number of distinct topic versions. We clamp to 0 to
  // prevent breakage in these rare circumstances.
  if (padding_needed < 0) {
    padding_needed = 0;
  }

  // Add the padding list at the end.
  header_list.push_back(net::structured_headers::ParameterizedMember(
      std::vector<net::structured_headers::ParameterizedItem>(),
      {{"p", net::structured_headers::Item(
                 base::StrCat({"P", std::string(padding_needed, '0')}),
                 net::structured_headers::Item::kTokenType)}}));

  std::optional<std::string> serialized_header =
      net::structured_headers::SerializeList(header_list);
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
  if (!request_initiator_frame.GetPage().IsPrimary()) {
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
