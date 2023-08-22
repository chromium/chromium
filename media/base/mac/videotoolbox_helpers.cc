// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mac/videotoolbox_helpers.h"

#include <array>
#include <vector>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "media/base/video_codecs.h"

namespace media {

namespace video_toolbox {

namespace {
static const char kAnnexBHeaderBytes[4] = {0, 0, 0, 1};
}  // anonymous namespace

base::apple::ScopedCFTypeRef<CFDictionaryRef>
DictionaryWithKeysAndValues(CFTypeRef* keys, CFTypeRef* values, size_t size) {
  return base::apple::ScopedCFTypeRef<CFDictionaryRef>(CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, size, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks));
}

base::apple::ScopedCFTypeRef<CFDictionaryRef> DictionaryWithKeyValue(
    CFTypeRef key,
    CFTypeRef value) {
  CFTypeRef keys[1] = {key};
  CFTypeRef values[1] = {value};
  return DictionaryWithKeysAndValues(keys, values, 1);
}

base::apple::ScopedCFTypeRef<CFArrayRef> ArrayWithIntegers(const int* v,
                                                           size_t size) {
  std::vector<CFNumberRef> numbers;
  numbers.reserve(size);
  for (const int* end = v + size; v < end; ++v)
    numbers.push_back(CFNumberCreate(nullptr, kCFNumberSInt32Type, v));
  base::apple::ScopedCFTypeRef<CFArrayRef> array(CFArrayCreate(
      kCFAllocatorDefault, reinterpret_cast<const void**>(&numbers[0]),
      numbers.size(), &kCFTypeArrayCallBacks));
  for (auto* number : numbers) {
    CFRelease(number);
  }
  return array;
}

base::apple::ScopedCFTypeRef<CFArrayRef> ArrayWithIntegerAndFloat(
    int int_val,
    float float_val) {
  std::array<CFNumberRef, 2> numbers = {
      {CFNumberCreate(nullptr, kCFNumberSInt32Type, &int_val),
       CFNumberCreate(nullptr, kCFNumberFloat32Type, &float_val)}};
  base::apple::ScopedCFTypeRef<CFArrayRef> array(CFArrayCreate(
      kCFAllocatorDefault, reinterpret_cast<const void**>(numbers.data()),
      numbers.size(), &kCFTypeArrayCallBacks));
  for (auto* number : numbers)
    CFRelease(number);
  return array;
}

// Wrapper class for writing AnnexBBuffer output into.
class AnnexBBuffer {
 public:
  virtual bool Reserve(size_t size) = 0;
  virtual void Append(const char* s, size_t n) = 0;
  virtual size_t GetReservedSize() const = 0;
};

class RawAnnexBBuffer : public AnnexBBuffer {
 public:
  RawAnnexBBuffer(char* annexb_buffer, size_t annexb_buffer_size)
      : annexb_buffer_(annexb_buffer),
        annexb_buffer_size_(annexb_buffer_size),
        annexb_buffer_offset_(0) {}
  RawAnnexBBuffer() = delete;
  RawAnnexBBuffer(const RawAnnexBBuffer&) = delete;
  RawAnnexBBuffer& operator=(const RawAnnexBBuffer&) = delete;

  bool Reserve(size_t size) override {
    reserved_size_ = size;
    return size <= annexb_buffer_size_;
  }
  void Append(const char* s, size_t n) override {
    memcpy(annexb_buffer_ + annexb_buffer_offset_, s, n);
    annexb_buffer_offset_ += n;
    DCHECK_GE(reserved_size_, annexb_buffer_offset_);
  }
  size_t GetReservedSize() const override { return reserved_size_; }

 private:
  raw_ptr<char, AllowPtrArithmetic> annexb_buffer_;
  size_t annexb_buffer_size_;
  size_t annexb_buffer_offset_;
  size_t reserved_size_;
};

class StringAnnexBBuffer : public AnnexBBuffer {
 public:
  explicit StringAnnexBBuffer(std::string* str_annexb_buffer)
      : str_annexb_buffer_(str_annexb_buffer) {}
  StringAnnexBBuffer() = delete;
  StringAnnexBBuffer(const StringAnnexBBuffer&) = delete;
  StringAnnexBBuffer& operator=(const StringAnnexBBuffer&) = delete;

  bool Reserve(size_t size) override {
    str_annexb_buffer_->reserve(size);
    return true;
  }
  void Append(const char* s, size_t n) override {
    str_annexb_buffer_->append(s, n);
  }
  size_t GetReservedSize() const override { return str_annexb_buffer_->size(); }

 private:
  raw_ptr<std::string> str_annexb_buffer_;
};

template <typename NalSizeType>
void CopyNalsToAnnexB(char* buffer,
                      const size_t buffer_size,
                      AnnexBBuffer* annexb_buffer) {
  static_assert(sizeof(NalSizeType) == 1 || sizeof(NalSizeType) == 2 ||
                    sizeof(NalSizeType) == 4,
                "NAL size type has unsupported size");
  DCHECK(buffer);
  DCHECK(annexb_buffer);
  size_t bytes_left = buffer_size;
  while (bytes_left > 0) {
    DCHECK_GT(bytes_left, sizeof(NalSizeType));
    NalSizeType nal_size;
    base::ReadBigEndian(reinterpret_cast<uint8_t*>(buffer), &nal_size);
    bytes_left -= sizeof(NalSizeType);
    buffer += sizeof(NalSizeType);

    DCHECK_GE(bytes_left, nal_size);
    annexb_buffer->Append(kAnnexBHeaderBytes, sizeof(kAnnexBHeaderBytes));
    annexb_buffer->Append(buffer, nal_size);
    bytes_left -= nal_size;
    buffer += nal_size;
  }
}

OSStatus GetParameterSetAtIndex(VideoCodec codec,
                                CMFormatDescriptionRef videoDesc,
                                size_t parameterSetIndex,
                                const uint8_t** parameterSetPointerOut,
                                size_t* parameterSetSizeOut,
                                size_t* parameterSetCountOut,
                                int* NALUnitHeaderLengthOut) {
  switch (codec) {
    case VideoCodec::kH264:
      return CMVideoFormatDescriptionGetH264ParameterSetAtIndex(
          videoDesc, parameterSetIndex, parameterSetPointerOut,
          parameterSetSizeOut, parameterSetCountOut, NALUnitHeaderLengthOut);
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    case VideoCodec::kHEVC:
      return CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(
          videoDesc, parameterSetIndex, parameterSetPointerOut,
          parameterSetSizeOut, parameterSetCountOut, NALUnitHeaderLengthOut);
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
    default:
      NOTREACHED_NORETURN();
  }
}

bool CopySampleBufferToAnnexBBuffer(VideoCodec codec,
                                    CMSampleBufferRef sbuf,
                                    AnnexBBuffer* annexb_buffer,
                                    bool keyframe) {
  // Perform two pass, one to figure out the total output size, and another to
  // copy the data after having performed a single output allocation. Note that
  // we'll allocate a bit more because we'll count 4 bytes instead of 3 for
  // video NALs.
  OSStatus status;

  // Get the sample buffer's block buffer and format description.
  auto* bb = CMSampleBufferGetDataBuffer(sbuf);
  DCHECK(bb);
  auto* fdesc = CMSampleBufferGetFormatDescription(sbuf);
  DCHECK(fdesc);

  size_t bb_size = CMBlockBufferGetDataLength(bb);
  size_t total_bytes = bb_size;

  size_t pset_count;
  int nal_size_field_bytes;
  status = GetParameterSetAtIndex(codec, fdesc, 0, nullptr, nullptr,
                                  &pset_count, &nal_size_field_bytes);
  if (status == kCMFormatDescriptionBridgeError_InvalidParameter) {
    DLOG(WARNING) << " assuming " << int(codec == VideoCodec::kHEVC ? 3 : 2)
                  << " parameter sets and 4 bytes NAL length header";
    pset_count = codec == VideoCodec::kHEVC ? 3 : 2;
    nal_size_field_bytes = 4;
  } else if (status != noErr) {
    DLOG(ERROR) << " GetParameterSetAtIndex failed: " << status;
    return false;
  }

  if (keyframe) {
    const uint8_t* pset;
    size_t pset_size;
    for (size_t pset_i = 0; pset_i < pset_count; ++pset_i) {
      status = GetParameterSetAtIndex(codec, fdesc, pset_i, &pset, &pset_size,
                                      nullptr, nullptr);
      if (status != noErr) {
        DLOG(ERROR) << " GetParameterSetAtIndex failed: " << status;
        return false;
      }
      total_bytes += pset_size + nal_size_field_bytes;
    }
  }

  if (!annexb_buffer->Reserve(total_bytes)) {
    DLOG(ERROR) << "Cannot fit encode output into bitstream buffer. Requested:"
                << total_bytes;
    return false;
  }

  // Copy all parameter sets before keyframes.
  if (keyframe) {
    const uint8_t* pset;
    size_t pset_size;
    for (size_t pset_i = 0; pset_i < pset_count; ++pset_i) {
      status = GetParameterSetAtIndex(codec, fdesc, pset_i, &pset, &pset_size,
                                      nullptr, nullptr);
      if (status != noErr) {
        DLOG(ERROR) << " GetParameterSetAtIndex failed: " << status;
        return false;
      }
      annexb_buffer->Append(kAnnexBHeaderBytes, sizeof(kAnnexBHeaderBytes));
      annexb_buffer->Append(reinterpret_cast<const char*>(pset), pset_size);
    }
  }

  // Block buffers can be composed of non-contiguous chunks. For the sake of
  // keeping this code simple, flatten non-contiguous block buffers.
  base::apple::ScopedCFTypeRef<CMBlockBufferRef> contiguous_bb(
      bb, base::scoped_policy::RETAIN);
  if (!CMBlockBufferIsRangeContiguous(bb, 0, 0)) {
    contiguous_bb.reset();
    status = CMBlockBufferCreateContiguous(kCFAllocatorDefault, bb,
                                           kCFAllocatorDefault, nullptr, 0, 0,
                                           0, contiguous_bb.InitializeInto());
    if (status != noErr) {
      DLOG(ERROR) << " CMBlockBufferCreateContiguous failed: " << status;
      return false;
    }
  }

  // Copy all the NAL units. In the process convert them from AVCC/HVCC format
  // (length header) to AnnexB format (start code).
  char* bb_data;
  status =
      CMBlockBufferGetDataPointer(contiguous_bb, 0, nullptr, nullptr, &bb_data);
  if (status != noErr) {
    DLOG(ERROR) << " CMBlockBufferGetDataPointer failed: " << status;
    return false;
  }

  if (nal_size_field_bytes == 1) {
    CopyNalsToAnnexB<uint8_t>(bb_data, bb_size, annexb_buffer);
  } else if (nal_size_field_bytes == 2) {
    CopyNalsToAnnexB<uint16_t>(bb_data, bb_size, annexb_buffer);
  } else if (nal_size_field_bytes == 4) {
    CopyNalsToAnnexB<uint32_t>(bb_data, bb_size, annexb_buffer);
  } else {
    NOTREACHED();
  }
  return true;
}

bool CopySampleBufferToAnnexBBuffer(VideoCodec codec,
                                    CMSampleBufferRef sbuf,
                                    bool keyframe,
                                    std::string* annexb_buffer) {
  StringAnnexBBuffer buffer(annexb_buffer);
  return CopySampleBufferToAnnexBBuffer(codec, sbuf, &buffer, keyframe);
}

bool CopySampleBufferToAnnexBBuffer(VideoCodec codec,
                                    CMSampleBufferRef sbuf,
                                    bool keyframe,
                                    size_t annexb_buffer_size,
                                    char* annexb_buffer,
                                    size_t* used_buffer_size) {
  RawAnnexBBuffer buffer(annexb_buffer, annexb_buffer_size);
  const bool copy_rv =
      CopySampleBufferToAnnexBBuffer(codec, sbuf, &buffer, keyframe);
  *used_buffer_size = buffer.GetReservedSize();
  return copy_rv;
}

SessionPropertySetter::SessionPropertySetter(
    base::apple::ScopedCFTypeRef<VTCompressionSessionRef> session)
    : session_(session) {}

SessionPropertySetter::~SessionPropertySetter() {}

bool SessionPropertySetter::IsSupported(CFStringRef key) {
  DCHECK(session_);
  if (!supported_keys_) {
    CFDictionaryRef dict_ref;
    if (VTSessionCopySupportedPropertyDictionary(session_, &dict_ref) == noErr)
      supported_keys_.reset(dict_ref);
  }
  return supported_keys_ && CFDictionaryContainsKey(supported_keys_, key);
}

bool SessionPropertySetter::Set(CFStringRef key, int32_t value) {
  DCHECK(session_);
  base::apple::ScopedCFTypeRef<CFNumberRef> cfvalue(
      CFNumberCreate(nullptr, kCFNumberSInt32Type, &value));
  return VTSessionSetProperty(session_, key, cfvalue) == noErr;
}

bool SessionPropertySetter::Set(CFStringRef key, bool value) {
  DCHECK(session_);
  CFBooleanRef cfvalue = (value) ? kCFBooleanTrue : kCFBooleanFalse;
  return VTSessionSetProperty(session_, key, cfvalue) == noErr;
}

bool SessionPropertySetter::Set(CFStringRef key, double value) {
  DCHECK(session_);
  base::apple::ScopedCFTypeRef<CFNumberRef> cfvalue(
      CFNumberCreate(nullptr, kCFNumberDoubleType, &value));
  return VTSessionSetProperty(session_, key, cfvalue) == noErr;
}

bool SessionPropertySetter::Set(CFStringRef key, CFStringRef value) {
  DCHECK(session_);
  return VTSessionSetProperty(session_, key, value) == noErr;
}

bool SessionPropertySetter::Set(CFStringRef key, CFArrayRef value) {
  DCHECK(session_);
  return VTSessionSetProperty(session_, key, value) == noErr;
}

}  // namespace video_toolbox

}  // namespace media
