// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/formats/mp4/box_reader.h"

#include <stddef.h>
#include <string.h>

#include <algorithm>
#include <set>

#include "base/numerics/byte_conversions.h"
#include "media/formats/mp4/box_definitions.h"

namespace media {
namespace mp4 {

Box::~Box() = default;

BufferReader::BufferReader(const uint8_t* buf, const size_t buf_size)
    :  // TODO(crbug.com/40284755): BufferReader should be receiving a span,
       // the construction of the span here is unsound as there's no way to
       // tell the size is correct from here.
      UNSAFE_TODO(buf_(buf, buf_size)) {}

BufferReader::~BufferReader() = default;

BufferReader::BufferReader(const BufferReader& other) = default;

bool BufferReader::Read1(uint8_t* v) {
  RCHECK(HasBytes(1));
  *v = base::U8FromBigEndian(buf_.subspan(pos_).first<1u>());
  pos_ += 1u;
  return true;
}
bool BufferReader::Read2(uint16_t* v) {
  RCHECK(HasBytes(2));
  *v = base::U16FromBigEndian(buf_.subspan(pos_).first<2u>());
  pos_ += 2u;
  return true;
}
bool BufferReader::Read2s(int16_t* v) {
  uint16_t tmp;
  RCHECK(Read2(&tmp));
  *v = static_cast<int16_t>(tmp);
  return true;
}
bool BufferReader::Read4(uint32_t* v) {
  RCHECK(HasBytes(4));
  *v = base::U32FromBigEndian(buf_.subspan(pos_).first<4u>());
  pos_ += 4u;
  return true;
}
bool BufferReader::Read4s(int32_t* v) {
  uint32_t tmp;
  RCHECK(Read4(&tmp));
  *v = static_cast<int32_t>(tmp);
  return true;
}
bool BufferReader::Read8(uint64_t* v) {
  RCHECK(HasBytes(8));
  *v = base::U64FromBigEndian(buf_.subspan(pos_).first<8u>());
  pos_ += 8u;
  return true;
}
bool BufferReader::Read8s(int64_t* v) {
  uint64_t tmp;
  RCHECK(Read8(&tmp));
  *v = static_cast<int64_t>(tmp);
  return true;
}

bool BufferReader::ReadFourCC(FourCC* v) {
  uint32_t tmp;
  RCHECK(Read4(&tmp));
  *v = static_cast<FourCC>(tmp);
  return true;
}

bool BufferReader::ReadVec(std::vector<uint8_t>* vec, uint64_t count) {
  RCHECK(HasBytes(count));
  vec->clear();
  auto range = buf_.subspan(pos_, count);
  vec->insert(vec->end(), range.begin(), range.end());
  pos_ += count;
  return true;
}

bool BufferReader::SkipBytes(uint64_t bytes) {
  RCHECK(HasBytes(bytes));
  pos_ += bytes;
  return true;
}

bool BufferReader::Read4Into8(uint64_t* v) {
  uint32_t tmp;
  RCHECK(Read4(&tmp));
  *v = tmp;
  return true;
}

bool BufferReader::Read4sInto8s(int64_t* v) {
  // Beware of the need for sign extension.
  int32_t tmp;
  RCHECK(Read4s(&tmp));
  *v = tmp;
  return true;
}

BoxReader::BoxReader(const uint8_t* buf,
                     const size_t buf_size,
                     MediaLog* media_log,
                     bool is_EOS)
    : BufferReader(buf, buf_size),
      media_log_(media_log),
      box_size_(0),
      box_size_known_(false),
      type_(FOURCC_NULL),
      version_(0),
      flags_(0),
      scanned_(false),
      is_EOS_(is_EOS) {}

BoxReader::BoxReader(const BoxReader& other) = default;

BoxReader::~BoxReader() {
  if (scanned_ && !children_.empty()) {
    for (auto itr = children_.begin(); itr != children_.end(); ++itr) {
      DVLOG(1) << "Skipping unknown box: " << FourCCToString(itr->first);
    }
  }
}

// static
ParseResult BoxReader::ReadTopLevelBox(const uint8_t* buf,
                                       const size_t buf_size,
                                       MediaLog* media_log,
                                       std::unique_ptr<BoxReader>* out_reader) {
  DCHECK(out_reader);
  std::unique_ptr<BoxReader> reader(
      new BoxReader(buf, buf_size, media_log, false));
  RCHECK_OK_PARSE_RESULT(reader->ReadHeader());
  if (!IsValidTopLevelBox(reader->type(), media_log))
    return ParseResult::kError;
  *out_reader = std::move(reader);
  return ParseResult::kOk;
}

// static
ParseResult BoxReader::StartTopLevelBox(const uint8_t* buf,
                                        const size_t buf_size,
                                        MediaLog* media_log,
                                        FourCC* out_type,
                                        size_t* out_box_size) {
  std::unique_ptr<BoxReader> reader;
  RCHECK_OK_PARSE_RESULT(ReadTopLevelBox(buf, buf_size, media_log, &reader));
  *out_type = reader->type();
  *out_box_size = reader->box_size();
  return ParseResult::kOk;
}

// static
BoxReader* BoxReader::ReadConcatentatedBoxes(const uint8_t* buf,
                                             const size_t buf_size,
                                             MediaLog* media_log) {
  BoxReader* reader = new BoxReader(buf, buf_size, media_log, true);

  // Concatenated boxes are passed in without a wrapping parent box. Set
  // |box_size_| to the concatenated buffer length to mimic having already
  // parsed the parent box.
  reader->box_size_ = buf_size;
  reader->box_size_known_ = true;

  return reader;
}

// static
bool BoxReader::IsValidTopLevelBox(const FourCC& type, MediaLog* media_log) {
  switch (type) {
    case FOURCC_FTYP:
    case FOURCC_PDIN:
    case FOURCC_BLOC:
    case FOURCC_MOOV:
    case FOURCC_MOOF:
    case FOURCC_MFRA:
    case FOURCC_MDAT:
    case FOURCC_FREE:
    case FOURCC_SKIP:
    case FOURCC_META:
    case FOURCC_MECO:
    case FOURCC_STYP:
    case FOURCC_SIDX:
    case FOURCC_SSIX:
    case FOURCC_PRFT:
    case FOURCC_UUID:
    case FOURCC_EMSG:
      return true;
    default:
      // Hex is used to show nonprintable characters and aid in debugging.
      MEDIA_LOG(ERROR, media_log)
          << "Invalid top-level ISO BMFF box type " << FourCCToString(type);
      return false;
  }
}

bool BoxReader::ScanChildren() {
  // Must be able to trust box_size_ below.
  RCHECK(box_size_known_);
  // This is implied by setting `box_size_known_` to true, as `buf_` is
  // truncated to `box_size_` at that time.
  CHECK_EQ(box_size_, buf_.size());

  DCHECK(!scanned_);
  scanned_ = true;

  while (pos_ < buf_.size()) {
    auto range = buf_.subspan(pos_);
    BoxReader child(range.data(), range.size(), media_log_, is_EOS_);
    if (child.ReadHeader() != ParseResult::kOk)
      return false;
    children_.insert(std::pair<FourCC, BoxReader>(child.type(), child));
    pos_ += child.box_size();
  }
  DCHECK_EQ(pos_, buf_.size());
  return true;
}

bool BoxReader::ReadDisplayMatrix(DisplayMatrix matrix) {
  for (int i = 0; i < kDisplayMatrixDimension; i++) {
    if (!Read4s(&matrix[i])) {
      return false;
    }
  }
  return true;
}

bool BoxReader::HasChild(Box* child) {
  DCHECK(scanned_);
  DCHECK(child);
  return children_.count(child->BoxType()) > 0;
}

bool BoxReader::ReadChild(Box* child) {
  DCHECK(scanned_);
  FourCC child_type = child->BoxType();

  auto itr = children_.find(child_type);
  RCHECK(itr != children_.end());
  DVLOG(2) << "Found a " << FourCCToString(child_type) << " box.";
  RCHECK(child->Parse(&itr->second));
  children_.erase(itr);
  return true;
}

bool BoxReader::MaybeReadChild(Box* child) {
  if (!children_.count(child->BoxType())) return true;
  return ReadChild(child);
}

bool BoxReader::ReadFullBoxHeader() {
  uint32_t vflags;
  RCHECK(Read4(&vflags));
  version_ = vflags >> 24;
  flags_ = vflags & 0xffffff;
  return true;
}

ParseResult BoxReader::ReadHeader() {
  uint64_t box_size = 0;

  if (!HasBytes(8))
    return is_EOS_ ? ParseResult::kError : ParseResult::kNeedMoreData;
  CHECK(Read4Into8(&box_size));
  CHECK(ReadFourCC(&type_));

  if (box_size == 0) {
    if (is_EOS_) {
      // All the data bytes are expected to be provided.
      // TODO(sandersd): The whole |is_EOS_| feature seems to exist just for
      // this special case (and is used only for PSSH parsing). Can we get rid
      // of it? The caller can treat kNeedMoreData as an error, and the only
      // difference would be lack of support for |box_size == 0|.
      box_size = base::strict_cast<uint64_t>(buf_.size());
    } else {
      MEDIA_LOG(DEBUG, media_log_)
          << "ISO BMFF boxes that run to EOS are not supported";
      return ParseResult::kError;
    }
  } else if (box_size == 1) {
    if (!HasBytes(8))
      return is_EOS_ ? ParseResult::kError : ParseResult::kNeedMoreData;
    CHECK(Read8(&box_size));
  }

  // Implementation-specific: support for boxes larger than 2^31 has been
  // removed.
  if (box_size < base::strict_cast<uint64_t>(pos_) ||
      box_size > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
    return ParseResult::kError;
  }

  // Make sure the buffer contains at least the expected number of bytes.
  // Since the data may be appended in pieces, this is only an error if EOS.
  if (box_size > base::strict_cast<uint64_t>(buf_.size())) {
    return is_EOS_ ? ParseResult::kError : ParseResult::kNeedMoreData;
  }

  // Note that the pos_ head has advanced to the byte immediately after the
  // header, which is where we want it.
  box_size_ = base::checked_cast<size_t>(box_size);
  box_size_known_ = true;

  // We don't want future reads to go beyond the box.
  buf_ = buf_.first(box_size_);

  return ParseResult::kOk;
}

}  // namespace mp4
}  // namespace media
