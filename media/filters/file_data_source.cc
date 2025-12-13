// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/file_data_source.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/callback.h"

namespace media {

FileDataSource::FileDataSource()
    : force_read_errors_(false),
      force_streaming_(false),
      bytes_read_(0) {
}

bool FileDataSource::Initialize(const base::FilePath& file_path) {
  DCHECK(!file_.IsValid());
  return file_.Initialize(file_path);
}

bool FileDataSource::Initialize(base::File file) {
  DCHECK(!file_.IsValid());
  return file_.Initialize(std::move(file));
}

void FileDataSource::Stop() {}

void FileDataSource::Abort() {}

void FileDataSource::Read(int64_t position,
                          base::span<uint8_t> data,
                          DataSource::ReadCB read_cb) {
  if (force_read_errors_ || !file_.IsValid()) {
    std::move(read_cb).Run(kReadError);
    return;
  }
  CHECK_GE(position, 0);

  auto file_data = file_.bytes();

  // Cap position and size within bounds.
  auto file_data_range = file_data.subspan(
      std::min(base::saturated_cast<size_t>(position), file_data.size()));
  const size_t clamped_size = std::min(data.size(), file_data_range.size());

  data.copy_prefix_from(file_data_range.first(clamped_size));
  bytes_read_ += clamped_size;
  std::move(read_cb).Run(clamped_size);
}

bool FileDataSource::GetSize(int64_t* size_out) {
  *size_out = file_.length();
  return true;
}

bool FileDataSource::IsStreaming() {
  return force_streaming_;
}

void FileDataSource::SetBitrate(int bitrate) {}

FileDataSource::~FileDataSource() = default;

bool FileDataSource::PassedTimingAllowOriginCheck() {
  // There are no HTTP responses, so this can safely return true.
  return true;
}

bool FileDataSource::WouldTaintOrigin() {
  // There are no HTTP responses, so this can safely return false.
  return false;
}

}  // namespace media
