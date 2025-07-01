// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_URL_REQUEST_INFO_DATA_H_
#define PPAPI_SHARED_IMPL_URL_REQUEST_INFO_DATA_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace ppapi {

class Resource;

struct PPAPI_SHARED_EXPORT URLRequestInfoData {
  struct PPAPI_SHARED_EXPORT BodyItem {
    BodyItem();
    explicit BodyItem(const std::string& data);
    BodyItem(Resource* file_ref,
             int64_t start_offset,
             int64_t number_of_bytes,
             PP_Time expected_last_modified_time);

    // Set if the input is a file, false means the |data| is valid.
    bool is_file;

    std::string data;

    // Only set on the plugin-side, for refcounting purposes. Only valid when
    // |is_file| is set.
    scoped_refptr<Resource> file_ref_resource;
    // This struct holds no ref to this resource. Only valid when |is_file| is
    // set.
    PP_Resource file_ref_pp_resource;

    int64_t start_offset;
    int64_t number_of_bytes;
    PP_Time expected_last_modified_time;

    // If you add more stuff here, be sure to modify the serialization rules in
    // ppapi_messages.h
  };

  URLRequestInfoData();
  ~URLRequestInfoData();

  std::string url;
  std::string method;
  std::string headers;

  bool follow_redirects;
  bool record_download_progress;
  bool record_upload_progress;

  // |has_custom_referrer_url| is set to false if a custom referrer hasn't been
  // set (or has been set to an Undefined Var) and the default referrer should
  // be used. (Setting the custom referrer to an empty string indicates that no
  // referrer header should be generated.)
  bool has_custom_referrer_url;
  std::string custom_referrer_url;

  bool allow_cross_origin_requests;
  bool allow_credentials;

  // Similar to the custom referrer (above), but for custom content transfer
  // encoding and custom user agent, respectively.
  bool has_custom_content_transfer_encoding;
  std::string custom_content_transfer_encoding;
  bool has_custom_user_agent;
  std::string custom_user_agent;

  int32_t prefetch_buffer_upper_threshold;
  int32_t prefetch_buffer_lower_threshold;

  std::vector<BodyItem> body;

  // If you add more stuff here, be sure to modify the serialization rules in
  // ppapi_messages.h
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_URL_REQUEST_INFO_DATA_H_
