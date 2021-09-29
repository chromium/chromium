// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_BOX_READER_H_
#define MEDIA_FORMATS_MP4_BOX_READER_H_

#include <stdint.h>

#include <limits>
#include <map>
#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/formats/mp4/fourccs.h"
#include "media/formats/mp4/parse_result.h"
#include "media/formats/mp4/rcheck.h"

namespace media {
namespace mp4 {

enum DisplayMatrixSize {
  kDisplayMatrixWidth = 3,
  kDisplayMatrixHeight = 3,
  kDisplayMatrixDimension = kDisplayMatrixHeight * kDisplayMatrixWidth
};

using DisplayMatrix = int32_t[kDisplayMatrixDimension];

class BoxReader;

struct MEDIA_EXPORT Box {
  virtual ~Box();

  // Parse errors may be logged using the BoxReader's media log.
  virtual bool Parse(BoxReader* reader) = 0;

  virtual FourCC BoxType() const = 0;
};

class MEDIA_EXPORT BufferReader {
 public:
  BufferReader(const uint8_t* buf, const size_t buf_size)
      : buf_(buf), buf_size_(buf_size), pos_(0) {
    CHECK(buf);
  }

  bool HasBytes(size_t count) {
    // As the size of a box is implementation limited to 2^31, fail if
    // attempting to check for too many bytes.
    const size_t impl_limit =
        static_cast<size_t>(std::numeric_limits<int32_t>::max());
    return pos_ <= buf_size_ && count <= impl_limit &&
           count <= buf_size_ - pos_;
  }

  // Read a value from the stream, performing endian correction, and advance the
  // stream pointer.
  bool Read1(uint8_t* v) WARN_UNUSED_RESULT;
  bool Read2(uint16_t* v) WARN_UNUSED_RESULT;
  bool Read2s(int16_t* v) WARN_UNUSED_RESULT;
  bool Read4(uint32_t* v) WARN_UNUSED_RESULT;
  bool Read4s(int32_t* v) WARN_UNUSED_RESULT;
  bool Read8(uint64_t* v) WARN_UNUSED_RESULT;
  bool Read8s(int64_t* v) WARN_UNUSED_RESULT;

  bool ReadFourCC(FourCC* v) WARN_UNUSED_RESULT;

  bool ReadVec(std::vector<uint8_t>* t, uint64_t count) WARN_UNUSED_RESULT;

  // These variants read a 4-byte integer of the corresponding signedness and
  // store it in the 8-byte return type.
  bool Read4Into8(uint64_t* v) WARN_UNUSED_RESULT;
  bool Read4sInto8s(int64_t* v) WARN_UNUSED_RESULT;

  // Advance the stream by this many bytes.
  bool SkipBytes(uint64_t nbytes) WARN_UNUSED_RESULT;

  const uint8_t* buffer() const { return buf_; }

  // Returns the size of the buffer. This may not match the size specified
  // in the mp4 box header and could be less than the box size when the full box
  // has not been appended. Always consult buffer_size() to avoid OOB reads.
  // See BoxReader::box_size().
  size_t buffer_size() const { return buf_size_; }
  size_t pos() const { return pos_; }

 protected:
  const uint8_t* buf_;
  size_t buf_size_;
  size_t pos_;

  template<typename T> bool Read(T* t) WARN_UNUSED_RESULT;
};

class MEDIA_EXPORT BoxReader : public BufferReader {
 public:
  BoxReader(const BoxReader& other);
  ~BoxReader();

  // Create a BoxReader from a buffer. If the result is kOk, then |out_reader|
  // will be set, otherwise |out_reader| will be unchanged.
  //
  // |buf| is retained but not owned, and must outlive the BoxReader instance.
  static ParseResult ReadTopLevelBox(const uint8_t* buf,
                                     const size_t buf_size,
                                     MediaLog* media_log,
                                     std::unique_ptr<BoxReader>* out_reader)
      WARN_UNUSED_RESULT;

  // Read the box header from the current buffer, and return its type and size.
  // This function returns kNeedMoreData if the box is incomplete, even if the
  // box header is complete.
  //
  // |buf| is not retained.
  static ParseResult StartTopLevelBox(const uint8_t* buf,
                                      const size_t buf_size,
                                      MediaLog* media_log,
                                      FourCC* out_type,
                                      size_t* out_box_size) WARN_UNUSED_RESULT;

  // Create a BoxReader from a buffer. |buf| must be the complete buffer, as
  // errors are returned when sufficient data is not available. |buf| can start
  // with any type of box -- it does not have to be IsValidTopLevelBox().
  //
  // |buf| is retained but not owned, and must outlive the BoxReader instance.
  static BoxReader* ReadConcatentatedBoxes(const uint8_t* buf,
                                           const size_t buf_size,
                                           MediaLog* media_log);

  // Returns true if |type| is recognized to be a top-level box, false
  // otherwise. This returns true for some boxes which we do not parse.
  // Helpful in debugging misaligned appends.
  static bool IsValidTopLevelBox(const FourCC& type, MediaLog* media_log);

  // Scan through all boxes within the current box, starting at the current
  // buffer position. Must be called before any of the *Child functions work.
  bool ScanChildren() WARN_UNUSED_RESULT;

  // Return true if child with type |child.BoxType()| exists.
  bool HasChild(Box* child) WARN_UNUSED_RESULT;

  // Read exactly one child box from the set of children. The type of the child
  // will be determined by the BoxType() method of |child|.
  bool ReadChild(Box* child) WARN_UNUSED_RESULT;

  // Read one child if available. Returns false on error, true on successful
  // read or on child absent.
  bool MaybeReadChild(Box* child) WARN_UNUSED_RESULT;

  // ISO-BMFF streams files use a 3x3 matrix consisting of 6 16.16 fixed point
  // decimals and 3 2.30 fixed point decimals.
  bool ReadDisplayMatrix(DisplayMatrix matrix);

  // Read at least one child. False means error or no such child present.
  template<typename T> bool ReadChildren(
      std::vector<T>* children) WARN_UNUSED_RESULT;

  // Read any number of children. False means error.
  template<typename T> bool MaybeReadChildren(
      std::vector<T>* children) WARN_UNUSED_RESULT;

  // Read all children, regardless of FourCC. This is used from exactly one box,
  // corresponding to a rather significant inconsistency in the BMFF spec.
  // Note that this method is mutually exclusive with ScanChildren() and
  // ReadAllChildrenAndCheckFourCC().
  template <typename T>
  bool ReadAllChildren(std::vector<T>* children) WARN_UNUSED_RESULT;

  // Read all children and verify that the FourCC matches what is expected.
  // Returns true if all children are successfully parsed and have the correct
  // box type for |T|. Note that this method is mutually exclusive with
  // ScanChildren() and ReadAllChildren().
  template <typename T>
  bool ReadAllChildrenAndCheckFourCC(std::vector<T>* children)
      WARN_UNUSED_RESULT;

  // Populate the values of 'version()' and 'flags()' from a full box header.
  // Many boxes, but not all, use these values. This call should happen after
  // the box has been initialized, and does not re-read the main box header.
  bool ReadFullBoxHeader() WARN_UNUSED_RESULT;

  size_t box_size() const {
    DCHECK(box_size_known_);
    return box_size_;
  }

  FourCC type() const   { return type_; }
  uint8_t version() const { return version_; }
  uint32_t flags() const { return flags_; }

  MediaLog* media_log() const { return media_log_; }

 private:
  // Create a BoxReader from |buf|. |is_EOS| should be true if |buf| is
  // complete stream (i.e. no additional data is expected to be appended).
  BoxReader(const uint8_t* buf,
            const size_t buf_size,
            MediaLog* media_log,
            bool is_EOS);

  // Must be called immediately after init.
  ParseResult ReadHeader() WARN_UNUSED_RESULT;

  // Read all children, optionally checking FourCC. Returns true if all
  // children are successfully parsed and, if |check_box_type|, have the
  // correct box type for |T|. Note that this method is mutually exclusive
  // with ScanChildren().
  template <typename T>
  bool ReadAllChildrenInternal(std::vector<T>* children, bool check_box_type);

  MediaLog* media_log_;
  size_t box_size_;
  bool box_size_known_;
  FourCC type_;
  uint8_t version_;
  uint32_t flags_;

  typedef std::multimap<FourCC, BoxReader> ChildMap;

  // The set of child box FourCCs and their corresponding buffer readers. Only
  // valid if scanned_ is true.
  ChildMap children_;
  bool scanned_;

  // True if the buffer provided to the reader is the complete stream.
  const bool is_EOS_;
};

// Template definitions
template<typename T> bool BoxReader::ReadChildren(std::vector<T>* children) {
  RCHECK(MaybeReadChildren(children) && !children->empty());
  return true;
}

template<typename T>
bool BoxReader::MaybeReadChildren(std::vector<T>* children) {
  DCHECK(scanned_);
  DCHECK(children->empty());

  children->resize(1);
  FourCC child_type = (*children)[0].BoxType();

  ChildMap::iterator start_itr = children_.lower_bound(child_type);
  ChildMap::iterator end_itr = children_.upper_bound(child_type);
  children->resize(std::distance(start_itr, end_itr));
  typename std::vector<T>::iterator child_itr = children->begin();
  for (ChildMap::iterator itr = start_itr; itr != end_itr; ++itr) {
    RCHECK(child_itr->Parse(&itr->second));
    ++child_itr;
  }
  children_.erase(start_itr, end_itr);

  DVLOG(2) << "Found " << children->size() << " "
           << FourCCToString(child_type) << " boxes.";
  return true;
}

template <typename T>
bool BoxReader::ReadAllChildren(std::vector<T>* children) {
  return ReadAllChildrenInternal(children, false);
}

template <typename T>
bool BoxReader::ReadAllChildrenAndCheckFourCC(std::vector<T>* children) {
  return ReadAllChildrenInternal(children, true);
}

template <typename T>
bool BoxReader::ReadAllChildrenInternal(std::vector<T>* children,
                                        bool check_box_type) {
  DCHECK(!scanned_);
  scanned_ = true;

  // Must know our box size before attempting to parse child boxes.
  RCHECK(box_size_known_);

  DCHECK_LE(pos_, box_size_);
  while (pos_ < box_size_) {
    BoxReader child_reader(&buf_[pos_], box_size_ - pos_, media_log_, is_EOS_);

    if (child_reader.ReadHeader() != ParseResult::kOk)
      return false;

    T child;
    RCHECK(!check_box_type || child_reader.type() == child.BoxType());
    RCHECK(child.Parse(&child_reader));
    children->push_back(child);
    pos_ += child_reader.box_size();
  }
  DCHECK_EQ(pos_, box_size_);
  return true;
}

}  // namespace mp4
}  // namespace media

#endif  // MEDIA_FORMATS_MP4_BOX_READER_H_
