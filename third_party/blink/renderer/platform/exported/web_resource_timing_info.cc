// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_resource_timing_info.h"

#include "third_party/blink/public/platform/web_url_load_timing.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

bool IsSameServerTimingInfo(const WebVector<WebServerTimingInfo>& lhs,
                            const WebVector<WebServerTimingInfo>& rhs) {
  if (lhs.size() != rhs.size())
    return false;
  for (size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i] != rhs[i])
      return false;
  }
  return true;
}

}  // namespace

bool WebServerTimingInfo::operator==(const WebServerTimingInfo& other) const {
  return name == other.name && duration == other.duration &&
         description == other.description;
}

bool WebServerTimingInfo::operator!=(const WebServerTimingInfo& other) const {
  return !(*this == other);
}

bool WebResourceTimingInfo::operator==(
    const WebResourceTimingInfo& other) const {
  return name == other.name && start_time == other.start_time &&
         alpn_negotiated_protocol == other.alpn_negotiated_protocol &&
         connection_info == other.connection_info && timing == other.timing &&
         last_redirect_end_time == other.last_redirect_end_time &&
         response_end == other.response_end &&
         transfer_size == other.transfer_size &&
         encoded_body_size == other.encoded_body_size &&
         decoded_body_size == other.decoded_body_size &&
         did_reuse_connection == other.did_reuse_connection &&
         is_secure_context == other.is_secure_context &&
         allow_timing_details == other.allow_timing_details &&
         allow_redirect_details == other.allow_redirect_details &&
         allow_negative_values == other.allow_negative_values &&
         IsSameServerTimingInfo(server_timing, other.server_timing);
}

}  // namespace blink

namespace WTF {
#if INSIDE_BLINK
CrossThreadCopier<blink::WebResourceTimingInfo>::Type
CrossThreadCopier<blink::WebResourceTimingInfo>::Copy(
    const blink::WebResourceTimingInfo& info) {
  blink::WebResourceTimingInfo copy;

  copy.name = String(info.name).IsolatedCopy();
  copy.start_time = info.start_time;

  copy.alpn_negotiated_protocol =
      String(info.alpn_negotiated_protocol).IsolatedCopy();
  copy.connection_info = String(info.connection_info).IsolatedCopy();

  if (!info.timing.IsNull())
    copy.timing = CrossThreadCopier<blink::WebURLLoadTiming>::Copy(info.timing);

  copy.last_redirect_end_time = info.last_redirect_end_time;
  copy.response_end = info.response_end;

  copy.transfer_size = info.transfer_size;
  copy.encoded_body_size = info.encoded_body_size;
  copy.decoded_body_size = info.decoded_body_size;

  copy.did_reuse_connection = info.did_reuse_connection;
  copy.is_secure_context = info.is_secure_context;

  copy.allow_timing_details = info.allow_timing_details;
  copy.allow_redirect_details = info.allow_redirect_details;

  copy.allow_negative_values = info.allow_negative_values;
  for (auto& entry : info.server_timing) {
    blink::WebServerTimingInfo entry_copy(
        String(entry.name).IsolatedCopy(), entry.duration,
        String(entry.description).IsolatedCopy());
    copy.server_timing.emplace_back(std::move(entry_copy));
  }

  return copy;
}
#endif
}  // namespace WTF
