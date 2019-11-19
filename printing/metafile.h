// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_METAFILE_H_
#define PRINTING_METAFILE_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "printing/native_drawing_context.h"
#include "printing/printing_export.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include "base/mac/scoped_cftyperef.h"
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
class PRINTING_EXPORT MetafilePlayer {
 public:
#if defined(OS_MACOSX)
  // |shrink_to_fit| specifies whether the output should be shrunk to fit a
  // destination page if the source PDF is bigger than the destination page in
  // any dimension. If this is false, parts of the source PDF page that lie
  // outside the bounds will be clipped.
  // |stretch_to_fit| specifies whether the output should be stretched to fit
  // the destination page if the source page size is smaller in all dimensions.
  // |center_horizontally| specifies whether the output (after any scaling is
  // done) should be centered horizontally within the destination page.
  // |center_vertically| specifies whether the output (after any scaling is
  // done) should be centered vertically within the destination page.
  // Note that all scaling preserves the original aspect ratio of the page.
  // |autorotate| specifies whether the source PDF should be autorotated to fit
  // on the destination page.
  struct MacRenderPageParams {
    MacRenderPageParams()
        : shrink_to_fit(false),
          stretch_to_fit(false),
          center_horizontally(false),
          center_vertically(false),
          autorotate(false) {}

    bool shrink_to_fit;
    bool stretch_to_fit;
    bool center_horizontally;
    bool center_vertically;
    bool autorotate;
  };
#endif  // defined(OS_MACOSX)
  MetafilePlayer();
  virtual ~MetafilePlayer();

#if defined(OS_WIN)
  // The slow version of Playback(). It enumerates all the records and play them
  // back in the HDC. The trick is that it skip over the records known to have
  // issue with some printers. See Emf::Record::SafePlayback implementation for
  // details.
  virtual bool SafePlayback(printing::NativeDrawingContext hdc) const = 0;

#elif defined(OS_MACOSX)
  // Renders the given page into |rect| in the given context.
  // Pages use a 1-based index. The rendering uses the arguments in
  // |params| to determine scaling, translation, and rotation.
  virtual bool RenderPage(unsigned int page_number,
                          printing::NativeDrawingContext context,
                          const CGRect rect,
                          const MacRenderPageParams& params) const = 0;
#endif  // if defined(OS_WIN)

  // Populates the buffer with the underlying data. This function should ONLY be
  // called after the metafile is closed. Returns true if writing succeeded.
  virtual bool GetDataAsVector(std::vector<char>* buffer) const = 0;

  // Saves the underlying data to the given file. This function should ONLY be
  // called after the metafile is closed. Returns true if writing succeeded.
  virtual bool SaveTo(base::File* file) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetafilePlayer);
};

// This class creates a graphics context that renders into a data stream
// (usually PDF or EMF).
class PRINTING_EXPORT Metafile : public MetafilePlayer {
 public:
  Metafile();
  ~Metafile() override;

  // Initializes a fresh new metafile for rendering. Returns false on failure.
  // Note: It should only be called from within the renderer process to allocate
  // rendering resources.
  virtual bool Init() = 0;

  // Initializes the metafile with the data in |src_buffer|. Returns true
  // on success.
  // Note: It should only be called from within the browser process.
  virtual bool InitFromData(const void* src_buffer, size_t src_buffer_size) = 0;

  // Prepares a context for rendering a new page with the given |page_size|,
  // |content_area| and a |scale_factor| to use for the drawing. The units are
  // in points (=1/72 in).
  virtual void StartPage(const gfx::Size& page_size,
                         const gfx::Rect& content_area,
                         const float& scale_factor) = 0;

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

  // Copies the first |dst_buffer_size| bytes of the underlying data stream into
  // |dst_buffer|. This function should ONLY be called after Close() is invoked.
  // Returns true if the copy succeeds.
  virtual bool GetData(void* dst_buffer, uint32_t dst_buffer_size) const = 0;

  virtual gfx::Rect GetPageBounds(unsigned int page_number) const = 0;
  virtual unsigned int GetPageCount() const = 0;

  virtual printing::NativeDrawingContext context() const = 0;

#if defined(OS_WIN)
  // "Plays" the EMF buffer in a HDC. It is the same effect as calling the
  // original GDI function that were called when recording the EMF. |rect| is in
  // "logical units" and is optional. If |rect| is NULL, the natural EMF bounds
  // are used.
  // Note: Windows has been known to have stack buffer overflow in its GDI
  // functions, whether used directly or indirectly through precompiled EMF
  // data. We have to accept the risk here. Since it is used only for printing,
  // it requires user intervention.
  virtual bool Playback(printing::NativeDrawingContext hdc,
                        const RECT* rect) const = 0;
#endif  // OS_WIN

  // MetfilePlayer
  bool GetDataAsVector(std::vector<char>* buffer) const override;
  bool SaveTo(base::File* file) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(Metafile);
};

}  // namespace printing

#endif  // PRINTING_METAFILE_H_
