// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/frame_navigation_entry.h"

#include <utility>

#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/page_state_serialization.h"

namespace content {

FrameNavigationEntry::FrameNavigationEntry()
    : item_sequence_number_(-1), document_sequence_number_(-1), post_id_(-1) {}

FrameNavigationEntry::FrameNavigationEntry(
    const std::string& frame_unique_name,
    int64_t item_sequence_number,
    int64_t document_sequence_number,
    scoped_refptr<SiteInstanceImpl> site_instance,
    scoped_refptr<SiteInstanceImpl> source_site_instance,
    const GURL& url,
    const url::Origin* origin,
    const Referrer& referrer,
    const base::Optional<url::Origin>& initiator_origin,
    const std::vector<GURL>& redirect_chain,
    const PageState& page_state,
    const std::string& method,
    int64_t post_id,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory)
    : frame_unique_name_(frame_unique_name),
      item_sequence_number_(item_sequence_number),
      document_sequence_number_(document_sequence_number),
      site_instance_(std::move(site_instance)),
      source_site_instance_(std::move(source_site_instance)),
      url_(url),
      referrer_(referrer),
      initiator_origin_(initiator_origin),
      redirect_chain_(redirect_chain),
      page_state_(page_state),
      method_(method),
      post_id_(post_id),
      blob_url_loader_factory_(std::move(blob_url_loader_factory)) {
  if (origin)
    committed_origin_ = *origin;
}

FrameNavigationEntry::~FrameNavigationEntry() {}

scoped_refptr<FrameNavigationEntry> FrameNavigationEntry::Clone() const {
  auto copy = base::MakeRefCounted<FrameNavigationEntry>();

  // Omit any fields cleared at commit time.
  copy->UpdateEntry(frame_unique_name_, item_sequence_number_,
                    document_sequence_number_, site_instance_.get(), nullptr,
                    url_, committed_origin_, referrer_, initiator_origin_,
                    redirect_chain_, page_state_, method_, post_id_,
                    nullptr /* blob_url_loader_factory */);
  return copy;
}

void FrameNavigationEntry::UpdateEntry(
    const std::string& frame_unique_name,
    int64_t item_sequence_number,
    int64_t document_sequence_number,
    SiteInstanceImpl* site_instance,
    scoped_refptr<SiteInstanceImpl> source_site_instance,
    const GURL& url,
    const base::Optional<url::Origin>& origin,
    const Referrer& referrer,
    const base::Optional<url::Origin>& initiator_origin,
    const std::vector<GURL>& redirect_chain,
    const PageState& page_state,
    const std::string& method,
    int64_t post_id,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  frame_unique_name_ = frame_unique_name;
  item_sequence_number_ = item_sequence_number;
  document_sequence_number_ = document_sequence_number;
  site_instance_ = site_instance;
  source_site_instance_ = std::move(source_site_instance);
  redirect_chain_ = redirect_chain;
  url_ = url;
  committed_origin_ = origin;
  referrer_ = referrer;
  initiator_origin_ = initiator_origin;
  page_state_ = page_state;
  method_ = method;
  post_id_ = post_id;
  blob_url_loader_factory_ = std::move(blob_url_loader_factory);
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

void FrameNavigationEntry::SetPageState(const PageState& page_state) {
  page_state_ = page_state;

  ExplodedPageState exploded_state;
  if (!DecodePageState(page_state_.ToEncodedData(), &exploded_state))
    return;

  item_sequence_number_ = exploded_state.top.item_sequence_number;
  document_sequence_number_ = exploded_state.top.document_sequence_number;
}

scoped_refptr<network::ResourceRequestBody> FrameNavigationEntry::GetPostData(
    std::string* content_type) const {
  if (method_ != "POST")
    return nullptr;

  // Generate the body from the PageState.
  ExplodedPageState exploded_state;
  if (!DecodePageState(page_state_.ToEncodedData(), &exploded_state))
    return nullptr;

  *content_type = base::UTF16ToASCII(
      exploded_state.top.http_body.http_content_type.value_or(
          base::string16()));
  return exploded_state.top.http_body.request_body;
}

}  // namespace content
