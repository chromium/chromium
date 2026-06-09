// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/emf_win.h"

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "base/check_op.h"
#include "base/compiler_specific.h"
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

const BITMAPINFOHEADER* GetBitmapInfoHeader(
    const EMRSTRETCHDIBITS* sdib_record) {
  // SAFETY: Trust that `emr.nSize` is set correctly.
  auto record_span = UNSAFE_BUFFERS(base::span(
      reinterpret_cast<const uint8_t*>(sdib_record), sdib_record->emr.nSize));

  return reinterpret_cast<const BITMAPINFOHEADER*>(
      record_span.subspan(sdib_record->offBmiSrc).data());
}

const BYTE* GetBitmapBits(const EMRSTRETCHDIBITS* sdib_record) {
  // SAFETY: Trust that `emr.nSize` is set correctly.
  auto record_span = UNSAFE_BUFFERS(base::span(
      reinterpret_cast<const uint8_t*>(sdib_record), sdib_record->emr.nSize));
  return record_span.subspan(sdib_record->offBitsSrc).data();
}

}  // namespace

Emf::Emf() = default;

Emf::~Emf() {
  Close();
}

void Emf::Close() {
  DCHECK(!hdc_);
  if (emf_)
    DeleteEnhMetaFile(emf_);
  emf_ = nullptr;
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
  CHECK(GetWorldTransform(context, &base_matrix));
  Emf::EnumerationContext playback_context(GetDataSize());
  playback_context.base_matrix = &base_matrix;
  gfx::Rect bound = GetPageBounds(1);
  RECT rect = bound.ToRECT();
  return bound.IsEmpty() ||
         ::EnumEnhMetaFile(context, emf_, &Emf::SafePlaybackProc,
                           reinterpret_cast<void*>(&playback_context),
                           &rect) != 0;
}

gfx::Rect Emf::GetPageBounds(unsigned int page_number) const {
  DCHECK(emf_ && !hdc_);
  DCHECK_EQ(1U, page_number);
  ENHMETAHEADER header;
  CHECK_EQ(GetEnhMetaFileHeader(emf_, sizeof(header), &header), sizeof(header));
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
  auto* context = reinterpret_cast<Emf::EnumerationContext*>(param);
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
    const ENHMETARECORD* emf_record = record.record();
    if (emf_record->iType != EMR_GDICOMMENT) {
      continue;
    }

    const auto* comment = reinterpret_cast<const EMRGDICOMMENT*>(emf_record);
    const char* data = reinterpret_cast<const char*>(comment->Data);
    // First uint16_t element in `data` holds the size of the rest of `data`,
    // which is the actual PostScript data. Windows requires this payload size +
    // PS data structure.
    const uint16_t ps_payload_size = *reinterpret_cast<const uint16_t*>(data);
    const uint32_t data_size = 2 + ps_payload_size;
    // Assume value used in PDFium's core/fxge/win32/cpsoutput.cpp is not going
    // to change.
    static constexpr uint16_t kExpectedPdfiumMax = 1024 + 2;
    if (data_size != comment->cbData || data_size > kExpectedPdfiumMax) {
      continue;
    }

    int ret = ExtEscape(hdc, PASSTHROUGH, data_size, data, 0, nullptr);
    DCHECK_EQ(ps_payload_size, ret);
  }
  return true;
}

Emf::EnumerationContext::EnumerationContext(uint32_t metafile_size)
    : remaining_metafile_size(metafile_size) {}

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
      const auto* sdib_record =
          reinterpret_cast<const EMRSTRETCHDIBITS*>(record());
      const BITMAPINFOHEADER* bmih = GetBitmapInfoHeader(sdib_record);
      const BYTE* bits = GetBitmapBits(sdib_record);
      bool play_normally = true;
      res = false;
      HDC hdc = context->hdc;
      SkBitmap bitmap;
      if (bmih->biCompression == BI_JPEG) {
        if (!DIBFormatNativelySupported(hdc, CHECKJPEGFORMAT, bits,
                                        bmih->biSizeImage)) {
          play_normally = false;
          // SAFETY: This interfaces with a system-generated metafile.
          bitmap = gfx::JPEGCodec::Decode(
              UNSAFE_BUFFERS(base::span(bits, bmih->biSizeImage)));
          DCHECK(!bitmap.isNull());
        }
      } else if (bmih->biCompression == BI_PNG) {
        if (!DIBFormatNativelySupported(hdc, CHECKPNGFORMAT, bits,
                                        bmih->biSizeImage)) {
          play_normally = false;
          // SAFETY: This interfaces with a system-generated metafile.
          bitmap = gfx::PNGCodec::Decode(
              UNSAFE_BUFFERS(base::span(bits, bmih->biSizeImage)));
          DCHECK(!bitmap.isNull());
        }
      }
      if (play_normally) {
        res = Play(context);
      } else {
        const uint32_t* pixels =
            static_cast<const uint32_t*>(bitmap.getPixels());
        CHECK(pixels);
        BITMAPINFOHEADER bmi = {0};
        skia::CreateBitmapHeaderForN32SkBitmap(bitmap, &bmi);
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
        res = 0 != ::SetWorldTransform(hdc, base_matrix) &&
              ::ModifyWorldTransform(hdc, xform, MWT_LEFTMULTIPLY);
      } else {
        res = 0 != ::SetWorldTransform(hdc, xform);
      }
      break;
    }
    case EMR_MODIFYWORLDTRANSFORM: {
      CHECK_EQ(record()->nSize, sizeof(EMRMODIFYWORLDTRANSFORM));
      const auto* modify_world_transform =
          reinterpret_cast<const EMRMODIFYWORLDTRANSFORM*>(record());
      HDC hdc = context->hdc;
      switch (modify_world_transform->iMode) {
        case MWT_IDENTITY:
          if (base_matrix) {
            res = 0 != ::SetWorldTransform(hdc, base_matrix);
          } else {
            res = 0 != ::ModifyWorldTransform(
                           hdc, &modify_world_transform->xform, MWT_IDENTITY);
          }
          break;
        case MWT_LEFTMULTIPLY:
        case MWT_RIGHTMULTIPLY:
          res = 0 != ::ModifyWorldTransform(hdc, &modify_world_transform->xform,
                                            modify_world_transform->iMode);
          break;
        case 4:  // MWT_SET
          if (base_matrix) {
            res = 0 != ::SetWorldTransform(hdc, base_matrix) &&
                  ::ModifyWorldTransform(hdc, &modify_world_transform->xform,
                                         MWT_LEFTMULTIPLY);
          } else {
            res = 0 != ::SetWorldTransform(hdc, &modify_world_transform->xform);
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

Emf::Enumerator::Enumerator(const Emf& emf, HDC context, const RECT* rect)
    : context_(emf.GetDataSize()) {
  CHECK(::EnumEnhMetaFile(context, emf.emf(), &Emf::Enumerator::EnhMetaFileProc,
                          this, rect));
  DCHECK_EQ(context_.hdc, context);
}

Emf::Enumerator::~Enumerator() = default;

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
