// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/connection_group_ref.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/connection_group.h"

namespace mojo {

ConnectionGroupRef::ConnectionGroupRef() = default;

ConnectionGroupRef::ConnectionGroupRef(const ConnectionGroupRef& other) {
  *this = other;
}

ConnectionGroupRef::ConnectionGroupRef(ConnectionGroupRef&& other) noexcept {
  *this = std::move(other);
}

ConnectionGroupRef::~ConnectionGroupRef() {
  reset();
}

ConnectionGroupRef& ConnectionGroupRef::operator=(
    const ConnectionGroupRef& other) {
  reset();
  type_ = Type::kStrong;
  group_ = other.group_;
  group_->AddGroupRef();
  return *this;
}

ConnectionGroupRef& ConnectionGroupRef::operator=(
    ConnectionGroupRef&& other) noexcept {
  reset();
  type_ = other.type_;
  group_.swap(other.group_);
  return *this;
}

void ConnectionGroupRef::reset() {
  if (type_ == Type::kStrong && group_) {
    group_->ReleaseGroupRef();
  }
  type_ = Type::kWeak;
  group_.reset();
}

ConnectionGroupRef ConnectionGroupRef::WeakCopy() const {
  DCHECK(group_->notification_task_runner_->RunsTasksInCurrentSequence());
  return ConnectionGroupRef(group_);
}

bool ConnectionGroupRef::HasZeroRefs() const {
  DCHECK(group_->notification_task_runner_->RunsTasksInCurrentSequence());
  return group_->num_refs_ == 0;
}

void ConnectionGroupRef::SetParentGroup(ConnectionGroupRef parent_group) {
  group_->notification_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ConnectionGroup::SetParentGroup, group_,
                                std::move(parent_group)));
}

ConnectionGroupRef::ConnectionGroupRef(scoped_refptr<ConnectionGroup> group)
    : group_(std::move(group)) {}

}  // namespace mojo
