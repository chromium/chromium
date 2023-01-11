// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_task.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "device/fido/fido_constants.h"

namespace device {

FidoTask::FidoTask(FidoDevice* device) : device_(device) {
  DCHECK(device_);
  DCHECK(device_->SupportedProtocolIsInitialized());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&FidoTask::StartTask, weak_factory_.GetWeakPtr()));
}

FidoTask::~FidoTask() = default;

}  // namespace device
