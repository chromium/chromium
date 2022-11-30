// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/httpfs/http_fs_node.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <ppapi/c/pp_errors.h>

#include "nacl_io/httpfs/http_fs.h"
#include "nacl_io/http_status_codes.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/osinttypes.h"

#if defined(WIN32)
#define snprintf _snprintf
#endif

namespace nacl_io {

namespace {

// If we're attempting to read a partial request, but the server returns a full
// request, we need to read all of the data up to the start of our partial
// request into a dummy buffer. This is the maximum size of that buffer.
const int MAX_READ_BUFFER_SIZE = 64 * 1024;

StringMap_t ParseHeaders(const char* headers, int32_t headers_length) {
  enum State {
    FINDING_KEY,
    SKIPPING_WHITESPACE,
    FINDING_VALUE,
  };

  StringMap_t result;
  std::string key;
  std::string value;

  State state = FINDING_KEY;
  const char* start = headers;
  for (int i = 0; i < headers_length; ++i) {
    switch (state) {
      case FINDING_KEY:
        if (headers[i] == ':') {
          // Found key.
          key.assign(start, &headers[i] - start);
          key = NormalizeHeaderKey(key);
          state = SKIPPING_WHITESPACE;
        }
        break;

      case SKIPPING_WHITESPACE:
        if (headers[i] == ' ') {
          // Found whitespace, keep going...
          break;
        }

        // Found a non-whitespace, mark this as the start of the value.
        start = &headers[i];
        state = FINDING_VALUE;

        // NOTE: Avoid fallthrough as it produces a warning on newer compilers,
        // but can't easily be silenced by the older NaCl compilers.
        //
        // Fallthrough to start processing value without incrementing i.
        goto finding_value;

      finding_value:
      case FINDING_VALUE:
        if (headers[i] == '\n') {
          // Found value.
          value.assign(start, &headers[i] - start);
          result[key] = value;
          start = &headers[i + 1];
          state = FINDING_KEY;
        }
        break;
    }
  }

  return result;
}

bool ParseContentLength(const StringMap_t& headers, off_t* content_length) {
  StringMap_t::const_iterator iter = headers.find("Content-Length");
  if (iter == headers.end())
    return false;

  *content_length = strtoull(iter->second.c_str(), NULL, 10);
  return true;
}

bool ParseContentRange(const StringMap_t& headers,
                       off_t* read_start,
                       off_t* read_end,
                       off_t* entity_length) {
  StringMap_t::const_iterator iter = headers.find("Content-Range");
  if (iter == headers.end())
    return false;

  // The key should look like "bytes ##-##/##" or "bytes ##-##/*". The last
  // value is the entity length, which can potentially be * (i.e. unknown).
  off_t read_start_int;
  off_t read_end_int;
  off_t entity_length_int;
  int result = sscanf(iter->second.c_str(),
                      "bytes %" SCNi64 "-%" SCNi64 "/%" SCNi64,
                      &read_start_int,
                      &read_end_int,
                      &entity_length_int);

  // The Content-Range header specifies an inclusive range: e.g. the first ten
  // bytes is "bytes 0-9/*". Convert it to a half-open range by incrementing
  // read_end.
  if (result == 2) {
    if (read_start)
      *read_start = read_start_int;
    if (read_end)
      *read_end = read_end_int + 1;
    if (entity_length)
      *entity_length = 0;
    return true;
  } else if (result == 3) {
    if (read_start)
      *read_start = read_start_int;
    if (read_end)
      *read_end = read_end_int + 1;
    if (entity_length)
      *entity_length = entity_length_int;
    return true;
  }

  return false;
}

// Maps an HTTP |status_code| onto the appropriate errno code.
int HTTPStatusCodeToErrno(int status_code) {
  switch (status_code) {
    case STATUSCODE_OK:
    case STATUSCODE_PARTIAL_CONTENT:
      return 0;
    case STATUSCODE_FORBIDDEN:
      return EACCES;
    case STATUSCODE_NOT_FOUND:
      return ENOENT;
  }
  if (status_code >= 400 && status_code < 500)
    return EINVAL;
  return EIO;
}

}  // namespace

void HttpFsNode::SetCachedSize(off_t size) {
  has_cached_size_ = true;
  stat_.st_size = size;
}

Error HttpFsNode::FSync() {
  return EACCES;
}

Error HttpFsNode::GetDents(size_t offs,
                           struct dirent* pdir,
                           size_t count,
                           int* out_bytes) {
  *out_bytes = 0;
  return EACCES;
}

Error HttpFsNode::GetStat(struct stat* stat) {
  AUTO_LOCK(node_lock_);
  return GetStat_Locked(stat);
}

Error HttpFsNode::Read(const HandleAttr& attr,
                       void* buf,
                       size_t count,
                       int* out_bytes) {
  *out_bytes = 0;

  AUTO_LOCK(node_lock_);
  if (cache_content_) {
    if (cached_data_.empty()) {
      Error error = DownloadToCache();
      if (error)
        return error;
    }

    return ReadPartialFromCache(attr, buf, count, out_bytes);
  }

  return DownloadPartial(attr, buf, count, out_bytes);
}

Error HttpFsNode::FTruncate(off_t size) {
  return EACCES;
}

Error HttpFsNode::Write(const HandleAttr& attr,
                        const void* buf,
                        size_t count,
                        int* out_bytes) {
  // TODO(binji): support POST?
  *out_bytes = 0;
  return EACCES;
}

Error HttpFsNode::GetSize(off_t* out_size) {
  *out_size = 0;

  // TODO(binji): This value should be cached properly; i.e. obey the caching
  // headers returned by the server.
  AUTO_LOCK(node_lock_);
  struct stat statbuf;
  Error error = GetStat_Locked(&statbuf);
  if (error)
    return error;

  *out_size = stat_.st_size;
  return 0;
}

HttpFsNode::HttpFsNode(Filesystem* filesystem,
                       const std::string& url,
                       bool cache_content)
    : Node(filesystem),
      url_(url),
      buffer_(NULL),
      buffer_len_(0),
      cache_content_(cache_content),
      has_cached_size_(false) {
  // http nodes are read-only by default
  SetMode(S_IRALL);
}

HttpFsNode::~HttpFsNode() {
  free(buffer_);
}

Error HttpFsNode::GetStat_Locked(struct stat* stat) {
  // Assume we need to 'HEAD' if we do not know the size, otherwise, assume
  // that the information is constant.  We can add a timeout if needed.
  HttpFs* filesystem = static_cast<HttpFs*>(filesystem_);
  if (!has_cached_size_ || !filesystem->cache_stat_) {
    StringMap_t headers;
    ScopedResource loader(filesystem_->ppapi());
    ScopedResource request(filesystem_->ppapi());
    ScopedResource response(filesystem_->ppapi());
    int32_t statuscode;
    StringMap_t response_headers;
    const char* method = "HEAD";

    if (filesystem->is_blob_url_) {
      // Blob URLs do not support HEAD requests, but do give the content length
      // in their response headers. We issue a single-byte GET request to
      // retrieve the content length.
      method = "GET";
      headers["Range"] = "bytes=0-0";
    }

    Error error = OpenUrl(method,
                          &headers,
                          &loader,
                          &request,
                          &response,
                          &statuscode,
                          &response_headers);
    if (error)
      return error;

    off_t entity_length;
    if (ParseContentRange(response_headers, NULL, NULL, &entity_length)) {
      SetCachedSize(static_cast<off_t>(entity_length));
    } else if (ParseContentLength(response_headers, &entity_length)) {
      SetCachedSize(static_cast<off_t>(entity_length));
    } else if (cache_content_) {
      // The server didn't give a content length; download the data to memory
      // via DownloadToCache, which will also set stat_.st_size;
      error = DownloadToCache();
      if (error)
        return error;
    } else {
      // The user doesn't want to cache content, but we didn't get a
      // "Content-Length" header. Read the entire entity, and throw it away.
      // Don't use DownloadToCache, as that will still allocate enough memory
      // for the entire entity.
      off_t bytes_read;
      error = DownloadToTemp(&bytes_read);
      if (error)
        return error;

      SetCachedSize(bytes_read);
    }

    stat_.st_atime = 0;  // TODO(binji): Use "Last-Modified".
    stat_.st_mtime = 0;
    stat_.st_ctime = 0;

    SetType(S_IFREG);
  }

  // Fill the stat structure if provided
  if (stat)
    *stat = stat_;

  return 0;
}

Error HttpFsNode::OpenUrl(const char* method,
                          StringMap_t* request_headers,
                          ScopedResource* out_loader,
                          ScopedResource* out_request,
                          ScopedResource* out_response,
                          int32_t* out_statuscode,
                          StringMap_t* out_response_headers) {
  // Clear all out parameters.
  *out_statuscode = 0;
  out_response_headers->clear();

  // Assume lock_ is already held.
  PepperInterface* ppapi = filesystem_->ppapi();

  HttpFs* mount_http = static_cast<HttpFs*>(filesystem_);
  out_request->Reset(
      mount_http->MakeUrlRequestInfo(url_, method, request_headers));
  if (!out_request->pp_resource())
    return EINVAL;

  URLLoaderInterface* loader_interface = ppapi->GetURLLoaderInterface();
  URLResponseInfoInterface* response_interface =
      ppapi->GetURLResponseInfoInterface();
  VarInterface* var_interface = ppapi->GetVarInterface();

  out_loader->Reset(loader_interface->Create(ppapi->GetInstance()));
  if (!out_loader->pp_resource())
    return EINVAL;

  int32_t result = loader_interface->Open(out_loader->pp_resource(),
                                          out_request->pp_resource(),
                                          PP_BlockUntilComplete());
  if (result != PP_OK)
    return PPERROR_TO_ERRNO(result);

  out_response->Reset(
      loader_interface->GetResponseInfo(out_loader->pp_resource()));
  if (!out_response->pp_resource())
    return EINVAL;

  // Get response statuscode.
  PP_Var statuscode = response_interface->GetProperty(
      out_response->pp_resource(), PP_URLRESPONSEPROPERTY_STATUSCODE);

  if (statuscode.type != PP_VARTYPE_INT32)
    return EINVAL;

  *out_statuscode = statuscode.value.as_int;

  // Only accept OK or Partial Content.
  Error error = HTTPStatusCodeToErrno(*out_statuscode);
  if (error)
    return error;

  // Get response headers.
  PP_Var response_headers_var = response_interface->GetProperty(
      out_response->pp_resource(), PP_URLRESPONSEPROPERTY_HEADERS);

  uint32_t response_headers_length;
  const char* response_headers_str =
      var_interface->VarToUtf8(response_headers_var, &response_headers_length);

  *out_response_headers =
      ParseHeaders(response_headers_str, response_headers_length);

  var_interface->Release(response_headers_var);

  return 0;
}

Error HttpFsNode::DownloadToCache() {
  StringMap_t headers;
  ScopedResource loader(filesystem_->ppapi());
  ScopedResource request(filesystem_->ppapi());
  ScopedResource response(filesystem_->ppapi());
  int32_t statuscode;
  StringMap_t response_headers;
  Error error = OpenUrl("GET",
                        &headers,
                        &loader,
                        &request,
                        &response,
                        &statuscode,
                        &response_headers);
  if (error)
    return error;

  off_t content_length = 0;
  if (ParseContentLength(response_headers, &content_length)) {
    cached_data_.resize(content_length);
    int real_size;
    error = ReadResponseToBuffer(
        loader, cached_data_.data(), content_length, &real_size);
    if (error)
      return error;

    SetCachedSize(real_size);
    cached_data_.resize(real_size);
    return 0;
  }

  int bytes_read;
  error = ReadEntireResponseToCache(loader, &bytes_read);
  if (error)
    return error;

  SetCachedSize(bytes_read);
  return 0;
}

Error HttpFsNode::ReadPartialFromCache(const HandleAttr& attr,
                                       void* buf,
                                       int count,
                                       int* out_bytes) {
  *out_bytes = 0;
  off_t size = cached_data_.size();

  if (attr.offs + count > size)
    count = size - attr.offs;

  if (count <= 0)
    return 0;

  memcpy(buf, &cached_data_.data()[attr.offs], count);
  *out_bytes = count;
  return 0;
}

Error HttpFsNode::DownloadPartial(const HandleAttr& attr,
                                  void* buf,
                                  off_t count,
                                  int* out_bytes) {
  *out_bytes = 0;

  StringMap_t headers;

  char buffer[100];
  // Range request is inclusive: 0-99 returns 100 bytes.
  snprintf(&buffer[0],
           sizeof(buffer),
           "bytes=%" PRIi64 "-%" PRIi64,
           attr.offs,
           attr.offs + count - 1);
  headers["Range"] = buffer;

  ScopedResource loader(filesystem_->ppapi());
  ScopedResource request(filesystem_->ppapi());
  ScopedResource response(filesystem_->ppapi());
  int32_t statuscode;
  StringMap_t response_headers;
  Error error = OpenUrl("GET",
                        &headers,
                        &loader,
                        &request,
                        &response,
                        &statuscode,
                        &response_headers);
  if (error) {
    if (statuscode == STATUSCODE_REQUESTED_RANGE_NOT_SATISFIABLE) {
      // We're likely trying to read past the end. Return 0 bytes.
      *out_bytes = 0;
      return 0;
    }

    return error;
  }

  off_t read_start = 0;
  if (statuscode == STATUSCODE_OK) {
    // No partial result, read everything starting from the part we care about.
    off_t content_length;
    if (ParseContentLength(response_headers, &content_length)) {
      if (attr.offs >= content_length)
        return EINVAL;

      // Clamp count, if trying to read past the end of the file.
      if (attr.offs + count > content_length) {
        count = content_length - attr.offs;
      }
    }
  } else if (statuscode == STATUSCODE_PARTIAL_CONTENT) {
    // Determine from the headers where we are reading.
    off_t read_end;
    off_t entity_length;
    if (ParseContentRange(
            response_headers, &read_start, &read_end, &entity_length)) {
      if (read_start > attr.offs || read_start > read_end) {
        // If this error occurs, the server is returning bogus values.
        return EINVAL;
      }

      // Clamp count, if trying to read past the end of the file.
      count = std::min(read_end - read_start, count);
    } else {
      // Partial Content without Content-Range. Assume that the server gave us
      // exactly what we asked for. This can happen even when the server
      // returns 200 -- the cache may return 206 in this case, but not modify
      // the headers.
      read_start = attr.offs;
    }
  }

  if (read_start < attr.offs) {
    // We aren't yet at the location where we want to start reading. Read into
    // our dummy buffer until then.
    int bytes_to_read = attr.offs - read_start;
    int bytes_read;
    error = ReadResponseToTemp(loader, bytes_to_read, &bytes_read);
    if (error)
      return error;

    // Tried to read past the end of the entity.
    if (bytes_read < bytes_to_read) {
      *out_bytes = 0;
      return 0;
    }
  }

  return ReadResponseToBuffer(loader, buf, count, out_bytes);
}

Error HttpFsNode::DownloadToTemp(off_t* out_bytes) {
  StringMap_t headers;
  ScopedResource loader(filesystem_->ppapi());
  ScopedResource request(filesystem_->ppapi());
  ScopedResource response(filesystem_->ppapi());
  int32_t statuscode;
  StringMap_t response_headers;
  Error error = OpenUrl("GET",
                        &headers,
                        &loader,
                        &request,
                        &response,
                        &statuscode,
                        &response_headers);
  if (error)
    return error;

  off_t content_length = 0;
  if (ParseContentLength(response_headers, &content_length)) {
    *out_bytes = content_length;
    return 0;
  }

  return ReadEntireResponseToTemp(loader, out_bytes);
}

Error HttpFsNode::ReadEntireResponseToTemp(const ScopedResource& loader,
                                           off_t* out_bytes) {
  *out_bytes = 0;

  const int kBytesToRead = MAX_READ_BUFFER_SIZE;
  buffer_ = (char*)realloc(buffer_, kBytesToRead);
  assert(buffer_);
  if (!buffer_) {
    buffer_len_ = 0;
    return ENOMEM;
  }
  buffer_len_ = kBytesToRead;

  while (true) {
    int bytes_read;
    Error error =
        ReadResponseToBuffer(loader, buffer_, kBytesToRead, &bytes_read);
    if (error)
      return error;

    *out_bytes += bytes_read;

    if (bytes_read < kBytesToRead)
      return 0;
  }
}

Error HttpFsNode::ReadEntireResponseToCache(const ScopedResource& loader,
                                            int* out_bytes) {
  *out_bytes = 0;
  const int kBytesToRead = MAX_READ_BUFFER_SIZE;

  while (true) {
    // Always recalculate the buf pointer because it may have moved when
    // cached_data_ was resized.
    cached_data_.resize(*out_bytes + kBytesToRead);
    void* buf = cached_data_.data() + *out_bytes;

    int bytes_read;
    Error error = ReadResponseToBuffer(loader, buf, kBytesToRead, &bytes_read);
    if (error)
      return error;

    *out_bytes += bytes_read;

    if (bytes_read < kBytesToRead) {
      // Shrink the cached data buffer to the correct size.
      cached_data_.resize(*out_bytes);
      return 0;
    }
  }
}

Error HttpFsNode::ReadResponseToTemp(const ScopedResource& loader,
                                     int count,
                                     int* out_bytes) {
  *out_bytes = 0;

  if (buffer_len_ < count) {
    int new_len = std::min(count, MAX_READ_BUFFER_SIZE);
    buffer_ = (char*)realloc(buffer_, new_len);
    assert(buffer_);
    if (!buffer_) {
      buffer_len_ = 0;
      return ENOMEM;
    }
    buffer_len_ = new_len;
  }

  int bytes_left = count;
  while (bytes_left > 0) {
    int bytes_to_read = std::min(bytes_left, buffer_len_);
    int bytes_read;
    Error error = ReadResponseToBuffer(
        loader, buffer_, bytes_to_read, &bytes_read);
    if (error)
      return error;

    if (bytes_read == 0)
      return 0;

    bytes_left -= bytes_read;
    *out_bytes += bytes_read;
  }

  return 0;
}

Error HttpFsNode::ReadResponseToBuffer(const ScopedResource& loader,
                                       void* buf,
                                       int count,
                                       int* out_bytes) {
  *out_bytes = 0;

  PepperInterface* ppapi = filesystem_->ppapi();
  URLLoaderInterface* loader_interface = ppapi->GetURLLoaderInterface();

  char* out_buffer = static_cast<char*>(buf);
  int bytes_to_read = count;
  while (bytes_to_read > 0) {
    int bytes_read =
        loader_interface->ReadResponseBody(loader.pp_resource(),
                                           out_buffer,
                                           bytes_to_read,
                                           PP_BlockUntilComplete());

    if (bytes_read == 0) {
      // This is not an error -- it may just be that we were trying to read
      // more data than exists.
      *out_bytes = count - bytes_to_read;
      return 0;
    }

    if (bytes_read < 0)
      return PPERROR_TO_ERRNO(bytes_read);

    assert(bytes_read <= bytes_to_read);
    bytes_to_read -= bytes_read;
    out_buffer += bytes_read;
  }

  *out_bytes = count;
  return 0;
}

}  // namespace nacl_io
