// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DAVXD_VIDEO_DECODER_BASE_H_
#define MEDIA_FILTERS_DAVXD_VIDEO_DECODER_BASE_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/base/agtm.h"
#include "media/base/decoder_status.h"
#include "media/base/frame_buffer_pool.h"
#include "media/base/hdr_metadata_reordering_map.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_types.h"
#include "media/filters/offloading_video_decoder.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

class CdmContext;
class DecoderBuffer;
class VideoFrame;

// Structural requirements shared by Dav1dPicture and Dav2dPicture.
template <typename T>
concept DavXdPicture = requires(T& p) {
  { p.p.w } -> std::same_as<int&>;
  { p.p.h } -> std::same_as<int&>;
  { p.p.bpc } -> std::same_as<int&>;
  requires std::same_as<decltype(p.data), void* [3]>;
  requires std::same_as<decltype(p.stride), ptrdiff_t[2]>;
  { p.m.timestamp } -> std::same_as<int64_t&>;
  requires std::is_pointer_v<decltype(p.itut_t35)>;
  requires std::same_as<std::remove_cvref_t<decltype(p.itut_t35->country_code)>,
                        uint8_t>;
  { p.itut_t35->payload } -> std::convertible_to<const uint8_t*>;
  requires std::same_as<std::remove_cvref_t<decltype(p.itut_t35->payload_size)>,
                        size_t>;
  { p.n_itut_t35 } -> std::same_as<size_t&>;
  { p.allocator_data } -> std::same_as<void*&>;
};

// Base class for dav1d-family software video decoders (e.g. Dav1dVideoDecoder).
// Handles threading, callback binding, frame pool allocation, monochrome UV
// plane synthesis, HDR metadata reordering, and VideoFrame wrapping.
class MEDIA_EXPORT DavXdVideoDecoderBase : public OffloadableVideoDecoder {
 public:
  explicit DavXdVideoDecoderBase(
      std::unique_ptr<MediaLog> media_log,
      OffloadState offload_state = OffloadState::kNormal);

  DavXdVideoDecoderBase(const DavXdVideoDecoderBase&) = delete;
  DavXdVideoDecoderBase& operator=(const DavXdVideoDecoderBase&) = delete;

  ~DavXdVideoDecoderBase() override;

  // VideoDecoder implementation.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;

  // OffloadableVideoDecoder implementation.
  void Detach() override;

 protected:
  // Hooks implemented by codec-specific subclasses.
  virtual VideoCodec GetSupportedCodec() const = 0;
  virtual DecoderStatus OpenDecoder(bool low_delay, int n_threads) = 0;
  virtual void CloseCodec() = 0;
  virtual void FlushCodec() = 0;
  virtual bool DecodeBuffer(scoped_refptr<DecoderBuffer> buffer) = 0;

  // Shared helpers for codec-specific subclasses.
  template <DavXdPicture PictureT>
  static int AllocateCustomPicture(PictureT* p,
                                   size_t alignment,
                                   VideoChromaSampling chroma_sampling,
                                   void* frame_pool_opaque) {
    return AllocatePictureBuffer(p->p.w, p->p.h, p->p.bpc, chroma_sampling,
                                 alignment, frame_pool_opaque, p->stride,
                                 p->data, &p->allocator_data);
  }

  template <DavXdPicture PictureT>
  static void ReleaseCustomPicture(PictureT* p, void* /*cookie*/) {
    if (!p || !p->allocator_data) {
      return;
    }
    ReleasePictureBuffer(std::exchange(p->allocator_data, nullptr));
  }

  static void ReleaseDecoderBuffer(const uint8_t* buffer, void* opaque);
  static void LogDecoderMessage(void* cookie, const char* format, va_list ap);

  // Releases any configured decoder and clears reordering state.
  void CloseDecoder();

  // Records HDR metadata from an input buffer prior to decoding.
  void RecordBufferMetadata(const DecoderBuffer& buffer);

  // Wraps `pic` in a VideoFrame, applies HDR/color space metadata, attaches the
  // frame pool destruction observer, and delivers it via `output_cb_`.
  template <DavXdPicture PictureT>
  bool OutputDecodedPicture(const PictureT& pic,
                            const VideoColorSpace& color_space) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    gfx::HDRMetadata hdr_metadata = config_.hdr_metadata();
    if (pic.itut_t35) {
      // SAFETY: The best we can do is trust the size provided by the codec
      // library.
      auto t35s = UNSAFE_BUFFERS(base::span(pic.itut_t35, pic.n_itut_t35));
      for (const auto& t35 : t35s) {
        // SAFETY: The best we can do is trust the size provided by the codec
        // library.
        auto payload = UNSAFE_BUFFERS(
            base::span<const uint8_t>(t35.payload, t35.payload_size));
        SetAgtmFromT35WithCountryCode(hdr_metadata, t35.country_code, payload);
      }
    }

    return OutputPicture(gfx::Size(pic.p.w, pic.p.h), pic.p.bpc,
                         base::Microseconds(pic.m.timestamp), color_space,
                         std::move(hdr_metadata), pic.allocator_data);
  }

  void SetErrorStatus(DecoderStatus::Codes status) { error_status_ = status; }
  MediaLog* media_log() const { return media_log_.get(); }
  FrameBufferPool* frame_pool() const { return frame_pool_.get(); }
  const VideoDecoderConfig& config() const { return config_; }

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  enum class DecoderState { kUninitialized, kNormal, kError };

  static int AllocatePictureBuffer(int width,
                                   int height,
                                   int bit_depth,
                                   VideoChromaSampling chroma_sampling,
                                   size_t alignment,
                                   void* frame_pool_opaque,
                                   base::span<ptrdiff_t, 2> strides,
                                   base::span<void*, 3> planes,
                                   void** allocator_data);
  static void ReleasePictureBuffer(void* allocator_data);

  bool OutputPicture(const gfx::Size& visible_size,
                     int bit_depth,
                     base::TimeDelta timestamp,
                     const VideoColorSpace& color_space,
                     gfx::HDRMetadata hdr_metadata,
                     const void* allocator_data);
  scoped_refptr<VideoFrame> BindImageToVideoFrame(const gfx::Size& visible_size,
                                                  int bit_depth,
                                                  base::TimeDelta timestamp,
                                                  const void* allocator_data);

  // Used to report error messages to the client.
  std::unique_ptr<MediaLog> media_log_;

  // Indicates if the decoder is being wrapped by OffloadingVideoDecoder;
  // controls whether callbacks are bound to the current loop on calls.
  const bool bind_callbacks_;

  // "Zero" filled UV data for monochrome images to use since Chromium doesn't
  // have support for I400P(8|10|12) images.
  scoped_refptr<base::RefCountedMemory> fake_uv_data_;

  // Current decoder state. Used to ensure methods are called as expected.
  DecoderState state_ = DecoderState::kUninitialized;

  // Callback given during Initialize() used for delivering decoded frames.
  OutputCB output_cb_;

  // The configuration passed to Initialize(), saved since some fields are
  // needed to annotate video frames after decoding.
  VideoDecoderConfig config_;

  // Picture allocation pool. Used instead of the default allocator so that OOM
  // failures can return an error instead of crashing the tab.
  scoped_refptr<FrameBufferPool> frame_pool_;

  // More specific error code to surface after an error occurs during decoding.
  DecoderStatus::Codes error_status_ = DecoderStatus::Codes::kFailed;

  // Since decoders can reorder frames, we need to store HDR metadata from the
  // DecoderBuffer and associate it with the frame's timestamp.
  HdrMetadataReorderingMap hdr_metadata_reordering_map_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_DAVXD_VIDEO_DECODER_BASE_H_
