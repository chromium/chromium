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

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_reader.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"

namespace blink {

// Number of bits in .BMP used to store the file header (doesn't match
// "sizeof(BMPImageDecoder::BitmapFileHeader)" since we omit some fields and
// don't pack).
static const size_t kSizeOfFileHeader = 14;

BMPImageDecoder::BMPImageDecoder(AlphaOption alpha_option,
                                 const ColorBehavior& color_behavior,
                                 size_t max_decoded_bytes)
    : ImageDecoder(alpha_option,
                   ImageDecoder::kDefaultBitDepth,
                   color_behavior,
                   max_decoded_bytes),
      decoded_offset_(0) {}

BMPImageDecoder::~BMPImageDecoder() = default;

void BMPImageDecoder::OnSetData(SegmentReader* data) {
  if (reader_)
    reader_->SetData(data);
}

bool BMPImageDecoder::SetFailed() {
  reader_.reset();
  return ImageDecoder::SetFailed();
}

void BMPImageDecoder::Decode(bool only_size) {
  if (Failed())
    return;

  // If we couldn't decode the image but we've received all the data, decoding
  // has failed.
  if (!DecodeHelper(only_size) && IsAllDataReceived())
    SetFailed();
  // If we're done decoding the image, we don't need the BMPImageReader
  // anymore.  (If we failed, |reader_| has already been cleared.)
  else if (!frame_buffer_cache_.IsEmpty() &&
           (frame_buffer_cache_.front().GetStatus() ==
            ImageFrame::kFrameComplete))
    reader_.reset();
}

bool BMPImageDecoder::DecodeHelper(bool only_size) {
  size_t img_data_offset = 0;
  if ((decoded_offset_ < kSizeOfFileHeader) &&
      !ProcessFileHeader(img_data_offset))
    return false;

  if (!reader_) {
    reader_ = std::make_unique<BMPImageReader>(this, decoded_offset_,
                                               img_data_offset, false);
    reader_->SetData(data_.get());
  }

  if (!frame_buffer_cache_.IsEmpty())
    reader_->SetBuffer(&frame_buffer_cache_.front());

  return reader_->DecodeBMP(only_size);
}

bool BMPImageDecoder::ProcessFileHeader(size_t& img_data_offset) {
  // Read file header.
  DCHECK(!decoded_offset_);
  FastSharedBufferReader fast_reader(data_);
  char buffer[kSizeOfFileHeader];
  const char* file_header;
  uint16_t file_type;
  if (!GetFileType(fast_reader, buffer, file_header, file_type))
    return false;

  // See if this is a bitmap filetype we understand.
  enum {
    BMAP = 0x424D,         // "BM"
    BITMAPARRAY = 0x4241,  // "BA"
    // The following additional OS/2 2.x header values (see
    // http://www.fileformat.info/format/os2bmp/egff.htm ) aren't widely
    // decoded, and are unlikely to be in much use.
    /*
    ICON = 0x4943,  // "IC"
    POINTER = 0x5054,  // "PT"
    COLORICON = 0x4349,  // "CI"
    COLORPOINTER = 0x4350,  // "CP"
    */
  };
  if (file_type == BITMAPARRAY) {
    // Skip initial 14-byte header, try to read the first entry as a BMAP.
    decoded_offset_ += kSizeOfFileHeader;
    if (!GetFileType(fast_reader, buffer, file_header, file_type))
      return false;
  }
  if (file_type != BMAP)
    return SetFailed();

  img_data_offset = BMPImageReader::ReadUint32(&file_header[10]);
  decoded_offset_ += kSizeOfFileHeader;
  return true;
}

bool BMPImageDecoder::GetFileType(const FastSharedBufferReader& fast_reader,
                                  char* buffer,
                                  const char*& file_header,
                                  uint16_t& file_type) const {
  if (data_->size() - decoded_offset_ < kSizeOfFileHeader)
    return false;
  file_header = fast_reader.GetConsecutiveData(decoded_offset_,
                                               kSizeOfFileHeader, buffer);
  file_type = (file_header[0] << 8) | static_cast<uint8_t>(file_header[1]);
  return true;
}

}  // namespace blink
