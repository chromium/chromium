// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_MEDIA_LOG_H_
#define MEDIA_MOJO_SERVICES_MOJO_MEDIA_LOG_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/media_log.h"
#include "media/mojo/mojom/media_log.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

// Client side for a MediaLog via mojo.
class MojoMediaLog final : public MediaLog {
 public:
  MojoMediaLog(mojo::PendingRemote<mojom::MediaLog> remote_media_log,
               scoped_refptr<base::SequencedTaskRunner> task_runner);
  MojoMediaLog(const MojoMediaLog&) = delete;
  MojoMediaLog& operator=(const MojoMediaLog&) = delete;
  ~MojoMediaLog() final;

 protected:
  // MediaLog implementation.  May be called from any thread, but will only
  // use |remote_media_log_| on |task_runner_|.
  void AddLogRecordLocked(std::unique_ptr<MediaLogRecord> event) override;

 private:
  mojo::Remote<mojom::MediaLog> remote_media_log_;

  // The mojo service thread on which we'll access |remote_media_log_|.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<MojoMediaLog> weak_ptr_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_MEDIA_LOG_H_
