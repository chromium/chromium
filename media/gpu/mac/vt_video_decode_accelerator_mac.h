// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VT_VIDEO_DECODE_ACCELERATOR_MAC_H_
#define MEDIA_GPU_MAC_VT_VIDEO_DECODE_ACCELERATOR_MAC_H_

#include <stdint.h>

#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/media_log.h"
#include "media/gpu/gpu_video_decode_accelerator_helpers.h"
#include "media/gpu/media_gpu_export.h"
#include "media/video/h264_parser.h"
#include "media/video/h264_poc.h"
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/video/h265_parser.h"
#include "media/video/h265_poc.h"
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_bindings.h"

// This must be included after gl_bindings.h, or the various GL headers on the
// system and in the source tree will conflict with each other.
#include <VideoToolbox/VideoToolbox.h>

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class VP9ConfigChangeDetector;
class VP9SuperFrameBitstreamFilter;

// Preload VideoToolbox libraries, needed for sandbox warmup.
MEDIA_GPU_EXPORT bool InitializeVideoToolbox();

// VideoToolbox.framework implementation of the VideoDecodeAccelerator
// interface for macOS.
class VTVideoDecodeAccelerator : public VideoDecodeAccelerator,
                                 public base::trace_event::MemoryDumpProvider {
 public:
  VTVideoDecodeAccelerator(const GpuVideoDecodeGLClient& gl_client_,
                           const gpu::GpuDriverBugWorkarounds& workarounds,
                           MediaLog* media_log);

  VTVideoDecodeAccelerator(const VTVideoDecodeAccelerator&) = delete;
  VTVideoDecodeAccelerator& operator=(const VTVideoDecodeAccelerator&) = delete;

  ~VTVideoDecodeAccelerator() override;

  // VideoDecodeAccelerator implementation.
  bool Initialize(const Config& config, Client* client) override;
  void Decode(BitstreamBuffer bitstream) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer,
              int32_t bitstream_id) override;
  void AssignPictureBuffers(
      const std::vector<PictureBuffer>& pictures) override;
  void ReusePictureBuffer(int32_t picture_id) override;
  void Flush() override;
  void Reset() override;
  void Destroy() override;
  bool TryToSetupDecodeOnSeparateSequence(
      const base::WeakPtr<Client>& decode_client,
      const scoped_refptr<base::SequencedTaskRunner>& decode_task_runner)
      override;
  bool SupportsSharedImagePictureBuffers() const override;

  // MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Called by OutputThunk() when VideoToolbox finishes decoding a frame.
  void Output(void* source_frame_refcon,
              OSStatus status,
              CVImageBufferRef image_buffer);

  static VideoDecodeAccelerator::SupportedProfiles GetSupportedProfiles(
      const gpu::GpuDriverBugWorkarounds& workarounds);

 private:
  // Logged to UMA, so never reuse values. Make sure to update
  // VTVDASessionFailureType in histograms.xml to match.
  enum VTVDASessionFailureType {
    SFT_SUCCESSFULLY_INITIALIZED = 0,
    SFT_PLATFORM_ERROR = 1,
    SFT_INVALID_STREAM = 2,
    SFT_UNSUPPORTED_STREAM_PARAMETERS = 3,
    SFT_DECODE_ERROR = 4,
    SFT_UNSUPPORTED_STREAM = 5,
    // Must always be equal to largest entry logged.
    SFT_MAX = SFT_UNSUPPORTED_STREAM
  };

  enum State {
    STATE_DECODING,
    STATE_ERROR,
    STATE_DESTROYING,
  };

  enum TaskType {
    TASK_FRAME,
    TASK_FLUSH,
    TASK_RESET,
    TASK_DESTROY,
  };

  struct Frame {
    explicit Frame(int32_t bitstream_id);
    ~Frame();

    // Associated bitstream buffer.
    int32_t bitstream_id;

    // Slice header information.
    bool has_slice = false;
    bool is_idr = false;
    bool has_recovery_point = false;
    bool has_mmco5 = false;
    int32_t pic_order_cnt = 0;
    int32_t reorder_window = 0;

    // Clean aperture size, as computed by CoreMedia.
    gfx::Size image_size;

    // Decoded image, if decoding was successful.
    base::ScopedCFTypeRef<CVImageBufferRef> image;

    // Dynamic HDR metadata, if any.
    absl::optional<gfx::HDRMetadata> hdr_metadata;
  };

  struct Task {
    Task(TaskType type);
    Task(Task&& other);
    ~Task();

    TaskType type;
    std::unique_ptr<Frame> frame;
  };

  struct PictureInfo {
    // A PictureInfo that specifies no texture IDs will be used for shared
    // images.
    PictureInfo();

    PictureInfo(const PictureInfo&) = delete;
    PictureInfo& operator=(const PictureInfo&) = delete;

    ~PictureInfo();

    int32_t bitstream_id = 0;

    // The shared image holder that will be passed to the client.
    std::vector<scoped_refptr<Picture::ScopedSharedImage>> scoped_shared_images;
  };

  struct FrameOrder {
    bool operator()(const std::unique_ptr<Frame>& lhs,
                    const std::unique_ptr<Frame>& rhs) const;
  };

  //
  // Methods for interacting with VideoToolbox. Run on |decoder_thread_|.
  //

  // Set up VideoToolbox using the current VPS (if codec is HEVC), SPS and PPS.
  // Returns true or calls NotifyError() before returning false.
  bool ConfigureDecoder();

  // Wait for VideoToolbox to output all pending frames. Returns true or calls
  // NotifyError() before returning false.
  bool FinishDelayedFrames();

  // |frame| is owned by |pending_frames_|.
  void DecodeTaskH264(scoped_refptr<DecoderBuffer> buffer, Frame* frame);
  void DecodeTaskVp9(scoped_refptr<DecoderBuffer> buffer, Frame* frame);
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  void DecodeTaskHEVC(scoped_refptr<DecoderBuffer> buffer, Frame* frame);
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  void DecodeDone(Frame* frame);

  //
  // Methods for interacting with |client_|. Run on |gpu_task_runner_|.
  //
  void NotifyError(Error vda_error_type,
                   VTVDASessionFailureType session_failure_type);

  // Since |media_log_| is invalidated in Destroy() on the GPU thread, the easy
  // thing to do is post to the GPU thread to use it. This helper handles the
  // thread hop if necessary.
  void WriteToMediaLog(MediaLogMessageLevel level, const std::string& message);

  // |type| is the type of task that the flush will complete, one of TASK_FLUSH,
  // TASK_RESET, or TASK_DESTROY.
  void QueueFlush(TaskType type);
  void FlushTask(TaskType type);
  void FlushDone(TaskType type);

  // Try to make progress on tasks in the |task_queue_| or sending frames in the
  // |reorder_queue_|.
  void ProcessWorkQueues();

  // These methods returns true if a task was completed, false otherwise.
  bool ProcessTaskQueue();
  bool ProcessReorderQueue();
  bool ProcessOutputQueue();
  bool ProcessFrame(const Frame& frame);
  bool SendFrame(const Frame& frame);

  //
  // GPU thread state.
  //
  const GpuVideoDecodeGLClient gl_client_;
  const gpu::GpuDriverBugWorkarounds workarounds_;
  std::unique_ptr<MediaLog> media_log_;

  raw_ptr<VideoDecodeAccelerator::Client, DanglingUntriaged> client_ = nullptr;
  State state_ = STATE_DECODING;

  // Queue of pending flush tasks. This is used to drop frames when a reset
  // is pending.
  base::queue<TaskType> pending_flush_tasks_;

  // Queue of tasks to complete in the GPU thread.
  base::queue<Task> task_queue_;

  // Queue of decoded frames in presentation order.
  std::priority_queue<std::unique_ptr<Frame>,
                      std::vector<std::unique_ptr<Frame>>,
                      FrameOrder>
      reorder_queue_;

  // Queue of decoded frames in presentation order. Used by codecs which don't
  // require reordering (VP9 only at the moment).
  std::deque<std::unique_ptr<Frame>> output_queue_;

  std::unique_ptr<VP9ConfigChangeDetector> cc_detector_;
  std::unique_ptr<VP9SuperFrameBitstreamFilter> vp9_bsf_;

  // Size of assigned picture buffers.
  gfx::Size picture_size_;

  // Format of the assigned picture buffers.
  VideoPixelFormat picture_format_ = PIXEL_FORMAT_UNKNOWN;

  // Corresponding SharedImageFormat.
  viz::SharedImageFormat si_format_ = viz::MultiPlaneFormat::kNV12;

  // Frames that have not yet been decoded, keyed by bitstream ID; maintains
  // ownership of Frame objects while they flow through VideoToolbox.
  base::flat_map<int32_t, std::unique_ptr<Frame>> pending_frames_;

  // Set of assigned bitstream IDs, so that Destroy() can release them all.
  std::set<int32_t> assigned_bitstream_ids_;

  // All picture buffers assigned to us. Used to check if reused picture buffers
  // should be added back to the available list or released. (They are not
  // released immediately because we need the reuse event to free the binding.)
  std::set<int32_t> assigned_picture_ids_;

  // Texture IDs and image buffers of assigned pictures.
  base::flat_map<int32_t, std::unique_ptr<PictureInfo>> picture_info_map_;

  // Pictures ready to be rendered to.
  std::vector<int32_t> available_picture_ids_;

  //
  // Decoder thread state.
  //
  VTDecompressionOutputCallbackRecord callback_;
  base::ScopedCFTypeRef<CMFormatDescriptionRef> format_;
  base::ScopedCFTypeRef<VTDecompressionSessionRef> session_;
  H264Parser h264_parser_;

  // SPSs and PPSs seen in the bitstream.
  base::flat_map<int, std::vector<uint8_t>> seen_sps_;
  base::flat_map<int, std::vector<uint8_t>> seen_spsext_;
  base::flat_map<int, std::vector<uint8_t>> seen_pps_;

  // SPS and PPS most recently activated by an IDR.
  // TODO(sandersd): Enable configuring with multiple PPSs.
  std::vector<uint8_t> active_sps_;
  std::vector<uint8_t> active_spsext_;
  std::vector<uint8_t> active_pps_;

  // SPS and PPS the decoder is currently confgured with.
  std::vector<uint8_t> configured_sps_;
  std::vector<uint8_t> configured_spsext_;
  std::vector<uint8_t> configured_pps_;

  H264POC h264_poc_;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  H265Parser hevc_parser_;

  // VPSs seen in the bitstream.
  base::flat_map<int, std::vector<uint8_t>> seen_vps_;
  // VPS most recently activated by an IDR.
  std::vector<uint8_t> active_vps_;

  // VPSs the decoder is currently confgured with.
  base::flat_map<int, std::vector<uint8_t>> configured_vpss_;
  // SPSs the decoder is currently confgured with.
  base::flat_map<int, std::vector<uint8_t>> configured_spss_;
  // PPSs the decoder is currently confgured with.
  base::flat_map<int, std::vector<uint8_t>> configured_ppss_;

  H265POC hevc_poc_;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

  Config config_;
  VideoCodec codec_;

  // Visible rect the decoder is configured to use.
  gfx::Size configured_size_;

  bool waiting_for_idr_ = true;
  bool missing_idr_logged_ = false;

  // currently only HEVC is supported, VideoToolbox doesn't
  // support VP9 with alpha for now.
  bool has_alpha_ = false;

  // Used to accumulate the output picture count as a workaround to solve
  // the VT CRA/RASL bug
  uint64_t output_count_for_cra_rasl_workaround_ = 0;

  // Id number for this instance for memory dumps.
  int memory_dump_id_ = 0;

  //
  // Shared state (set up and torn down on GPU thread).
  //
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;

  // WeakPtr to |this| for tasks on the |decoder_task_runner_|. Invalidated
  // on the |decoder_task_runner_| during FlushTask(TASK_DESTROY).
  base::WeakPtr<VTVideoDecodeAccelerator> decoder_weak_this_;

  base::WeakPtr<VTVideoDecodeAccelerator> weak_this_;

  // Declared last to ensure that all weak pointers are invalidated before
  // other destructors run.
  base::WeakPtrFactory<VTVideoDecodeAccelerator> decoder_weak_this_factory_;
  base::WeakPtrFactory<VTVideoDecodeAccelerator> weak_this_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VT_VIDEO_DECODE_ACCELERATOR_MAC_H_
