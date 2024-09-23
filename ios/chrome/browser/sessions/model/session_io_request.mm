// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_io_request.h"

#import "base/check.h"
#import "base/threading/scoped_blocking_call.h"
#import "ios/chrome/browser/sessions/model/session_internal_util.h"
#import "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace ios::sessions {

WriteDataIORequest::WriteDataIORequest(base::FilePath path, NSData* data)
    : path_(std::move(path)), data_(data) {
  DCHECK(path_.IsAbsolute());
  DCHECK(data_.length);
}

WriteDataIORequest::~WriteDataIORequest() = default;

void WriteDataIORequest::Execute() const {
  // Ignore the result as there is nothing that can be done by this point
  // if the write fails, and `ios::sessions::WriteFile()` already reports
  // the failure to the log.
  base::ScopedBlockingCall _(FROM_HERE, base::BlockingType::MAY_BLOCK);
  std::ignore = ios::sessions::WriteFile(path_, data_);
}

WriteProtoIORequest::WriteProtoIORequest(base::FilePath path,
                                         std::unique_ptr<Proto> proto)
    : path_(std::move(path)), proto_(std::move(proto)) {
  DCHECK(path_.IsAbsolute());
  DCHECK(proto_);
}

WriteProtoIORequest::~WriteProtoIORequest() = default;

void WriteProtoIORequest::Execute() const {
  // Ignore the result as there is nothing that can be done by this point
  // if the write fails, and `ios::sessions::WriteProto()` already reports
  // the failure to the log.
  base::ScopedBlockingCall _(FROM_HERE, base::BlockingType::MAY_BLOCK);
  std::ignore = ios::sessions::WriteProto(path_, *proto_);
}

CopyPathIORequest::~CopyPathIORequest() = default;

CopyPathIORequest::CopyPathIORequest(base::FilePath from_path,
                                     base::FilePath dest_path)
    : from_path_(std::move(from_path)), dest_path_(std::move(dest_path)) {
  DCHECK(from_path_.IsAbsolute());
  DCHECK(dest_path_.IsAbsolute());
}

void CopyPathIORequest::Execute() const {
  // Ignore the result as there is nothing that can be done by this point
  // if the write fails.
  base::ScopedBlockingCall _(FROM_HERE, base::BlockingType::MAY_BLOCK);
  std::ignore = ios::sessions::CopyDirectory(from_path_, dest_path_);
}

DeletePathIORequest::~DeletePathIORequest() = default;

DeletePathIORequest::DeletePathIORequest(base::FilePath path)
    : path_(std::move(path)) {
  DCHECK(path_.IsAbsolute());
}

void DeletePathIORequest::Execute() const {
  // Ignore the result as there is nothing that can be done by this point
  // if the deletion fails.
  base::ScopedBlockingCall _(FROM_HERE, base::BlockingType::MAY_BLOCK);
  std::ignore = ios::sessions::DeleteRecursively(path_);
}

void ExecuteIORequests(IORequestList requests) {
  base::ScopedBlockingCall _(FROM_HERE, base::BlockingType::MAY_BLOCK);
  for (const auto& request : requests) {
    request->Execute();
  }
}

}  // namespace ios::sessions
