// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_drm_bridge_delegate.h"

#include "base/check.h"

namespace media {

MediaDrmBridgeDelegate::MediaDrmBridgeDelegate() {
}

MediaDrmBridgeDelegate::~MediaDrmBridgeDelegate() {
}

bool MediaDrmBridgeDelegate::OnCreateSession(
    const EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::vector<uint8_t>* init_data_out,
    std::vector<std::string>* optional_parameters_out) {
  DCHECK(init_data_out->empty());
  DCHECK(optional_parameters_out->empty());
  return true;
}

}  // namespace media
