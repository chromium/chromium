// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/drag_download_util.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace content {

bool ParseDownloadMetadata(const std::u16string& metadata,
                           std::u16string* mime_type,
                           base::FilePath* file_name,
                           GURL* url) {
  const char16_t separator = L':';

  size_t mime_type_end_pos = metadata.find(separator);
  if (mime_type_end_pos == std::u16string::npos)
    return false;

  size_t file_name_end_pos = metadata.find(separator, mime_type_end_pos + 1);
  if (file_name_end_pos == std::u16string::npos)
    return false;

  GURL parsed_url = GURL(metadata.substr(file_name_end_pos + 1));
  if (!parsed_url.is_valid())
    return false;

  if (mime_type)
    *mime_type = metadata.substr(0, mime_type_end_pos);
  if (file_name) {
    std::u16string file_name_str = metadata.substr(
        mime_type_end_pos + 1, file_name_end_pos - mime_type_end_pos - 1);
    *file_name = base::FilePath::FromUTF16Unsafe(file_name_str);
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
#if BUILDFLAG(IS_WIN)
      std::wstring suffix = L"-" + base::NumberToWString(seq);
#else
      std::string suffix = "-" + base::NumberToString(seq);
#endif
      new_file_path = file_path->InsertBeforeExtension(suffix);
    }

    // http://crbug.com/110709
    base::ScopedAllowBlocking allow_blocking;

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
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PromiseFileFinalizer::Cleanup, this));
}

void PromiseFileFinalizer::OnDownloadAborted() {
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&PromiseFileFinalizer::Cleanup, this));
}

PromiseFileFinalizer::~PromiseFileFinalizer() {}

void PromiseFileFinalizer::Cleanup() {
  drag_file_downloader_.reset();
}

}  // namespace content
