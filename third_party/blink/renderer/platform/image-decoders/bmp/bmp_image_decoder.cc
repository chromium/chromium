// Copyright 2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_reader.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"

namespace blink {

// Number of bytes in .BMP used to store the file header. This is effectively
// `sizeof(BITMAPFILEHEADER)`, as defined in
// https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapfileheader
static const wtf_size_t kSizeOfFileHeader = 14;

BMPImageDecoder::BMPImageDecoder(AlphaOption alpha_option,
                                 ColorBehavior color_behavior,
                                 wtf_size_t max_decoded_bytes)
    : ImageDecoder(alpha_option,
                   ImageDecoder::kDefaultBitDepth,
                   color_behavior,
                   cc::AuxImage::kDefault,
                   max_decoded_bytes),
      decoded_offset_(0) {}

BMPImageDecoder::~BMPImageDecoder() = default;

String BMPImageDecoder::FilenameExtension() const {
  return "bmp";
}

const AtomicString& BMPImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, bmp_mime_type, ("image/bmp"));
  return bmp_mime_type;
}

void BMPImageDecoder::OnSetData(scoped_refptr<SegmentReader> data) {
  if (reader_) {
    reader_->SetData(std::move(data));
  }
}

bool BMPImageDecoder::SetFailed() {
  reader_.reset();
  return ImageDecoder::SetFailed();
}

void BMPImageDecoder::DecodeSize() {
  Decode(true);
}

void BMPImageDecoder::Decode(wtf_size_t) {
  Decode(false);
}

void BMPImageDecoder::Decode(bool only_size) {
  if (Failed()) {
    return;
  }

  if (!DecodeHelper(only_size) && IsAllDataReceived()) {
    // If we couldn't decode the image but we've received all the data, decoding
    // has failed.
    SetFailed();
  } else if (!frame_buffer_cache_.empty() &&
             (frame_buffer_cache_.front().GetStatus() ==
              ImageFrame::kFrameComplete)) {
    // If we're done decoding the image, we don't need the BMPImageReader
    // anymore.  (If we failed, |reader_| has already been cleared.)
    reader_.reset();
  }
}

bool BMPImageDecoder::DecodeHelper(bool only_size) {
  wtf_size_t img_data_offset = 0;
  if ((decoded_offset_ < kSizeOfFileHeader) &&
      !ProcessFileHeader(img_data_offset)) {
    return false;
  }

  if (!reader_) {
    reader_ = std::make_unique<BMPImageReader>(this, decoded_offset_,
                                               img_data_offset, false);
    reader_->SetData(data_);
  }

  if (!frame_buffer_cache_.empty()) {
    reader_->SetBuffer(&frame_buffer_cache_.front());
  }

  return reader_->DecodeBMP(only_size);
}

bool BMPImageDecoder::ProcessFileHeader(wtf_size_t& img_data_offset) {
  // Read file header.
  DCHECK(!decoded_offset_);
  FastSharedBufferReader fast_reader(data_);
  char buffer[kSizeOfFileHeader];
  const char* file_header;
  uint16_t file_type;
  if (!GetFileType(fast_reader, buffer, file_header, file_type)) {
    return false;
  }

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
    if (!GetFileType(fast_reader, buffer, file_header, file_type)) {
      return false;
    }
  }
  if (file_type != BMAP) {
    return SetFailed();
  }

  img_data_offset = BMPImageReader::ReadUint32(&file_header[10]);
  decoded_offset_ += kSizeOfFileHeader;
  return true;
}

bool BMPImageDecoder::GetFileType(const FastSharedBufferReader& fast_reader,
                                  char* buffer,
                                  const char*& file_header,
                                  uint16_t& file_type) const {
  if (data_->size() - decoded_offset_ < kSizeOfFileHeader) {
    return false;
  }
  file_header = fast_reader.GetConsecutiveData(decoded_offset_,
                                               kSizeOfFileHeader, buffer);
  file_type = (static_cast<uint16_t>(file_header[0]) << 8) |
              static_cast<uint8_t>(file_header[1]);
  return true;
}

}  // namespace blink
