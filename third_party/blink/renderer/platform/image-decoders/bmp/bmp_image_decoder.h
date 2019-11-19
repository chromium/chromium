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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_BMP_BMP_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_BMP_BMP_IMAGE_DECODER_H_

#include <memory>
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"

namespace blink {

class BMPImageReader;
class FastSharedBufferReader;

// This class decodes the BMP image format.
class PLATFORM_EXPORT BMPImageDecoder final : public ImageDecoder {
 public:
  BMPImageDecoder(AlphaOption, const ColorBehavior&, size_t max_decoded_bytes);

  ~BMPImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override { return "bmp"; }
  void OnSetData(SegmentReader*) override;
  // CAUTION: SetFailed() deletes |reader_|.  Be careful to avoid
  // accessing deleted memory, especially when calling this from inside
  // BMPImageReader!
  bool SetFailed() override;

 private:
  // ImageDecoder:
  void DecodeSize() override { Decode(true); }
  void Decode(size_t) override { Decode(false); }

  // Decodes the image.  If |only_size| is true, stops decoding after
  // calculating the image size. If decoding fails but there is no more
  // data coming, sets the "decode failure" flag.
  void Decode(bool only_size);

  // Decodes the image.  If |only_size| is true, stops decoding after
  // calculating the image size. Returns whether decoding succeeded.
  bool DecodeHelper(bool only_size);

  // Processes the file header at the beginning of the data.  Sets
  // |img_data_offset| based on the header contents. Returns true if the
  // file header could be decoded.
  bool ProcessFileHeader(size_t& img_data_offset);

  // Uses |fast_reader| and |buffer| to read the file header into |file_header|.
  // Computes |file_type| from the file header.  Returns whether there was
  // sufficient data available to read the header.
  bool GetFileType(const FastSharedBufferReader& fast_reader,
                   char* buffer,
                   const char*& file_header,
                   uint16_t& file_type) const;

  // An index into |data_| representing how much we've already decoded.
  // Note that this only tracks data _this_ class decodes; once the
  // BMPImageReader takes over this will not be updated further.
  size_t decoded_offset_;

  // The reader used to do most of the BMP decoding.
  std::unique_ptr<BMPImageReader> reader_;
};

}  // namespace blink

#endif
