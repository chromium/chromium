// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/api/display_source/wifi_display/wifi_display_media_encoder.h"

#include "base/check_op.h"
#include "media/base/bind_to_current_loop.h"

namespace extensions {

WiFiDisplayEncodedUnit::WiFiDisplayEncodedUnit(
    std::string data,
    base::TimeTicks reference_timestamp,
    bool key_frame)
    : data(std::move(data)), pts(reference_timestamp), key_frame(key_frame) {}

WiFiDisplayEncodedUnit::WiFiDisplayEncodedUnit(
    std::string data,
    base::TimeTicks reference_timestamp,
    base::TimeTicks encode_timestamp,
    bool key_frame)
    : WiFiDisplayEncodedUnit(std::move(data), reference_timestamp, key_frame) {
  if (encode_timestamp >= reference_timestamp) {
    dts = reference_timestamp - (encode_timestamp - reference_timestamp);
    DCHECK_LE(dts, pts);
  }
}

WiFiDisplayMediaEncoder::WiFiDisplayMediaEncoder() = default;
WiFiDisplayMediaEncoder::~WiFiDisplayMediaEncoder() = default;

void WiFiDisplayMediaEncoder::SetCallbacks(EncodedUnitCallback encoded_callback,
                                           base::OnceClosure error_callback) {
  DCHECK(client_thread_checker_.CalledOnValidThread());
  // This is not thread-safe if encoding has been started thus allow
  // this to be called only once.
  DCHECK(!encoded_callback_ && !error_callback_);
  encoded_callback_ = media::BindToCurrentLoop(std::move(encoded_callback));
  error_callback_ = media::BindToCurrentLoop(std::move(error_callback));
}

}  // namespace extensions
