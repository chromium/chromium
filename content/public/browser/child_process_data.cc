// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/child_process_data.h"

namespace content {

ChildProcessData::ChildProcessData(int process_type, ChildProcessId id)
    : process_type(process_type),
      // TODO(crbug.com/379869738): Remove once id references are deleted
      id(id.GetUnsafeValue()),
      child_process_id_(id) {}

ChildProcessData::ChildProcessData(ChildProcessData&& rhs) = default;

ChildProcessData::~ChildProcessData() {}

const ChildProcessId& ChildProcessData::GetChildProcessId() const {
  // To reduce the number of changes necessary as APIs switch,
  // id must be accessible to outside parties.
  CHECK_EQ(child_process_id_, ChildProcessId(id));
  return child_process_id_;
}

}  // namespace content
