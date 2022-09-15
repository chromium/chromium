// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/load_info_util.h"

#include "net/base/load_states.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"

namespace network {

bool LoadInfoIsMoreInteresting(const mojom::LoadInfo& a,
                               const mojom::LoadInfo& b) {
  // Set |*_uploading_size| to be the size of the corresponding upload body if
  // it's currently being uploaded.

  uint64_t a_uploading_size = 0;
  if (a.load_state == net::LOAD_STATE_SENDING_REQUEST)
    a_uploading_size = a.upload_size;

  uint64_t b_uploading_size = 0;
  if (b.load_state == net::LOAD_STATE_SENDING_REQUEST)
    b_uploading_size = b.upload_size;

  if (a_uploading_size != b_uploading_size)
    return a_uploading_size > b_uploading_size;

  return a.load_state > b.load_state;
}

}  // namespace network
