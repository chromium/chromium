// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_IO_REQUEST_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_IO_REQUEST_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "base/files/file_path.h"

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace ios::sessions {

// Representing a delayed IO request for session restoration. Supposed to
// be posted on a background sequence.
class IORequest {
 public:
  IORequest() = default;
  virtual ~IORequest() = default;

  // Perform the IO request.
  virtual void Execute() const = 0;
};

// An IO request asking to write data (stored as NSData*) to path.
class WriteDataIORequest final : public IORequest {
 public:
  WriteDataIORequest(base::FilePath path, NSData* data);
  ~WriteDataIORequest() final;

  // IORequest implementation.
  void Execute() const final;

 private:
  const base::FilePath path_;
  NSData* const data_;
};

// An IO request asking to write daya (stored as a protobuf message) to path.
class WriteProtoIORequest final : public IORequest {
 public:
  using Proto = google::protobuf::MessageLite;

  WriteProtoIORequest(base::FilePath path, std::unique_ptr<Proto> proto);
  ~WriteProtoIORequest() final;

  // IORequest implementation.
  void Execute() const final;

 private:
  const base::FilePath path_;
  const std::unique_ptr<Proto> proto_;
};

// An IO request asking to copy recursively `from_path` to `dest_path`.
class CopyPathIORequest final : public IORequest {
 public:
  CopyPathIORequest(base::FilePath from_path, base::FilePath dest_path);
  ~CopyPathIORequest() final;

  // IORequest implementation.
  void Execute() const final;

 private:
  const base::FilePath from_path_;
  const base::FilePath dest_path_;
};

// An IO request asking to delete recursively a filesystem item (file or
// directory) at path.
class DeletePathIORequest final : public IORequest {
 public:
  explicit DeletePathIORequest(base::FilePath path);
  ~DeletePathIORequest() final;

  // IORequest implementation.
  void Execute() const final;

 private:
  const base::FilePath path_;
};

// An ordered list of IORequest objects.
using IORequestList = std::vector<std::unique_ptr<IORequest>>;

// Executes all `requests` in order.
void ExecuteIORequests(IORequestList requests);

}  // namespace ios::sessions

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_IO_REQUEST_H_
