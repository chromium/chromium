// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_DETAILS_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_DETAILS_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/values.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/common/extension_id.h"
#include "url/origin.h"

namespace net {
class AuthChallengeInfo;
class HttpRequestHeaders;
class HttpResponseHeaders;
}  // namespace net

namespace extensions {

class PermissionHelper;
struct WebRequestInfo;

// This helper class is used to construct the details for a webRequest event
// dictionary. Some keys are present on every event, others are only relevant
// for a few events. And some keys must only be added to the event details if
// requested at the registration of the event listener (in ExtraInfoSpec).
// This class provides setters that are aware of these rules.
//
// Not all keys are managed by this class. Keys that do not require a special
// treatment can be set using the generic SetBoolean / SetInteger / SetString
// methods (e.g. to set "error", "message", "redirectUrl", "stage" or "tabId").
//
// This class should be constructed on the IO thread. It can safely be used on
// other threads, as long as there is no concurrent access.
class WebRequestEventDetails {
 public:
  using DeterminedFrameDataCallback =
      base::Callback<void(std::unique_ptr<WebRequestEventDetails>)>;

  // Create a WebRequestEventDetails with the following keys:
  // - method
  // - requestId
  // - tabId
  // - timeStamp
  // - type
  // - url
  WebRequestEventDetails(const WebRequestInfo& request, int extra_info_spec);
  ~WebRequestEventDetails();

  // Sets the following key:
  // - requestBody (on demand)
  // Takes ownership of |request_body_data| in |*request|.
  void SetRequestBody(WebRequestInfo* request);

  // Sets the following key:
  // - requestHeaders (on demand)
  void SetRequestHeaders(const net::HttpRequestHeaders& request_headers);

  // Sets the following keys:
  // - challenger
  // - isProxy
  // - realm
  // - scheme
  void SetAuthInfo(const net::AuthChallengeInfo& auth_info);

  // Sets the following keys:
  // - responseHeaders (on demand)
  // - statusCode
  // - statusLine
  void SetResponseHeaders(const WebRequestInfo& request,
                          const net::HttpResponseHeaders* response_headers);

  // Sets the following key:
  // - fromCache
  // - ip
  void SetResponseSource(const WebRequestInfo& request);

  void SetBoolean(const std::string& key, bool value) {
    dict_.SetBoolean(key, value);
  }

  void SetInteger(const std::string& key, int value) {
    dict_.SetInteger(key, value);
  }

  void SetString(const std::string& key, const std::string& value) {
    dict_.SetString(key, value);
  }

  // Create an event dictionary that contains all required keys, and also the
  // extra keys as specified by the |extra_info_spec| filter. If the listener
  // this event will be dispatched to doesn't have permission for the initiator
  // then the initiator will not be populated.
  // This can be called from any thread.
  std::unique_ptr<base::DictionaryValue> GetFilteredDict(
      int extra_info_spec,
      PermissionHelper* permission_helper,
      const ExtensionId& extension_id,
      bool crosses_incognito) const;

  // Get the internal dictionary, unfiltered. After this call, the internal
  // dictionary is empty.
  std::unique_ptr<base::DictionaryValue> GetAndClearDict();

  // Returns a filtered copy with only whitelisted data for public session.
  std::unique_ptr<WebRequestEventDetails> CreatePublicSessionCopy();

 private:
  FRIEND_TEST_ALL_PREFIXES(
      WebRequestEventDetailsTest, WhitelistedCopyForPublicSession);

  // Empty constructor used in unittests.
  WebRequestEventDetails();

  // The details that are always included in a webRequest event object.
  base::DictionaryValue dict_;

  // Extra event details: Only included when |extra_info_spec_| matches.
  std::unique_ptr<base::DictionaryValue> request_body_;
  std::unique_ptr<base::ListValue> request_headers_;
  std::unique_ptr<base::ListValue> response_headers_;
  base::Optional<url::Origin> initiator_;

  int extra_info_spec_;

  // Used to determine the tabId, frameId and parentFrameId.
  int render_process_id_;
  int render_frame_id_;

  DISALLOW_COPY_AND_ASSIGN(WebRequestEventDetails);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_DETAILS_H_
