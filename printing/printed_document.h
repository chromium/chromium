// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTED_DOCUMENT_H_
#define PRINTING_PRINTED_DOCUMENT_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "printing/native_drawing_context.h"
#include "printing/print_settings.h"

namespace base {
class RefCountedMemory;
}

namespace printing {

class MetafilePlayer;
class PrintedPage;
class PrintingContext;

// A collection of rendered pages. The settings are immutable. If the print
// settings are changed, a new PrintedDocument must be created.
// Warning: May be accessed from many threads at the same time. Only one thread
// will have write access. Sensible functions are protected by a lock.
// Warning: Once a page is loaded, it cannot be replaced. Pages may be discarded
// under low memory conditions.
class PRINTING_EXPORT PrintedDocument
    : public base::RefCountedThreadSafe<PrintedDocument> {
 public:
  // The cookie shall be unique and has a specific relationship with its
  // originating source and settings.
  PrintedDocument(std::unique_ptr<PrintSettings> settings,
                  const base::string16& name,
                  int cookie);

#if defined(OS_WIN)
  // Indicates that the PDF has been generated and the document is waiting for
  // conversion for printing. This is needed on Windows so that the print job
  // is not cancelled if the web contents dies before PDF conversion finishes.
  void SetConvertingPdf();

  // Sets a page's data. 0-based. Note: locks for a short amount of time.
  void SetPage(int page_number,
               std::unique_ptr<MetafilePlayer> metafile,
               float shrink,
               const gfx::Size& page_size,
               const gfx::Rect& page_content_rect);

  // Retrieves a page. If the page is not available right now, it
  // requests to have this page be rendered and returns NULL.
  // Note: locks for a short amount of time.
  scoped_refptr<PrintedPage> GetPage(int page_number);
#else
  // Sets the document data. Note: locks for a short amount of time.
  void SetDocument(std::unique_ptr<MetafilePlayer> metafile,
                   const gfx::Size& page_size,
                   const gfx::Rect& page_content_rect);

  // Retrieves the metafile with the data to print. Lock must be held when
  // calling this function
  const MetafilePlayer* GetMetafile();
#endif

// Draws the page in the context.
// Note: locks for a short amount of time in debug only.
#if defined(OS_WIN)
  void RenderPrintedPage(const PrintedPage& page,
                         printing::NativeDrawingContext context) const;
#elif defined(OS_POSIX)
  // Draws the document in the context. Returns true on success and false on
  // failure. Fails if context->NewPage() or context->PageDone() fails.
  bool RenderPrintedDocument(PrintingContext* context);
#endif

  // Returns true if all the necessary pages for the settings are already
  // rendered.
  // Note: This function always locks and may parse the whole tree.
  bool IsComplete() const;

  // Sets the number of pages in the document to be rendered. Can only be set
  // once.
  // Note: locks for a short amount of time.
  void set_page_count(int max_page);

  // Number of pages in the document.
  // Note: locks for a short amount of time.
  int page_count() const;

  // Returns the number of expected pages to be rendered. It is a non-linear
  // series if settings().ranges is not empty. It is the same value as
  // document_page_count() otherwise.
  // Note: locks for a short amount of time.
  int expected_page_count() const;

  // Getters. All these items are immutable hence thread-safe.
  const PrintSettings& settings() const { return *immutable_.settings_; }
  const base::string16& name() const { return immutable_.name_; }
  int cookie() const { return immutable_.cookie_; }

  // Sets a path where to dump printing output files for debugging. If never
  // set, no files are generated. |debug_dump_path| must not be empty.
  static void SetDebugDumpPath(const base::FilePath& debug_dump_path);

  // Returns true if SetDebugDumpPath() has been called.
  static bool HasDebugDumpPath();

  // Creates debug file name from given |document_name| and |extension|.
  // |extension| should include the leading dot. e.g. ".pdf"
  // Should only be called when debug dumps are enabled.
  static base::FilePath CreateDebugDumpPath(
      const base::string16& document_name,
      const base::FilePath::StringType& extension);

  // Dump data on blocking task runner.
  // Should only be called when debug dumps are enabled.
  void DebugDumpData(const base::RefCountedMemory* data,
                     const base::FilePath::StringType& extension);

#if defined(OS_WIN) || defined(OS_MACOSX)
  // Get page content rect adjusted based on
  // http://dev.w3.org/csswg/css3-page/#positioning-page-box
  gfx::Rect GetCenteredPageContentRect(const gfx::Size& paper_size,
                                       const gfx::Size& page_size,
                                       const gfx::Rect& content_rect) const;
#endif

 private:
  friend class base::RefCountedThreadSafe<PrintedDocument>;

  ~PrintedDocument();

  // Array of data for each print previewed page.
  using PrintedPages = std::map<int, scoped_refptr<PrintedPage>>;

  // Contains all the mutable stuff. All this stuff MUST be accessed with the
  // lock held.
  struct Mutable {
    Mutable();
    ~Mutable();

    // Number of expected pages to be rendered.
    // Warning: Lock must be held when accessing this member.
    int expected_page_count_ = 0;

    // The total number of pages in the document.
    int page_count_ = 0;

#if defined(OS_WIN)
    // Contains the pages' representation. This is a collection of PrintedPage.
    // Warning: Lock must be held when accessing this member.
    PrintedPages pages_;

    // Whether the PDF is being converted for printing.
    bool converting_pdf_ = false;
#else
    std::unique_ptr<MetafilePlayer> metafile_;
#endif
#if defined(OS_MACOSX)
    gfx::Size page_size_;
    gfx::Rect page_content_rect_;
#endif
  };

  // Contains all the immutable stuff. All this stuff can be accessed without
  // any lock held. This is because it can't be changed after the object's
  // construction.
  struct Immutable {
    Immutable(std::unique_ptr<PrintSettings> settings,
              const base::string16& name,
              int cookie);
    ~Immutable();

    // Print settings used to generate this document. Immutable.
    std::unique_ptr<PrintSettings> settings_;

    // Document name. Immutable.
    base::string16 name_;

    // Cookie to uniquely identify this document. It is used to make sure that a
    // PrintedPage is correctly belonging to the PrintedDocument. Since
    // PrintedPage generation is completely asynchronous, it could be easy to
    // mess up and send the page to the wrong document. It can be viewed as a
    // simpler hash of PrintSettings since a new document is made each time the
    // print settings change.
    int cookie_;
  };

  // All writable data member access must be guarded by this lock. Needs to be
  // mutable since it can be acquired from const member functions.
  mutable base::Lock lock_;

  // All the mutable members.
  Mutable mutable_;

  // All the immutable members.
  const Immutable immutable_;

  DISALLOW_COPY_AND_ASSIGN(PrintedDocument);
};

}  // namespace printing

#endif  // PRINTING_PRINTED_DOCUMENT_H_
