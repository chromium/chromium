// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For loading files, we make use of overlapped i/o to ensure that reading from
// the filesystem (e.g., a network filesystem) does not block the calling
// thread.  An alternative approach would be to use a background thread or pool
// of threads, but it seems better to leverage the operating system's ability
// to do background file reads for us.
//
// Since overlapped reads require a 'static' buffer for the duration of the
// asynchronous read, the URLRequestTestJobBackedByFile keeps a buffer as a
// member var.  In URLRequestTestJobBackedByFile::Read, data is simply copied
// from the object's buffer into the given buffer.  If there is no data to copy,
// the URLRequestTestJobBackedByFile attempts to read more from the file to fill
// its buffer.  If reading from the file does not complete synchronously, then
// the URLRequestTestJobBackedByFile waits for a signal from the OS that the
// overlapped read has completed.  It does so by leveraging the
// MessageLoop::WatchObject API.

#include "net/test/url_request/url_request_test_job_backed_by_file.h"

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/task/task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "net/base/file_stream.h"
#include "net/base/filename_util.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/filter/gzip_source_stream.h"
#include "net/filter/source_stream.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request_error_job.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/shortcut.h"
#endif

namespace net {

URLRequestTestJobBackedByFile::FileMetaInfo::FileMetaInfo() = default;

URLRequestTestJobBackedByFile::URLRequestTestJobBackedByFile(
    URLRequest* request,
    const base::FilePath& file_path,
    const scoped_refptr<base::TaskRunner>& file_task_runner)
    : URLRequestJob(request),
      file_path_(file_path),
      stream_(std::make_unique<FileStream>(file_task_runner)),
      file_task_runner_(file_task_runner) {}

void URLRequestTestJobBackedByFile::Start() {
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&URLRequestTestJobBackedByFile::FetchMetaInfo, file_path_),
      base::BindOnce(&URLRequestTestJobBackedByFile::DidFetchMetaInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void URLRequestTestJobBackedByFile::Kill() {
  stream_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

  URLRequestJob::Kill();
}

int URLRequestTestJobBackedByFile::ReadRawData(IOBuffer* dest, int dest_size) {
  DCHECK_NE(dest_size, 0);
  DCHECK_GE(remaining_bytes_, 0);

  if (remaining_bytes_ < dest_size)
    dest_size = remaining_bytes_;

  // If we should copy zero bytes because |remaining_bytes_| is zero, short
  // circuit here.
  if (!dest_size)
    return 0;

  int rv = stream_->Read(dest, dest_size,
                         base::BindOnce(&URLRequestTestJobBackedByFile::DidRead,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        base::WrapRefCounted(dest)));
  if (rv >= 0) {
    remaining_bytes_ -= rv;
    DCHECK_GE(remaining_bytes_, 0);
  }

  return rv;
}

bool URLRequestTestJobBackedByFile::GetMimeType(std::string* mime_type) const {
  DCHECK(request_);
  if (meta_info_.mime_type_result) {
    *mime_type = meta_info_.mime_type;
    return true;
  }
  return false;
}

void URLRequestTestJobBackedByFile::SetExtraRequestHeaders(
    const HttpRequestHeaders& headers) {
  std::optional<std::string> range_header =
      headers.GetHeader(HttpRequestHeaders::kRange);
  if (range_header) {
    // This job only cares about the Range header. This method stashes the value
    // for later use in DidOpen(), which is responsible for some of the range
    // validation as well. NotifyStartError is not legal to call here since
    // the job has not started.
    std::vector<HttpByteRange> ranges;
    if (HttpUtil::ParseRangeHeader(*range_header, &ranges)) {
      if (ranges.size() == 1) {
        byte_range_ = ranges[0];
      } else {
        // We don't support multiple range requests in one single URL request,
        // because we need to do multipart encoding here.
        // TODO(hclam): decide whether we want to support multiple range
        // requests.
        range_parse_result_ = ERR_REQUEST_RANGE_NOT_SATISFIABLE;
      }
    }
  }
}

void URLRequestTestJobBackedByFile::GetResponseInfo(HttpResponseInfo* info) {
  if (!serve_mime_type_as_content_type_ || !meta_info_.mime_type_result)
    return;
  auto headers =
      base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 200 OK");
  headers->AddHeader(net::HttpRequestHeaders::kContentType,
                     meta_info_.mime_type);
  info->headers = headers;
}

void URLRequestTestJobBackedByFile::OnOpenComplete(int result) {}

void URLRequestTestJobBackedByFile::OnSeekComplete(int64_t result) {}

void URLRequestTestJobBackedByFile::OnReadComplete(IOBuffer* buf, int result) {}

URLRequestTestJobBackedByFile::~URLRequestTestJobBackedByFile() = default;

std::unique_ptr<SourceStream>
URLRequestTestJobBackedByFile::SetUpSourceStream() {
  std::unique_ptr<SourceStream> source = URLRequestJob::SetUpSourceStream();
  if (!base::EqualsCaseInsensitiveASCII(file_path_.Extension(), ".svgz"))
    return source;

  return GzipSourceStream::Create(std::move(source), SourceStream::TYPE_GZIP);
}

std::unique_ptr<URLRequestTestJobBackedByFile::FileMetaInfo>
URLRequestTestJobBackedByFile::FetchMetaInfo(const base::FilePath& file_path) {
  auto meta_info = std::make_unique<FileMetaInfo>();
  base::File::Info file_info;
  meta_info->file_exists = base::GetFileInfo(file_path, &file_info);
  if (meta_info->file_exists) {
    meta_info->file_size = file_info.size;
    meta_info->is_directory = file_info.is_directory;
  }
  // On Windows GetMimeTypeFromFile() goes to the registry. Thus it should be
  // done in WorkerPool.
  meta_info->mime_type_result =
      GetMimeTypeFromFile(file_path, &meta_info->mime_type);
  meta_info->absolute_path = base::MakeAbsoluteFilePath(file_path);
  return meta_info;
}

void URLRequestTestJobBackedByFile::DidFetchMetaInfo(
    std::unique_ptr<FileMetaInfo> meta_info) {
  meta_info_ = *meta_info;

  if (!meta_info_.file_exists) {
    DidOpen(ERR_FILE_NOT_FOUND);
    return;
  }

  // This class is only used for mocking out network requests in test by using a
  // file as a response body. It doesn't need to support directory listings.
  if (meta_info_.is_directory) {
    DidOpen(ERR_INVALID_ARGUMENT);
    return;
  }

  int flags =
      base::File::FLAG_OPEN | base::File::FLAG_READ | base::File::FLAG_ASYNC;
  int rv = stream_->Open(file_path_, flags,
                         base::BindOnce(&URLRequestTestJobBackedByFile::DidOpen,
                                        weak_ptr_factory_.GetWeakPtr()));
  if (rv != ERR_IO_PENDING)
    DidOpen(rv);
}

void URLRequestTestJobBackedByFile::DidOpen(int result) {
  OnOpenComplete(result);
  if (result != OK) {
    NotifyStartError(result);
    return;
  }

  if (range_parse_result_ != OK ||
      !byte_range_.ComputeBounds(meta_info_.file_size)) {
    DidSeek(ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  remaining_bytes_ =
      byte_range_.last_byte_position() - byte_range_.first_byte_position() + 1;
  DCHECK_GE(remaining_bytes_, 0);

  if (remaining_bytes_ > 0 && byte_range_.first_byte_position() != 0) {
    int rv =
        stream_->Seek(byte_range_.first_byte_position(),
                      base::BindOnce(&URLRequestTestJobBackedByFile::DidSeek,
                                     weak_ptr_factory_.GetWeakPtr()));
    if (rv != ERR_IO_PENDING)
      DidSeek(ERR_REQUEST_RANGE_NOT_SATISFIABLE);
  } else {
    // We didn't need to call stream_->Seek() at all, so we pass to DidSeek()
    // the value that would mean seek success. This way we skip the code
    // handling seek failure.
    DidSeek(byte_range_.first_byte_position());
  }
}

void URLRequestTestJobBackedByFile::DidSeek(int64_t result) {
  DCHECK(result < 0 || result == byte_range_.first_byte_position());

  OnSeekComplete(result);
  if (result < 0) {
    NotifyStartError(ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  set_expected_content_size(remaining_bytes_);
  NotifyHeadersComplete();
}

void URLRequestTestJobBackedByFile::DidRead(scoped_refptr<IOBuffer> buf,
                                            int result) {
  if (result >= 0) {
    remaining_bytes_ -= result;
    DCHECK_GE(remaining_bytes_, 0);
  }

  OnReadComplete(buf.get(), result);
  buf = nullptr;

  ReadRawDataComplete(result);
}

}  // namespace net
