// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/child_process_data.h"

namespace content {

ChildProcessData::ChildProcessData(int process_type, ChildProcessId id)
    : process_type(process_type),
      child_process_id_(id) {}

ChildProcessData::ChildProcessData(const ChildProcessData&) = default;
ChildProcessData& ChildProcessData::operator=(const ChildProcessData&) =
    default;

ChildProcessData::ChildProcessData(ChildProcessData&&) = default;
ChildProcessData& ChildProcessData::operator=(ChildProcessData&&) = default;

ChildProcessData::~ChildProcessData() {}

const ChildProcessId& ChildProcessData::GetChildProcessId() const {
  return child_process_id_;
}

}  // namespace content
