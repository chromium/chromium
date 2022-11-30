// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_package_serialization_handler.h"

#include <utility>

namespace content {

SavePackageSerializationHandler::SavePackageSerializationHandler(
    const DidReceiveDataCallback& did_serialize_data_callback,
    DoneCallback done_callback)
    : did_serialize_data_callback_(did_serialize_data_callback),
      done_callback_(std::move(done_callback)) {}

SavePackageSerializationHandler::~SavePackageSerializationHandler() = default;

void SavePackageSerializationHandler::DidReceiveData(
    const std::string& data_buffer) {
  // This callback should always have been set.
  DCHECK(did_serialize_data_callback_);
  did_serialize_data_callback_.Run(data_buffer);
}

void SavePackageSerializationHandler::Done() {
  // This callback should always have been set and only called once.
  DCHECK(done_callback_);
  std::move(done_callback_).Run();
}

}  // namespace content
