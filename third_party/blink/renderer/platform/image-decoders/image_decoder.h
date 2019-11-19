/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_IMAGE_DECODER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/image-decoders/image_animation.h"
#include "third_party/blink/renderer/platform/image-decoders/image_frame.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/third_party/skcms/skcms.h"

class SkColorSpace;

namespace blink {

#if SK_B32_SHIFT
inline skcms_PixelFormat XformColorFormat() {
  return skcms_PixelFormat_RGBA_8888;
}
#else
inline skcms_PixelFormat XformColorFormat() {
  return skcms_PixelFormat_BGRA_8888;
}
#endif

// ImagePlanes can be used to decode color components into provided buffers
// instead of using an ImageFrame.
class PLATFORM_EXPORT ImagePlanes final {
  USING_FAST_MALLOC(ImagePlanes);

 public:
  ImagePlanes();
  // TODO(crbug/910276): To support YUVA, ImagePlanes needs to support a
  // variable number of planes.
  ImagePlanes(void* planes[3], const size_t row_bytes[3]);

  void* Plane(int);
  size_t RowBytes(int) const;

 private:
  void* planes_[3];
  size_t row_bytes_[3];

  DISALLOW_COPY_AND_ASSIGN(ImagePlanes);
};

class PLATFORM_EXPORT ColorProfile final {
  USING_FAST_MALLOC(ColorProfile);

 public:
  ColorProfile(const skcms_ICCProfile&, std::unique_ptr<uint8_t[]> = nullptr);
  static std::unique_ptr<ColorProfile> Create(const void* buffer, size_t size);

  const skcms_ICCProfile* GetProfile() const { return &profile_; }

 private:
  skcms_ICCProfile profile_;
  std::unique_ptr<uint8_t[]> buffer_;

  DISALLOW_COPY_AND_ASSIGN(ColorProfile);
};

class PLATFORM_EXPORT ColorProfileTransform final {
  USING_FAST_MALLOC(ColorProfileTransform);

 public:
  ColorProfileTransform(const skcms_ICCProfile* src_profile,
                        const skcms_ICCProfile* dst_profile);

  const skcms_ICCProfile* SrcProfile() const;
  const skcms_ICCProfile* DstProfile() const;

 private:
  const skcms_ICCProfile* src_profile_;
  skcms_ICCProfile dst_profile_;

  DISALLOW_COPY_AND_ASSIGN(ColorProfileTransform);
};

// ImageDecoder is a base for all format-specific decoders
// (e.g. JPEGImageDecoder). This base manages the ImageFrame cache.
//
class PLATFORM_EXPORT ImageDecoder {
  USING_FAST_MALLOC(ImageDecoder);

 public:
  static const size_t kNoDecodedImageByteLimit =
      Platform::kNoDecodedImageByteLimit;

  enum AlphaOption { kAlphaPremultiplied, kAlphaNotPremultiplied };
  enum HighBitDepthDecodingOption {
    // Decode everything to uint8 pixel format (kN32 channel order).
    kDefaultBitDepth,
    // Decode high bit depth images to half float pixel format.
    kHighBitDepthToHalfFloat
  };

  // The first three values are as defined in webp/decode.h, the last value
  // specifies WebP animation formats.
  enum CompressionFormat {
    kUndefinedFormat = 0,
    kLossyFormat = 1,
    kLosslessFormat = 2,
    kWebPAnimationFormat = 3,
    kMaxValue = kWebPAnimationFormat,
  };

  // Enforces YUV decoding to be disallowed in the image decoder. The default
  // value defers to the YUV decoding decision to the decoder.
  enum class OverrideAllowDecodeToYuv {
    kDefault,
    kDeny,
  };

  virtual ~ImageDecoder() = default;

  // Returns a caller-owned decoder of the appropriate type.  Returns nullptr if
  // we can't sniff a supported type from the provided data (possibly
  // because there isn't enough data yet).
  // Sets |max_decoded_bytes_| to Platform::MaxImageDecodedBytes().
  static std::unique_ptr<ImageDecoder> Create(
      scoped_refptr<SegmentReader> data,
      bool data_complete,
      AlphaOption,
      HighBitDepthDecodingOption,
      const ColorBehavior&,
      const OverrideAllowDecodeToYuv allow_decode_to_yuv =
          OverrideAllowDecodeToYuv::kDefault,
      const SkISize& desired_size = SkISize::MakeEmpty());
  static std::unique_ptr<ImageDecoder> Create(
      scoped_refptr<SharedBuffer> data,
      bool data_complete,
      AlphaOption alpha_option,
      HighBitDepthDecodingOption high_bit_depth_decoding_option,
      const ColorBehavior& color_behavior,
      const OverrideAllowDecodeToYuv allow_decode_to_yuv =
          OverrideAllowDecodeToYuv::kDefault,
      const SkISize& desired_size = SkISize::MakeEmpty()) {
    return Create(SegmentReader::CreateFromSharedBuffer(std::move(data)),
                  data_complete, alpha_option, high_bit_depth_decoding_option,
                  color_behavior, allow_decode_to_yuv, desired_size);
  }

  virtual String FilenameExtension() const = 0;

  bool IsAllDataReceived() const { return is_all_data_received_; }

  // Returns true if the decoder supports decoding to high bit depth. The
  // decoded output will be high bit depth (half float backed bitmap) iff
  // encoded image is high bit depth and high_bit_depth_decoding_option_ is set
  // to kHighBitDepthToHalfFloat.
  virtual bool ImageIsHighBitDepth() { return false; }

  // Returns true if the buffer holds enough data to instantiate a decoder.
  // This is useful for callers to determine whether a decoder instantiation
  // failure is due to insufficient or bad data.
  static bool HasSufficientDataToSniffImageType(const SharedBuffer&);

  // Looks at the image data to determine and return the image MIME type.
  static String SniffImageType(scoped_refptr<SharedBuffer> image_data);

  // Returns the image data's compression format.
  static CompressionFormat GetCompressionFormat(
      scoped_refptr<SharedBuffer> image_data,
      String mime_type);

  void SetData(scoped_refptr<SegmentReader> data, bool all_data_received) {
    if (failed_)
      return;
    data_ = std::move(data);
    is_all_data_received_ = all_data_received;
    OnSetData(data_.get());
  }

  void SetData(scoped_refptr<SharedBuffer> data, bool all_data_received) {
    SetData(SegmentReader::CreateFromSharedBuffer(std::move(data)),
            all_data_received);
  }

  virtual void OnSetData(SegmentReader* data) {}

  bool IsSizeAvailable() {
    if (failed_)
      return false;
    if (!size_available_)
      DecodeSize();
    return IsDecodedSizeAvailable();
  }

  bool IsDecodedSizeAvailable() const { return !failed_ && size_available_; }

  virtual IntSize Size() const { return size_; }
  virtual Vector<SkISize> GetSupportedDecodeSizes() const { return {}; }

  // Decoders which downsample images should override this method to
  // return the actual decoded size.
  virtual IntSize DecodedSize() const { return Size(); }

  // Image decoders that support YUV decoding must override this to
  // provide the size of each component.
  virtual IntSize DecodedYUVSize(int component) const {
    NOTREACHED();
    return IntSize();
  }

  // Image decoders that support YUV decoding must override this to
  // return the width of each row of the memory allocation.
  virtual size_t DecodedYUVWidthBytes(int component) const {
    NOTREACHED();
    return 0;
  }

  // Image decoders that support YUV decoding must override this to
  // return the SkYUVColorSpace that is used to convert from YUV
  // to RGB.
  virtual SkYUVColorSpace GetYUVColorSpace() const {
    NOTREACHED();
    return SkYUVColorSpace::kIdentity_SkYUVColorSpace;
  }

  // Returns the information required to decide whether or not hardware
  // acceleration can be used to decode this image. Callers of this function
  // must ensure the header was successfully parsed prior to calling this
  // method, i.e., IsDecodedSizeAvailable() must return true.
  virtual cc::ImageHeaderMetadata MakeMetadataForDecodeAcceleration() const;

  // This will only differ from size() for ICO (where each frame is a
  // different icon) or other formats where different frames are different
  // sizes. This does NOT differ from size() for GIF or WebP, since
  // decoding GIF or WebP composites any smaller frames against previous
  // frames to create full-size frames.
  virtual IntSize FrameSizeAtIndex(size_t) const { return Size(); }

  // Returns whether the size is legal (i.e. not going to result in
  // overflow elsewhere).  If not, marks decoding as failed.
  virtual bool SetSize(unsigned width, unsigned height) {
    unsigned decoded_bytes_per_pixel = 4;
    if (ImageIsHighBitDepth() &&
        high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat)
      decoded_bytes_per_pixel = 8;
    if (SizeCalculationMayOverflow(width, height, decoded_bytes_per_pixel))
      return SetFailed();

    size_ = IntSize(width, height);
    size_available_ = true;
    return true;
  }

  // Calls DecodeFrameCount() to get the frame count (if possible), without
  // decoding the individual frames.  Resizes |frame_buffer_cache_| to the
  // correct size and returns its size.
  size_t FrameCount();

  virtual int RepetitionCount() const { return kAnimationNone; }

  // Decodes as much of the requested frame as possible, and returns an
  // ImageDecoder-owned pointer.
  ImageFrame* DecodeFrameBufferAtIndex(size_t);

  // Whether the requested frame has alpha.
  virtual bool FrameHasAlphaAtIndex(size_t) const;

  // Whether or not the frame is fully received.
  virtual bool FrameIsReceivedAtIndex(size_t) const;

  // Returns true if a cached complete decode is available.
  bool FrameIsDecodedAtIndex(size_t) const;

  // Duration for displaying a frame. This method is only used by animated
  // images.
  virtual base::TimeDelta FrameDurationAtIndex(size_t) const {
    return base::TimeDelta();
  }

  // Number of bytes in the decoded frame. Returns 0 if the decoder doesn't
  // have this frame cached (either because it hasn't been decoded, or because
  // it has been cleared).
  virtual size_t FrameBytesAtIndex(size_t) const;

  ImageOrientation Orientation() const { return orientation_; }

  bool IgnoresColorSpace() const { return color_behavior_.IsIgnore(); }
  const ColorBehavior& GetColorBehavior() const { return color_behavior_; }

  // This returns the color space that will be included in the SkImageInfo of
  // SkImages created from this decoder. This will be nullptr unless the
  // decoder was created with the option ColorSpaceTagged.
  sk_sp<SkColorSpace> ColorSpaceForSkImages();

  // This returns whether or not the image included a not-ignored embedded
  // color profile. This is independent of whether or not that profile's
  // transform has been baked into the pixel values.
  bool HasEmbeddedColorProfile() const { return embedded_color_profile_.get(); }

  void SetEmbeddedColorProfile(std::unique_ptr<ColorProfile> profile);

  // Transformation from embedded color space to target color space.
  ColorProfileTransform* ColorTransform();

  AlphaOption GetAlphaOption() const {
    return premultiply_alpha_ ? kAlphaPremultiplied : kAlphaNotPremultiplied;
  }

  size_t GetMaxDecodedBytes() const { return max_decoded_bytes_; }

  // Sets the "decode failure" flag.  For caller convenience (since so
  // many callers want to return false after calling this), returns false
  // to enable easy tailcalling.  Subclasses may override this to also
  // clean up any local data.
  virtual bool SetFailed() {
    failed_ = true;
    return false;
  }

  bool Failed() const { return failed_; }

  // Clears decoded pixel data from all frames except the provided frame. If
  // subsequent frames depend on this frame's required previous frame, then that
  // frame is also kept in cache to prevent re-decoding from the beginning.
  // Callers may pass WTF::kNotFound to clear all frames.
  // Note: If |frame_buffer_cache_| contains only one frame, it won't be
  // cleared. Returns the number of bytes of frame data actually cleared.
  virtual size_t ClearCacheExceptFrame(size_t);

  // If the image has a cursor hot-spot, stores it in the argument
  // and returns true. Otherwise returns false.
  virtual bool HotSpot(IntPoint&) const { return false; }

  virtual void SetMemoryAllocator(SkBitmap::Allocator* allocator) {
    // FIXME: this doesn't work for images with multiple frames.
    if (frame_buffer_cache_.IsEmpty()) {
      // Ensure that InitializeNewFrame is called, after parsing if
      // necessary.
      if (!FrameCount())
        return;
    }

    frame_buffer_cache_[0].SetMemoryAllocator(allocator);
  }

  bool CanDecodeToYUV() { return allow_decode_to_yuv_; }
  // Should only be called if CanDecodeToYuv() returns true, in which case
  // the subclass of ImageDecoder must override this method.
  virtual void DecodeToYUV() { NOTREACHED(); }
  void SetImagePlanes(std::unique_ptr<ImagePlanes> image_planes) {
    image_planes_ = std::move(image_planes);
  }

 protected:
  ImageDecoder(AlphaOption alpha_option,
               HighBitDepthDecodingOption high_bit_depth_decoding_option,
               const ColorBehavior& color_behavior,
               size_t max_decoded_bytes,
               const bool allow_decode_to_yuv = false)
      : premultiply_alpha_(alpha_option == kAlphaPremultiplied),
        high_bit_depth_decoding_option_(high_bit_depth_decoding_option),
        color_behavior_(color_behavior),
        max_decoded_bytes_(max_decoded_bytes),
        allow_decode_to_yuv_(allow_decode_to_yuv),
        purge_aggressively_(false) {}

  // Calculates the most recent frame whose image data may be needed in
  // order to decode frame |frame_index|, based on frame disposal methods
  // and |frame_rect_is_opaque|, where |frame_rect_is_opaque| signifies whether
  // the rectangle of frame at |frame_index| is known to be opaque.
  // If no previous frame's data is required, returns WTF::kNotFound.
  //
  // This function requires that the previous frame's
  // |required_previous_frame_index_| member has been set correctly. The
  // easiest way to ensure this is for subclasses to call this method and
  // store the result on the frame via SetRequiredPreviousFrameIndex()
  // as soon as the frame has been created and parsed sufficiently to
  // determine the disposal method; assuming this happens for all frames
  // in order, the required invariant will hold.
  //
  // Image formats which do not use more than one frame do not need to
  // worry about this; see comments on
  // ImageFrame::required_previous_frame+index_.
  size_t FindRequiredPreviousFrame(size_t frame_index,
                                   bool frame_rect_is_opaque);

  // This is called by ClearCacheExceptFrame() if that method decides it wants
  // to preserve another frame, to avoid unnecessary redecoding.
  size_t ClearCacheExceptTwoFrames(size_t, size_t);
  virtual void ClearFrameBuffer(size_t frame_index);

  // Decodes the image sufficiently to determine the image size.
  virtual void DecodeSize() = 0;

  // Decodes the image sufficiently to determine the number of frames and
  // returns that number.
  virtual size_t DecodeFrameCount() { return 1; }

  // Called to initialize the frame buffer with the given index, based on the
  // provided and previous frame's characteristics. Returns true on success.
  // Before calling this method, the caller must verify that the frame exists.
  // On failure, the client should call SetFailed. This method does not call
  // SetFailed itself because that might delete the object directly making this
  // call.
  bool InitFrameBuffer(size_t);

  // Performs any additional setup of the requested frame after it has been
  // initially created, e.g. setting a duration or disposal method.
  virtual void InitializeNewFrame(size_t) {}

  // Decodes the requested frame.
  virtual void Decode(size_t) = 0;

  // This method is only required for animated images. It returns a vector with
  // all frame indices that need to be decoded in order to succesfully decode
  // the provided frame.  The indices are returned in reverse order, so the
  // last frame needs to be decoded first.  Before calling this method, the
  // caller must verify that the frame exists.
  Vector<size_t> FindFramesToDecode(size_t) const;

  // This is called by Decode() after decoding a frame in an animated image.
  // Before calling this method, the caller must verify that the frame exists.
  // @return true  if the frame was fully decoded,
  //         false otherwise.
  bool PostDecodeProcessing(size_t);

  // The GIF and PNG decoders set the default alpha setting of the ImageFrame to
  // true. When the frame rect does not contain any (semi-) transparent pixels,
  // this may need to be changed to false. This depends on whether the required
  // previous frame adds transparency to the image, outside of the frame rect.
  // This methods corrects the alpha setting of the frame buffer to false when
  // the whole frame is opaque.
  //
  // This method should be called by the GIF and PNG decoder when the pixels in
  // the frame rect do *not* contain any transparent pixels. Before calling
  // this method, the caller must verify that the frame exists.
  void CorrectAlphaWhenFrameBufferSawNoAlpha(size_t);

  scoped_refptr<SegmentReader> data_;  // The encoded data.
  Vector<ImageFrame, 1> frame_buffer_cache_;
  const bool premultiply_alpha_;
  const HighBitDepthDecodingOption high_bit_depth_decoding_option_;
  const ColorBehavior color_behavior_;
  ImageOrientation orientation_;

  // The maximum amount of memory a decoded image should require. Ideally,
  // image decoders should downsample large images to fit under this limit
  // (and then return the downsampled size from DecodedSize()). Ignoring
  // this limit can cause excessive memory use or even crashes on low-
  // memory devices.
  const size_t max_decoded_bytes_;

  // While decoding, we may learn that there are so many animation frames that
  // we would go beyond our cache budget.
  // If that happens, purge_aggressively_ is set to true. This signals
  // future decodes to purge old frames as it goes.
  void UpdateAggressivePurging(size_t index);

  // The method is only relevant for multi-frame images.
  //
  // This method indicates whether the provided frame has enough data to decode
  // successive frames that depend on it. It is used by ClearCacheExceptFrame
  // to determine which frame to keep in cache when the indicated frame is not
  // yet sufficiently decoded.
  //
  // The default condition is that the frame status needs to be FramePartial or
  // FrameComplete, since the data of previous frames is copied in
  // InitFrameBuffer() before setting the status to FramePartial. For WebP,
  // however, the status needs to be FrameComplete since the complete buffer is
  // used to do alpha blending in WEBPImageDecoder::ApplyPostProcessing().
  //
  // Before calling this, verify that frame |index| exists by checking that
  // |index| is smaller than |frame_buffer_cache_|.size().
  virtual bool FrameStatusSufficientForSuccessors(wtf_size_t index) {
    DCHECK(index < frame_buffer_cache_.size());
    ImageFrame::Status frame_status = frame_buffer_cache_[index].GetStatus();
    return frame_status == ImageFrame::kFramePartial ||
           frame_status == ImageFrame::kFrameComplete;
  }

  // Note that |allow_decode_to_yuv_| being true merely means that the
  // ImageDecoder supports decoding to YUV. Other layers higher in the
  // stack (the PaintImageGenerator, ImageFrameGenerator, or cache) may
  // decline to go down the YUV path.
  bool allow_decode_to_yuv_;
  std::unique_ptr<ImagePlanes> image_planes_;

 private:
  // The YUV subsampling of the image.
  virtual cc::YUVSubsampling GetYUVSubsampling() const {
    return cc::YUVSubsampling::kUnknown;
  }

  // Some code paths compute the size of the image as "width * height * 4 or 8"
  // and return it as a (signed) int.  Avoid overflow.
  inline bool SizeCalculationMayOverflow(unsigned width,
                                         unsigned height,
                                         unsigned decoded_bytes_per_pixel) {
    base::CheckedNumeric<int32_t> total_size = width;
    total_size *= height;
    total_size *= decoded_bytes_per_pixel;
    return !total_size.IsValid();
  }

  bool purge_aggressively_;

  // This methods gets called at the end of InitFrameBuffer. Subclasses can do
  // format specific initialization, for e.g. alpha settings, here.
  virtual void OnInitFrameBuffer(size_t) {}

  // Called by InitFrameBuffer to determine if it can take the bitmap of the
  // previous frame. This condition is different for GIF and WEBP.
  virtual bool CanReusePreviousFrameBuffer(size_t) const { return false; }

  IntSize size_;
  bool size_available_ = false;
  bool is_all_data_received_ = false;
  bool failed_ = false;

  std::unique_ptr<ColorProfile> embedded_color_profile_;
  sk_sp<SkColorSpace> color_space_for_sk_images_;

  bool source_to_target_color_transform_needs_update_ = false;
  std::unique_ptr<ColorProfileTransform> source_to_target_color_transform_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecoder);
};

}  // namespace blink

#endif
