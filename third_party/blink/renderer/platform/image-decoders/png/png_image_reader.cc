/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * Portions are Copyright (C) 2001 mozilla.org
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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/image-decoders/png/png_image_reader.h"

#include <memory>
#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"
#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "zlib.h"

namespace {

inline blink::PNGImageDecoder* imageDecoder(png_structp png) {
  return static_cast<blink::PNGImageDecoder*>(png_get_progressive_ptr(png));
}

void PNGAPI pngHeaderAvailable(png_structp png, png_infop) {
  imageDecoder(png)->HeaderAvailable();
}

void PNGAPI pngRowAvailable(png_structp png,
                            png_bytep row,
                            png_uint_32 rowIndex,
                            int state) {
  imageDecoder(png)->RowAvailable(row, rowIndex, state);
}

void PNGAPI pngFrameComplete(png_structp png, png_infop) {
  imageDecoder(png)->FrameComplete();
}

void PNGAPI pngFailed(png_structp png, png_const_charp) {
  longjmp(JMPBUF(png), 1);
}

}  // namespace

namespace blink {

PNGImageReader::PNGImageReader(PNGImageDecoder* decoder,
                               wtf_size_t initial_offset)
    : width_(0),
      height_(0),
      decoder_(decoder),
      initial_offset_(initial_offset),
      read_offset_(initial_offset),
      progressive_decode_offset_(0),
      ihdr_offset_(0),
      idat_offset_(0),
      idat_is_part_of_animation_(false),
      expect_idats_(true),
      is_animated_(false),
      parsed_signature_(false),
      parsed_ihdr_(false),
      parse_completed_(false),
      reported_frame_count_(0),
      next_sequence_number_(0),
      fctl_needs_dat_chunk_(false),
      ignore_animation_(false) {
  png_ = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, pngFailed,
                                nullptr);
  // Configure the PNG encoder to always keep the cICP, cLLi and mDCv chunks if
  // present.
  // TODO(veluca): when libpng starts supporting cICP/cLLi chunks explicitly,
  // remove this code.
  png_set_keep_unknown_chunks(
      png_, PNG_HANDLE_CHUNK_ALWAYS,
      reinterpret_cast<const png_byte*>("cICP\0cLLi\0mDCv"), 3);
  info_ = png_create_info_struct(png_);
  png_set_progressive_read_fn(png_, decoder_, nullptr, pngRowAvailable,
                              pngFrameComplete);
  // This setting ensures that we display images with incorrect CMF bytes.
  // See crbug.com/807324.
  png_set_option(png_, PNG_MAXIMUM_INFLATE_WINDOW, PNG_OPTION_ON);
}

PNGImageReader::~PNGImageReader() {
  png_destroy_read_struct(png_ ? &png_ : nullptr, info_ ? &info_ : nullptr,
                          nullptr);
  DCHECK(!png_ && !info_);
}

// This method reads from the FastSharedBufferReader, starting at offset,
// and returns |length| bytes in the form of a pointer to a const png_byte*.
// This function is used to make it easy to access data from the reader in a
// png friendly way, and pass it to libpng for decoding.
//
// Pre-conditions before using this:
// - |reader|.size() >= |read_offset| + |length|
// - |buffer|.size() >= |length|
// - |length| <= |kPngReadBufferSize|
//
// The reason for the last two precondition is that currently the png signature
// plus IHDR chunk (8B + 25B = 33B) is the largest chunk that is read using this
// method. If the data is not consecutive, it is stored in |buffer|, which must
// have the size of (at least) |length|, but there's no need for it to be larger
// than |kPngReadBufferSize|.
static constexpr wtf_size_t kPngReadBufferSize = 33;
const png_byte* ReadAsConstPngBytep(const FastSharedBufferReader& reader,
                                    wtf_size_t read_offset,
                                    wtf_size_t length,
                                    char* buffer) {
  DCHECK_LE(length, kPngReadBufferSize);
  return reinterpret_cast<const png_byte*>(
      reader.GetConsecutiveData(read_offset, length, buffer));
}

bool PNGImageReader::ShouldDecodeWithNewPNG(wtf_size_t index) const {
  if (!png_) {
    return true;
  }
  const bool first_frame_decode_in_progress = progressive_decode_offset_;
  const bool frame_size_matches_ihdr =
      frame_info_[index].frame_rect == gfx::Rect(0, 0, width_, height_);
  if (index) {
    return first_frame_decode_in_progress || !frame_size_matches_ihdr;
  }
  return !first_frame_decode_in_progress && !frame_size_matches_ihdr;
}

// Return false on a fatal error.
bool PNGImageReader::Decode(SegmentReader& data, wtf_size_t index) {
  if (index >= frame_info_.size()) {
    return true;
  }

  const FastSharedBufferReader reader(&data);

  if (!is_animated_) {
    if (setjmp(JMPBUF(png_))) {
      return false;
    }
    DCHECK_EQ(0u, index);
    progressive_decode_offset_ += ProcessData(
        reader, frame_info_[0].start_offset + progressive_decode_offset_, 0);
    return true;
  }

  DCHECK(is_animated_);

  const bool decode_with_new_png = ShouldDecodeWithNewPNG(index);
  if (decode_with_new_png) {
    ClearDecodeState(0);
    png_ = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, pngFailed,
                                  nullptr);
    info_ = png_create_info_struct(png_);
    png_set_progressive_read_fn(png_, decoder_, pngHeaderAvailable,
                                pngRowAvailable, pngFrameComplete);
  }

  if (setjmp(JMPBUF(png_))) {
    return false;
  }

  if (decode_with_new_png) {
    StartFrameDecoding(reader, index);
  }

  if (!index && (!FirstFrameFullyReceived() || progressive_decode_offset_)) {
    const bool decoded_entire_frame = ProgressivelyDecodeFirstFrame(reader);
    if (!decoded_entire_frame) {
      return true;
    }
    progressive_decode_offset_ = 0;
  } else {
    DecodeFrame(reader, index);
  }

  static png_byte iend[12] = {0, 0, 0, 0, 'I', 'E', 'N', 'D', 174, 66, 96, 130};
  png_process_data(png_, info_, iend, 12);
  png_destroy_read_struct(&png_, &info_, nullptr);
  DCHECK(!png_ && !info_);

  return true;
}

void PNGImageReader::StartFrameDecoding(const FastSharedBufferReader& reader,
                                        wtf_size_t index) {
  DCHECK_GT(ihdr_offset_, initial_offset_);
  ProcessData(reader, initial_offset_, ihdr_offset_ - initial_offset_);

  const gfx::Rect& frame_rect = frame_info_[index].frame_rect;
  if (frame_rect == gfx::Rect(0, 0, width_, height_)) {
    DCHECK_GT(idat_offset_, ihdr_offset_);
    ProcessData(reader, ihdr_offset_, idat_offset_ - ihdr_offset_);
    return;
  }

  // Process the IHDR chunk, but change the width and height so it reflects
  // the frame's width and height. ImageDecoder will apply the x,y offset.
  constexpr wtf_size_t kHeaderSize = 25;
  char read_buffer[kHeaderSize];
  const png_byte* chunk =
      ReadAsConstPngBytep(reader, ihdr_offset_, kHeaderSize, read_buffer);
  png_byte* header = reinterpret_cast<png_byte*>(read_buffer);
  if (chunk != header) {
    memcpy(header, chunk, kHeaderSize);
  }
  png_save_uint_32(header + 8, frame_rect.width());
  png_save_uint_32(header + 12, frame_rect.height());
  // IHDR has been modified, so tell libpng to ignore CRC errors.
  png_set_crc_action(png_, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
  png_process_data(png_, info_, header, kHeaderSize);

  // Process the rest of the header chunks.
  DCHECK_GE(idat_offset_, ihdr_offset_ + kHeaderSize);
  ProcessData(reader, ihdr_offset_ + kHeaderSize,
              idat_offset_ - ihdr_offset_ - kHeaderSize);
}

// Determine if the bytes 4 to 7 of |chunk| indicate that it is a |tag| chunk.
// - The length of |chunk| must be >= 8
// - The length of |tag| must be = 4
static inline bool IsChunk(const png_byte* chunk, const char* tag) {
  return memcmp(chunk + 4, tag, 4) == 0;
}

bool PNGImageReader::ProgressivelyDecodeFirstFrame(
    const FastSharedBufferReader& reader) {
  wtf_size_t offset = frame_info_[0].start_offset;

  // Loop while there is enough data to do progressive decoding.
  while (reader.size() >= offset + 8) {
    char read_buffer[8];
    // At the beginning of each loop, the offset is at the start of a chunk.
    const png_byte* chunk = ReadAsConstPngBytep(reader, offset, 8, read_buffer);

    // A large length would have been rejected in Parse.
    const png_uint_32 length = png_get_uint_32(chunk);
    DCHECK_LE(length, PNG_UINT_31_MAX);

    // When an fcTL or IEND chunk is encountered, the frame data has ended.
    if (IsChunk(chunk, "fcTL") || IsChunk(chunk, "IEND")) {
      return true;
    }

    const wtf_size_t chunk_end_offset = offset + length + 12;
    DCHECK_GT(chunk_end_offset, offset);

    // If this chunk was already decoded, move on to the next.
    if (progressive_decode_offset_ >= chunk_end_offset) {
      offset = chunk_end_offset;
      continue;
    }

    // Three scenarios are possible here:
    // 1) Some bytes of this chunk were already decoded in a previous call.
    //    Continue from there.
    // 2) This is an fdAT chunk. Convert it to an IDAT chunk to decode.
    // 3) This is any other chunk. Pass it to libpng for processing.
    if (progressive_decode_offset_ >= offset + 8) {
      offset = progressive_decode_offset_;
    } else if (IsChunk(chunk, "fdAT")) {
      ProcessFdatChunkAsIdat(length);
      // Skip the sequence number.
      offset += 12;
    } else {
      png_process_data(png_, info_, const_cast<png_byte*>(chunk), 8);
      offset += 8;
    }

    wtf_size_t bytes_left_in_chunk = chunk_end_offset - offset;
    wtf_size_t bytes_decoded = ProcessData(reader, offset, bytes_left_in_chunk);
    progressive_decode_offset_ = offset + bytes_decoded;
    if (bytes_decoded < bytes_left_in_chunk) {
      return false;
    }
    offset += bytes_decoded;
  }

  return false;
}

void PNGImageReader::ProcessFdatChunkAsIdat(png_uint_32 fdat_length) {
  // An fdAT chunk is built as follows:
  // - |length| (4B)
  // - fdAT tag (4B)
  // - sequence number (4B)
  // - frame data (|length| - 4B)
  // - CRC (4B)
  // Thus, to reformat this into an IDAT chunk, do the following:
  // - write |length| - 4 as the new length, since the sequence number
  //   must be removed.
  // - change the tag to IDAT.
  // - omit the sequence number from the data part of the chunk.
  png_byte chunk_idat[] = {0, 0, 0, 0, 'I', 'D', 'A', 'T'};
  DCHECK_GE(fdat_length, 4u);
  png_save_uint_32(chunk_idat, fdat_length - 4u);
  // The CRC is incorrect when applied to the modified fdAT.
  png_set_crc_action(png_, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
  png_process_data(png_, info_, chunk_idat, 8);
}

void PNGImageReader::DecodeFrame(const FastSharedBufferReader& reader,
                                 wtf_size_t index) {
  wtf_size_t offset = frame_info_[index].start_offset;
  wtf_size_t end_offset = offset + frame_info_[index].byte_length;
  char read_buffer[8];

  while (offset < end_offset) {
    const png_byte* chunk = ReadAsConstPngBytep(reader, offset, 8, read_buffer);
    const png_uint_32 length = png_get_uint_32(chunk);
    DCHECK_LE(length, PNG_UINT_31_MAX);

    if (IsChunk(chunk, "fdAT")) {
      ProcessFdatChunkAsIdat(length);
      // The frame data and the CRC span |length| bytes, so skip the
      // sequence number and process |length| bytes to decode the frame.
      ProcessData(reader, offset + 12, length);
    } else {
      png_process_data(png_, info_, const_cast<png_byte*>(chunk), 8);
      ProcessData(reader, offset + 8, length + 4);
    }

    offset += 12 + length;
  }
}

// Compute the CRC and compare to the stored value.
static bool CheckCrc(const FastSharedBufferReader& reader,
                     wtf_size_t chunk_start,
                     wtf_size_t chunk_length) {
  constexpr wtf_size_t kSizeNeededForfcTL = 26 + 4;
  char read_buffer[kSizeNeededForfcTL];
  DCHECK_LE(chunk_length + 4u, kSizeNeededForfcTL);
  const png_byte* chunk = ReadAsConstPngBytep(reader, chunk_start + 4,
                                              chunk_length + 4, read_buffer);

  char crc_buffer[4];
  const png_byte* crc_position = ReadAsConstPngBytep(
      reader, chunk_start + 8 + chunk_length, 4, crc_buffer);
  png_uint_32 crc = png_get_uint_32(crc_position);
  return crc == crc32(crc32(0, Z_NULL, 0), chunk, chunk_length + 4);
}

bool PNGImageReader::CheckSequenceNumber(const png_byte* position) {
  png_uint_32 sequence = png_get_uint_32(position);
  if (sequence != next_sequence_number_ || sequence > PNG_UINT_31_MAX) {
    return false;
  }

  ++next_sequence_number_;
  return true;
}

// Return false if there was a fatal error; true otherwise.
bool PNGImageReader::Parse(SegmentReader& data, ParseQuery query) {
  if (parse_completed_) {
    return true;
  }

  const FastSharedBufferReader reader(&data);

  if (!ParseSize(reader)) {
    return false;
  }

  if (!decoder_->IsDecodedSizeAvailable()) {
    return true;
  }

  // For non animated images (identified by no acTL chunk before the IDAT),
  // there is no need to continue parsing.
  if (!is_animated_) {
    FrameInfo frame;
    frame.start_offset = read_offset_;
    // This should never be read in this case, but initialize just in case.
    frame.byte_length = kFirstFrameIndicator;
    frame.duration = 0;
    frame.frame_rect = gfx::Rect(0, 0, width_, height_);
    frame.disposal_method = ImageFrame::DisposalMethod::kDisposeKeep;
    frame.alpha_blend = ImageFrame::AlphaBlendSource::kBlendAtopBgcolor;
    DCHECK(frame_info_.empty());
    frame_info_.push_back(frame);
    parse_completed_ = true;
    return true;
  }

  if (query == ParseQuery::kSize) {
    return true;
  }

  DCHECK_EQ(ParseQuery::kMetaData, query);
  DCHECK(is_animated_);

  // Loop over the data and manually register all frames. Nothing is passed to
  // libpng for processing. A frame is registered on the next fcTL chunk or
  // when the IEND chunk is found. This ensures that only complete frames are
  // reported, unless there is an error in the stream.
  char read_buffer[kPngReadBufferSize];
  for (;;) {
    constexpr wtf_size_t kChunkHeaderSize = 8;
    wtf_size_t chunk_start_offset;
    if (!base::CheckAdd(read_offset_, kChunkHeaderSize)
             .AssignIfValid(&chunk_start_offset)) {
      // Overflow.
      return false;
    }
    if (reader.size() < chunk_start_offset) {
      // Insufficient data to decode the next chunk header.
      break;
    }
    const png_byte* chunk = ReadAsConstPngBytep(reader, read_offset_,
                                                kChunkHeaderSize, read_buffer);
    const wtf_size_t length = png_get_uint_32(chunk);
    if (length > PNG_UINT_31_MAX) {
      return false;
    }
    wtf_size_t chunk_end_offset;
    if (!base::CheckAdd(read_offset_, base::CheckAdd(12, length))
             .AssignIfValid(&chunk_end_offset)) {
      // Overflow.
      return false;
    }

    const bool idat = IsChunk(chunk, "IDAT");
    if (idat && !expect_idats_) {
      return false;
    }

    const bool fdat = IsChunk(chunk, "fdAT");
    if (fdat && expect_idats_) {
      return false;
    }

    if (fdat || (idat && idat_is_part_of_animation_)) {
      fctl_needs_dat_chunk_ = false;
      if (!new_frame_.start_offset) {
        // Beginning of a new frame's data.
        new_frame_.start_offset = read_offset_;

        if (frame_info_.empty()) {
          // This is the first frame. Report it immediately so it can be
          // decoded progressively.
          new_frame_.byte_length = kFirstFrameIndicator;
          frame_info_.push_back(new_frame_);
        }
      }

      if (fdat) {
        if (length < 4) {
          // The sequence number requires 4 bytes. Further,
          // ProcessFdatChunkAsIdat expects to be able to create an IDAT with
          // |newLength| = length - 4. Prevent underflow in that calculation.
          return false;
        }
        if (reader.size() < read_offset_ + 8 + 4) {
          return true;
        }
        const png_byte* sequence_position =
            ReadAsConstPngBytep(reader, read_offset_ + 8, 4, read_buffer);
        if (!CheckSequenceNumber(sequence_position)) {
          return false;
        }
      }

    } else if (IsChunk(chunk, "fcTL") || IsChunk(chunk, "IEND")) {
      // This marks the end of the previous frame.
      if (new_frame_.start_offset) {
        new_frame_.byte_length = read_offset_ - new_frame_.start_offset;
        if (frame_info_[0].byte_length == kFirstFrameIndicator) {
          frame_info_[0].byte_length = new_frame_.byte_length;
        } else {
          frame_info_.push_back(new_frame_);
          if (IsChunk(chunk, "fcTL")) {
            if (frame_info_.size() >= reported_frame_count_) {
              return false;
            }
          } else {  // IEND
            if (frame_info_.size() != reported_frame_count_) {
              return false;
            }
          }
        }

        new_frame_.start_offset = 0;
      }

      if (reader.size() < chunk_end_offset) {
        return true;
      }

      if (IsChunk(chunk, "IEND")) {
        parse_completed_ = true;
        return true;
      }

      if (length != 26 || !CheckCrc(reader, read_offset_, length)) {
        return false;
      }

      chunk =
          ReadAsConstPngBytep(reader, read_offset_ + 8, length, read_buffer);
      if (!ParseFrameInfo(chunk)) {
        return false;
      }

      expect_idats_ = false;
    } else if (IsChunk(chunk, "acTL")) {
      // There should only be one acTL chunk, and it should be before the
      // IDAT chunk.
      return false;
    }

    read_offset_ = chunk_end_offset;
  }
  return true;
}

// If |length| == 0, read until the stream ends. Return number of bytes
// processed.
wtf_size_t PNGImageReader::ProcessData(const FastSharedBufferReader& reader,
                                       wtf_size_t offset,
                                       wtf_size_t length) {
  const char* segment;
  wtf_size_t total_processed_bytes = 0;
  while (reader.size() > offset) {
    size_t segment_length = reader.GetSomeData(segment, offset);
    if (length > 0 && segment_length + total_processed_bytes > length) {
      segment_length = length - total_processed_bytes;
    }

    png_process_data(png_, info_,
                     reinterpret_cast<png_byte*>(const_cast<char*>(segment)),
                     segment_length);
    offset += segment_length;
    total_processed_bytes += segment_length;
    if (total_processed_bytes == length) {
      return length;
    }
  }
  return total_processed_bytes;
}

// Process up to the start of the IDAT with libpng.
// Return false for a fatal error. True otherwise.
bool PNGImageReader::ParseSize(const FastSharedBufferReader& reader) {
  if (decoder_->IsDecodedSizeAvailable()) {
    return true;
  }

  char read_buffer[kPngReadBufferSize];

  if (setjmp(JMPBUF(png_))) {
    return false;
  }

  if (!parsed_signature_) {
    constexpr wtf_size_t kNumSignatureBytes = 8;
    wtf_size_t signature_end_offset;
    if (!base::CheckAdd(read_offset_, kNumSignatureBytes)
             .AssignIfValid(&signature_end_offset)) {
      return false;
    }
    if (reader.size() < signature_end_offset) {
      return true;
    }
    const png_byte* chunk = ReadAsConstPngBytep(
        reader, read_offset_, kNumSignatureBytes, read_buffer);
    png_process_data(png_, info_, const_cast<png_byte*>(chunk),
                     kNumSignatureBytes);
    read_offset_ = signature_end_offset;
    parsed_signature_ = true;
    new_frame_.start_offset = 0;
  }

  // Process some chunks manually, and pass some to libpng.
  for (png_uint_32 length = 0; reader.size() >= read_offset_ + 8;
       // This call will not overflow since it was already checked below, after
       // calculating chunk_end_offset.
       read_offset_ += length + 12) {
    const png_byte* chunk =
        ReadAsConstPngBytep(reader, read_offset_, 8, read_buffer);
    length = png_get_uint_32(chunk);
    if (length > PNG_UINT_31_MAX) {
      return false;
    }
    wtf_size_t chunk_end_offset;
    if (!base::CheckAdd(read_offset_, base::CheckAdd(12, length))
             .AssignIfValid(&chunk_end_offset)) {
      // Overflow
      return false;
    }

    if (IsChunk(chunk, "IDAT")) {
      // Done with header chunks.
      idat_offset_ = read_offset_;
      fctl_needs_dat_chunk_ = false;
      if (ignore_animation_) {
        is_animated_ = false;
      }
      // SetSize() requires bit depth information to correctly fallback to 8888
      // decoding if there is not enough memory to decode to f16 pixel format.
      // SetBitDepth() requires repition count to correctly fallback to 8888
      // decoding for multi-frame APNGs (https://crbug.com/874057). Therefore,
      // the order of the next three calls matters.
      if (!is_animated_ || 1 == reported_frame_count_) {
        decoder_->SetRepetitionCount(kAnimationNone);
      }
      decoder_->SetBitDepth();
      if (!decoder_->SetSize(width_, height_)) {
        return false;
      }
      decoder_->SetColorSpace();
      decoder_->HeaderAvailable();
      return true;
    }

    // Wait until the entire chunk is available for parsing simplicity.
    if (reader.size() < chunk_end_offset) {
      break;
    }

    if (IsChunk(chunk, "acTL")) {
      if (ignore_animation_) {
        continue;
      }
      if (is_animated_ || length != 8 || !parsed_ihdr_ ||
          !CheckCrc(reader, read_offset_, 8)) {
        ignore_animation_ = true;
        continue;
      }
      chunk =
          ReadAsConstPngBytep(reader, read_offset_ + 8, length, read_buffer);
      reported_frame_count_ = png_get_uint_32(chunk);
      if (!reported_frame_count_ || reported_frame_count_ > PNG_UINT_31_MAX) {
        ignore_animation_ = true;
        continue;
      }
      png_uint_32 repetition_count = png_get_uint_32(chunk + 4);
      if (repetition_count > PNG_UINT_31_MAX) {
        ignore_animation_ = true;
        continue;
      }
      is_animated_ = true;
      decoder_->SetRepetitionCount(static_cast<int>(repetition_count) - 1);
    } else if (IsChunk(chunk, "fcTL")) {
      if (ignore_animation_) {
        continue;
      }
      if (length != 26 || !parsed_ihdr_ ||
          !CheckCrc(reader, read_offset_, 26)) {
        ignore_animation_ = true;
        continue;
      }
      chunk =
          ReadAsConstPngBytep(reader, read_offset_ + 8, length, read_buffer);
      if (!ParseFrameInfo(chunk) ||
          new_frame_.frame_rect != gfx::Rect(0, 0, width_, height_)) {
        ignore_animation_ = true;
        continue;
      }
      idat_is_part_of_animation_ = true;
    } else if (IsChunk(chunk, "fdAT")) {
      ignore_animation_ = true;
    } else {
      auto is_necessary_ancillary = [](const png_byte* chunk) {
        for (const char* tag : {"tRNS", "cHRM", "iCCP", "sRGB", "gAMA", "cICP",
                                "cLLi", "mDCv", "eXIf"}) {
          if (IsChunk(chunk, tag)) {
            return true;
          }
        }
        return false;
      };
      // Determine if the chunk type of |chunk| is "critical".
      // (Ancillary bit == 0; the chunk is required for display).
      bool is_critical_chunk = (chunk[4] & 1u << 5) == 0;
      if (is_critical_chunk || is_necessary_ancillary(chunk)) {
        png_process_data(png_, info_, const_cast<png_byte*>(chunk), 8);
        ProcessData(reader, read_offset_ + 8, length + 4);
        if (IsChunk(chunk, "IHDR")) {
          parsed_ihdr_ = true;
          ihdr_offset_ = read_offset_;
          width_ = png_get_image_width(png_, info_);
          height_ = png_get_image_height(png_, info_);
        }
      }
    }
  }

  // Not enough data to call HeaderAvailable.
  return true;
}

void PNGImageReader::ClearDecodeState(wtf_size_t index) {
  if (index) {
    return;
  }
  png_destroy_read_struct(png_ ? &png_ : nullptr, info_ ? &info_ : nullptr,
                          nullptr);
  DCHECK(!png_ && !info_);
  progressive_decode_offset_ = 0;
}

const PNGImageReader::FrameInfo& PNGImageReader::GetFrameInfo(
    wtf_size_t index) const {
  DCHECK(index < frame_info_.size());
  return frame_info_[index];
}

// Extract the fcTL frame control info and store it in new_frame_. The length
// check on the fcTL data has been done by the calling code.
bool PNGImageReader::ParseFrameInfo(const png_byte* data) {
  if (fctl_needs_dat_chunk_) {
    return false;
  }

  png_uint_32 frame_width = png_get_uint_32(data + 4);
  png_uint_32 frame_height = png_get_uint_32(data + 8);
  png_uint_32 x_offset = png_get_uint_32(data + 12);
  png_uint_32 y_offset = png_get_uint_32(data + 16);
  png_uint_16 delay_numerator = png_get_uint_16(data + 20);
  png_uint_16 delay_denominator = png_get_uint_16(data + 22);

  if (!CheckSequenceNumber(data)) {
    return false;
  }
  if (!frame_width || !frame_height) {
    return false;
  }
  {
    png_uint_32 frame_right;
    if (!base::CheckAdd(x_offset, frame_width).AssignIfValid(&frame_right) ||
        frame_right > width_) {
      return false;
    }
  }
  {
    png_uint_32 frame_bottom;
    if (!base::CheckAdd(y_offset, frame_height).AssignIfValid(&frame_bottom) ||
        frame_bottom > height_) {
      return false;
    }
  }

  new_frame_.frame_rect =
      gfx::Rect(x_offset, y_offset, frame_width, frame_height);

  if (delay_denominator) {
    new_frame_.duration = delay_numerator * 1000 / delay_denominator;
  } else {
    new_frame_.duration = delay_numerator * 10;
  }

  enum DisposeOperations : png_byte {
    kAPNG_DISPOSE_OP_NONE = 0,
    kAPNG_DISPOSE_OP_BACKGROUND = 1,
    kAPNG_DISPOSE_OP_PREVIOUS = 2,
  };
  const png_byte& dispose_op = data[24];
  switch (dispose_op) {
    case kAPNG_DISPOSE_OP_NONE:
      new_frame_.disposal_method = ImageFrame::DisposalMethod::kDisposeKeep;
      break;
    case kAPNG_DISPOSE_OP_BACKGROUND:
      new_frame_.disposal_method =
          ImageFrame::DisposalMethod::kDisposeOverwriteBgcolor;
      break;
    case kAPNG_DISPOSE_OP_PREVIOUS:
      new_frame_.disposal_method =
          ImageFrame::DisposalMethod::kDisposeOverwritePrevious;
      break;
    default:
      return false;
  }

  enum BlendOperations : png_byte {
    kAPNG_BLEND_OP_SOURCE = 0,
    kAPNG_BLEND_OP_OVER = 1,
  };
  const png_byte& blend_op = data[25];
  switch (blend_op) {
    case kAPNG_BLEND_OP_SOURCE:
      new_frame_.alpha_blend = ImageFrame::AlphaBlendSource::kBlendAtopBgcolor;
      break;
    case kAPNG_BLEND_OP_OVER:
      new_frame_.alpha_blend =
          ImageFrame::AlphaBlendSource::kBlendAtopPreviousFrame;
      break;
    default:
      return false;
  }

  fctl_needs_dat_chunk_ = true;
  return true;
}

}  // namespace blink
