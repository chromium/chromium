// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/connection_group.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace mojo {

ConnectionGroup::Ref::Ref() = default;

ConnectionGroup::Ref::Ref(const Ref& other) {
  *this = other;
}

ConnectionGroup::Ref::Ref(Ref&& other) noexcept {
  *this = std::move(other);
}

ConnectionGroup::Ref::~Ref() {
  reset();
}

ConnectionGroup::Ref& ConnectionGroup::Ref::operator=(const Ref& other) {
  reset();
  type_ = Type::kStrong;
  group_ = other.group_;
  group_->AddGroupRef();
  return *this;
}

ConnectionGroup::Ref& ConnectionGroup::Ref::operator=(Ref&& other) noexcept {
  reset();
  type_ = other.type_;
  group_.swap(other.group_);
  return *this;
}

void ConnectionGroup::Ref::reset() {
  if (type_ == Type::kStrong && group_)
    group_->ReleaseGroupRef();
  type_ = Type::kWeak;
  group_.reset();
}

ConnectionGroup::Ref ConnectionGroup::Ref::WeakCopy() const {
  DCHECK(group_->notification_task_runner_->RunsTasksInCurrentSequence());
  return Ref(group_);
}

bool ConnectionGroup::Ref::HasZeroRefs() const {
  DCHECK(group_->notification_task_runner_->RunsTasksInCurrentSequence());
  return group_->num_refs_ == 0;
}

void ConnectionGroup::Ref::SetParentGroup(Ref parent_group) {
  group_->notification_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ConnectionGroup::SetParentGroup, group_,
                                std::move(parent_group)));
}

ConnectionGroup::Ref::Ref(scoped_refptr<ConnectionGroup> group)
    : group_(std::move(group)) {}

// static
ConnectionGroup::Ref ConnectionGroup::Create(
    base::RepeatingClosure callback,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return Ref(base::WrapRefCounted(
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

void ConnectionGroup::SetParentGroup(Ref parent_group) {
  DCHECK(notification_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!parent_group_);
  parent_group_ = std::move(parent_group);
}

}  // namespace mojo
