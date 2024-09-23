// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_DETAILS_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_DETAILS_H_

#include <memory>
#include <optional>
#include <string>

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
  // Create a WebRequestEventDetails with the following keys:
  // - method
  // - requestId
  // - tabId
  // - timeStamp
  // - type
  // - url
  WebRequestEventDetails(const WebRequestInfo& request, int extra_info_spec);

  WebRequestEventDetails(const WebRequestEventDetails&) = delete;
  WebRequestEventDetails& operator=(const WebRequestEventDetails&) = delete;

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

  void SetBoolean(const std::string& key, bool value) { dict_.Set(key, value); }

  void SetInteger(const std::string& key, int value) { dict_.Set(key, value); }

  void SetString(const std::string& key, const std::string& value) {
    dict_.Set(key, value);
  }

  // Create an event dictionary that contains all required keys, and also the
  // extra keys as specified by the |extra_info_spec| filter. If the listener
  // this event will be dispatched to doesn't have permission for the initiator
  // then the initiator will not be populated.
  // This can be called from any thread.
  base::Value::Dict GetFilteredDict(int extra_info_spec,
                                    PermissionHelper* permission_helper,
                                    const ExtensionId& extension_id,
                                    bool crosses_incognito) const;

  // Get the internal dictionary, unfiltered. After this call, the internal
  // dictionary is empty.
  base::Value::Dict GetAndClearDict();

 private:
  // The details that are always included in a webRequest event object.
  base::Value::Dict dict_;

  // Extra event details: Only included when |extra_info_spec_| matches.
  std::optional<base::Value::Dict> request_body_;
  std::optional<base::Value::List> request_headers_;
  std::optional<base::Value::List> response_headers_;
  std::optional<url::Origin> initiator_;

  int extra_info_spec_;

  int render_process_id_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_EVENT_DETAILS_H_
