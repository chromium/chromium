// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_MEDIA_LOG_SERVICE_H_
#define MEDIA_MOJO_CLIENTS_MOJO_MEDIA_LOG_SERVICE_H_

#include <memory>

#include "media/base/media_log.h"
#include "media/mojo/mojom/media_log.mojom.h"

namespace media {

// Implementation of a mojom::MediaLog service which wraps a media::MediaLog.
class MojoMediaLogService final : public mojom::MediaLog {
 public:
  explicit MojoMediaLogService(std::unique_ptr<media::MediaLog> media_log);
  MojoMediaLogService(const MojoMediaLogService&) = delete;
  MojoMediaLogService& operator=(const MojoMediaLogService&) = delete;
  ~MojoMediaLogService() final;

  // mojom::MediaLog implementation
  void AddLogRecord(const MediaLogRecord& event) final;

 private:
  // `media::` is needed to be distinguished from `mojom::MediaLog`.
  std::unique_ptr<media::MediaLog> media_log_;
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_MEDIA_LOG_SERVICE_H_
