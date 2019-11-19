// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of a VP9 bitstream parser. The main
// purpose of this parser is to support hardware decode acceleration. Some
// accelerators, e.g. libva which implements VA-API, require the caller
// (chrome) to feed them parsed VP9 frame header.
//
// See media::VP9Decoder for example usage.
//
#ifndef MEDIA_FILTERS_VP9_PARSER_H_
#define MEDIA_FILTERS_VP9_PARSER_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <memory>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decrypt_config.h"
#include "media/base/media_export.h"
#include "media/base/video_color_space.h"
#include "ui/gfx/geometry/size.h"

namespace media {

const int kVp9MaxProfile = 4;
const int kVp9NumRefFramesLog2 = 3;
const size_t kVp9NumRefFrames = 1 << kVp9NumRefFramesLog2;
const uint8_t kVp9MaxProb = 255;
const size_t kVp9NumRefsPerFrame = 3;
const size_t kVp9NumFrameContextsLog2 = 2;
const size_t kVp9NumFrameContexts = 1 << kVp9NumFrameContextsLog2;

using Vp9Prob = uint8_t;

enum class Vp9ColorSpace {
  UNKNOWN = 0,
  BT_601 = 1,
  BT_709 = 2,
  SMPTE_170 = 3,
  SMPTE_240 = 4,
  BT_2020 = 5,
  RESERVED = 6,
  SRGB = 7,
};

enum Vp9InterpolationFilter {
  EIGHTTAP = 0,
  EIGHTTAP_SMOOTH = 1,
  EIGHTTAP_SHARP = 2,
  BILINEAR = 3,
  SWITCHABLE = 4,
};

enum Vp9RefType {
  VP9_FRAME_INTRA = 0,
  VP9_FRAME_LAST = 1,
  VP9_FRAME_GOLDEN = 2,
  VP9_FRAME_ALTREF = 3,
  VP9_FRAME_MAX = 4,
};

enum Vp9ReferenceMode {
  SINGLE_REFERENCE = 0,
  COMPOUND_REFERENCE = 1,
  REFERENCE_MODE_SELECT = 2,
};

struct MEDIA_EXPORT Vp9SegmentationParams {
  static const size_t kNumSegments = 8;
  static const size_t kNumTreeProbs = kNumSegments - 1;
  static const size_t kNumPredictionProbs = 3;
  enum SegmentLevelFeature {
    SEG_LVL_ALT_Q = 0,
    SEG_LVL_ALT_LF = 1,
    SEG_LVL_REF_FRAME = 2,
    SEG_LVL_SKIP = 3,
    SEG_LVL_MAX
  };

  bool enabled;

  bool update_map;
  uint8_t tree_probs[kNumTreeProbs];
  bool temporal_update;
  uint8_t pred_probs[kNumPredictionProbs];

  bool update_data;
  bool abs_or_delta_update;
  bool feature_enabled[kNumSegments][SEG_LVL_MAX];
  int16_t feature_data[kNumSegments][SEG_LVL_MAX];

  int16_t y_dequant[kNumSegments][2];
  int16_t uv_dequant[kNumSegments][2];

  bool FeatureEnabled(size_t seg_id, SegmentLevelFeature feature) const {
    return feature_enabled[seg_id][feature];
  }

  int16_t FeatureData(size_t seg_id, SegmentLevelFeature feature) const {
    return feature_data[seg_id][feature];
  }
};

struct MEDIA_EXPORT Vp9LoopFilterParams {
  static const size_t kNumModeDeltas = 2;

  uint8_t level;
  uint8_t sharpness;

  bool delta_enabled;
  bool delta_update;
  bool update_ref_deltas[VP9_FRAME_MAX];
  int8_t ref_deltas[VP9_FRAME_MAX];
  bool update_mode_deltas[kNumModeDeltas];
  int8_t mode_deltas[kNumModeDeltas];

  // Calculated from above fields.
  uint8_t lvl[Vp9SegmentationParams::kNumSegments][VP9_FRAME_MAX]
             [kNumModeDeltas];
};

// Members of Vp9FrameHeader will be 0-initialized by Vp9Parser::ParseNextFrame.
struct MEDIA_EXPORT Vp9QuantizationParams {
  bool IsLossless() const {
    return base_q_idx == 0 && delta_q_y_dc == 0 && delta_q_uv_dc == 0 &&
           delta_q_uv_ac == 0;
  }

  uint8_t base_q_idx;
  int8_t delta_q_y_dc;
  int8_t delta_q_uv_dc;
  int8_t delta_q_uv_ac;
};

// Entropy context for frame parsing
struct MEDIA_EXPORT Vp9FrameContext {
  bool IsValid() const;

  Vp9Prob tx_probs_8x8[2][1];
  Vp9Prob tx_probs_16x16[2][2];
  Vp9Prob tx_probs_32x32[2][3];

  Vp9Prob coef_probs[4][2][2][6][6][3];
  Vp9Prob skip_prob[3];
  Vp9Prob inter_mode_probs[7][3];
  Vp9Prob interp_filter_probs[4][2];
  Vp9Prob is_inter_prob[4];

  Vp9Prob comp_mode_prob[5];
  Vp9Prob single_ref_prob[5][2];
  Vp9Prob comp_ref_prob[5];

  Vp9Prob y_mode_probs[4][9];
  Vp9Prob uv_mode_probs[10][9];
  Vp9Prob partition_probs[16][3];

  Vp9Prob mv_joint_probs[3];
  Vp9Prob mv_sign_prob[2];
  Vp9Prob mv_class_probs[2][10];
  Vp9Prob mv_class0_bit_prob[2];
  Vp9Prob mv_bits_prob[2][10];
  Vp9Prob mv_class0_fr_probs[2][2][3];
  Vp9Prob mv_fr_probs[2][3];
  Vp9Prob mv_class0_hp_prob[2];
  Vp9Prob mv_hp_prob[2];
};

struct MEDIA_EXPORT Vp9CompressedHeader {
  enum Vp9TxMode {
    ONLY_4X4 = 0,
    ALLOW_8X8 = 1,
    ALLOW_16X16 = 2,
    ALLOW_32X32 = 3,
    TX_MODE_SELECT = 4,
    TX_MODES = 5,
  };

  Vp9TxMode tx_mode;
  Vp9ReferenceMode reference_mode;
};

// VP9 frame header.
struct MEDIA_EXPORT Vp9FrameHeader {
  enum FrameType {
    KEYFRAME = 0,
    INTERFRAME = 1,
  };

  bool IsKeyframe() const;
  bool IsIntra() const;
  bool RefreshFlag(size_t i) const {
    return !!(refresh_frame_flags & (1u << i));
  }
  VideoColorSpace GetColorSpace() const;

  uint8_t profile;

  bool show_existing_frame;
  uint8_t frame_to_show_map_idx;

  FrameType frame_type;

  bool show_frame;
  bool error_resilient_mode;

  uint8_t bit_depth;
  Vp9ColorSpace color_space;
  bool color_range;
  uint8_t subsampling_x;
  uint8_t subsampling_y;

  // The range of frame_width and frame_height is 1..2^16.
  uint32_t frame_width;
  uint32_t frame_height;
  uint32_t render_width;
  uint32_t render_height;

  bool intra_only;
  uint8_t reset_frame_context;
  uint8_t refresh_frame_flags;
  uint8_t ref_frame_idx[kVp9NumRefsPerFrame];
  bool ref_frame_sign_bias[Vp9RefType::VP9_FRAME_MAX];
  bool allow_high_precision_mv;
  Vp9InterpolationFilter interpolation_filter;

  bool refresh_frame_context;
  bool frame_parallel_decoding_mode;
  uint8_t frame_context_idx;
  // |frame_context_idx_to_save_probs| is to be used by save_probs() only, and
  // |frame_context_idx| otherwise.
  uint8_t frame_context_idx_to_save_probs;

  Vp9QuantizationParams quant_params;

  uint8_t tile_cols_log2;
  uint8_t tile_rows_log2;

  // Pointer to the beginning of frame data. It is a responsibility of the
  // client of the Vp9Parser to maintain validity of this data while it is
  // being used outside of that class.
  const uint8_t* data;

  // Size of |data| in bytes.
  size_t frame_size;

  // Size of compressed header in bytes.
  size_t header_size_in_bytes;

  // Size of uncompressed header in bytes.
  size_t uncompressed_header_size;

  Vp9CompressedHeader compressed_header;
  // Initial frame entropy context after load_probs2(frame_context_idx).
  Vp9FrameContext initial_frame_context;
  // Current frame entropy context after header parsing.
  Vp9FrameContext frame_context;

  // Segmentation and loop filter params from uncompressed header
  Vp9SegmentationParams segmentation;
  Vp9LoopFilterParams loop_filter;
};

// A parser for VP9 bitstream.
class MEDIA_EXPORT Vp9Parser {
 public:
  // If context update is needed after decoding a frame, the client must
  // execute this callback, passing the updated context state.
  using ContextRefreshCallback = base::Callback<void(const Vp9FrameContext&)>;

  // ParseNextFrame() return values. See documentation for ParseNextFrame().
  enum Result {
    kOk,
    kInvalidStream,
    kEOStream,
    kAwaitingRefresh,
  };

  // The parsing context to keep track of references.
  struct ReferenceSlot {
    bool initialized;
    uint32_t frame_width;
    uint32_t frame_height;
    uint8_t subsampling_x;
    uint8_t subsampling_y;
    uint8_t bit_depth;

    // More fields for consistency checking.
    uint8_t profile;
    Vp9ColorSpace color_space;
  };

  // The parsing context that persists across frames.
  class Context {
   public:
    class Vp9FrameContextManager {
     public:
      Vp9FrameContextManager();
      ~Vp9FrameContextManager();
      bool initialized() const { return initialized_; }
      bool needs_client_update() const { return needs_client_update_; }
      const Vp9FrameContext& frame_context() const;

      // Resets to uninitialized state.
      void Reset();

      // Marks this context as requiring an update from parser's client.
      void SetNeedsClientUpdate();

      // Updates frame context.
      void Update(const Vp9FrameContext& frame_context);

      // Returns a callback to update frame context at a later time with.
      ContextRefreshCallback GetUpdateCb();

     private:
      // Updates frame context from parser's client.
      void UpdateFromClient(const Vp9FrameContext& frame_context);

      bool initialized_ = false;
      bool needs_client_update_ = false;
      Vp9FrameContext frame_context_;

      base::WeakPtrFactory<Vp9FrameContextManager> weak_ptr_factory_{this};
    };

    void Reset();

    // Mark |frame_context_idx| as requiring update from the client.
    void MarkFrameContextForUpdate(size_t frame_context_idx);

    // Update frame context at |frame_context_idx| with the contents of
    // |frame_context|.
    void UpdateFrameContext(size_t frame_context_idx,
                            const Vp9FrameContext& frame_context);

    // Return ReferenceSlot for frame at |ref_idx|.
    const ReferenceSlot& GetRefSlot(size_t ref_idx) const;

    // Update contents of ReferenceSlot at |ref_idx| with the contents of
    // |ref_slot|.
    void UpdateRefSlot(size_t ref_idx, const ReferenceSlot& ref_slot);

    const Vp9SegmentationParams& segmentation() const { return segmentation_; }

    const Vp9LoopFilterParams& loop_filter() const { return loop_filter_; }

   private:
    friend class Vp9UncompressedHeaderParser;
    friend class Vp9Parser;

    // Segmentation and loop filter state.
    Vp9SegmentationParams segmentation_;
    Vp9LoopFilterParams loop_filter_;

    // Frame references.
    ReferenceSlot ref_slots_[kVp9NumRefFrames];

    Vp9FrameContextManager frame_context_managers_[kVp9NumFrameContexts];
  };

  // The constructor. See ParseNextFrame() for comments for
  // |parsing_compressed_header|.
  explicit Vp9Parser(bool parsing_compressed_header);
  ~Vp9Parser();

  // Set a new stream buffer to read from, starting at |stream| and of size
  // |stream_size| in bytes. |stream| must point to the beginning of a single
  // frame or a single superframe, is owned by caller and must remain valid
  // until the next call to SetStream(). |spatial_layer_frame_size| may be
  // filled if the parsed stream is VP9 SVC. It stands for frame sizes of
  // spatial layers. SVC frame might have multiple frames without superframe
  // index. The info helps Vp9Parser detecting the beginning of each frame.
  void SetStream(const uint8_t* stream,
                 off_t stream_size,
                 const std::vector<uint32_t>& spatial_layer_frame_size,
                 std::unique_ptr<DecryptConfig> stream_config);

  void SetStream(const uint8_t* stream,
                 off_t stream_size,
                 std::unique_ptr<DecryptConfig> stream_config);

  // Parse the next frame in the current stream buffer, filling |fhdr| with
  // the parsed frame header and updating current segmentation and loop filter
  // state. The necessary frame size to decode |fhdr| fills in |allocate_size|.
  // The size can be larger than frame size of |fhdr| in the case of SVC stream.
  // Also fills |frame_decrypt_config| _if_ the parser was set to use a super
  // frame decrypt config.
  // Return kOk if a frame has successfully been parsed,
  //        kEOStream if there is no more data in the current stream buffer,
  //        kAwaitingRefresh if this frame awaiting frame context update, or
  //        kInvalidStream on error.
  Result ParseNextFrame(Vp9FrameHeader* fhdr,
                        gfx::Size* allocate_size,
                        std::unique_ptr<DecryptConfig>* frame_decrypt_config);

  // Perform the same superframe parsing logic, but don't attempt to parse
  // the normal frame headers afterwards, and then only return the decrypt
  // config, since the frame itself isn't useful for the testing.
  // Returns |true| if a frame would have been sent to |ParseUncompressedHeader|
  //         |false| if there was an error parsing the superframe.
  std::unique_ptr<DecryptConfig> NextFrameDecryptContextForTesting();
  std::string IncrementIVForTesting(const std::string& iv, uint32_t by);

  // Return current parsing context.
  const Context& context() const { return context_; }

  // Return a ContextRefreshCallback, which, if not null, has to be called with
  // the new context state after the frame associated with |frame_context_idx|
  // is decoded.
  ContextRefreshCallback GetContextRefreshCb(size_t frame_context_idx);

  // Clear parser state and return to an initialized state.
  void Reset();

 private:
  // Stores start pointer and size of each frame within the current superframe.
  struct FrameInfo {
    FrameInfo();
    FrameInfo(const FrameInfo& copy_from);
    FrameInfo(const uint8_t* ptr, off_t size);
    ~FrameInfo();

    FrameInfo& operator=(const FrameInfo& copy_from);
    bool IsValid() const { return ptr != nullptr; }
    void Reset() { ptr = nullptr; }

    // Starting address of the frame.
    const uint8_t* ptr = nullptr;

    // Size of the frame in bytes.
    off_t size = 0;

    // Necessary height and width to decode the frame.
    // This is filled only if the stream is SVC.
    gfx::Size allocate_size;

    std::unique_ptr<DecryptConfig> decrypt_config;
  };

  base::circular_deque<FrameInfo> ParseSuperframe();
  // Parses a frame in SVC stream with |spatial_layer_frame_size_|.
  base::circular_deque<FrameInfo> ParseSVCFrame();

  // Returns true and populates |result| with the parsing result if parsing of
  // current frame is finished (possibly unsuccessfully). |fhdr| will only be
  // populated and valid if |result| is kOk. Otherwise return false, indicating
  // that the compressed header must be parsed next.
  bool ParseUncompressedHeader(const FrameInfo& frame_info,
                               Vp9FrameHeader* fhdr,
                               Result* result,
                               Vp9Parser::Context* context);

  // Returns true if parsing of current frame is finished and |result| will be
  // populated with value of parsing result. Otherwise, needs to continue setup
  // current frame.
  bool ParseCompressedHeader(const FrameInfo& frame_info, Result* result);

  int64_t GetQIndex(const Vp9QuantizationParams& quant, size_t segid) const;
  // Returns true if the setup to |context_| succeeded.
  bool SetupSegmentationDequant();
  void SetupLoopFilter();
  // Returns true if the setup to |context| succeeded.
  void UpdateSlots(Vp9Parser::Context* context);

  // Current address in the bitstream buffer.
  const uint8_t* stream_;

  // Remaining bytes in stream_.
  off_t bytes_left_;

  const bool parsing_compressed_header_;

  // FrameInfo for the remaining frames in the current superframe to be parsed.
  base::circular_deque<FrameInfo> frames_;

  Context context_;

  // Encrypted stream info.
  std::unique_ptr<DecryptConfig> stream_decrypt_config_;

  // The frame size of each spatial layer.
  std::vector<uint32_t> spatial_layer_frame_size_;

  FrameInfo curr_frame_info_;
  Vp9FrameHeader curr_frame_header_;

  DISALLOW_COPY_AND_ASSIGN(Vp9Parser);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VP9_PARSER_H_
