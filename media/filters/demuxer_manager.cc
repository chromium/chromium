// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/demuxer_manager.h"

namespace media {

DemuxerManager::DemuxerManager(Client* client) : client_(client) {
  DCHECK(client_);
}

DemuxerManager::~DemuxerManager() {}

}  // namespace media
