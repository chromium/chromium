/*
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * Portions are Copyright (C) 2001-6 mozilla.org
 *
 * Other contributors:
 *   Stuart Parmenter <stuart@mozilla.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "third_party/blink/renderer/platform/image-decoders/exif_reader.h"

#include "base/notreached.h"

namespace blink {

namespace {

constexpr unsigned kExifHeaderSize = 8;

ImageOrientationEnum ImageOrientationFromEXIFValue(unsigned exif_value) {
  switch (exif_value) {
    case 1:
      return ImageOrientationEnum::kOriginTopLeft;
    case 2:
      return ImageOrientationEnum::kOriginTopRight;
    case 3:
      return ImageOrientationEnum::kOriginBottomRight;
    case 4:
      return ImageOrientationEnum::kOriginBottomLeft;
    case 5:
      return ImageOrientationEnum::kOriginLeftTop;
    case 6:
      return ImageOrientationEnum::kOriginRightTop;
    case 7:
      return ImageOrientationEnum::kOriginRightBottom;
    case 8:
      return ImageOrientationEnum::kOriginLeftBottom;
    default:
      // Values direct from images may be invalid, in which case we use the
      // default.
      return ImageOrientationEnum::kDefault;
  }
  NOTREACHED_NORETURN();
}

unsigned ReadUint16(const uint8_t* data, bool is_big_endian) {
  if (is_big_endian) {
    return (data[0] << 8) | data[1];
  }
  return (data[1] << 8) | data[0];
}

unsigned ReadUint32(const uint8_t* data, bool is_big_endian) {
  if (is_big_endian) {
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
  }
  return (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0];
}

float ReadUnsignedRational(const uint8_t* data, bool is_big_endian) {
  unsigned nom = ReadUint32(data, is_big_endian);
  unsigned denom = ReadUint32(data + 4, is_big_endian);
  if (!denom) {
    return 0;
  }
  return float(nom) / float(denom);
}

bool ReadExifHeader(base::span<const uint8_t> data, bool& is_big_endian) {
  // A TIFF file starts with 'I', 'I' (Intel / little endian byte order) or
  // 'M', 'M' (Motorola / big endian byte order), followed by (uint16_t)42,
  // followed by an uint32_t with the offset to the initial (0th) IFD tag
  // block, relative to the TIFF file start.
  //
  // Header in summary:
  // <byte-order tag> (2 bytes), <magic> (2 bytes), <0th IFD offset> (4 bytes)
  if (data.size() < kExifHeaderSize) {
    return false;
  }
  const uint8_t byte_order = data[0];
  if (byte_order != data[1]) {
    return false;
  }
  if (byte_order != 'I' && byte_order != 'M') {
    return false;
  }
  is_big_endian = byte_order == 'M';
  if (ReadUint16(data.data() + 2, is_big_endian) != 42) {
    return false;
  }
  return true;
}

void ReadExifDirectory(const uint8_t* dir_start,
                       const uint8_t* data_start,
                       const uint8_t* data_end,
                       bool is_big_endian,
                       DecodedImageMetaData& metadata,
                       bool is_root = true) {
  const unsigned kUnsignedShortType = 3;
  const unsigned kUnsignedLongType = 4;
  const unsigned kUnsignedRationalType = 5;

  enum ExifTags {
    kOrientationTag = 0x112,
    kResolutionXTag = 0x11a,
    kResolutionYTag = 0x11b,
    kResolutionUnitTag = 0x128,
    kPixelXDimensionTag = 0xa002,
    kPixelYDimensionTag = 0xa003,
    kExifOffsetTag = 0x8769
  };

  if (data_end - dir_start < 2) {
    return;
  }

  const unsigned max_offset =
      base::checked_cast<unsigned>(data_end - data_start);
  unsigned tag_count = ReadUint16(dir_start, is_big_endian);
  const uint8_t* ifd =
      dir_start + 2;  // Skip over the uint16 that was just read.

  // A TIFF image file directory (IFD) consists of a uint16_t describing the
  // number of IFD entries, followed by that many entries. Every IFD entry is 2
  // bytes of tag, 2 bytes of contents datatype, 4 bytes of number-of-elements,
  // and 4 bytes of either offset to the tag data, or if the data is small
  // enough, the inlined data itself.
  const int kIfdEntrySize = 12;
  for (unsigned i = 0; i < tag_count && data_end - ifd >= kIfdEntrySize;
       ++i, ifd += kIfdEntrySize) {
    unsigned tag = ReadUint16(ifd, is_big_endian);
    unsigned type = ReadUint16(ifd + 2, is_big_endian);
    unsigned count = ReadUint32(ifd + 4, is_big_endian);
    const uint8_t* value_ptr = ifd + 8;

    // EXIF stores the value with an offset if it's bigger than 4 bytes, e.g.
    // for rational values.
    if (type == kUnsignedRationalType) {
      unsigned offset = ReadUint32(value_ptr, is_big_endian);
      if (offset > max_offset) {
        continue;
      }
      value_ptr = data_start + offset;
      // Make sure offset points to a valid location.
      if (value_ptr > data_end - 16) {
        continue;
      }
    }

    switch (tag) {
      case ExifTags::kOrientationTag:
        if (type == kUnsignedShortType && count == 1) {
          metadata.orientation = ImageOrientationFromEXIFValue(
              ReadUint16(value_ptr, is_big_endian));
        }
        break;

      case ExifTags::kResolutionUnitTag:
        if (type == kUnsignedShortType && count == 1) {
          metadata.resolution_unit = ReadUint16(value_ptr, is_big_endian);
        }
        break;

      case ExifTags::kResolutionXTag:
        if (type == kUnsignedRationalType && count == 1) {
          metadata.resolution.set_width(
              ReadUnsignedRational(value_ptr, is_big_endian));
        }
        break;

      case ExifTags::kResolutionYTag:
        if (type == kUnsignedRationalType && count == 1) {
          metadata.resolution.set_height(
              ReadUnsignedRational(value_ptr, is_big_endian));
        }
        break;

      case ExifTags::kPixelXDimensionTag:
        if (count != 1) {
          break;
        }
        switch (type) {
          case kUnsignedShortType:
            metadata.size.set_width(ReadUint16(value_ptr, is_big_endian));
            break;
          case kUnsignedLongType:
            metadata.size.set_width(ReadUint32(value_ptr, is_big_endian));
            break;
        }
        break;

      case ExifTags::kPixelYDimensionTag:
        if (count != 1) {
          break;
        }
        switch (type) {
          case kUnsignedShortType:
            metadata.size.set_height(ReadUint16(value_ptr, is_big_endian));
            break;
          case kUnsignedLongType:
            metadata.size.set_height(ReadUint32(value_ptr, is_big_endian));
            break;
        }
        break;

      case ExifTags::kExifOffsetTag:
        if (type == kUnsignedLongType && count == 1 && is_root) {
          unsigned offset = ReadUint32(value_ptr, is_big_endian);
          if (offset > max_offset) {
            break;
          }
          const uint8_t* subdir = data_start + offset;
          ReadExifDirectory(subdir, data_start, data_end, is_big_endian,
                            metadata, false);
        }
        break;
    }
  }
}

}  // namespace

bool ReadExif(base::span<const uint8_t> data, DecodedImageMetaData& metadata) {
  bool is_big_endian;
  if (!ReadExifHeader(data, is_big_endian)) {
    return false;
  }
  const unsigned ifd_offset = ReadUint32(data.data() + 4, is_big_endian);
  if (ifd_offset < kExifHeaderSize || ifd_offset >= data.size()) {
    return false;
  }

  const uint8_t* data_end = data.data() + data.size();
  const uint8_t* data_start = data.data();
  const uint8_t* ifd0 = data_start + ifd_offset;
  ReadExifDirectory(ifd0, data_start, data_end, is_big_endian, metadata);
  return true;
}

}  // namespace blink
