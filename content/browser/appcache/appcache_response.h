// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_APPCACHE_RESPONSE_H_
#define CONTENT_BROWSER_APPCACHE_APPCACHE_RESPONSE_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/content_export.h"
#include "net/base/completion_once_callback.h"
#include "net/http/http_response_info.h"
#include "url/gurl.h"

namespace net {
class IOBuffer;
}

namespace content {
class AppCacheDiskCache;
class AppCacheDiskCacheEntry;
class AppCacheStorage;

static const int kUnknownResponseDataSize = -1;

using OnceCompletionCallback = base::OnceCallback<void(int)>;

// Response info for a particular response id. Instances are tracked in
// the working set.
class CONTENT_EXPORT AppCacheResponseInfo
    : public base::RefCounted<AppCacheResponseInfo> {
 public:
  AppCacheResponseInfo(base::WeakPtr<AppCacheStorage> storage,
                       const GURL& manifest_url,
                       int64_t response_id,
                       std::unique_ptr<net::HttpResponseInfo> http_info,
                       int64_t response_data_size);

  const GURL& manifest_url() const { return manifest_url_; }
  int64_t response_id() const { return response_id_; }
  const net::HttpResponseInfo& http_response_info() const {
    return *http_response_info_;
  }
  int64_t response_data_size() const { return response_data_size_; }

 private:
  friend class base::RefCounted<AppCacheResponseInfo>;
  ~AppCacheResponseInfo();

  const GURL manifest_url_;
  const int64_t response_id_;
  const std::unique_ptr<net::HttpResponseInfo> http_response_info_;
  const int64_t response_data_size_;
  base::WeakPtr<AppCacheStorage> storage_;
};

// A refcounted wrapper for HttpResponseInfo so we can apply the
// refcounting semantics used with IOBuffer with these structures too.
struct CONTENT_EXPORT HttpResponseInfoIOBuffer
    : public base::RefCountedThreadSafe<HttpResponseInfoIOBuffer> {
  std::unique_ptr<net::HttpResponseInfo> http_info;
  int response_data_size;

  HttpResponseInfoIOBuffer();
  explicit HttpResponseInfoIOBuffer(
      std::unique_ptr<net::HttpResponseInfo> info);

 private:
  friend class base::RefCountedThreadSafe<HttpResponseInfoIOBuffer>;
  ~HttpResponseInfoIOBuffer();
};

// Common base class for response reader and writer.
class CONTENT_EXPORT AppCacheResponseIO {
 public:
  virtual ~AppCacheResponseIO();
  int64_t response_id() const { return response_id_; }

 protected:
  AppCacheResponseIO(int64_t response_id,
                     base::WeakPtr<AppCacheDiskCache> disk_cache);

  virtual void OnIOComplete(int result) = 0;
  virtual void OnOpenEntryComplete() {}

  bool IsIOPending() const { return !callback_.is_null(); }
  void ScheduleIOCompletionCallback(int result);
  void InvokeUserCompletionCallback(int result);
  void ReadRaw(int index, int offset, net::IOBuffer* buf, int buf_len);
  void WriteRaw(int index, int offset, net::IOBuffer* buf, int buf_len);
  void OpenEntryIfNeeded();

  // Methods in this class use weak pointers. The weak pointer factories must be
  // defined in the subclasses, to avoid use-after-free situations.
  virtual base::WeakPtr<AppCacheResponseIO> GetWeakPtr() = 0;

  const int64_t response_id_;
  base::WeakPtr<AppCacheDiskCache> disk_cache_;
  AppCacheDiskCacheEntry* entry_;
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer_;
  scoped_refptr<net::IOBuffer> buffer_;
  int buffer_len_;
  OnceCompletionCallback callback_;
  net::CompletionOnceCallback open_callback_;

 private:
  void OnRawIOComplete(int result);
  static void OpenEntryCallback(base::WeakPtr<AppCacheResponseIO> response,
                                AppCacheDiskCacheEntry** entry,
                                int rv);
};

// Reads existing response data from storage. If the object is deleted
// and there is a read in progress, the implementation will return
// immediately but will take care of any side effect of cancelling the
// operation.  In other words, instances are safe to delete at will.
class CONTENT_EXPORT AppCacheResponseReader : public AppCacheResponseIO {
 public:
  // Use AppCacheStorage::CreateResponseReader() instead of calling directly.
  //
  // The constructor is exposed for std::make_unique.
  AppCacheResponseReader(int64_t response_id,
                         base::WeakPtr<AppCacheDiskCache> disk_cache);

  ~AppCacheResponseReader() override;

  // Reads http info from storage. Always returns the result of the read
  // asynchronously through the 'callback'. Returns the number of bytes read
  // or a net:: error code. Guaranteed to not perform partial reads of
  // the info data. The reader acquires a reference to the 'info_buf' until
  // completion at which time the callback is invoked with either a negative
  // error code or the number of bytes read. The 'info_buf' argument should
  // contain a NULL http_info when ReadInfo is called. The 'callback' is a
  // required parameter.
  // Should only be called where there is no Read operation in progress.
  // (virtual for testing)
  virtual void ReadInfo(HttpResponseInfoIOBuffer* info_buf,
                        OnceCompletionCallback callback);

  // Reads data from storage. Always returns the result of the read
  // asynchronously through the 'callback'. Returns the number of bytes read
  // or a net:: error code. EOF is indicated with a return value of zero.
  // The reader acquires a reference to the provided 'buf' until completion
  // at which time the callback is invoked with either a negative error code
  // or the number of bytes read. The 'callback' is a required parameter.
  // Should only be called where there is no Read operation in progress.
  // (virtual for testing)
  virtual void ReadData(net::IOBuffer* buf,
                        int buf_len,
                        OnceCompletionCallback callback);

  // Returns true if there is a read operation, for data or info, pending.
  bool IsReadPending() { return IsIOPending(); }

  // Used to support range requests. If not called, the reader will
  // read the entire response body. If called, this must be called prior
  // to the first call to the ReadData method.
  void SetReadRange(int offset, int length);

 protected:
  void OnIOComplete(int result) override;
  void OnOpenEntryComplete() override;
  base::WeakPtr<AppCacheResponseIO> GetWeakPtr() override;

  void ContinueReadInfo();
  void ContinueReadData();

  int range_offset_;
  int range_length_;
  int read_position_;
  int reading_metadata_size_;

  base::WeakPtrFactory<AppCacheResponseReader> weak_factory_{this};
};

// Writes new response data to storage. If the object is deleted
// and there is a write in progress, the implementation will return
// immediately but will take care of any side effect of cancelling the
// operation. In other words, instances are safe to delete at will.
class CONTENT_EXPORT AppCacheResponseWriter : public AppCacheResponseIO {
 public:
  // Use AppCacheStorage::CreateResponseWriter() instead of calling directly.
  //
  // The constructor is exposed for std::make_unique.
  explicit AppCacheResponseWriter(int64_t response_id,
                                  base::WeakPtr<AppCacheDiskCache> disk_cache);

  ~AppCacheResponseWriter() override;

  // Writes the http info to storage. Always returns the result of the write
  // asynchronously through the 'callback'. Returns the number of bytes written
  // or a net:: error code. The writer acquires a reference to the 'info_buf'
  // until completion at which time the callback is invoked with either a
  // negative error code or the number of bytes written. The 'callback' is a
  // required parameter. The contents of 'info_buf' are not modified.
  // Should only be called where there is no Write operation in progress.
  // (virtual for testing)
  virtual void WriteInfo(HttpResponseInfoIOBuffer* info_buf,
                         OnceCompletionCallback callback);

  // Writes data to storage. Always returns the result of the write
  // asynchronously through the 'callback'. Returns the number of bytes written
  // or a net:: error code. Guaranteed to not perform partial writes.
  // The writer acquires a reference to the provided 'buf' until completion at
  // which time the callback is invoked with either a negative error code or
  // the number of bytes written. The 'callback' is a required parameter.
  // The contents of 'buf' are not modified.
  // Should only be called where there is no Write operation in progress.
  // (virtual for testing)
  virtual void WriteData(net::IOBuffer* buf,
                         int buf_len,
                         OnceCompletionCallback callback);

  // Returns true if there is a write pending.
  bool IsWritePending() { return IsIOPending(); }

  // Returns the amount written, info and data.
  int64_t amount_written() { return info_size_ + write_position_; }

 private:
  enum CreationPhase {
    NO_ATTEMPT,
    INITIAL_ATTEMPT,
    DOOM_EXISTING,
    SECOND_ATTEMPT
  };

  void OnIOComplete(int result) override;
  base::WeakPtr<AppCacheResponseIO> GetWeakPtr() override;

  void ContinueWriteInfo();
  void ContinueWriteData();
  void CreateEntryIfNeededAndContinue();
  static void OnCreateEntryComplete(
      base::WeakPtr<AppCacheResponseWriter> writer,
      AppCacheDiskCacheEntry** entry,
      int rv);

  int info_size_;
  int write_position_;
  int write_amount_;
  CreationPhase creation_phase_;
  base::WeakPtrFactory<AppCacheResponseWriter> weak_factory_{this};
};

// Writes metadata of the existing response to storage. If the object is deleted
// and there is a write in progress, the implementation will return
// immediately but will take care of any side effect of cancelling the
// operation. In other words, instances are safe to delete at will.
class CONTENT_EXPORT AppCacheResponseMetadataWriter
    : public AppCacheResponseIO {
 public:
  // Use AppCacheStorage::CreateResponseMetadataWriter() instead of calling
  // directly.
  //
  // The constructor is exposed for std::make_unique.
  AppCacheResponseMetadataWriter(int64_t response_id,
                                 base::WeakPtr<AppCacheDiskCache> disk_cache);

  ~AppCacheResponseMetadataWriter() override;

  // Writes metadata to storage. Always returns the result of the write
  // asynchronously through the 'callback'. Returns the number of bytes written
  // or a net:: error code. Guaranteed to not perform partial writes.
  // The writer acquires a reference to the provided 'buf' until completion at
  // which time the callback is invoked with either a negative error code or
  // the number of bytes written. The 'callback' is a required parameter.
  // The contents of 'buf' are not modified.
  // Should only be called where there is no WriteMetadata operation in
  // progress.
  void WriteMetadata(net::IOBuffer* buf,
                     int buf_len,
                     net::CompletionOnceCallback callback);

  // Returns true if there is a write pending.
  bool IsWritePending() { return IsIOPending(); }

 private:
  void OnIOComplete(int result) override;
  void OnOpenEntryComplete() override;
  base::WeakPtr<AppCacheResponseIO> GetWeakPtr() override;

  int write_amount_;
  base::WeakPtrFactory<AppCacheResponseMetadataWriter> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_APPCACHE_APPCACHE_RESPONSE_H_
