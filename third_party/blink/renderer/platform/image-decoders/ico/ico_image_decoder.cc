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

#include "third_party/blink/renderer/platform/image-decoders/ico/ico_image_decoder.h"

#include <algorithm>
#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"

namespace blink {

// Number of bits in .ICO/.CUR used to store the directory and its entries,
// respectively (doesn't match sizeof values for member structs since we omit
// some fields).
static const size_t kSizeOfDirectory = 6;
static const size_t kSizeOfDirEntry = 16;

ICOImageDecoder::ICOImageDecoder(AlphaOption alpha_option,
                                 const ColorBehavior& color_behavior,
                                 size_t max_decoded_bytes)
    : ImageDecoder(alpha_option,
                   ImageDecoder::kDefaultBitDepth,
                   color_behavior,
                   max_decoded_bytes) {}

ICOImageDecoder::~ICOImageDecoder() = default;

void ICOImageDecoder::OnSetData(SegmentReader* data) {
  fast_reader_.SetData(data);

  for (BMPReaders::iterator i(bmp_readers_.begin()); i != bmp_readers_.end();
       ++i) {
    if (*i)
      (*i)->SetData(data);
  }
  for (size_t i = 0; i < png_decoders_.size(); ++i)
    SetDataForPNGDecoderAtIndex(i);
}

IntSize ICOImageDecoder::Size() const {
  return frame_size_.IsEmpty() ? ImageDecoder::Size() : frame_size_;
}

IntSize ICOImageDecoder::FrameSizeAtIndex(size_t index) const {
  return (index && (index < dir_entries_.size())) ? dir_entries_[index].size_
                                                  : Size();
}

bool ICOImageDecoder::SetSize(unsigned width, unsigned height) {
  // The size calculated inside the BMPImageReader had better match the one in
  // the icon directory.
  return frame_size_.IsEmpty()
             ? ImageDecoder::SetSize(width, height)
             : ((IntSize(width, height) == frame_size_) || SetFailed());
}

bool ICOImageDecoder::FrameIsReceivedAtIndex(size_t index) const {
  if (index >= dir_entries_.size())
    return false;

  SECURITY_DCHECK(data_);
  const IconDirectoryEntry& dir_entry = dir_entries_[index];
  return (dir_entry.image_offset_ + dir_entry.byte_size_) <= data_->size();
}

bool ICOImageDecoder::SetFailed() {
  bmp_readers_.clear();
  png_decoders_.clear();
  return ImageDecoder::SetFailed();
}

bool ICOImageDecoder::HotSpot(IntPoint& hot_spot) const {
  // When unspecified, the default frame is always frame 0. This is consistent
  // with BitmapImage, where CurrentFrame() starts at 0 and only increases when
  // animation is requested.
  return HotSpotAtIndex(0, hot_spot);
}

bool ICOImageDecoder::HotSpotAtIndex(size_t index, IntPoint& hot_spot) const {
  if (index >= dir_entries_.size() || file_type_ != CURSOR)
    return false;

  hot_spot = dir_entries_[index].hot_spot_;
  return true;
}

// static
bool ICOImageDecoder::CompareEntries(const IconDirectoryEntry& a,
                                     const IconDirectoryEntry& b) {
  // Larger icons are better.  After that, higher bit-depth icons are better.
  const int a_entry_area = a.size_.Width() * a.size_.Height();
  const int b_entry_area = b.size_.Width() * b.size_.Height();
  return (a_entry_area == b_entry_area) ? (a.bit_count_ > b.bit_count_)
                                        : (a_entry_area > b_entry_area);
}

size_t ICOImageDecoder::DecodeFrameCount() {
  DecodeSize();

  // If DecodeSize() fails, return the existing number of frames.  This way
  // if we get halfway through the image before decoding fails, we won't
  // suddenly start reporting that the image has zero frames.
  if (Failed() || !data_)
    return frame_buffer_cache_.size();

  // If the file is incomplete, return the length of the sequence of completely
  // received frames.  We don't do this when the file is fully received, since
  // some ICOs have entries whose claimed offset + size extends past the end of
  // the file, and we still want to display these if they don't trigger decoding
  // failures elsewhere.
  if (!IsAllDataReceived()) {
    for (size_t i = 0; i < dir_entries_.size(); ++i) {
      const IconDirectoryEntry& dir_entry = dir_entries_[i];
      if ((dir_entry.image_offset_ + dir_entry.byte_size_) > data_->size())
        return i;
    }
  }
  return dir_entries_.size();
}

void ICOImageDecoder::SetDataForPNGDecoderAtIndex(size_t index) {
  if (!png_decoders_[index])
    return;

  png_decoders_[index]->SetData(data_.get(), IsAllDataReceived());
}

void ICOImageDecoder::Decode(size_t index, bool only_size) {
  if (Failed() || !data_)
    return;

  // Defensively clear the FastSharedBufferReader's cache, as another caller
  // may have called SharedBuffer::MergeSegmentsIntoBuffer().
  fast_reader_.ClearCache();

  // If we couldn't decode the image but we've received all the data, decoding
  // has failed.
  if ((!DecodeDirectory() || (!only_size && !DecodeAtIndex(index))) &&
      IsAllDataReceived()) {
    SetFailed();
    // If we're done decoding this frame, we don't need the BMPImageReader or
    // PNGImageDecoder anymore.  (If we failed, these have already been
    // cleared.)
  } else if ((frame_buffer_cache_.size() > index) &&
             (frame_buffer_cache_[index].GetStatus() ==
              ImageFrame::kFrameComplete)) {
    bmp_readers_[index].reset();
    png_decoders_[index].reset();
  }
}

bool ICOImageDecoder::DecodeDirectory() {
  // Read and process directory.
  if ((decoded_offset_ < kSizeOfDirectory) && !ProcessDirectory())
    return false;

  // Read and process directory entries.
  return (decoded_offset_ >=
          (kSizeOfDirectory + (dir_entries_count_ * kSizeOfDirEntry))) ||
         ProcessDirectoryEntries();
}

bool ICOImageDecoder::DecodeAtIndex(size_t index) {
  SECURITY_DCHECK(index < dir_entries_.size());
  const IconDirectoryEntry& dir_entry = dir_entries_[index];
  const ImageType image_type = ImageTypeAtIndex(index);
  if (image_type == kUnknown)
    return false;  // Not enough data to determine image type yet.

  if (image_type == BMP) {
    if (!bmp_readers_[index]) {
      bmp_readers_[index] = std::make_unique<BMPImageReader>(
          this, dir_entry.image_offset_, 0, true);
      bmp_readers_[index]->SetData(data_.get());
    }
    // Update the pointer to the buffer as it could change after
    // frame_buffer_cache_.resize().
    bmp_readers_[index]->SetBuffer(&frame_buffer_cache_[index]);
    frame_size_ = dir_entry.size_;
    bool result = bmp_readers_[index]->DecodeBMP(false);
    frame_size_ = IntSize();
    return result;
  }

  if (!png_decoders_[index]) {
    AlphaOption alpha_option =
        premultiply_alpha_ ? kAlphaPremultiplied : kAlphaNotPremultiplied;
    png_decoders_[index] = std::make_unique<PNGImageDecoder>(
        alpha_option, ImageDecoder::kDefaultBitDepth, color_behavior_,
        max_decoded_bytes_, dir_entry.image_offset_);
    SetDataForPNGDecoderAtIndex(index);
  }
  auto* png_decoder = png_decoders_[index].get();
  if (png_decoder->IsSizeAvailable()) {
    // Fail if the size the PNGImageDecoder calculated does not match the size
    // in the directory.
    if (png_decoder->Size() != dir_entry.size_)
      return SetFailed();

    png_decoder->SetMemoryAllocator(frame_buffer_cache_[index].GetAllocator());
    const auto* frame = png_decoder->DecodeFrameBufferAtIndex(0);
    png_decoder->SetMemoryAllocator(nullptr);

    if (frame)
      frame_buffer_cache_[index] = *frame;
  }
  if (png_decoder->Failed())
    return SetFailed();
  return frame_buffer_cache_[index].GetStatus() == ImageFrame::kFrameComplete;
}

bool ICOImageDecoder::ProcessDirectory() {
  // Read directory.
  SECURITY_DCHECK(data_);
  DCHECK(!decoded_offset_);
  if (data_->size() < kSizeOfDirectory)
    return false;
  const uint16_t file_type = ReadUint16(2);
  dir_entries_count_ = ReadUint16(4);
  decoded_offset_ = kSizeOfDirectory;

  // See if this is an icon filetype we understand, and make sure we have at
  // least one entry in the directory.
  if (((file_type != ICON) && (file_type != CURSOR)) || (!dir_entries_count_))
    return SetFailed();

  file_type_ = static_cast<FileType>(file_type);
  return true;
}

bool ICOImageDecoder::ProcessDirectoryEntries() {
  // Read directory entries.
  SECURITY_DCHECK(data_);
  DCHECK_EQ(decoded_offset_, kSizeOfDirectory);
  if ((decoded_offset_ > data_->size()) ||
      ((data_->size() - decoded_offset_) <
       (dir_entries_count_ * kSizeOfDirEntry)))
    return false;

  // Enlarge member vectors to hold all the entries.
  dir_entries_.resize(dir_entries_count_);
  bmp_readers_.resize(dir_entries_count_);
  png_decoders_.resize(dir_entries_count_);

  for (IconDirectoryEntries::iterator i(dir_entries_.begin());
       i != dir_entries_.end(); ++i)
    *i = ReadDirectoryEntry();  // Updates decoded_offset_.

  // Make sure the specified image offsets are past the end of the directory
  // entries.
  for (IconDirectoryEntries::iterator i(dir_entries_.begin());
       i != dir_entries_.end(); ++i) {
    if (i->image_offset_ < decoded_offset_)
      return SetFailed();
  }

  // Arrange frames in decreasing quality order.
  std::sort(dir_entries_.begin(), dir_entries_.end(), CompareEntries);

  // The image size is the size of the largest entry.
  const IconDirectoryEntry& dir_entry = dir_entries_.front();
  // Technically, this next call shouldn't be able to fail, since the width
  // and height here are each <= 256, and |frame_size_| is empty.
  return SetSize(dir_entry.size_.Width(), dir_entry.size_.Height());
}

ICOImageDecoder::IconDirectoryEntry ICOImageDecoder::ReadDirectoryEntry() {
  // Read icon data.
  // The following calls to ReadUint8() return a uint8_t, which is appropriate
  // because that's the on-disk type of the width and height values.  Storing
  // them in ints (instead of matching uint8_ts) is so we can record dimensions
  // of size 256 (which is what a zero byte really means).
  int width = ReadUint8(0);
  if (!width)
    width = 256;
  int height = ReadUint8(1);
  if (!height)
    height = 256;
  IconDirectoryEntry entry;
  entry.size_ = IntSize(width, height);
  if (file_type_ == CURSOR) {
    entry.bit_count_ = 0;
    entry.hot_spot_ = IntPoint(ReadUint16(4), ReadUint16(6));
  } else {
    entry.bit_count_ = ReadUint16(6);
    entry.hot_spot_ = IntPoint();
  }
  entry.byte_size_ = ReadUint32(8);
  entry.image_offset_ = ReadUint32(12);

  // Some icons don't have a bit depth, only a color count.  Convert the
  // color count to the minimum necessary bit depth.  It doesn't matter if
  // this isn't quite what the bitmap info header says later, as we only use
  // this value to determine which icon entry is best.
  if (!entry.bit_count_) {
    int color_count = ReadUint8(2);
    if (!color_count)
      color_count = 256;  // Vague in the spec, needed by real-world icons.
    for (--color_count; color_count; color_count >>= 1)
      ++entry.bit_count_;
  }

  decoded_offset_ += kSizeOfDirEntry;
  return entry;
}

ICOImageDecoder::ImageType ICOImageDecoder::ImageTypeAtIndex(size_t index) {
  // Check if this entry is a BMP or a PNG; we need 4 bytes to check the magic
  // number.
  SECURITY_DCHECK(data_);
  SECURITY_DCHECK(index < dir_entries_.size());
  const uint32_t image_offset = dir_entries_[index].image_offset_;
  if ((image_offset > data_->size()) || ((data_->size() - image_offset) < 4))
    return kUnknown;
  char buffer[4];
  const char* data = fast_reader_.GetConsecutiveData(image_offset, 4, buffer);
  return strncmp(data, "\x89PNG", 4) ? BMP : PNG;
}

}  // namespace blink
