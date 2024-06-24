// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_AV1_DECODER_H_
#define MEDIA_GPU_VAAPI_TEST_AV1_DECODER_H_

#include <memory>
#include <optional>
#include <vector>

#include "media/gpu/vaapi/test/video_decoder.h"
#include "media/parsers/ivf_parser.h"
// For libgav1::ObuSequenceHeader. std::optional demands ObuSequenceHeader to
// fulfill std::is_trivially_constructible if it is forward-declared. But
// ObuSequenceHeader doesn't.
#include "third_party/libgav1/src/src/obu_parser.h"

namespace media {
namespace vaapi_test {

class ScopedVAConfig;
class ScopedVAContext;
class SharedVASurface;
class VaapiDevice;

constexpr size_t kAv1NumRefFrames = libgav1::kNumReferenceFrameTypes;

// An Av1Decoder decodes AV1-encoded IVF streams using direct libva calls.
class Av1Decoder : public VideoDecoder {
 public:
  Av1Decoder(std::unique_ptr<IvfParser> ivf_parser,
             const VaapiDevice& va_device,
             SharedVASurface::FetchPolicy fetch_policy);
  Av1Decoder(const Av1Decoder&) = delete;
  Av1Decoder& operator=(const Av1Decoder&) = delete;
  ~Av1Decoder() override;

  // VideoDecoder implementation.
  VideoDecoder::Result DecodeNextFrame() override;

 private:
  enum class ParsingResult {
    kFailed,
    kOk,
    kEOStream,
  };

  // Reads an OBU frame, if there is one available.
  // If an |obu_parser_| didn't exist and there is data to be read,
  // |obu_parser_| will be created. If there is an existing
  // |current_sequence_header_|, this will be passed to the ObuParser that is
  // created. If successful (indicated by returning VideoDecoder::kOk), then the
  // fields |ivf_frame_header_|, |ivf_frame_data_|, and |current_frame_| will be
  // set upon completion.
  ParsingResult ReadNextFrame(libgav1::RefCountedBufferPtr& current_frame);

  // Refreshes current |ref_frames_| to refer to |surface|, |display_surfaces_|
  // to refer to |display_surface|, and |state_| to refer to |current_frame|
  // according to |refresh_frame_flags|.
  void RefreshReferenceSlots(uint8_t refresh_frame_flags,
                             scoped_refptr<SharedVASurface> surface,
                             libgav1::RefCountedBufferPtr current_frame,
                             scoped_refptr<SharedVASurface> display_surface);

  IvfFrameHeader ivf_frame_header_{};
  const uint8_t* ivf_frame_data_ = nullptr;

  // VA handles.
  std::unique_ptr<ScopedVAConfig> va_config_;
  std::unique_ptr<ScopedVAContext> va_context_;

  // AV1-specific data.
  std::unique_ptr<libgav1::ObuParser> obu_parser_;
  std::unique_ptr<libgav1::BufferPool> buffer_pool_;
  std::unique_ptr<libgav1::DecoderState> state_;
  std::optional<libgav1::ObuSequenceHeader> current_sequence_header_;
  std::vector<scoped_refptr<SharedVASurface>> ref_frames_;
  // If film grain is applied, the film grain surface is stored in
  // |display_surfaces_|. Otherwise, matches |ref_frames_|.
  std::vector<scoped_refptr<SharedVASurface>> display_surfaces_;

  // Parser for the IVF stream to decode.
  const std::unique_ptr<IvfParser> ivf_parser_;
};

}  // namespace vaapi_test
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_AV1_DECODER_H_
