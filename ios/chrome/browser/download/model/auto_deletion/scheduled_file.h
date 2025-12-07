// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_SCHEDULED_FILE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_SCHEDULED_FILE_H_

#import <memory>
#import <string>

#import "base/files/file_path.h"
#import "base/memory/raw_ref.h"
#import "base/time/time.h"

namespace auto_deletion::proto {
class ScheduledFile;
}  // namespace auto_deletion::proto
namespace base {
class FilePath;
}  // namespace base

namespace auto_deletion {

// The class represents a file downloaded to the device that has been scheduled
// for deletion.
class ScheduledFile {
 public:
  ScheduledFile(const base::FilePath& filepath,
                const std::string& hash,
                base::Time download_time);
  ScheduledFile(const ScheduledFile& file);
  ScheduledFile& operator=(const ScheduledFile& file);
  ScheduledFile(ScheduledFile&&);
  ScheduledFile& operator=(ScheduledFile&&);
  ~ScheduledFile();

  // Converts the ScheduledFile object into a ScheduledFile proto.
  auto_deletion::proto::ScheduledFile Serialize() const;

  // Converts the serialized string into a ScheduledFile object.
  static std::optional<ScheduledFile> Deserialize(
      const auto_deletion::proto::ScheduledFile& proto);

  // Returns the filepath of the file.
  const base::FilePath& filepath() const { return filepath_; }
  // Returns the hash of the file.
  const std::string& hash() const { return hash_; }
  // Returns a timestamp of the date the file was downloaded.
  const base::Time download_time() const { return download_time_; }

 private:
  base::FilePath filepath_;
  std::string hash_;
  base::Time download_time_;
};

bool operator==(const ScheduledFile& lhs, const ScheduledFile& rhs);
bool operator!=(const ScheduledFile& lhs, const ScheduledFile& rhs);

}  // namespace auto_deletion

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_SCHEDULED_FILE_H_
