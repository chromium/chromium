// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_APPLE_SAMPLE_BUFFER_TRANSFORMER_H_
#define MEDIA_CAPTURE_VIDEO_APPLE_SAMPLE_BUFFER_TRANSFORMER_H_

#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/feature_list.h"
#include "media/capture/capture_export.h"
#include "media/capture/video/apple/pixel_buffer_pool.h"
#include "media/capture/video/apple/pixel_buffer_transferer.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_IOS)
#include "media/capture/video/ios/pixel_buffer_rotator.h"
#endif

namespace media {

// Capable of converting from any supported capture format (NV12, YUY2, UYVY and
// MJPEG) to NV12 or I420 and doing rescaling. This class can be configured to
// use VTPixelTransferSession (sometimes HW-accelerated) or third_party/libyuv
// (SW-only). The output is always an IOSurface-backed pixel buffer that comes
// from an internal pixel buffer pool.
class CAPTURE_EXPORT SampleBufferTransformer {
 public:
  enum class Transformer {
    kNotConfigured,
    // Supports (Any except MJPEG) -> (NV12, I420, ...)
    kPixelBufferTransfer,
    // Supports (Any) -> (NV12 or I420)
    kLibyuv,
  };

  // TODO(crbug.com/40747532): Make determining the optimal Transformer
  // an implementation detail determined at Transform()-time, making
  // Reconfigure() only care about destination resolution and pixel format. Then
  // make it possible to override this decision explicitly but only do that for
  // testing and measurements purposes, not in default capturer integration.
  static const Transformer kBestTransformerForPixelBufferToNv12Output;
  static Transformer GetBestTransformerForNv12Output(
      CMSampleBufferRef sample_buffer);

  static std::unique_ptr<SampleBufferTransformer> Create();
  ~SampleBufferTransformer();

  Transformer transformer() const;
  OSType destination_pixel_format() const;
  const gfx::Size& destination_size() const;

  // Future calls to Transform() will output pixel buffers according to this
  // configuration. Changing configuration will allocate a new buffer pool, but
  // calling Reconfigure() multiple times with the same parameters is a NO-OP.
  void Reconfigure(Transformer transformer,
                   OSType destination_pixel_format,
                   const gfx::Size& destination_size,
                   int rotation_angle,
                   std::optional<size_t> buffer_pool_size = std::nullopt);

  // Converts the input buffer to an IOSurface-backed pixel buffer according to
  // current configurations. If no transformation is needed (input format is the
  // same as the configured output format), the input pixel buffer is returned.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> Transform(
      CVPixelBufferRef pixel_buffer);
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> Transform(
      CMSampleBufferRef sample_buffer);

#if BUILDFLAG(IS_IOS)
  // Rotates a source pixel buffer and returns rotated pixel buffer as a output.
  base::apple::ScopedCFTypeRef<CVPixelBufferRef> Rotate(
      CVPixelBufferRef source_pixel_buffer);
#endif

 private:
  friend std::unique_ptr<SampleBufferTransformer>
  std::make_unique<SampleBufferTransformer>();

  SampleBufferTransformer();

  // Sample buffers from the camera contain pixel buffers when an uncompressed
  // pixel format is used (i.e. it's not MJPEG).
  void TransformPixelBuffer(CVPixelBufferRef source_pixel_buffer,
                            CVPixelBufferRef destination_pixel_buffer);
  // (Any uncompressed -> Any uncompressed)
  void TransformPixelBufferWithPixelTransfer(
      CVPixelBufferRef source_pixel_buffer,
      CVPixelBufferRef destination_pixel_buffer);
  // (Any uncompressed -> NV12 or I420)
  void TransformPixelBufferWithLibyuv(
      CVPixelBufferRef source_pixel_buffer,
      CVPixelBufferRef destination_pixel_buffer);
  void TransformPixelBufferWithLibyuvFromAnyToI420(
      CVPixelBufferRef source_pixel_buffer,
      CVPixelBufferRef destination_pixel_buffer);
  void TransformPixelBufferWithLibyuvFromAnyToNV12(
      CVPixelBufferRef source_pixel_buffer,
      CVPixelBufferRef destination_pixel_buffer);
  // Sample buffers from the camera contain byte buffers when MJPEG is used.
  bool TransformSampleBuffer(CMSampleBufferRef source_sample_buffer,
                             CVPixelBufferRef destination_pixel_buffer);
  bool TransformSampleBufferFromMjpegToI420(
      uint8_t* source_buffer_data_base_address,
      size_t source_buffer_data_size,
      size_t source_width,
      size_t source_height,
      CVPixelBufferRef destination_pixel_buffer);
  bool TransformSampleBufferFromMjpegToNV12(
      uint8_t* source_buffer_data_base_address,
      size_t source_buffer_data_size,
      size_t source_width,
      size_t source_height,
      CVPixelBufferRef destination_pixel_buffer);

  Transformer transformer_;
  OSType destination_pixel_format_;
  gfx::Size destination_size_;
  std::unique_ptr<PixelBufferPool> destination_pixel_buffer_pool_;
  // For kPixelBufferTransfer.
  std::unique_ptr<PixelBufferTransferer> pixel_buffer_transferer_;
  // For kLibyuv in cases where an intermediate buffer is needed.
  std::vector<uint8_t> intermediate_i420_buffer_;
  std::vector<uint8_t> intermediate_nv12_buffer_;

  int rotation_angle_;
#if BUILDFLAG(IS_IOS)
  std::unique_ptr<PixelBufferPool> rotated_destination_pixel_buffer_pool_;
  std::unique_ptr<PixelBufferRotator> pixel_buffer_rotator_;
#endif
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_APPLE_SAMPLE_BUFFER_TRANSFORMER_H_
