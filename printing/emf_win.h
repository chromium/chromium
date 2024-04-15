// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_EMF_WIN_H_
#define PRINTING_EMF_WIN_H_

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "printing/metafile.h"

namespace base {
class FilePath;
}

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace printing {

// Simple wrapper class that manage an EMF data stream and its virtual HDC.
class COMPONENT_EXPORT(PRINTING_METAFILE) Emf : public Metafile {
 public:
  class Record;
  class Enumerator;
  struct EnumerationContext;

  // Generates a virtual HDC that will record every GDI commands and compile
  // it in a EMF data stream.
  Emf();
  Emf(const Emf&) = delete;
  Emf& operator=(const Emf&) = delete;
  ~Emf() override;

  // Closes metafile.
  void Close();

  // Generates a new metafile that will record every GDI command, and will
  // be saved to `metafile_path`.
  bool InitToFile(const base::FilePath& metafile_path);

  // Initializes the Emf with the data in `metafile_path`.
  bool InitFromFile(const base::FilePath& metafile_path);

  // Metafile methods.
  bool Init() override;
  bool InitFromData(base::span<const uint8_t> data) override;

  // Inserts a custom GDICOMMENT records indicating StartPage/EndPage calls
  // (since StartPage and EndPage do not work in a metafile DC). Only valid
  // when hdc_ is non-NULL. `page_size`, `content_area`, and `scale_factor` are
  // ignored.
  void StartPage(const gfx::Size& page_size,
                 const gfx::Rect& content_area,
                 float scale_factor,
                 mojom::PageOrientation page_orientation) override;
  bool FinishPage() override;
  bool FinishDocument() override;

  uint32_t GetDataSize() const override;
  bool GetData(void* buffer, uint32_t size) const override;
  bool ShouldCopySharedMemoryRegionData() const override;
  mojom::MetafileDataType GetDataType() const override;

  // Should be passed to Playback to keep the exact same size.
  gfx::Rect GetPageBounds(unsigned int page_number) const override;

  unsigned int GetPageCount() const override;
  HDC context() const override;
  bool Playback(HDC hdc, const RECT* rect) const override;
  bool SafePlayback(HDC hdc) const override;

  HENHMETAFILE emf() const { return emf_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(EmfTest, DC);
  FRIEND_TEST_ALL_PREFIXES(EmfPrintingTest, PageBreak);
  FRIEND_TEST_ALL_PREFIXES(EmfTest, FileBackedEmf);

  // Playbacks safely one EMF record.
  static int CALLBACK SafePlaybackProc(HDC hdc,
                                       HANDLETABLE* handle_table,
                                       const ENHMETARECORD* record,
                                       int objects_count,
                                       LPARAM param);

  // Compiled EMF data handle.
  HENHMETAFILE emf_;

  // Valid when generating EMF data through a virtual HDC.
  HDC hdc_;
};

// Emf subclass that knows how to play back PostScript data embedded as EMF
// comment records.
class COMPONENT_EXPORT(PRINTING_METAFILE) PostScriptMetaFile : public Emf {
 public:
  PostScriptMetaFile();

  PostScriptMetaFile(const PostScriptMetaFile&) = delete;
  PostScriptMetaFile& operator=(const PostScriptMetaFile&) = delete;

  ~PostScriptMetaFile() override;

  // `Emf` overrides:
  mojom::MetafileDataType GetDataType() const override;
  bool SafePlayback(HDC hdc) const override;
};

struct Emf::EnumerationContext {
  EnumerationContext();

  raw_ptr<HANDLETABLE> handle_table;
  int objects_count;
  HDC hdc;
  raw_ptr<const XFORM> base_matrix;
  int dc_on_page_start;
};

// One EMF record. It keeps pointers to the EMF buffer held by Emf::emf_.
// The entries become invalid once Emf::CloseEmf() is called.
class COMPONENT_EXPORT(PRINTING_METAFILE) Emf::Record {
 public:
  // Plays the record.
  bool Play(EnumerationContext* context) const;

  // Plays the record working around quirks with SetLayout,
  // SetWorldTransform and ModifyWorldTransform. See implementation for details.
  bool SafePlayback(EnumerationContext* context) const;

  // Access the underlying EMF record.
  const ENHMETARECORD* record() const { return record_; }

 protected:
  explicit Record(const ENHMETARECORD* record);

 private:
  friend class Emf;
  friend class Enumerator;
  raw_ptr<const ENHMETARECORD> record_;
};

// Retrieves individual records out of a Emf buffer. The main use is to skip
// over records that are unsupported on a specific printer or to play back
// only a part of an EMF buffer.
class COMPONENT_EXPORT(PRINTING_METAFILE) Emf::Enumerator {
 public:
  // Iterator type used for iterating the records.
  typedef std::vector<Record>::const_iterator const_iterator;

  // Enumerates the records at construction time. `hdc` and `rect` are
  // both optional at the same time or must both be valid.
  // Warning: `emf` must be kept valid for the time this object is alive.
  Enumerator(const Emf& emf, HDC hdc, const RECT* rect);
  Enumerator(const Enumerator&) = delete;
  Enumerator& operator=(const Enumerator&) = delete;
  ~Enumerator();

  // Retrieves the first Record.
  const_iterator begin() const;

  // Retrieves the end of the array.
  const_iterator end() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(EmfPrintingTest, Enumerate);

  // Processes one EMF record and saves it in the items_ array.
  static int CALLBACK EnhMetaFileProc(HDC hdc,
                                      HANDLETABLE* handle_table,
                                      const ENHMETARECORD* record,
                                      int objects_count,
                                      LPARAM param);

  // The collection of every EMF records in the currently loaded EMF buffer.
  // Initialized by Enumerate(). It keeps pointers to the EMF buffer held by
  // Emf::emf_. The entries become invalid once Emf::CloseEmf() is called.
  std::vector<Record> items_;

  EnumerationContext context_;
};

}  // namespace printing

#endif  // PRINTING_EMF_WIN_H_
