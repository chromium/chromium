// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/connection_group.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace mojo {

// static
ConnectionGroupRef ConnectionGroup::Create(
    base::RepeatingClosure callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return ConnectionGroupRef(base::WrapRefCounted(
      new ConnectionGroup(std::move(callback), std::move(task_runner))));
}

ConnectionGroup::ConnectionGroup(
    base::RepeatingClosure callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : notification_callback_(std::move(callback)),
      notification_task_runner_(std::move(task_runner)) {}

ConnectionGroup::~ConnectionGroup() {
  // Just a sanity check. This ref-count should always be adjusted before the
  // internal RefCountedThreadSafe ref-count, so we should never hit the
  // destructor with a non-zero |num_refs_|.
  DCHECK_EQ(num_refs_, 0u);
}

void ConnectionGroup::AddGroupRef() {
  ++num_refs_;
}

void ConnectionGroup::ReleaseGroupRef() {
  DCHECK_GT(num_refs_, 0u);
  --num_refs_;
  if (num_refs_ == 0 && notification_task_runner_) {
    notification_task_runner_->PostTask(FROM_HERE,
                                        base::BindOnce(notification_callback_));
  }
}

void ConnectionGroup::SetParentGroup(ConnectionGroupRef parent_group) {
  DCHECK(notification_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!parent_group_);
  parent_group_ = std::move(parent_group);
}

}  // namespace mojo
