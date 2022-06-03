// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/data_source.h"


namespace media {

DataSource::DataSource() = default;

DataSource::~DataSource() = default;

bool DataSource::AssumeFullyBuffered() const {
  return true;
}

int64_t DataSource::GetMemoryUsage() {
  int64_t temp;
  return GetSize(&temp) ? temp : 0;
}

}  // namespace media
