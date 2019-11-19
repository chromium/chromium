// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_BLOB_INFO_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_BLOB_INFO_H_

#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/platform/web_string.h"

namespace blink {

class BlobDataHandle;

class WebBlobInfo {
 public:
  WebBlobInfo()
      : is_file_(false),
        size_(std::numeric_limits<uint64_t>::max()),
        last_modified_(0) {}
  BLINK_EXPORT WebBlobInfo(const WebString& uuid,
                           const WebString& type,
                           uint64_t size,
                           mojo::ScopedMessagePipeHandle);
  BLINK_EXPORT WebBlobInfo(const WebString& uuid,
                           const WebString& file_path,
                           const WebString& file_name,
                           const WebString& type,
                           double last_modified,
                           uint64_t size,
                           mojo::ScopedMessagePipeHandle);

  // For testing purposes, these two methods create a WebBlobInfo connected to a
  // dangling mojo message pipe. This means that any operations that actually
  // depend on the mojo connection to exist will fail, but otherwise you should
  // be able to safely pass around these blobs.
  BLINK_EXPORT static WebBlobInfo BlobForTesting(const WebString& uuid,
                                                 const WebString& type,
                                                 uint64_t size);
  BLINK_EXPORT static WebBlobInfo FileForTesting(const WebString& uuid,
                                                 const WebString& file_path,
                                                 const WebString& file_name,
                                                 const WebString& type);

  BLINK_EXPORT ~WebBlobInfo();

  BLINK_EXPORT WebBlobInfo(const WebBlobInfo& other);
  BLINK_EXPORT WebBlobInfo& operator=(const WebBlobInfo& other);

  bool IsFile() const { return is_file_; }
  const WebString& Uuid() const { return uuid_; }
  const WebString& GetType() const { return type_; }
  uint64_t size() const { return size_; }
  const WebString& FilePath() const { return file_path_; }
  const WebString& FileName() const { return file_name_; }
  double LastModified() const { return last_modified_; }
  BLINK_EXPORT mojo::ScopedMessagePipeHandle CloneBlobHandle() const;

#if INSIDE_BLINK
  BLINK_EXPORT WebBlobInfo(scoped_refptr<BlobDataHandle>);
  BLINK_EXPORT WebBlobInfo(scoped_refptr<BlobDataHandle>,
                           const WebString& file_path,
                           const WebString& file_name,
                           double last_modified);
  // TODO(mek): Get rid of these constructors after ensuring that the
  // BlobDataHandle always has the correct type and size.
  BLINK_EXPORT WebBlobInfo(scoped_refptr<BlobDataHandle>,
                           const WebString& type,
                           uint64_t size);
  BLINK_EXPORT WebBlobInfo(scoped_refptr<BlobDataHandle>,
                           const WebString& file_path,
                           const WebString& file_name,
                           const WebString& type,
                           double last_modified,
                           uint64_t size);
  BLINK_EXPORT scoped_refptr<BlobDataHandle> GetBlobHandle() const;
#endif

 private:
  bool is_file_;
  WebString uuid_;
  WebString type_;  // MIME type
  uint64_t size_;
  WebPrivatePtr<BlobDataHandle> blob_handle_;
  WebString file_path_;   // Only for File
  WebString file_name_;   // Only for File
  double last_modified_;  // Only for File
};

}  // namespace blink

#endif
