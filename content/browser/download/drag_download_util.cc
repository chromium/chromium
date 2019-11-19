// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/drag_download_util.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace content {

bool ParseDownloadMetadata(const base::string16& metadata,
                           base::string16* mime_type,
                           base::FilePath* file_name,
                           GURL* url) {
  const base::char16 separator = L':';

  size_t mime_type_end_pos = metadata.find(separator);
  if (mime_type_end_pos == base::string16::npos)
    return false;

  size_t file_name_end_pos = metadata.find(separator, mime_type_end_pos + 1);
  if (file_name_end_pos == base::string16::npos)
    return false;

  GURL parsed_url = GURL(metadata.substr(file_name_end_pos + 1));
  if (!parsed_url.is_valid())
    return false;

  if (mime_type)
    *mime_type = metadata.substr(0, mime_type_end_pos);
  if (file_name) {
    base::string16 file_name_str = metadata.substr(
        mime_type_end_pos + 1, file_name_end_pos - mime_type_end_pos  - 1);
#if defined(OS_WIN)
    *file_name = base::FilePath(file_name_str);
#else
    *file_name = base::FilePath(base::UTF16ToUTF8(file_name_str));
#endif
  }
  if (url)
    *url = parsed_url;

  return true;
}

base::File CreateFileForDrop(base::FilePath* file_path) {
  DCHECK(file_path && !file_path->empty());

  const int kMaxSeq = 99;
  for (int seq = 0; seq <= kMaxSeq; seq++) {
    base::FilePath new_file_path;
    if (seq == 0) {
      new_file_path = *file_path;
    } else {
#if defined(OS_WIN)
      base::string16 suffix =
          base::ASCIIToUTF16("-") + base::NumberToString16(seq);
#else
      std::string suffix = std::string("-") + base::NumberToString(seq);
#endif
      new_file_path = file_path->InsertBeforeExtension(suffix);
    }

    // http://crbug.com/110709
    base::ThreadRestrictions::ScopedAllowIO allow_io;

    base::File file(
        new_file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    if (file.IsValid()) {
      *file_path = new_file_path;
      return file;
    }
  }

  return base::File();
}

PromiseFileFinalizer::PromiseFileFinalizer(
    std::unique_ptr<DragDownloadFile> drag_file_downloader)
    : drag_file_downloader_(std::move(drag_file_downloader)) {}

void PromiseFileFinalizer::OnDownloadCompleted(
    const base::FilePath& file_path) {
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&PromiseFileFinalizer::Cleanup, this));
}

void PromiseFileFinalizer::OnDownloadAborted() {
  base::PostTask(FROM_HERE, {BrowserThread::UI},
                 base::BindOnce(&PromiseFileFinalizer::Cleanup, this));
}

PromiseFileFinalizer::~PromiseFileFinalizer() {}

void PromiseFileFinalizer::Cleanup() {
  drag_file_downloader_.reset();
}

}  // namespace content
