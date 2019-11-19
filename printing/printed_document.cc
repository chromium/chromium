// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/i18n/file_util_icu.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/values.h"
#include "printing/metafile.h"
#include "printing/page_number.h"
#include "printing/print_settings_conversion.h"
#include "printing/units.h"
#include "ui/gfx/font.h"
#include "ui/gfx/text_elider.h"

#if defined(OS_WIN)
#include "printing/printed_page_win.h"
#endif

namespace printing {

namespace {

base::LazyInstance<base::FilePath>::Leaky g_debug_dump_info =
    LAZY_INSTANCE_INITIALIZER;

#if defined(OS_WIN)
void DebugDumpPageTask(const base::string16& doc_name,
                       const PrintedPage* page) {
  DCHECK(PrintedDocument::HasDebugDumpPath());

  static constexpr base::FilePath::CharType kExtension[] =
      FILE_PATH_LITERAL(".emf");

  base::string16 name = doc_name;
  name += base::ASCIIToUTF16(base::StringPrintf("_%04d", page->page_number()));
  base::FilePath path = PrintedDocument::CreateDebugDumpPath(name, kExtension);
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  page->metafile()->SaveTo(&file);
}
#else
void DebugDumpTask(const base::string16& doc_name,
                   const MetafilePlayer* metafile) {
  DCHECK(PrintedDocument::HasDebugDumpPath());

  static constexpr base::FilePath::CharType kExtension[] =
      FILE_PATH_LITERAL(".pdf");

  base::string16 name = doc_name;
  base::FilePath path = PrintedDocument::CreateDebugDumpPath(name, kExtension);
  base::File file(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  metafile->SaveTo(&file);
}
#endif

void DebugDumpDataTask(const base::string16& doc_name,
                       const base::FilePath::StringType& extension,
                       const base::RefCountedMemory* data) {
  base::FilePath path =
      PrintedDocument::CreateDebugDumpPath(doc_name, extension);
  if (path.empty())
    return;
  base::WriteFile(path, reinterpret_cast<const char*>(data->front()),
                  base::checked_cast<int>(data->size()));
}

void DebugDumpSettings(const base::string16& doc_name,
                       const PrintSettings& settings) {
  base::DictionaryValue job_settings;
  PrintSettingsToJobSettingsDebug(settings, &job_settings);
  std::string settings_str;
  base::JSONWriter::WriteWithOptions(
      job_settings, base::JSONWriter::OPTIONS_PRETTY_PRINT, &settings_str);
  scoped_refptr<base::RefCountedMemory> data =
      base::RefCountedString::TakeString(&settings_str);
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&DebugDumpDataTask, doc_name, FILE_PATH_LITERAL(".json"),
                     base::RetainedRef(data)));
}

}  // namespace

PrintedDocument::PrintedDocument(std::unique_ptr<PrintSettings> settings,
                                 const base::string16& name,
                                 int cookie)
    : immutable_(std::move(settings), name, cookie) {
  // If there is a range, set the number of page
  for (const PageRange& range : immutable_.settings_->ranges())
    mutable_.expected_page_count_ += range.to - range.from + 1;

  if (HasDebugDumpPath())
    DebugDumpSettings(name, *immutable_.settings_);
}

PrintedDocument::~PrintedDocument() = default;

#if defined(OS_WIN)
void PrintedDocument::SetConvertingPdf() {
  base::AutoLock lock(lock_);
  mutable_.converting_pdf_ = true;
}

void PrintedDocument::SetPage(int page_number,
                              std::unique_ptr<MetafilePlayer> metafile,
                              float shrink,
                              const gfx::Size& page_size,
                              const gfx::Rect& page_content_rect) {
  // Notice the page_number + 1, the reason is that this is the value that will
  // be shown. Users dislike 0-based counting.
  auto page = base::MakeRefCounted<PrintedPage>(
      page_number + 1, std::move(metafile), page_size, page_content_rect);
  page->set_shrink_factor(shrink);
  {
    base::AutoLock lock(lock_);
    mutable_.pages_[page_number] = page;
  }

  if (HasDebugDumpPath()) {
    base::PostTask(
        FROM_HERE,
        {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&DebugDumpPageTask, name(), base::RetainedRef(page)));
  }
}

scoped_refptr<PrintedPage> PrintedDocument::GetPage(int page_number) {
  scoped_refptr<PrintedPage> page;
  {
    base::AutoLock lock(lock_);
    PrintedPages::const_iterator it = mutable_.pages_.find(page_number);
    if (it != mutable_.pages_.end())
      page = it->second;
  }
  return page;
}

#else
void PrintedDocument::SetDocument(std::unique_ptr<MetafilePlayer> metafile,
                                  const gfx::Size& page_size,
                                  const gfx::Rect& page_content_rect) {
  {
    base::AutoLock lock(lock_);
    mutable_.metafile_ = std::move(metafile);
#if defined(OS_MACOSX)
    mutable_.page_size_ = page_size;
    mutable_.page_content_rect_ = page_content_rect;
#endif
  }

  if (HasDebugDumpPath()) {
    base::PostTask(
        FROM_HERE,
        {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&DebugDumpTask, name(), mutable_.metafile_.get()));
  }
}

const MetafilePlayer* PrintedDocument::GetMetafile() {
  return mutable_.metafile_.get();
}

#endif

bool PrintedDocument::IsComplete() const {
  base::AutoLock lock(lock_);
  if (!mutable_.page_count_)
    return false;
#if defined(OS_WIN)
  if (mutable_.converting_pdf_)
    return true;

  PageNumber page(*immutable_.settings_, mutable_.page_count_);
  if (page == PageNumber::npos())
    return false;

  for (; page != PageNumber::npos(); ++page) {
    PrintedPages::const_iterator it = mutable_.pages_.find(page.ToInt());
    if (it == mutable_.pages_.end() || !it->second.get() ||
        !it->second->metafile()) {
      return false;
    }
  }
  return true;
#else
  return !!mutable_.metafile_;
#endif
}

void PrintedDocument::set_page_count(int max_page) {
  base::AutoLock lock(lock_);
  DCHECK_EQ(0, mutable_.page_count_);
  mutable_.page_count_ = max_page;
  if (immutable_.settings_->ranges().empty()) {
    mutable_.expected_page_count_ = max_page;
  } else {
    // If there is a range, don't bother since expected_page_count_ is already
    // initialized.
    DCHECK_NE(mutable_.expected_page_count_, 0);
  }
}

int PrintedDocument::page_count() const {
  base::AutoLock lock(lock_);
  return mutable_.page_count_;
}

int PrintedDocument::expected_page_count() const {
  base::AutoLock lock(lock_);
  return mutable_.expected_page_count_;
}

// static
void PrintedDocument::SetDebugDumpPath(const base::FilePath& debug_dump_path) {
  DCHECK(!debug_dump_path.empty());
  g_debug_dump_info.Get() = debug_dump_path;
}

// static
bool PrintedDocument::HasDebugDumpPath() {
  return g_debug_dump_info.IsCreated();
}

// static
base::FilePath PrintedDocument::CreateDebugDumpPath(
    const base::string16& document_name,
    const base::FilePath::StringType& extension) {
  DCHECK(HasDebugDumpPath());

  // Create a filename.
  base::string16 filename;
  base::Time now(base::Time::Now());
  filename = base::TimeFormatShortDateAndTime(now);
  filename += base::ASCIIToUTF16("_");
  filename += document_name;
  base::FilePath::StringType system_filename;
#if defined(OS_WIN)
  system_filename = filename;
#else   // OS_WIN
  system_filename = base::UTF16ToUTF8(filename);
#endif  // OS_WIN
  base::i18n::ReplaceIllegalCharactersInPath(&system_filename, '_');
  const auto& dump_path = g_debug_dump_info.Get();
  DCHECK(!dump_path.empty());
  return dump_path.Append(system_filename).AddExtension(extension);
}

void PrintedDocument::DebugDumpData(
    const base::RefCountedMemory* data,
    const base::FilePath::StringType& extension) {
  DCHECK(HasDebugDumpPath());
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&DebugDumpDataTask, name(), extension,
                     base::RetainedRef(data)));
}

#if defined(OS_WIN) || defined(OS_MACOSX)
gfx::Rect PrintedDocument::GetCenteredPageContentRect(
    const gfx::Size& paper_size,
    const gfx::Size& page_size,
    const gfx::Rect& page_content_rect) const {
  gfx::Rect content_rect = page_content_rect;
  if (paper_size.width() > page_size.width()) {
    int diff = paper_size.width() - page_size.width();
    content_rect.set_x(content_rect.x() + diff / 2);
  }
  if (paper_size.height() > page_size.height()) {
    int diff = paper_size.height() - page_size.height();
    content_rect.set_y(content_rect.y() + diff / 2);
  }
  return content_rect;
}
#endif

PrintedDocument::Mutable::Mutable() = default;

PrintedDocument::Mutable::~Mutable() = default;

PrintedDocument::Immutable::Immutable(std::unique_ptr<PrintSettings> settings,
                                      const base::string16& name,
                                      int cookie)
    : settings_(std::move(settings)), name_(name), cookie_(cookie) {}

PrintedDocument::Immutable::~Immutable() = default;

}  // namespace printing
