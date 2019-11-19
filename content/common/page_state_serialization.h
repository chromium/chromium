// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PAGE_STATE_SERIALIZATION_H_
#define CONTENT_COMMON_PAGE_STATE_SERIALIZATION_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/referrer_policy.mojom.h"
#include "third_party/blink/public/platform/web_history_scroll_restoration_type.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

constexpr int kMaxScrollAnchorSelectorLength = 500;

struct CONTENT_EXPORT ExplodedHttpBody {
  base::Optional<base::string16> http_content_type;
  scoped_refptr<network::ResourceRequestBody> request_body;
  bool contains_passwords;

  ExplodedHttpBody();
  ~ExplodedHttpBody();
};

struct CONTENT_EXPORT ExplodedFrameState {
  base::Optional<base::string16> url_string;
  base::Optional<base::string16> referrer;
  base::Optional<url::Origin> initiator_origin;
  base::Optional<base::string16> target;
  base::Optional<base::string16> state_object;
  std::vector<base::Optional<base::string16>> document_state;
  blink::WebHistoryScrollRestorationType scroll_restoration_type;
  bool did_save_scroll_or_scale_state;
  gfx::PointF visual_viewport_scroll_offset;
  gfx::Point scroll_offset;
  int64_t item_sequence_number;
  int64_t document_sequence_number;
  double page_scale_factor;
  network::mojom::ReferrerPolicy referrer_policy;
  ExplodedHttpBody http_body;
  base::Optional<base::string16> scroll_anchor_selector;
  gfx::PointF scroll_anchor_offset;
  uint64_t scroll_anchor_simhash;
  std::vector<ExplodedFrameState> children;

  ExplodedFrameState();
  ExplodedFrameState(const ExplodedFrameState& other);
  ~ExplodedFrameState();
  void operator=(const ExplodedFrameState& other);

private:
  void assign(const ExplodedFrameState& other);
};

struct CONTENT_EXPORT ExplodedPageState {
  // TODO(creis, lukasza): Instead of storing them in |referenced_files|,
  // extract referenced files from ExplodedHttpBody.  |referenced_files|
  // currently contains a list from all frames, but cannot be deserialized into
  // the files referenced by each frame.  See http://crbug.com/441966.
  std::vector<base::Optional<base::string16>> referenced_files;
  ExplodedFrameState top;

  ExplodedPageState();
  ~ExplodedPageState();
};

CONTENT_EXPORT bool DecodePageState(const std::string& encoded,
                                    ExplodedPageState* exploded);
// Similar to |DecodePageState()|, but returns an int indicating the original
// version number of the encoded state. Returns -1 on failure.
CONTENT_EXPORT int DecodePageStateForTesting(const std::string& encoded,
                                             ExplodedPageState* exploded);
CONTENT_EXPORT void EncodePageState(const ExplodedPageState& exploded,
                                    std::string* encoded);
CONTENT_EXPORT void LegacyEncodePageStateForTesting(
    const ExplodedPageState& exploded,
    int version,
    std::string* encoded);

#if defined(OS_ANDROID)
CONTENT_EXPORT bool DecodePageStateWithDeviceScaleFactorForTesting(
    const std::string& encoded,
    float device_scale_factor,
    ExplodedPageState* exploded);

// Converts results of EncodeResourceRequestBody (passed in as a pair of |data|
// + |size|) back into a ResourceRequestBody.  Returns nullptr if the
// decoding fails (e.g. if |data| is malformed).
scoped_refptr<network::ResourceRequestBody> DecodeResourceRequestBody(
    const char* data,
    size_t size);

// Encodes |resource_request_body| into |encoded|.
std::string EncodeResourceRequestBody(
    const network::ResourceRequestBody& resource_request_body);
#endif

}  // namespace content

#endif  // CONTENT_COMMON_PAGE_STATE_SERIALIZATION_H_
