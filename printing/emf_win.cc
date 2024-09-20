// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "printing/emf_win.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "printing/mojom/print.mojom.h"
#include "skia/ext/skia_utils_win.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

bool DIBFormatNativelySupported(HDC dc,
                                uint32_t escape,
                                const BYTE* bits,
                                int size) {
  BOOL supported = FALSE;
  if (ExtEscape(dc, QUERYESCSUPPORT, sizeof(escape),
                reinterpret_cast<LPCSTR>(&escape), 0, 0) > 0) {
    ExtEscape(dc, escape, size, reinterpret_cast<LPCSTR>(bits),
              sizeof(supported), reinterpret_cast<LPSTR>(&supported));
  }
  return !!supported;
}

}  // namespace

Emf::Emf() : emf_(nullptr), hdc_(nullptr) {}

Emf::~Emf() {
  Close();
}

void Emf::Close() {
  DCHECK(!hdc_);
  if (emf_)
    DeleteEnhMetaFile(emf_);
  emf_ = nullptr;
}

bool Emf::InitToFile(const base::FilePath& metafile_path) {
  DCHECK(!emf_ && !hdc_);
  hdc_ = CreateEnhMetaFile(nullptr, metafile_path.value().c_str(), nullptr,
                           nullptr);
  DCHECK(hdc_);
  return !!hdc_;
}

bool Emf::InitFromFile(const base::FilePath& metafile_path) {
  DCHECK(!emf_ && !hdc_);
  emf_ = GetEnhMetaFile(metafile_path.value().c_str());
  DCHECK(emf_);
  return !!emf_;
}

bool Emf::Init() {
  DCHECK(!emf_ && !hdc_);
  hdc_ = CreateEnhMetaFile(nullptr, nullptr, nullptr, nullptr);
  DCHECK(hdc_);
  return !!hdc_;
}

bool Emf::InitFromData(base::span<const uint8_t> data) {
  DCHECK(!emf_ && !hdc_);
  if (!base::IsValueInRangeForNumericType<UINT>(data.size()))
    return false;

  emf_ = SetEnhMetaFileBits(static_cast<UINT>(data.size()), data.data());
  return !!emf_;
}

bool Emf::FinishDocument() {
  DCHECK(!emf_ && hdc_);
  emf_ = CloseEnhMetaFile(hdc_);
  DCHECK(emf_);
  hdc_ = nullptr;
  return !!emf_;
}

bool Emf::Playback(HDC hdc, const RECT* rect) const {
  DCHECK(emf_ && !hdc_);
  RECT bounds;
  if (!rect) {
    // Get the natural bounds of the EMF buffer.
    bounds = GetPageBounds(1).ToRECT();
    rect = &bounds;
  }
  return PlayEnhMetaFile(hdc, emf_, rect) != 0;
}

bool Emf::SafePlayback(HDC context) const {
  DCHECK(emf_ && !hdc_);
  XFORM base_matrix;
  if (!GetWorldTransform(context, &base_matrix)) {
    NOTREACHED();
  }
  Emf::EnumerationContext playback_context;
  playback_context.base_matrix = &base_matrix;
  gfx::Rect bound = GetPageBounds(1);
  RECT rect = bound.ToRECT();
  return bound.IsEmpty() ||
         EnumEnhMetaFile(context, emf_, &Emf::SafePlaybackProc,
                         reinterpret_cast<void*>(&playback_context),
                         &rect) != 0;
}

gfx::Rect Emf::GetPageBounds(unsigned int page_number) const {
  DCHECK(emf_ && !hdc_);
  DCHECK_EQ(1U, page_number);
  ENHMETAHEADER header;
  if (GetEnhMetaFileHeader(emf_, sizeof(header), &header) != sizeof(header)) {
    NOTREACHED();
  }
  // Add 1 to right and bottom because it's inclusive rectangle.
  // See ENHMETAHEADER.
  return gfx::Rect(header.rclBounds.left, header.rclBounds.top,
                   header.rclBounds.right - header.rclBounds.left + 1,
                   header.rclBounds.bottom - header.rclBounds.top + 1);
}

unsigned int Emf::GetPageCount() const {
  return 1;
}

HDC Emf::context() const {
  return hdc_;
}

uint32_t Emf::GetDataSize() const {
  DCHECK(emf_ && !hdc_);
  return GetEnhMetaFileBits(emf_, 0, nullptr);
}

bool Emf::GetData(void* buffer, uint32_t size) const {
  DCHECK(emf_ && !hdc_);
  DCHECK(buffer && size);
  uint32_t size2 =
      GetEnhMetaFileBits(emf_, size, reinterpret_cast<BYTE*>(buffer));
  DCHECK(size2 == size);
  return size2 == size && size2 != 0;
}

bool Emf::ShouldCopySharedMemoryRegionData() const {
  // `InitFromData()` operates directly upon memory provide to it, so any
  // caller for cases where this data is shared cross-process should have the
  // data copied before it is operated upon.
  return true;
}

mojom::MetafileDataType Emf::GetDataType() const {
  return mojom::MetafileDataType::kEMF;
}

int CALLBACK Emf::SafePlaybackProc(HDC hdc,
                                   HANDLETABLE* handle_table,
                                   const ENHMETARECORD* record,
                                   int objects_count,
                                   LPARAM param) {
  Emf::EnumerationContext* context =
      reinterpret_cast<Emf::EnumerationContext*>(param);
  context->handle_table = handle_table;
  context->objects_count = objects_count;
  context->hdc = hdc;
  Record record_instance(record);
  bool success = record_instance.SafePlayback(context);
  DCHECK(success);
  return 1;
}

PostScriptMetaFile::PostScriptMetaFile() = default;

PostScriptMetaFile::~PostScriptMetaFile() = default;

mojom::MetafileDataType PostScriptMetaFile::GetDataType() const {
  return mojom::MetafileDataType::kPostScriptEmf;
}

bool PostScriptMetaFile::SafePlayback(HDC hdc) const {
  Emf::Enumerator emf_enum(*this, nullptr, nullptr);
  for (const Emf::Record& record : emf_enum) {
    auto* emf_record = record.record();
    if (emf_record->iType != EMR_GDICOMMENT) {
      continue;
    }

    auto* comment = reinterpret_cast<const EMRGDICOMMENT*>(emf_record);
    const char* data = reinterpret_cast<const char*>(comment->Data);
    const uint16_t* ptr = reinterpret_cast<const uint16_t*>(data);
    int ret = ExtEscape(hdc, PASSTHROUGH, 2 + *ptr, data, 0, nullptr);
    DCHECK_EQ(*ptr, ret);
  }
  return true;
}

Emf::EnumerationContext::EnumerationContext() {
  memset(this, 0, sizeof(*this));
}

Emf::Record::Record(const ENHMETARECORD* record) : record_(record) {
  DCHECK(record_);
}

bool Emf::Record::Play(Emf::EnumerationContext* context) const {
  return 0 != PlayEnhMetaFileRecord(context->hdc, context->handle_table,
                                    record_, context->objects_count);
}

bool Emf::Record::SafePlayback(Emf::EnumerationContext* context) const {
  // For EMF field description, see [MS-EMF] Enhanced Metafile Format
  // Specification.
  //
  // This is the second major EMF breakage I get; the first one being
  // SetDCBrushColor/SetDCPenColor/DC_PEN/DC_BRUSH being silently ignored.
  //
  // This function is the guts of the fix for bug 1186598. Some printer drivers
  // somehow choke on certain EMF records, but calling the corresponding
  // function directly on the printer HDC is fine. Still, playing the EMF record
  // fails. Go figure.
  //
  // The main issue is that SetLayout is totally unsupported on these printers
  // (HP 4500/4700). I used to call SetLayout and I stopped. I found out this is
  // not sufficient because GDI32!PlayEnhMetaFile internally calls SetLayout(!)
  // Damn.
  //
  // So I resorted to manually parse the EMF records and play them one by one.
  // The issue with this method compared to using PlayEnhMetaFile to play back
  // an EMF buffer is that the later silently fixes the matrix to take in
  // account the matrix currently loaded at the time of the call.
  // The matrix magic is done transparently when using PlayEnhMetaFile but since
  // I'm processing one field at a time, I need to do the fixup myself. Note
  // that PlayEnhMetaFileRecord doesn't fix the matrix correctly even when
  // called inside an EnumEnhMetaFile loop. Go figure (bis).
  //
  // So when I see a EMR_SETWORLDTRANSFORM and EMR_MODIFYWORLDTRANSFORM, I need
  // to fix the matrix according to the matrix previously loaded before playing
  // back the buffer. Otherwise, the previously loaded matrix would be ignored
  // and the EMF buffer would always be played back at its native resolution.
  // Duh.
  //
  // I also use this opportunity to skip over eventual EMR_SETLAYOUT record that
  // could remain.
  //
  // Another tweak we make is for JPEGs/PNGs in calls to StretchDIBits.
  // (Our Pepper plugin code uses a JPEG). If the printer does not support
  // JPEGs/PNGs natively we decompress the JPEG/PNG and then set it to the
  // device.
  // TODO(sanjeevr): We should also add JPEG/PNG support for SetSIBitsToDevice
  //
  // We also process any custom EMR_GDICOMMENT records which are our
  // placeholders for StartPage and EndPage.
  // Note: I should probably care about view ports and clipping, eventually.
  bool res = false;
  const XFORM* base_matrix = context->base_matrix;
  switch (record()->iType) {
    case EMR_STRETCHDIBITS: {
      const EMRSTRETCHDIBITS* sdib_record =
          reinterpret_cast<const EMRSTRETCHDIBITS*>(record());
      const BYTE* record_start = reinterpret_cast<const BYTE*>(record());
      const BITMAPINFOHEADER* bmih = reinterpret_cast<const BITMAPINFOHEADER*>(
          record_start + sdib_record->offBmiSrc);
      const BYTE* bits = record_start + sdib_record->offBitsSrc;
      bool play_normally = true;
      res = false;
      HDC hdc = context->hdc;
      std::unique_ptr<SkBitmap> bitmap;
      if (bmih->biCompression == BI_JPEG) {
        if (!DIBFormatNativelySupported(hdc, CHECKJPEGFORMAT, bits,
                                        bmih->biSizeImage)) {
          play_normally = false;
          bitmap = gfx::JPEGCodec::Decode(bits, bmih->biSizeImage);
          DCHECK(bitmap);
          DCHECK(!bitmap->isNull());
        }
      } else if (bmih->biCompression == BI_PNG) {
        if (!DIBFormatNativelySupported(hdc, CHECKPNGFORMAT, bits,
                                        bmih->biSizeImage)) {
          play_normally = false;
          bitmap = std::make_unique<SkBitmap>();
          bool png_ok =
              gfx::PNGCodec::Decode(bits, bmih->biSizeImage, &*bitmap);
          DCHECK(png_ok);
          DCHECK(!bitmap->isNull());
        }
      }
      if (play_normally) {
        res = Play(context);
      } else {
        const uint32_t* pixels =
            static_cast<const uint32_t*>(bitmap->getPixels());
        if (!pixels) {
          NOTREACHED();
        }
        BITMAPINFOHEADER bmi = {0};
        skia::CreateBitmapHeaderForN32SkBitmap(*bitmap, &bmi);
        res =
            (0 != StretchDIBits(hdc, sdib_record->xDest, sdib_record->yDest,
                                sdib_record->cxDest, sdib_record->cyDest,
                                sdib_record->xSrc, sdib_record->ySrc,
                                sdib_record->cxSrc, sdib_record->cySrc, pixels,
                                reinterpret_cast<const BITMAPINFO*>(&bmi),
                                sdib_record->iUsageSrc, sdib_record->dwRop));
      }
      break;
    }
    case EMR_SETWORLDTRANSFORM: {
      DCHECK_EQ(record()->nSize, sizeof(DWORD) * 2 + sizeof(XFORM));
      const XFORM* xform = reinterpret_cast<const XFORM*>(record()->dParm);
      HDC hdc = context->hdc;
      if (base_matrix) {
        res = 0 != SetWorldTransform(hdc, base_matrix) &&
              ModifyWorldTransform(hdc, xform, MWT_LEFTMULTIPLY);
      } else {
        res = 0 != SetWorldTransform(hdc, xform);
      }
      break;
    }
    case EMR_MODIFYWORLDTRANSFORM: {
      DCHECK_EQ(record()->nSize,
                sizeof(DWORD) * 2 + sizeof(XFORM) + sizeof(DWORD));
      const XFORM* xform = reinterpret_cast<const XFORM*>(record()->dParm);
      const DWORD* option = reinterpret_cast<const DWORD*>(xform + 1);
      HDC hdc = context->hdc;
      switch (*option) {
        case MWT_IDENTITY:
          if (base_matrix) {
            res = 0 != SetWorldTransform(hdc, base_matrix);
          } else {
            res = 0 != ModifyWorldTransform(hdc, xform, MWT_IDENTITY);
          }
          break;
        case MWT_LEFTMULTIPLY:
        case MWT_RIGHTMULTIPLY:
          res = 0 != ModifyWorldTransform(hdc, xform, *option);
          break;
        case 4:  // MWT_SET
          if (base_matrix) {
            res = 0 != SetWorldTransform(hdc, base_matrix) &&
                  ModifyWorldTransform(hdc, xform, MWT_LEFTMULTIPLY);
          } else {
            res = 0 != SetWorldTransform(hdc, xform);
          }
          break;
        default:
          res = false;
          break;
      }
      break;
    }
    case EMR_SETLAYOUT:
      // Ignore it.
      res = true;
      break;
    default: {
      res = Play(context);
      break;
    }
  }
  return res;
}

void Emf::StartPage(const gfx::Size& /*page_size*/,
                    const gfx::Rect& /*content_area*/,
                    float /*scale_factor*/,
                    mojom::PageOrientation /*page_orientation*/) {}

bool Emf::FinishPage() {
  return true;
}

Emf::Enumerator::Enumerator(const Emf& emf, HDC context, const RECT* rect) {
  items_.clear();
  if (!EnumEnhMetaFile(context, emf.emf(), &Emf::Enumerator::EnhMetaFileProc,
                       reinterpret_cast<void*>(this), rect)) {
    NOTREACHED();
  }
  DCHECK_EQ(context_.hdc, context);
}

Emf::Enumerator::~Enumerator() {}

Emf::Enumerator::const_iterator Emf::Enumerator::begin() const {
  return items_.begin();
}

Emf::Enumerator::const_iterator Emf::Enumerator::end() const {
  return items_.end();
}

int CALLBACK Emf::Enumerator::EnhMetaFileProc(HDC hdc,
                                              HANDLETABLE* handle_table,
                                              const ENHMETARECORD* record,
                                              int objects_count,
                                              LPARAM param) {
  Enumerator& emf = *reinterpret_cast<Enumerator*>(param);
  if (!emf.context_.handle_table) {
    DCHECK(!emf.context_.handle_table);
    DCHECK(!emf.context_.objects_count);
    emf.context_.handle_table = handle_table;
    emf.context_.objects_count = objects_count;
    emf.context_.hdc = hdc;
  } else {
    DCHECK_EQ(emf.context_.handle_table, handle_table);
    DCHECK_EQ(emf.context_.objects_count, objects_count);
    DCHECK_EQ(emf.context_.hdc, hdc);
  }
  emf.items_.push_back(Record(record));
  return 1;
}

}  // namespace printing
