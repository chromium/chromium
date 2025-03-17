// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/scheduled_file.h"

#import <vector>

#import "base/files/file_path.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"
#import "ios/chrome/browser/download/model/auto_deletion/proto/scheduled_file.pb.h"

namespace auto_deletion {

ScheduledFile::ScheduledFile(const base::FilePath& filepath,
                             const std::string& hash,
                             base::Time download_time)
    : filepath_(filepath), hash_(hash), download_time_(download_time) {}
ScheduledFile::ScheduledFile(const ScheduledFile&) = default;
ScheduledFile::ScheduledFile(ScheduledFile&&) = default;
ScheduledFile& ScheduledFile::operator=(const ScheduledFile&) = default;
ScheduledFile& ScheduledFile::operator=(ScheduledFile&&) = default;
ScheduledFile::~ScheduledFile() = default;

auto_deletion::proto::ScheduledFile ScheduledFile::Serialize() const {
  auto_deletion::proto::ScheduledFile proto;
  proto.set_path(filepath_.AsUTF8Unsafe());
  proto.set_hash(hash_);
  proto.set_download_timestamp(
      download_time_.ToDeltaSinceWindowsEpoch().InMicroseconds());

  return proto;
}

std::optional<ScheduledFile> ScheduledFile::Deserialize(
    const auto_deletion::proto::ScheduledFile& proto) {
  if (proto.path().empty() || proto.hash().empty()) {
    return std::nullopt;
  }

  base::FilePath path = base::FilePath::FromUTF8Unsafe(proto.path());
  base::Time timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(proto.download_timestamp()));
  return ScheduledFile(path, proto.hash(), timestamp);
}

bool operator==(const ScheduledFile& lhs, const ScheduledFile& rhs) {
  return lhs.filepath() == rhs.filepath() && lhs.hash() == rhs.hash() &&
         lhs.download_time() == rhs.download_time();
}

bool operator!=(const ScheduledFile& lhs, const ScheduledFile& rhs) {
  return lhs.filepath() != rhs.filepath() || lhs.hash() != rhs.hash() ||
         lhs.download_time() != rhs.download_time();
}
}  // namespace auto_deletion
