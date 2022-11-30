// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/url_request_info_data.h"

#include "ppapi/shared_impl/resource.h"

namespace ppapi {

namespace {

const int32_t kDefaultPrefetchBufferUpperThreshold = 100 * 1000 * 1000;
const int32_t kDefaultPrefetchBufferLowerThreshold = 50 * 1000 * 1000;

}  // namespace

URLRequestInfoData::BodyItem::BodyItem()
    : is_file(false),
      file_ref_pp_resource(0),
      start_offset(0),
      number_of_bytes(-1),
      expected_last_modified_time(0.0) {}

URLRequestInfoData::BodyItem::BodyItem(const std::string& data)
    : is_file(false),
      data(data),
      file_ref_pp_resource(0),
      start_offset(0),
      number_of_bytes(-1),
      expected_last_modified_time(0.0) {}

URLRequestInfoData::BodyItem::BodyItem(Resource* file_ref,
                                       int64_t start_offset,
                                       int64_t number_of_bytes,
                                       PP_Time expected_last_modified_time)
    : is_file(true),
      file_ref_resource(file_ref),
      file_ref_pp_resource(file_ref->pp_resource()),
      start_offset(start_offset),
      number_of_bytes(number_of_bytes),
      expected_last_modified_time(expected_last_modified_time) {}

URLRequestInfoData::URLRequestInfoData()
    : url(),
      method(),
      headers(),
      follow_redirects(true),
      record_download_progress(false),
      record_upload_progress(false),
      has_custom_referrer_url(false),
      custom_referrer_url(),
      allow_cross_origin_requests(false),
      allow_credentials(false),
      has_custom_content_transfer_encoding(false),
      custom_content_transfer_encoding(),
      has_custom_user_agent(false),
      custom_user_agent(),
      prefetch_buffer_upper_threshold(kDefaultPrefetchBufferUpperThreshold),
      prefetch_buffer_lower_threshold(kDefaultPrefetchBufferLowerThreshold),
      body() {}

URLRequestInfoData::~URLRequestInfoData() {}

}  // namespace ppapi
