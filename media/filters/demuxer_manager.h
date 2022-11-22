// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DEMUXER_MANAGER_H_
#define MEDIA_FILTERS_DEMUXER_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/media_export.h"

namespace media {

// This class manages both an implementation of media::Demuxer and of
// media::DataSource. DataSource, in particular may be null, since both MSE
// playback and Android's MediaPlayerRenderer do not make use of it. In the
// case that DataSource is present, these objects should have a similar
// lifetime, and both must be destroyed on the media thread, so owning them
// together makes sense. Additionally, the demuxer or data source can change
// during the lifetime of the player that owns them, so encapsulating that
// change logic separately lets the media player impl (WMPI) be a bit simpler,
// and dedicate a higher percentage of it's complexity to managing playback
// state.
class MEDIA_EXPORT DemuxerManager {
 public:
  class Client {
    // TODO(crbug/1377053): To be implemented in a future CL.
  };

  explicit DemuxerManager(Client* client);
  ~DemuxerManager();

 private:
  // This is usually just the WebMediPlayerImpl.
  raw_ptr<Client> client_;

  // Weak pointer implementation.
  base::WeakPtrFactory<DemuxerManager> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_FILTERS_DEMUXER_MANAGER_H_
