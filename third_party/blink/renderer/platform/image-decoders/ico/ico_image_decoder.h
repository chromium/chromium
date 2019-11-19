/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_ICO_ICO_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_ICO_ICO_IMAGE_DECODER_H_

#include <memory>

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_reader.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"

namespace blink {

class PNGImageDecoder;

// This class decodes the ICO and CUR image formats.
class PLATFORM_EXPORT ICOImageDecoder final : public ImageDecoder {
 public:
  ICOImageDecoder(AlphaOption, const ColorBehavior&, size_t max_decoded_bytes);
  ~ICOImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override { return "ico"; }
  void OnSetData(SegmentReader*) override;
  IntSize Size() const override;
  IntSize FrameSizeAtIndex(size_t) const override;
  bool SetSize(unsigned width, unsigned height) override;
  bool FrameIsReceivedAtIndex(size_t) const override;
  // CAUTION: SetFailed() deletes all readers and decoders.  Be careful to
  // avoid accessing deleted memory, especially when calling this from
  // inside BMPImageReader!
  bool SetFailed() override;
  bool HotSpot(IntPoint&) const override;

 private:
  enum ImageType {
    kUnknown,
    BMP,
    PNG,
  };

  enum FileType {
    ICON = 1,
    CURSOR = 2,
  };

  struct IconDirectoryEntry {
    DISALLOW_NEW();
    IntSize size_;
    uint16_t bit_count_;
    IntPoint hot_spot_;
    uint32_t image_offset_;
    uint32_t byte_size_;
  };

  // Returns true if |a| is a preferable icon entry to |b|.
  // Larger sizes, or greater bitdepths at the same size, are preferable.
  static bool CompareEntries(const IconDirectoryEntry& a,
                             const IconDirectoryEntry& b);

  // ImageDecoder:
  void DecodeSize() override { Decode(0, true); }
  size_t DecodeFrameCount() override;
  void Decode(size_t index) override { Decode(index, false); }

  // TODO (scroggo): These functions are identical to functions in
  // BMPImageReader. Share code?
  inline uint8_t ReadUint8(size_t offset) const {
    return fast_reader_.GetOneByte(decoded_offset_ + offset);
  }

  inline uint16_t ReadUint16(int offset) const {
    char buffer[2];
    const char* data =
        fast_reader_.GetConsecutiveData(decoded_offset_ + offset, 2, buffer);
    return BMPImageReader::ReadUint16(data);
  }

  inline uint32_t ReadUint32(int offset) const {
    char buffer[4];
    const char* data =
        fast_reader_.GetConsecutiveData(decoded_offset_ + offset, 4, buffer);
    return BMPImageReader::ReadUint32(data);
  }

  // If the desired PNGImageDecoder exists, gives it the appropriate data.
  void SetDataForPNGDecoderAtIndex(size_t);

  // Decodes the entry at |index|.  If |only_size| is true, stops decoding
  // after calculating the image size.  If decoding fails but there is no
  // more data coming, sets the "decode failure" flag.
  void Decode(size_t index, bool only_size);

  // Decodes the directory and directory entries at the beginning of the
  // data, and initializes members.  Returns true if all decoding
  // succeeded.  Once this returns true, all entries' sizes are known.
  bool DecodeDirectory();

  // Decodes the specified entry.
  bool DecodeAtIndex(size_t);

  // Processes the ICONDIR at the beginning of the data.  Returns true if
  // the directory could be decoded.
  bool ProcessDirectory();

  // Processes the ICONDIRENTRY records after the directory.  Keeps the
  // "best" entry as the one we'll decode.  Returns true if the entries
  // could be decoded.
  bool ProcessDirectoryEntries();

  // Stores the hot-spot for |index| in |hot_spot| and returns true,
  // or returns false if there is none.
  bool HotSpotAtIndex(size_t index, IntPoint& hot_spot) const;

  // Reads and returns a directory entry from the current offset into
  // |data|.
  IconDirectoryEntry ReadDirectoryEntry();

  // Determines whether the desired entry is a BMP or PNG.  Returns true
  // if the type could be determined.
  ImageType ImageTypeAtIndex(size_t);

  FastSharedBufferReader fast_reader_{nullptr};

  // An index into |data_| representing how much we've already decoded.
  // Note that this only tracks data _this_ class decodes; once the
  // BMPImageReader takes over this will not be updated further.
  size_t decoded_offset_ = 0;

  // Which type of file (ICO/CUR) this is.
  FileType file_type_;

  // The headers for the ICO.
  typedef Vector<IconDirectoryEntry> IconDirectoryEntries;
  IconDirectoryEntries dir_entries_;

  // Count of directory entries is parsed from header before initializing
  // dir_entries_. dir_entries_ is populated only when full header
  // information including directory entries is available.
  size_t dir_entries_count_ = 0;

  // The image decoders for the various frames.
  typedef Vector<std::unique_ptr<BMPImageReader>> BMPReaders;
  BMPReaders bmp_readers_;
  typedef Vector<std::unique_ptr<PNGImageDecoder>> PNGDecoders;
  PNGDecoders png_decoders_;

  // Valid only while a BMPImageReader is decoding, this holds the size
  // for the particular entry being decoded.
  IntSize frame_size_;

  DISALLOW_COPY_AND_ASSIGN(ICOImageDecoder);
};

}  // namespace blink

#endif
