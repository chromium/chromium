// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_MEDIA_LOG_SERVICE_H_
#define MEDIA_MOJO_CLIENTS_MOJO_MEDIA_LOG_SERVICE_H_

#include <stdint.h>

#include "base/macros.h"
#include "media/base/media_log.h"
#include "media/mojo/mojom/media_log.mojom.h"

namespace media {

// Implementation of a mojom::MediaLog service which wraps a media::MediaLog.
class MojoMediaLogService : public mojom::MediaLog {
 public:
  explicit MojoMediaLogService(media::MediaLog* media_log);
  ~MojoMediaLogService() final;

  // mojom::MediaLog implementation
  void AddEvent(const media::MediaLogEvent& event) final;

 private:
  media::MediaLog* media_log_;

  DISALLOW_COPY_AND_ASSIGN(MojoMediaLogService);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_MEDIA_LOG_SERVICE_H_
