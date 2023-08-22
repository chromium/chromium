// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/frame_navigation_entry.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"

namespace content {

FrameNavigationEntry::FrameNavigationEntry(
    const std::string& frame_unique_name,
    int64_t item_sequence_number,
    int64_t document_sequence_number,
    const std::string& navigation_api_key,
    scoped_refptr<SiteInstanceImpl> site_instance,
    scoped_refptr<SiteInstanceImpl> source_site_instance,
    const GURL& url,
    const absl::optional<url::Origin>& origin,
    const Referrer& referrer,
    const absl::optional<url::Origin>& initiator_origin,
    const absl::optional<GURL>& initiator_base_url,
    const std::vector<GURL>& redirect_chain,
    const blink::PageState& page_state,
    const std::string& method,
    int64_t post_id,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    std::unique_ptr<PolicyContainerPolicies> policy_container_policies,
    bool protect_url_in_navigation_api)
    : frame_unique_name_(frame_unique_name),
      item_sequence_number_(item_sequence_number),
      document_sequence_number_(document_sequence_number),
      navigation_api_key_(navigation_api_key),
      site_instance_(std::move(site_instance)),
      source_site_instance_(std::move(source_site_instance)),
      url_(url),
      committed_origin_(origin),
      referrer_(referrer),
      initiator_origin_(initiator_origin),
      initiator_base_url_(initiator_base_url),
      redirect_chain_(redirect_chain),
      page_state_(page_state),
      bindings_(kInvalidBindings),
      method_(method),
      post_id_(post_id),
      blob_url_loader_factory_(std::move(blob_url_loader_factory)),
      policy_container_policies_(std::move(policy_container_policies)),
      protect_url_in_navigation_api_(protect_url_in_navigation_api) {}

FrameNavigationEntry::~FrameNavigationEntry() = default;

scoped_refptr<FrameNavigationEntry> FrameNavigationEntry::Clone() const {
  // Omit any fields cleared at commit time.
  auto copy = base::MakeRefCounted<FrameNavigationEntry>(
      frame_unique_name_, item_sequence_number_, document_sequence_number_,
      navigation_api_key_, site_instance_, /*source_site_instance=*/nullptr,
      url_, committed_origin_, referrer_, initiator_origin_,
      initiator_base_url_, redirect_chain_, page_state_, method_, post_id_,
      /*blob_url_loader_factory=*/nullptr,
      policy_container_policies_ ? policy_container_policies_->ClonePtr()
                                 : nullptr,
      protect_url_in_navigation_api_);

  // |bindings_| gets only updated through the SetBindings API, so make a copy
  // of it explicitly here as part of cloning.
  copy->bindings_ = bindings_;
  return copy;
}

void FrameNavigationEntry::UpdateEntry(
    const std::string& frame_unique_name,
    int64_t item_sequence_number,
    int64_t document_sequence_number,
    const std::string& navigation_api_key,
    SiteInstanceImpl* site_instance,
    scoped_refptr<SiteInstanceImpl> source_site_instance,
    const GURL& url,
    const absl::optional<url::Origin>& origin,
    const Referrer& referrer,
    const absl::optional<url::Origin>& initiator_origin,
    const absl::optional<GURL>& initiator_base_url,
    const std::vector<GURL>& redirect_chain,
    const blink::PageState& page_state,
    const std::string& method,
    int64_t post_id,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    std::unique_ptr<PolicyContainerPolicies> policy_container_policies,
    bool protect_url_in_navigation_api) {
  frame_unique_name_ = frame_unique_name;
  item_sequence_number_ = item_sequence_number;
  document_sequence_number_ = document_sequence_number;
  navigation_api_key_ = navigation_api_key;
  // TODO(nasko, creis): The SiteInstance of a FrameNavigationEntry should
  // not change once it has been assigned.  See https://crbug.com/849430.
  site_instance_ = site_instance;
  source_site_instance_ = std::move(source_site_instance);
  redirect_chain_ = redirect_chain;
  url_ = url;
  committed_origin_ = origin;
  referrer_ = referrer;
  initiator_origin_ = initiator_origin;
  initiator_base_url_ = initiator_base_url;
  page_state_ = page_state;
  method_ = method;
  post_id_ = post_id;
  blob_url_loader_factory_ = std::move(blob_url_loader_factory);
  policy_container_policies_ = std::move(policy_container_policies);
  protect_url_in_navigation_api_ = protect_url_in_navigation_api;
}

void FrameNavigationEntry::set_item_sequence_number(
    int64_t item_sequence_number) {
  // Once assigned, the item sequence number shouldn't change.
  DCHECK(item_sequence_number_ == -1 ||
         item_sequence_number_ == item_sequence_number);
  item_sequence_number_ = item_sequence_number;
}

void FrameNavigationEntry::set_document_sequence_number(
    int64_t document_sequence_number) {
  // Once assigned, the document sequence number shouldn't change.
  DCHECK(document_sequence_number_ == -1 ||
         document_sequence_number_ == document_sequence_number);
  document_sequence_number_ = document_sequence_number;
}

void FrameNavigationEntry::set_navigation_api_key(
    const std::string& navigation_api_key) {
  // Once assigned, the navigation API key shouldn't change.
  DCHECK(navigation_api_key_.empty() ||
         navigation_api_key_ == navigation_api_key);
  navigation_api_key_ = navigation_api_key;
}

void FrameNavigationEntry::SetPageState(const blink::PageState& page_state) {
  page_state_ = page_state;

  blink::ExplodedPageState exploded_state;
  if (!blink::DecodePageState(page_state_.ToEncodedData(), &exploded_state))
    return;

  item_sequence_number_ = exploded_state.top.item_sequence_number;
  document_sequence_number_ = exploded_state.top.document_sequence_number;
  navigation_api_key_ = base::UTF16ToUTF8(
      exploded_state.top.navigation_api_key.value_or(std::u16string()));
}

void FrameNavigationEntry::SetBindings(int bindings) {
  // Ensure this is set to a valid value, and that it stays the same once set.
  CHECK_NE(bindings, kInvalidBindings);
  CHECK(bindings_ == kInvalidBindings || bindings_ == bindings);
  bindings_ = bindings;
}

scoped_refptr<network::ResourceRequestBody> FrameNavigationEntry::GetPostData(
    std::string* content_type) const {
  if (method_ != "POST")
    return nullptr;

  // Generate the body from the PageState.
  blink::ExplodedPageState exploded_state;
  if (!blink::DecodePageState(page_state_.ToEncodedData(), &exploded_state))
    return nullptr;

  *content_type = base::UTF16ToASCII(
      exploded_state.top.http_body.http_content_type.value_or(
          std::u16string()));
  return exploded_state.top.http_body.request_body;
}

}  // namespace content
