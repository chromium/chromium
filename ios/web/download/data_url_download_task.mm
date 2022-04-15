// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/download/data_url_download_task.h"

#import "base/check.h"
#import "base/files/file_path.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "ios/web/download/download_result.h"
#import "net/base/data_url.h"
#import "net/base/io_buffer.h"
#import "net/url_request/url_fetcher_response_writer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

DataUrlDownloadTask::DataUrlDownloadTask(WebState* web_state,
                                         const GURL& original_url,
                                         NSString* http_method,
                                         const std::string& content_disposition,
                                         int64_t total_bytes,
                                         const std::string& mime_type,
                                         NSString* identifier,
                                         Delegate* delegate)
    : DownloadTaskImpl(web_state,
                       original_url,
                       http_method,
                       content_disposition,
                       total_bytes,
                       mime_type,
                       identifier,
                       delegate) {
  DCHECK(original_url_.SchemeIs(url::kDataScheme));
}

DataUrlDownloadTask::~DataUrlDownloadTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

NSData* DataUrlDownloadTask::GetResponseData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(writer_);
  net::URLFetcherStringWriter* string_writer = writer_->AsStringWriter();
  if (string_writer) {
    const std::string& data = string_writer->data();
    return [NSData dataWithBytes:data.c_str() length:data.size()];
  }

  net::URLFetcherFileWriter* file_writer = writer_->AsFileWriter();
  if (file_writer) {
    const base::FilePath& path = file_writer->file_path();
    return [NSData
        dataWithContentsOfFile:base::SysUTF8ToNSString(path.AsUTF8Unsafe())];
  }

  return nil;
}

const base::FilePath& DataUrlDownloadTask::GetResponsePath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(writer_);
  net::URLFetcherFileWriter* file_writer = writer_->AsFileWriter();
  if (file_writer) {
    const base::FilePath& path = file_writer->file_path();
    return path;
  }

  static const base::FilePath kEmptyPath;
  return kEmptyPath;
}

void DataUrlDownloadTask::Start(const base::FilePath& path,
                                Destination destination_hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DownloadTaskImpl::Start(path, destination_hint);
  if (destination_hint == Destination::kToMemory) {
    OnWriterInitialized(std::make_unique<net::URLFetcherStringWriter>(),
                        net::OK);
  } else {
    DCHECK(path != base::FilePath());
    auto task_runner = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
    auto writer =
        std::make_unique<net::URLFetcherFileWriter>(task_runner, path);
    net::URLFetcherFileWriter* writer_ptr = writer.get();
    writer_ptr->Initialize(
        base::BindOnce(&DataUrlDownloadTask::OnWriterInitialized,
                       weak_factory_.GetWeakPtr(), std::move(writer)));
  }
}

void DataUrlDownloadTask::OnWriterDownloadFinished(int error_code) {
  // If downloads manager's flag is enabled, keeps the downloaded file. The
  // writer deletes it if it owns it, that's why it shouldn't owns it anymore
  // when the current download is finished.
  // Check if writer_->AsFileWriter() is necessary because in some cases the
  // writer isn't a fileWriter as for PaFGsskit downloads for example.
  if (writer_->AsFileWriter())
    writer_->AsFileWriter()->DisownFile();
  DownloadTaskImpl::OnDownloadFinished(DownloadResult(error_code));
}

void DataUrlDownloadTask::OnWriterInitialized(
    std::unique_ptr<net::URLFetcherResponseWriter> writer,
    int writer_initialization_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kInProgress);
  writer_ = std::move(writer);
  DCHECK(writer_);

  if (writer_initialization_status != net::OK) {
    OnWriterDownloadFinished(writer_initialization_status);
  } else {
    StartDataUrlParsing();
  }
}

void DataUrlDownloadTask::StartDataUrlParsing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mime_type_.clear();
  std::string charset;
  std::string data;
  if (!net::DataURL::Parse(original_url_, &mime_type_, &charset, &data)) {
    OnWriterDownloadFinished(net::ERR_INVALID_URL);
    return;
  }
  auto callback = base::BindOnce(&DataUrlDownloadTask::OnDataUrlWritten,
                                 weak_factory_.GetWeakPtr());
  auto buffer = base::MakeRefCounted<net::IOBuffer>(data.size());
  memcpy(buffer->data(), data.c_str(), data.size());
  int written = writer_->Write(buffer.get(), data.size(), std::move(callback));
  if (written != net::ERR_IO_PENDING) {
    OnDataUrlWritten(written);
  }
}

void DataUrlDownloadTask::OnDataUrlWritten(int bytes_written) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  percent_complete_ = 100;
  total_bytes_ = bytes_written;
  received_bytes_ = total_bytes_;
  auto callback = base::BindOnce(&DataUrlDownloadTask::OnWriterDownloadFinished,
                                 weak_factory_.GetWeakPtr());
  if (writer_->Finish(net::OK, std::move(callback)) != net::ERR_IO_PENDING) {
    OnWriterDownloadFinished(net::OK);
  }
}

}  // namespace web
