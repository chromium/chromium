// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_log.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"

namespace media {

MojoMediaLog::MojoMediaLog(
    mojo::PendingRemote<mojom::MediaLog> remote_media_log,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : remote_media_log_(std::move(remote_media_log)),
      task_runner_(std::move(task_runner)) {
  DVLOG(1) << __func__;
}

MojoMediaLog::~MojoMediaLog() {
  DVLOG(1) << __func__;
  // Note that we're not invalidating the remote side.  We're only invalidating
  // anything that was cloned from us.  Effectively, we're a log that just
  // happens to operate via mojo.
  InvalidateLog();
}

void MojoMediaLog::AddLogRecordLocked(std::unique_ptr<MediaLogRecord> event) {
  DVLOG(2) << __func__;
  DCHECK(event);

  // Don't post unless we need to.  Otherwise, we can order a log entry after
  // our own destruction.  While safe, this loses the message.  This can happen,
  // for example, when we're logging why a VideoDecoder failed to initialize.
  // It will be destroyed synchronously when Initialize returns.
  //
  // Also, we post here, so this is the base case.  :)
  if (task_runner_->RunsTasksInCurrentSequence()) {
    remote_media_log_->AddLogRecord(*event);
    return;
  }

  // From other threads, we have little choice.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&MojoMediaLog::AddLogRecord,
                     weak_ptr_factory_.GetWeakPtr(), std::move(event)));
}

}  // namespace media
