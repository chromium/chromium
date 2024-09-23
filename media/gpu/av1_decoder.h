// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AV1_DECODER_H_
#define MEDIA_GPU_AV1_DECODER_H_

#include <array>
#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_types.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/av1_picture.h"
#include "media/gpu/media_gpu_export.h"
#include "third_party/libgav1/src/src/utils/constants.h"

// For libgav1::RefCountedBufferPtr.
#include "third_party/libgav1/src/src/buffer_pool.h"
// For libgav1::ObuSequenceHeader. std::optional demands ObuSequenceHeader to
// fulfill std::is_trivially_constructible if it is forward-declared. But
// ObuSequenceHeader doesn't.
#include "third_party/libgav1/src/src/obu_parser.h"
#include "ui/gfx/hdr_metadata.h"

namespace libgav1 {
struct DecoderState;
struct ObuFrameHeader;
template <typename T>
class Vector;
}  // namespace libgav1

namespace media {
using AV1ReferenceFrameVector =
    std::array<scoped_refptr<AV1Picture>, libgav1::kNumReferenceFrameTypes>;

// Clients of this class are expected to pass an AV1 OBU stream and are expected
// to provide an implementation of AV1Accelerator for offloading final steps
// of the decoding process.
//
// This class must be created, called and destroyed on a single thread, and
// does nothing internally on any other thread.
class MEDIA_GPU_EXPORT AV1Decoder : public AcceleratedVideoDecoder {
 public:
  class MEDIA_GPU_EXPORT AV1Accelerator {
   public:
    // Methods may return kTryAgain if they need additional data (provided
    // independently) in order to proceed. Examples are things like not having
    // an appropriate key to decode encrypted content. This is not considered an
    // unrecoverable error, but rather a pause to allow an application to
    // independently provide the required data. When AV1Decoder::Decode()
    // is called again, it will attempt to resume processing of the stream
    // by calling the same method again.
    enum class Status {
      // Operation completed successfully.
      kOk,

      // Operation failed.
      kFail,

      // Operation failed because some external data is missing. Retry the same
      // operation later, once the data has been provided.
      kTryAgain,
    };
    AV1Accelerator() = default;
    virtual ~AV1Accelerator() = default;
    AV1Accelerator(const AV1Accelerator&) = delete;
    AV1Accelerator& operator=(const AV1Accelerator&) = delete;

    // Creates an AV1Picture that the AV1Decoder can use to store some of the
    // information needed to request accelerated decoding. This picture is later
    // passed when calling SubmitDecode() so that the AV1Accelerator can submit
    // the decode request to the driver. It may also be stored for use as
    // reference to decode other pictures.
    // When a picture is no longer needed by the decoder, it will just drop
    // its reference to it, and it may do so at any time.
    // Note that this may return nullptr if the accelerator is not able to
    // provide any new pictures at the given time. The decoder must handle this
    // case and treat it as normal, returning kRanOutOfSurfaces from Decode().
    virtual scoped_refptr<AV1Picture> CreateAV1Picture(bool apply_grain) = 0;

    // |secure_handle| is a reference to the corresponding secure memory when
    // doing secure decoding on ARM. This is invoked instead of CreateAV1Picture
    // when doing secure decoding on ARM. Default implementation returns
    // nullptr.
    // TODO(jkardatzke): Remove this once we move to the V4L2 flat stateless
    // decoder and add a field to media::CodecPicture instead.
    virtual scoped_refptr<AV1Picture> CreateAV1PictureSecure(
        bool apply_grain,
        uint64_t secure_handle);

    // Submits |pic| to the driver for accelerated decoding. The following
    // parameters are also passed:
    // - |sequence_header|: the current OBU sequence header.
    // - |ref_frames|: the pictures used as reference for decoding |pic|.
    // - |tile_buffers|: tile information.
    // - |data|: the entire data of the DecoderBuffer set by
    //           AV1Decoder::SetStream().
    // Note that returning from this method does not mean that the decode
    // process is finished, but the caller may drop its references to |pic|
    // and |ref_frames| immediately, and |data| does not need to remain valid
    // after this method returns.
    virtual Status SubmitDecode(
        const AV1Picture& pic,
        const libgav1::ObuSequenceHeader& sequence_header,
        const AV1ReferenceFrameVector& ref_frames,
        const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
        base::span<const uint8_t> data) = 0;

    // Schedules output (display) of |pic|.
    // Note that returning from this method does not mean that |pic| has already
    // been outputted (displayed), but guarantees that all pictures will be
    // outputted in the same order as this method was called for them, and that
    // they are decoded before outputting (assuming SubmitDecode() has been
    // called for them beforehand).
    // Returns true when successful, false otherwise.
    virtual bool OutputPicture(const AV1Picture& pic) = 0;

    // Notifies the accelerater whenever there is a new stream to process.
    // The lifetime of the stream is determined by the caller of
    // AV1Decoder::SetStream(). `data` spans passed to SubmitDecode() will be
    // contained in `stream` (in fact exactly the same span as `stream` in the
    // current implementation).
    virtual Status SetStream(base::span<const uint8_t> stream,
                             const DecryptConfig* decrypt_config);
  };

  AV1Decoder(std::unique_ptr<AV1Accelerator> accelerator,
             VideoCodecProfile profile,
             const VideoColorSpace& container_color_space = VideoColorSpace());
  ~AV1Decoder() override;
  AV1Decoder(const AV1Decoder&) = delete;
  AV1Decoder& operator=(const AV1Decoder&) = delete;

  // AcceleratedVideoDecoder implementation.
  void SetStream(int32_t id, const DecoderBuffer& decoder_buffer) override;
  [[nodiscard]] bool Flush() override;
  void Reset() override;
  [[nodiscard]] DecodeResult Decode() override;
  gfx::Size GetPicSize() const override;
  gfx::Rect GetVisibleRect() const override;
  VideoCodecProfile GetProfile() const override;
  uint8_t GetBitDepth() const override;
  VideoChromaSampling GetChromaSampling() const override;
  VideoColorSpace GetVideoColorSpace() const override;
  std::optional<gfx::HDRMetadata> GetHDRMetadata() const override;
  size_t GetRequiredNumOfPictures() const override;
  size_t GetNumReferenceFrames() const override;

 private:
  friend class AV1DecoderTest;

  AV1Accelerator::Status DecodeAndOutputPicture(
      scoped_refptr<AV1Picture> pic,
      const libgav1::Vector<libgav1::TileBuffer>& tile_buffers);
  void UpdateReferenceFrames(scoped_refptr<AV1Picture> pic);
  void ClearReferenceFrames();
  // Checks that |ref_frames_| is consistent with libgav1's reference frame
  // state (returns false if not) and cleans old reference frames from
  // |ref_frames_| as needed. Also asserts that all reference frames needed by
  // |current_frame_header_| are in |ref_frames_|. This method should be called
  // prior to using |ref_frames_| (which includes calling
  // |accelerator_|->SubmitDecode());
  bool CheckAndCleanUpReferenceFrames();
  void ClearCurrentFrame();
  DecodeResult DecodeInternal();

  bool on_error_ = false;

  std::unique_ptr<libgav1::BufferPool> buffer_pool_;
  std::unique_ptr<libgav1::DecoderState> state_;
  std::unique_ptr<libgav1::ObuParser> parser_;

  const std::unique_ptr<AV1Accelerator> accelerator_;
  AV1ReferenceFrameVector ref_frames_;

  std::optional<libgav1::ObuSequenceHeader> current_sequence_header_;
  std::optional<libgav1::ObuFrameHeader> current_frame_header_;
  libgav1::RefCountedBufferPtr current_frame_;

  gfx::Rect visible_rect_;
  gfx::Size frame_size_;
  VideoCodecProfile profile_;
  VideoColorSpace container_color_space_;
  VideoColorSpace picture_color_space_;
  uint8_t bit_depth_ = 0;
  VideoChromaSampling chroma_sampling_ = VideoChromaSampling::kUnknown;
  std::optional<gfx::HDRMetadata> hdr_metadata_;

  int32_t stream_id_ = 0;
  raw_ptr<const uint8_t, DanglingUntriaged> stream_ = nullptr;
  size_t stream_size_ = 0;
  std::unique_ptr<DecryptConfig> decrypt_config_;

  // Secure handle to pass through to the accelerator when doing secure playback
  // on ARM.
  uint64_t secure_handle_ = 0;

  // Pending picture for decode when accelerator returns kTryAgain.
  scoped_refptr<AV1Picture> pending_pic_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_AV1_DECODER_H_
