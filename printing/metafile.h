// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_METAFILE_H_
#define PRINTING_METAFILE_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "build/build_config.h"
#include "printing/mojom/print.mojom-forward.h"
#include "printing/native_drawing_context.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#elif BUILDFLAG(IS_APPLE)
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace base {
class File;
}

namespace gfx {
class Rect;
class Size;
}

namespace printing {

// This class plays metafiles from data stream (usually PDF or EMF).
class COMPONENT_EXPORT(PRINTING_METAFILE) MetafilePlayer {
 public:
  MetafilePlayer();
  MetafilePlayer(const MetafilePlayer&) = delete;
  MetafilePlayer& operator=(const MetafilePlayer&) = delete;
  virtual ~MetafilePlayer();

#if BUILDFLAG(IS_WIN)
  // The slow version of Playback(). It enumerates all the records and play them
  // back in the HDC. The trick is that it skip over the records known to have
  // issue with some printers. See Emf::Record::SafePlayback implementation for
  // details.
  virtual bool SafePlayback(printing::NativeDrawingContext hdc) const = 0;

#elif BUILDFLAG(IS_APPLE)
  // Renders the given page into `rect` in the given context.
  // Pages use a 1-based index. `autorotate` determines whether the source PDF
  // should be autorotated to fit on the destination page. `fit_to_page`
  // determines whether the source PDF should be scaled to fit on the
  // destination page.
  virtual bool RenderPage(unsigned int page_number,
                          printing::NativeDrawingContext context,
                          const CGRect& rect,
                          bool autorotate,
                          bool fit_to_page) const = 0;
#endif  // BUILDFLAG(IS_WIN)

  // Populates the buffer with the underlying data. This function should ONLY be
  // called after the metafile is closed. Returns true if writing succeeded.
  virtual bool GetDataAsVector(std::vector<char>* buffer) const = 0;

  // Generates a read-only shared memory region for the underlying data. This
  // function should ONLY be called after the metafile is closed.  The returned
  // region will be invalid if there is an error trying to generate the mapping.
  virtual base::MappedReadOnlyRegion GetDataAsSharedMemoryRegion() const = 0;

  // Determine if a copy of the data should be explicitly made before operating
  // on metafile data.  For security purposes it is important to not operate
  // directly on the metafile data shared across processes, but instead work on
  // a local copy made of such data.  This query determines if such a copy needs
  // to be made by the caller, since not all implementations are required to
  // automatically do so.
  // TODO(crbug.com/40151989)  Eliminate concern about making a copy when the
  // shared memory can't be written by the sender.
  virtual bool ShouldCopySharedMemoryRegionData() const = 0;

  // Identifies the type of encapsulated.
  virtual mojom::MetafileDataType GetDataType() const = 0;

#if BUILDFLAG(IS_ANDROID)
  // Similar to bool SaveTo(base::File* file) const, but write the data to the
  // file descriptor directly. This is because Android doesn't allow file
  // ownership exchange. This function should ONLY be called after the metafile
  // is closed. Returns true if writing succeeded.
  virtual bool SaveToFileDescriptor(int fd) const = 0;
#else
  // Saves the underlying data to the given file. This function should ONLY be
  // called after the metafile is closed. Returns true if writing succeeded.
  virtual bool SaveTo(base::File* file) const = 0;
#endif  // BUILDFLAG(IS_ANDROID)
};

// This class creates a graphics context that renders into a data stream
// (usually PDF or EMF).
class COMPONENT_EXPORT(PRINTING_METAFILE) Metafile : public MetafilePlayer {
 public:
  Metafile();
  Metafile(const Metafile&) = delete;
  Metafile& operator=(const Metafile&) = delete;
  ~Metafile() override;

  // Initializes a fresh new metafile for rendering. Returns false on failure.
  // Note: It should only be called from within the renderer process to allocate
  // rendering resources.
  virtual bool Init() = 0;

  // Initializes the metafile with `data`. Returns true on success.
  virtual bool InitFromData(base::span<const uint8_t> data) = 0;

  // Prepares a context for rendering a new page with the given `page_size`,
  // `content_area` and a `scale_factor` to use for the drawing. The units are
  // in points (=1/72 in).
  virtual void StartPage(const gfx::Size& page_size,
                         const gfx::Rect& content_area,
                         float scale_factor,
                         mojom::PageOrientation page_orientation) = 0;

  // Closes the current page and destroys the context used in rendering that
  // page. The results of current page will be appended into the underlying
  // data stream. Returns true on success.
  virtual bool FinishPage() = 0;

  // Closes the metafile. No further rendering is allowed (the current page
  // is implicitly closed).
  virtual bool FinishDocument() = 0;

  // Returns the size of the underlying data stream. Only valid after Close()
  // has been called.
  virtual uint32_t GetDataSize() const = 0;

  // Copies the first `dst_buffer_size` bytes of the underlying data stream into
  // `dst_buffer`. This function should ONLY be called after Close() is invoked.
  // Returns true if the copy succeeds.
  virtual bool GetData(void* dst_buffer, uint32_t dst_buffer_size) const = 0;

  virtual gfx::Rect GetPageBounds(unsigned int page_number) const = 0;
  virtual unsigned int GetPageCount() const = 0;

  virtual printing::NativeDrawingContext context() const = 0;

#if BUILDFLAG(IS_WIN)
  // "Plays" the EMF buffer in a HDC. It is the same effect as calling the
  // original GDI function that were called when recording the EMF. `rect` is in
  // "logical units" and is optional. If `rect` is NULL, the natural EMF bounds
  // are used.
  // Note: Windows has been known to have stack buffer overflow in its GDI
  // functions, whether used directly or indirectly through precompiled EMF
  // data. We have to accept the risk here. Since it is used only for printing,
  // it requires user intervention.
  virtual bool Playback(printing::NativeDrawingContext hdc,
                        const RECT* rect) const = 0;
#endif  // BUILDFLAG(IS_WIN)

  // MetfilePlayer implementation.
  bool GetDataAsVector(std::vector<char>* buffer) const override;
  base::MappedReadOnlyRegion GetDataAsSharedMemoryRegion() const override;
#if !BUILDFLAG(IS_ANDROID)
  bool SaveTo(base::File* file) const override;
#endif  // !BUILDFLAG(IS_ANDROID)
};

}  // namespace printing

#endif  // PRINTING_METAFILE_H_
