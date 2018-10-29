// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vt_video_decode_accelerator_mac.h"

#include <CoreVideo/CoreVideo.h>
#include <OpenGL/CGLIOSurface.h>
#include <OpenGL/gl.h>
#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <memory>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/mac_logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "base/sys_info.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/version.h"
#include "media/base/limits.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_io_surface.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/scoped_binders.h"

#define NOTIFY_STATUS(name, status, session_failure) \
  do {                                               \
    OSSTATUS_DLOG(ERROR, status) << name;            \
    NotifyError(PLATFORM_FAILURE, session_failure);  \
  } while (0)

namespace media {

namespace {

// A sequence of ids for memory tracing.
base::AtomicSequenceNumber g_memory_dump_ids;

// A sequence of shared memory ids for CVPixelBufferRefs.
base::AtomicSequenceNumber g_cv_pixel_buffer_ids;

// Only H.264 with 4:2:0 chroma sampling is supported.
const VideoCodecProfile kSupportedProfiles[] = {
    H264PROFILE_BASELINE, H264PROFILE_EXTENDED, H264PROFILE_MAIN,
    H264PROFILE_HIGH,

    // TODO(hubbe): Re-enable this once software fallback is working.
    // http://crbug.com/605790
    // H264PROFILE_HIGH10PROFILE,

    // TODO(sandersd): Find and test media with these profiles before enabling.
    // H264PROFILE_SCALABLEBASELINE,
    // H264PROFILE_SCALABLEHIGH,
    // H264PROFILE_STEREOHIGH,
    // H264PROFILE_MULTIVIEWHIGH,
};

// Size to use for NALU length headers in AVC format (can be 1, 2, or 4).
const int kNALUHeaderLength = 4;

// We request 16 picture buffers from the client, each of which has a texture ID
// that we can bind decoded frames to. The resource requirements are low, as we
// don't need the textures to be backed by storage.
//
// The lower limit is |limits::kMaxVideoFrames + 1|, enough to have one
// composited frame plus |limits::kMaxVideoFrames| frames to satisfy preroll.
//
// However, there can be pathological behavior where VideoRendererImpl will
// continue to call Decode() as long as it is willing to queue more output
// frames, which is variable but starts at |limits::kMaxVideoFrames +
// GetMaxDecodeRequests()|. If we don't have enough picture buffers, it will
// continue to call Decode() until we stop calling NotifyEndOfBistreamBuffer(),
// which for VTVDA is when the reorder queue is full. In testing this results in
// ~20 extra frames held by VTVDA.
//
// Allocating more picture buffers than VideoRendererImpl is willing to queue
// counterintuitively reduces memory usage in this case.
const int kNumPictureBuffers = limits::kMaxVideoFrames * 4;

// Maximum number of frames to queue for reordering. (Also controls the maximum
// number of in-flight frames, since NotifyEndOfBitstreamBuffer() is called when
// frames are moved into the reorder queue.)
//
// Since the maximum possible |reorder_window| is 16 for H.264, 17 is the
// minimum safe (static) size of the reorder queue.
const int kMaxReorderQueueSize = 17;

// Build an |image_config| dictionary for VideoToolbox initialization.
base::ScopedCFTypeRef<CFMutableDictionaryRef> BuildImageConfig(
    CMVideoDimensions coded_dimensions) {
  base::ScopedCFTypeRef<CFMutableDictionaryRef> image_config;

  // Note that 4:2:0 textures cannot be used directly as RGBA in OpenGL, but are
  // lower power than 4:2:2 when composited directly by CoreAnimation.
  int32_t pixel_format = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange;
#define CFINT(i) CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i)
  base::ScopedCFTypeRef<CFNumberRef> cf_pixel_format(CFINT(pixel_format));
  base::ScopedCFTypeRef<CFNumberRef> cf_width(CFINT(coded_dimensions.width));
  base::ScopedCFTypeRef<CFNumberRef> cf_height(CFINT(coded_dimensions.height));
#undef CFINT
  if (!cf_pixel_format.get() || !cf_width.get() || !cf_height.get())
    return image_config;

  image_config.reset(CFDictionaryCreateMutable(
      kCFAllocatorDefault,
      3,  // capacity
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  if (!image_config.get())
    return image_config;

  CFDictionarySetValue(image_config, kCVPixelBufferPixelFormatTypeKey,
                       cf_pixel_format);
  CFDictionarySetValue(image_config, kCVPixelBufferWidthKey, cf_width);
  CFDictionarySetValue(image_config, kCVPixelBufferHeightKey, cf_height);

  return image_config;
}

// Create a VTDecompressionSession using the provided |pps| and |sps|. If
// |require_hardware| is true, the session must uses real hardware decoding
// (as opposed to software decoding inside of VideoToolbox) to be considered
// successful.
//
// TODO(sandersd): Merge with ConfigureDecoder(), as the code is very similar.
bool CreateVideoToolboxSession(const uint8_t* sps,
                               size_t sps_size,
                               const uint8_t* pps,
                               size_t pps_size,
                               bool require_hardware) {
  const uint8_t* data_ptrs[] = {sps, pps};
  const size_t data_sizes[] = {sps_size, pps_size};

  base::ScopedCFTypeRef<CMFormatDescriptionRef> format;
  OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault,
      2,                  // parameter_set_count
      data_ptrs,          // &parameter_set_pointers
      data_sizes,         // &parameter_set_sizes
      kNALUHeaderLength,  // nal_unit_header_length
      format.InitializeInto());
  if (status) {
    OSSTATUS_DLOG(WARNING, status)
        << "Failed to create CMVideoFormatDescription";
    return false;
  }

  base::ScopedCFTypeRef<CFMutableDictionaryRef> decoder_config(
      CFDictionaryCreateMutable(kCFAllocatorDefault,
                                1,  // capacity
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  if (!decoder_config.get())
    return false;

  if (require_hardware) {
    CFDictionarySetValue(
        decoder_config,
        // kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder
        CFSTR("RequireHardwareAcceleratedVideoDecoder"), kCFBooleanTrue);
  }

  CGRect visible_rect = CMVideoFormatDescriptionGetCleanAperture(format, true);
  CMVideoDimensions visible_dimensions = {visible_rect.size.width,
                                          visible_rect.size.height};
  base::ScopedCFTypeRef<CFMutableDictionaryRef> image_config(
      BuildImageConfig(visible_dimensions));
  if (!image_config.get())
    return false;

  VTDecompressionOutputCallbackRecord callback = {0};

  base::ScopedCFTypeRef<VTDecompressionSessionRef> session;
  status = VTDecompressionSessionCreate(
      kCFAllocatorDefault,
      format,          // video_format_description
      decoder_config,  // video_decoder_specification
      image_config,    // destination_image_buffer_attributes
      &callback,       // output_callback
      session.InitializeInto());
  if (status) {
    OSSTATUS_DLOG(WARNING, status) << "Failed to create VTDecompressionSession";
    return false;
  }

  return true;
}

// The purpose of this function is to preload the generic and hardware-specific
// libraries required by VideoToolbox before the GPU sandbox is enabled.
// VideoToolbox normally loads the hardware-specific libraries lazily, so we
// must actually create a decompression session. If creating a decompression
// session fails, hardware decoding will be disabled (Initialize() will always
// return false).
bool InitializeVideoToolboxInternal() {
  // Create a hardware decoding session.
  // SPS and PPS data are taken from a 480p sample (buck2.mp4).
  const uint8_t sps_normal[] = {0x67, 0x64, 0x00, 0x1e, 0xac, 0xd9, 0x80, 0xd4,
                                0x3d, 0xa1, 0x00, 0x00, 0x03, 0x00, 0x01, 0x00,
                                0x00, 0x03, 0x00, 0x30, 0x8f, 0x16, 0x2d, 0x9a};
  const uint8_t pps_normal[] = {0x68, 0xe9, 0x7b, 0xcb};
  if (!CreateVideoToolboxSession(sps_normal, arraysize(sps_normal), pps_normal,
                                 arraysize(pps_normal), true)) {
    DLOG(WARNING) << "Hardware decoding with VideoToolbox is not supported";
    return false;
  }

  // Create a software decoding session.
  // SPS and PPS data are taken from a 18p sample (small2.mp4).
  const uint8_t sps_small[] = {0x67, 0x64, 0x00, 0x0a, 0xac, 0xd9, 0x89, 0x7e,
                               0x22, 0x10, 0x00, 0x00, 0x3e, 0x90, 0x00, 0x0e,
                               0xa6, 0x08, 0xf1, 0x22, 0x59, 0xa0};
  const uint8_t pps_small[] = {0x68, 0xe9, 0x79, 0x72, 0xc0};
  if (!CreateVideoToolboxSession(sps_small, arraysize(sps_small), pps_small,
                                 arraysize(pps_small), false)) {
    DLOG(WARNING) << "Software decoding with VideoToolbox is not supported";
    return false;
  }

  return true;
}

// TODO(sandersd): Share this computation with the VAAPI decoder.
int32_t ComputeReorderWindow(const H264SPS* sps) {
  // When |pic_order_cnt_type| == 2, decode order always matches presentation
  // order.
  // TODO(sandersd): For |pic_order_cnt_type| == 1, analyze the delta cycle to
  // find the minimum required reorder window.
  if (sps->pic_order_cnt_type == 2)
    return 0;

  // TODO(sandersd): Compute MaxDpbFrames.
  int32_t max_dpb_frames = 16;

  // See AVC spec section E.2.1 definition of |max_num_reorder_frames|.
  if (sps->vui_parameters_present_flag && sps->bitstream_restriction_flag) {
    return std::min(sps->max_num_reorder_frames, max_dpb_frames);
  } else if (sps->constraint_set3_flag) {
    if (sps->profile_idc == 44 || sps->profile_idc == 86 ||
        sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 244) {
      return 0;
    }
  }
  return max_dpb_frames;
}

// Route decoded frame callbacks back into the VTVideoDecodeAccelerator.
void OutputThunk(void* decompression_output_refcon,
                 void* source_frame_refcon,
                 OSStatus status,
                 VTDecodeInfoFlags info_flags,
                 CVImageBufferRef image_buffer,
                 CMTime presentation_time_stamp,
                 CMTime presentation_duration) {
  VTVideoDecodeAccelerator* vda =
      reinterpret_cast<VTVideoDecodeAccelerator*>(decompression_output_refcon);
  vda->Output(source_frame_refcon, status, image_buffer);
}

// Read the value for the key in |key| to CFString and convert it to IdType.
// Use the list of pairs in |cfstr_id_pairs| to do the conversion (by doing a
// linear lookup).
template <typename IdType, typename StringIdPair>
bool GetImageBufferProperty(CVImageBufferRef image_buffer,
                            CFStringRef key,
                            const StringIdPair* cfstr_id_pairs,
                            size_t cfstr_id_pairs_size,
                            IdType* value_as_id) {
  CFStringRef value_as_string = reinterpret_cast<CFStringRef>(
      CVBufferGetAttachment(image_buffer, key, nullptr));
  if (!value_as_string)
    return false;

  for (size_t i = 0; i < cfstr_id_pairs_size; ++i) {
    if (!CFStringCompare(value_as_string, cfstr_id_pairs[i].cfstr, 0)) {
      *value_as_id = cfstr_id_pairs[i].id;
      return true;
    }
  }

  return false;
}

gfx::ColorSpace GetImageBufferColorSpace(CVImageBufferRef image_buffer) {
  // The named primaries. Default to BT709.
  gfx::ColorSpace::PrimaryID primary_id = gfx::ColorSpace::PrimaryID::BT709;
  struct {
    const CFStringRef cfstr;
    const gfx::ColorSpace::PrimaryID id;
  } primaries[] = {
      {
          kCVImageBufferColorPrimaries_ITU_R_709_2,
          gfx::ColorSpace::PrimaryID::BT709,
      },
      {
          kCVImageBufferColorPrimaries_EBU_3213,
          gfx::ColorSpace::PrimaryID::BT470BG,
      },
      {
          kCVImageBufferColorPrimaries_SMPTE_C,
          gfx::ColorSpace::PrimaryID::SMPTE240M,
      },
  };
  if (!GetImageBufferProperty(image_buffer, kCVImageBufferColorPrimariesKey,
                              primaries, arraysize(primaries), &primary_id)) {
    DLOG(ERROR) << "Filed to find CVImageBufferRef primaries.";
  }

  // The named transfer function.
  gfx::ColorSpace::TransferID transfer_id = gfx::ColorSpace::TransferID::BT709;
  SkColorSpaceTransferFn custom_tr_fn = {2.2f, 1, 0, 1, 0, 0, 0};
  struct {
    const CFStringRef cfstr;
    gfx::ColorSpace::TransferID id;
  } transfers[] = {
      {
          kCVImageBufferTransferFunction_ITU_R_709_2,
          gfx::ColorSpace::TransferID::BT709_APPLE,
      },
      {
          kCVImageBufferTransferFunction_SMPTE_240M_1995,
          gfx::ColorSpace::TransferID::SMPTE240M,
      },
      {
          kCVImageBufferTransferFunction_UseGamma,
          gfx::ColorSpace::TransferID::CUSTOM,
      },
  };
  if (!GetImageBufferProperty(image_buffer, kCVImageBufferTransferFunctionKey,
                              transfers, arraysize(transfers), &transfer_id)) {
    DLOG(ERROR) << "Filed to find CVImageBufferRef transfer.";
  }

  // Transfer functions can also be specified as a gamma value.
  if (transfer_id == gfx::ColorSpace::TransferID::CUSTOM) {
    // If we fail to find the custom transfer function parameters, fall back to
    // BT709.
    transfer_id = gfx::ColorSpace::TransferID::BT709;
    CFNumberRef gamma_number =
        reinterpret_cast<CFNumberRef>(CVBufferGetAttachment(
            image_buffer, kCVImageBufferGammaLevelKey, nullptr));
    if (gamma_number) {
      CGFloat gamma_float = 0;
      if (CFNumberGetValue(gamma_number, kCFNumberCGFloatType, &gamma_float)) {
        transfer_id = gfx::ColorSpace::TransferID::CUSTOM;
        custom_tr_fn.fG = gamma_float;
      } else {
        DLOG(ERROR) << "Filed to get CVImageBufferRef gamma level as float.";
      }
    } else {
      DLOG(ERROR) << "Filed to get CVImageBufferRef gamma level.";
    }
  }

  // Read the RGB to YUV matrix ID.
  gfx::ColorSpace::MatrixID matrix_id = gfx::ColorSpace::MatrixID::BT709;
  struct {
    const CFStringRef cfstr;
    gfx::ColorSpace::MatrixID id;
  } matrices[] = {{
                      kCVImageBufferYCbCrMatrix_ITU_R_709_2,
                      gfx::ColorSpace::MatrixID::BT709,
                  },
                  {
                      kCVImageBufferYCbCrMatrix_ITU_R_601_4,
                      gfx::ColorSpace::MatrixID::SMPTE170M,
                  },
                  {
                      kCVImageBufferYCbCrMatrix_SMPTE_240M_1995,
                      gfx::ColorSpace::MatrixID::SMPTE240M,
                  }};
  if (!GetImageBufferProperty(image_buffer, kCVImageBufferYCbCrMatrixKey,
                              matrices, arraysize(matrices), &matrix_id)) {
    DLOG(ERROR) << "Filed to find CVImageBufferRef YUV matrix.";
  }

  // It is specified to the decoder to use luma=[16,235] chroma=[16,240] via
  // the kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange.
  gfx::ColorSpace::RangeID range_id = gfx::ColorSpace::RangeID::LIMITED;

  if (transfer_id == gfx::ColorSpace::TransferID::CUSTOM)
    return gfx::ColorSpace(primary_id, custom_tr_fn, matrix_id, range_id);
  return gfx::ColorSpace(primary_id, transfer_id, matrix_id, range_id);
}

}  // namespace

bool InitializeVideoToolbox() {
  // InitializeVideoToolbox() is called only from the GPU process main thread:
  // once for sandbox warmup, and then once each time a VTVideoDecodeAccelerator
  // is initialized. This ensures that everything is loaded whether or not the
  // sandbox is enabled.
  static bool succeeded = InitializeVideoToolboxInternal();
  return succeeded;
}

VTVideoDecodeAccelerator::Task::Task(TaskType type) : type(type) {}

VTVideoDecodeAccelerator::Task::Task(const Task& other) = default;

VTVideoDecodeAccelerator::Task::~Task() {}

VTVideoDecodeAccelerator::Frame::Frame(int32_t bitstream_id)
    : bitstream_id(bitstream_id) {}

VTVideoDecodeAccelerator::Frame::~Frame() {}

VTVideoDecodeAccelerator::PictureInfo::PictureInfo(uint32_t client_texture_id,
                                                   uint32_t service_texture_id)
    : bitstream_id(0),
      client_texture_id(client_texture_id),
      service_texture_id(service_texture_id) {}

VTVideoDecodeAccelerator::PictureInfo::~PictureInfo() {}

bool VTVideoDecodeAccelerator::FrameOrder::operator()(
    const linked_ptr<Frame>& lhs,
    const linked_ptr<Frame>& rhs) const {
  // TODO(sandersd): When it is provided, use the bitstream timestamp.
  if (lhs->pic_order_cnt != rhs->pic_order_cnt)
    return lhs->pic_order_cnt > rhs->pic_order_cnt;

  // If |pic_order_cnt| is the same, fall back on using the bitstream order.
  return lhs->bitstream_id > rhs->bitstream_id;
}

VTVideoDecodeAccelerator::VTVideoDecodeAccelerator(
    const BindGLImageCallback& bind_image_cb,
    MediaLog* media_log)
    : bind_image_cb_(bind_image_cb),
      media_log_(media_log),
      gpu_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      decoder_thread_("VTDecoderThread"),
      weak_this_factory_(this) {
  DCHECK(bind_image_cb_);

  callback_.decompressionOutputCallback = OutputThunk;
  callback_.decompressionOutputRefCon = this;
  weak_this_ = weak_this_factory_.GetWeakPtr();

  memory_dump_id_ = g_memory_dump_ids.GetNext();
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "VTVideoDecodeAccelerator", gpu_task_runner_);
}

VTVideoDecodeAccelerator::~VTVideoDecodeAccelerator() {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool VTVideoDecodeAccelerator::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  // Dump output pictures (decoded frames for which PictureReady() has been
  // called already).
  for (const auto& it : picture_info_map_) {
    PictureInfo* picture_info = it.second.get();
    if (picture_info->gl_image) {
      std::string dump_name =
          base::StringPrintf("media/vt_video_decode_accelerator_%d/picture_%d",
                             memory_dump_id_, picture_info->bitstream_id);
      picture_info->gl_image->OnMemoryDump(pmd, 0, dump_name);
    }
  }

  // Dump the output queue (decoded frames for which
  // NotifyEndOfBitstreamBuffer() has not been called yet).
  {
    uint64_t total_count = 0;
    uint64_t total_size = 0;
    for (const auto& it : base::GetUnderlyingContainer(task_queue_)) {
      if (it.frame.get() && it.frame->image) {
        IOSurfaceRef io_surface = CVPixelBufferGetIOSurface(it.frame->image);
        if (io_surface) {
          ++total_count;
          total_size += IOSurfaceGetAllocSize(io_surface);
        }
      }
    }
    base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
        base::StringPrintf("media/vt_video_decode_accelerator_%d/output_queue",
                           memory_dump_id_));
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                    base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                    total_count);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    total_size);
  }

  // Dump the reorder queue (decoded frames for which
  // NotifyEndOfBitstreamBuffer() has been called already).
  {
    uint64_t total_count = 0;
    uint64_t total_size = 0;
    for (const auto& it : base::GetUnderlyingContainer(reorder_queue_)) {
      if (it.get() && it->image) {
        IOSurfaceRef io_surface = CVPixelBufferGetIOSurface(it->image);
        if (io_surface) {
          ++total_count;
          total_size += IOSurfaceGetAllocSize(io_surface);
        }
      }
    }
    base::trace_event::MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(
        base::StringPrintf("media/vt_video_decode_accelerator_%d/reorder_queue",
                           memory_dump_id_));
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameObjectCount,
                    base::trace_event::MemoryAllocatorDump::kUnitsObjects,
                    total_count);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    total_size);
  }

  return true;
}

bool VTVideoDecodeAccelerator::Initialize(const Config& config,
                                          Client* client) {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  // All of these checks should be handled by the caller inspecting
  // SupportedProfiles(). PPAPI does not do that, however.
  if (config.output_mode != Config::OutputMode::ALLOCATE) {
    DVLOG(2) << "Output mode must be ALLOCATE";
    return false;
  }

  if (config.is_encrypted()) {
    DVLOG(2) << "Encrypted streams are not supported";
    return false;
  }

  if (std::find(std::begin(kSupportedProfiles), std::end(kSupportedProfiles),
                config.profile) == std::end(kSupportedProfiles)) {
    DVLOG(2) << "Unsupported profile";
    return false;
  }

  if (!InitializeVideoToolbox()) {
    DVLOG(2) << "VideoToolbox is unavailable";
    return false;
  }

  client_ = client;

  // Spawn a thread to handle parsing and calling VideoToolbox.
  if (!decoder_thread_.Start()) {
    DLOG(ERROR) << "Failed to start decoder thread";
    return false;
  }

  // Count the session as successfully initialized.
  UMA_HISTOGRAM_ENUMERATION("Media.VTVDA.SessionFailureReason",
                            SFT_SUCCESSFULLY_INITIALIZED, SFT_MAX + 1);
  return true;
}

bool VTVideoDecodeAccelerator::FinishDelayedFrames() {
  DVLOG(3) << __func__;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  if (session_) {
    OSStatus status = VTDecompressionSessionWaitForAsynchronousFrames(session_);
    if (status) {
      NOTIFY_STATUS("VTDecompressionSessionWaitForAsynchronousFrames()", status,
                    SFT_PLATFORM_ERROR);
      return false;
    }
  }
  return true;
}

bool VTVideoDecodeAccelerator::ConfigureDecoder() {
  DVLOG(2) << __func__;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!active_sps_.empty());
  DCHECK(!active_pps_.empty());

  // Build the configuration records.
  std::vector<const uint8_t*> nalu_data_ptrs;
  std::vector<size_t> nalu_data_sizes;
  nalu_data_ptrs.reserve(3);
  nalu_data_sizes.reserve(3);
  nalu_data_ptrs.push_back(&active_sps_.front());
  nalu_data_sizes.push_back(active_sps_.size());
  if (!last_spsext_.empty()) {
    nalu_data_ptrs.push_back(&active_spsext_.front());
    nalu_data_sizes.push_back(active_spsext_.size());
  }
  nalu_data_ptrs.push_back(&active_pps_.front());
  nalu_data_sizes.push_back(active_pps_.size());

  // Construct a new format description from the parameter sets.
  format_.reset();
  OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault,
      nalu_data_ptrs.size(),     // parameter_set_count
      &nalu_data_ptrs.front(),   // &parameter_set_pointers
      &nalu_data_sizes.front(),  // &parameter_set_sizes
      kNALUHeaderLength,         // nal_unit_header_length
      format_.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMVideoFormatDescriptionCreateFromH264ParameterSets()",
                  status, SFT_PLATFORM_ERROR);
    return false;
  }

  // Prepare VideoToolbox configuration dictionaries.
  base::ScopedCFTypeRef<CFMutableDictionaryRef> decoder_config(
      CFDictionaryCreateMutable(kCFAllocatorDefault,
                                1,  // capacity
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  if (!decoder_config.get()) {
    DLOG(ERROR) << "Failed to create CFMutableDictionary";
    NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
    return false;
  }

  CFDictionarySetValue(
      decoder_config,
      // kVTVideoDecoderSpecification_EnableHardwareAcceleratedVideoDecoder
      CFSTR("EnableHardwareAcceleratedVideoDecoder"), kCFBooleanTrue);

  // VideoToolbox scales the visible rect to the output size, so we set the
  // output size for a 1:1 ratio. (Note though that VideoToolbox does not handle
  // top or left crops correctly.) We expect the visible rect to be integral.
  CGRect visible_rect = CMVideoFormatDescriptionGetCleanAperture(format_, true);
  CMVideoDimensions visible_dimensions = {visible_rect.size.width,
                                          visible_rect.size.height};
  base::ScopedCFTypeRef<CFMutableDictionaryRef> image_config(
      BuildImageConfig(visible_dimensions));
  if (!image_config.get()) {
    DLOG(ERROR) << "Failed to create decoder image configuration";
    NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
    return false;
  }

  // Ensure that the old decoder emits all frames before the new decoder can
  // emit any.
  if (!FinishDelayedFrames())
    return false;

  session_.reset();
  status = VTDecompressionSessionCreate(
      kCFAllocatorDefault,
      format_,         // video_format_description
      decoder_config,  // video_decoder_specification
      image_config,    // destination_image_buffer_attributes
      &callback_,      // output_callback
      session_.InitializeInto());
  if (status) {
    NOTIFY_STATUS("VTDecompressionSessionCreate()", status,
                  SFT_UNSUPPORTED_STREAM_PARAMETERS);
    return false;
  }

  // Report whether hardware decode is being used.
  bool using_hardware = false;
  base::ScopedCFTypeRef<CFBooleanRef> cf_using_hardware;
  if (VTSessionCopyProperty(
          session_,
          // kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder
          CFSTR("UsingHardwareAcceleratedVideoDecoder"), kCFAllocatorDefault,
          cf_using_hardware.InitializeInto()) == 0) {
    using_hardware = CFBooleanGetValue(cf_using_hardware);
  }
  UMA_HISTOGRAM_BOOLEAN("Media.VTVDA.HardwareAccelerated", using_hardware);

  // Record that the configuration change is complete.
  configured_sps_ = active_sps_;
  configured_spsext_ = active_spsext_;
  configured_pps_ = active_pps_;
  configured_size_.SetSize(visible_rect.size.width, visible_rect.size.height);

  return true;
}

void VTVideoDecodeAccelerator::DecodeTask(scoped_refptr<DecoderBuffer> buffer,
                                          Frame* frame) {
  DVLOG(2) << __func__ << "(" << frame->bitstream_id << ")";
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  // NALUs are stored with Annex B format in the bitstream buffer (start codes),
  // but VideoToolbox expects AVC format (length headers), so we must rewrite
  // the data.
  //
  // Locate relevant NALUs and compute the size of the rewritten data. Also
  // record parameter sets for VideoToolbox initialization.
  size_t data_size = 0;
  std::vector<H264NALU> nalus;
  parser_.SetStream(buffer->data(), buffer->data_size());
  H264NALU nalu;
  while (true) {
    H264Parser::Result result = parser_.AdvanceToNextNALU(&nalu);
    if (result == H264Parser::kEOStream)
      break;
    if (result == H264Parser::kUnsupportedStream) {
      WriteToMediaLog(MediaLog::MEDIALOG_ERROR, "Unsupported H.264 stream");
      NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
      return;
    }
    if (result != H264Parser::kOk) {
      WriteToMediaLog(MediaLog::MEDIALOG_ERROR, "Failed to parse H.264 stream");
      NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
      return;
    }
    switch (nalu.nal_unit_type) {
      case H264NALU::kSPS:
        result = parser_.ParseSPS(&last_sps_id_);
        if (result == H264Parser::kUnsupportedStream) {
          WriteToMediaLog(MediaLog::MEDIALOG_ERROR, "Unsupported SPS");
          NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
          return;
        }
        if (result != H264Parser::kOk) {
          WriteToMediaLog(MediaLog::MEDIALOG_ERROR, "Could not parse SPS");
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }
        last_sps_.assign(nalu.data, nalu.data + nalu.size);
        last_spsext_.clear();
        break;

      case H264NALU::kSPSExt:
        last_spsext_.assign(nalu.data, nalu.data + nalu.size);
        break;

      case H264NALU::kPPS:
        result = parser_.ParsePPS(&last_pps_id_);
        if (result == H264Parser::kUnsupportedStream) {
          WriteToMediaLog(MediaLog::MEDIALOG_ERROR, "Unsupported PPS");
          NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
          return;
        }
        if (result != H264Parser::kOk) {
          WriteToMediaLog(MediaLog::MEDIALOG_ERROR, "Could not parse PPS");
          NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
          return;
        }
        last_pps_.assign(nalu.data, nalu.data + nalu.size);
        break;

      case H264NALU::kSliceDataA:
      case H264NALU::kSliceDataB:
      case H264NALU::kSliceDataC:
      case H264NALU::kNonIDRSlice:
      case H264NALU::kIDRSlice:
        // Only the first slice is examined. Other slices are at least one of:
        // the same frame, not decoded, invalid.
        if (!frame->has_slice) {
          // Parse slice header.
          H264SliceHeader slice_hdr;
          result = parser_.ParseSliceHeader(nalu, &slice_hdr);
          if (result == H264Parser::kUnsupportedStream) {
            WriteToMediaLog(MediaLog::MEDIALOG_ERROR,
                            "Unsupported slice header");
            NotifyError(PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM);
            return;
          }
          if (result != H264Parser::kOk) {
            WriteToMediaLog(MediaLog::MEDIALOG_ERROR,
                            "Could not parse slice header");
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          // Lookup SPS and PPS.
          DCHECK_EQ(slice_hdr.pic_parameter_set_id, last_pps_id_);
          const H264PPS* pps = parser_.GetPPS(slice_hdr.pic_parameter_set_id);
          if (!pps) {
            WriteToMediaLog(MediaLog::MEDIALOG_ERROR,
                            "Missing PPS referenced by slice");
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          DCHECK_EQ(pps->seq_parameter_set_id, last_sps_id_);
          const H264SPS* sps = parser_.GetSPS(pps->seq_parameter_set_id);
          if (!sps) {
            WriteToMediaLog(MediaLog::MEDIALOG_ERROR,
                            "Missing SPS referenced by PPS");
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          // Record the configuration.
          // TODO(sandersd): Ideally this would be skipped if we know there
          // have not been any parameter sets since the last frame.
          active_sps_ = last_sps_;
          active_spsext_ = last_spsext_;
          active_pps_ = last_pps_;

          // Compute and store frame properties. |image_size| gets filled in
          // later, since it comes from the decoder configuration.
          base::Optional<int32_t> pic_order_cnt =
              poc_.ComputePicOrderCnt(sps, slice_hdr);
          if (!pic_order_cnt.has_value()) {
            WriteToMediaLog(MediaLog::MEDIALOG_ERROR, "Unable to compute POC");
            NotifyError(UNREADABLE_INPUT, SFT_INVALID_STREAM);
            return;
          }

          frame->has_slice = true;
          frame->is_idr = nalu.nal_unit_type == media::H264NALU::kIDRSlice;
          frame->has_mmco5 = poc_.IsPendingMMCO5();
          frame->pic_order_cnt = *pic_order_cnt;
          frame->reorder_window = ComputeReorderWindow(sps);
        }
        FALLTHROUGH;

      default:
        nalus.push_back(nalu);
        data_size += kNALUHeaderLength + nalu.size;
        break;
    }
  }

  if (frame->is_idr)
    waiting_for_idr_ = false;

  // If no IDR has been seen yet, skip decoding. Note that Flash sends
  // configuration changes as a bitstream with only SPS/PPS; we don't print
  // error messages for those.
  if (frame->has_slice && waiting_for_idr_) {
    if (!missing_idr_logged_) {
      WriteToMediaLog(MediaLog::MEDIALOG_ERROR,
                      ("Illegal attempt to decode without IDR. "
                       "Discarding decode requests until the next IDR."));
      missing_idr_logged_ = true;
    }
    frame->has_slice = false;
  }

  // If there is nothing to decode, drop the request by returning a frame with
  // no image.
  if (!frame->has_slice) {
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VTVideoDecodeAccelerator::DecodeDone, weak_this_, frame));
    return;
  }

  // Apply any configuration change, but only at an IDR. If there is no IDR, we
  // just hope for the best from the decoder.
  if (frame->is_idr &&
      (configured_sps_ != active_sps_ || configured_spsext_ != active_spsext_ ||
       configured_pps_ != active_pps_)) {
    if (active_sps_.empty()) {
      WriteToMediaLog(MediaLog::MEDIALOG_ERROR,
                      "Invalid configuration (no SPS)");
      NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
      return;
    }
    if (active_pps_.empty()) {
      WriteToMediaLog(MediaLog::MEDIALOG_ERROR,
                      "Invalid configuration (no PPS)");
      NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
      return;
    }

    // ConfigureDecoder() calls NotifyError() on failure.
    if (!ConfigureDecoder())
      return;
  }

  // If the session is not configured by this point, fail.
  if (!session_) {
    WriteToMediaLog(MediaLog::MEDIALOG_ERROR,
                    "Cannot decode without configuration");
    NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
    return;
  }

  // Now that the configuration is up to date, copy it into the frame.
  frame->image_size = configured_size_;

  // Create a memory-backed CMBlockBuffer for the translated data.
  // TODO(sandersd): Pool of memory blocks.
  base::ScopedCFTypeRef<CMBlockBufferRef> data;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      kCFAllocatorDefault,
      nullptr,              // &memory_block
      data_size,            // block_length
      kCFAllocatorDefault,  // block_allocator
      nullptr,              // &custom_block_source
      0,                    // offset_to_data
      data_size,            // data_length
      0,                    // flags
      data.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMBlockBufferCreateWithMemoryBlock()", status,
                  SFT_PLATFORM_ERROR);
    return;
  }

  // Make sure that the memory is actually allocated.
  // CMBlockBufferReplaceDataBytes() is documented to do this, but prints a
  // message each time starting in Mac OS X 10.10.
  status = CMBlockBufferAssureBlockMemory(data);
  if (status) {
    NOTIFY_STATUS("CMBlockBufferAssureBlockMemory()", status,
                  SFT_PLATFORM_ERROR);
    return;
  }

  // Copy NALU data into the CMBlockBuffer, inserting length headers.
  size_t offset = 0;
  for (size_t i = 0; i < nalus.size(); i++) {
    H264NALU& nalu = nalus[i];
    uint32_t header = base::HostToNet32(static_cast<uint32_t>(nalu.size));
    status =
        CMBlockBufferReplaceDataBytes(&header, data, offset, kNALUHeaderLength);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status,
                    SFT_PLATFORM_ERROR);
      return;
    }
    offset += kNALUHeaderLength;
    status = CMBlockBufferReplaceDataBytes(nalu.data, data, offset, nalu.size);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status,
                    SFT_PLATFORM_ERROR);
      return;
    }
    offset += nalu.size;
  }

  // Package the data in a CMSampleBuffer.
  base::ScopedCFTypeRef<CMSampleBufferRef> sample;
  status = CMSampleBufferCreate(kCFAllocatorDefault,
                                data,        // data_buffer
                                true,        // data_ready
                                nullptr,     // make_data_ready_callback
                                nullptr,     // make_data_ready_refcon
                                format_,     // format_description
                                1,           // num_samples
                                0,           // num_sample_timing_entries
                                nullptr,     // &sample_timing_array
                                1,           // num_sample_size_entries
                                &data_size,  // &sample_size_array
                                sample.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMSampleBufferCreate()", status, SFT_PLATFORM_ERROR);
    return;
  }

  // Send the frame for decoding.
  // Asynchronous Decompression allows for parallel submission of frames
  // (without it, DecodeFrame() does not return until the frame has been
  // decoded). We don't enable Temporal Processing because we are not passing
  // timestamps anyway.
  VTDecodeFrameFlags decode_flags =
      kVTDecodeFrame_EnableAsynchronousDecompression;
  status = VTDecompressionSessionDecodeFrame(
      session_,
      sample,                          // sample_buffer
      decode_flags,                    // decode_flags
      reinterpret_cast<void*>(frame),  // source_frame_refcon
      nullptr);                        // &info_flags_out
  if (status) {
    NOTIFY_STATUS("VTDecompressionSessionDecodeFrame()", status,
                  SFT_DECODE_ERROR);
    return;
  }
}

// This method may be called on any VideoToolbox thread.
void VTVideoDecodeAccelerator::Output(void* source_frame_refcon,
                                      OSStatus status,
                                      CVImageBufferRef image_buffer) {
  if (status) {
    NOTIFY_STATUS("Decoding", status, SFT_DECODE_ERROR);
    return;
  }

  // The type of |image_buffer| is CVImageBuffer, but we only handle
  // CVPixelBuffers. This should be guaranteed as we set
  // kCVPixelBufferOpenGLCompatibilityKey in |image_config|.
  //
  // Sometimes, for unknown reasons (http://crbug.com/453050), |image_buffer| is
  // NULL, which causes CFGetTypeID() to crash. While the rest of the code would
  // smoothly handle NULL as a dropped frame, we choose to fail permanantly here
  // until the issue is better understood.
  if (!image_buffer || CFGetTypeID(image_buffer) != CVPixelBufferGetTypeID()) {
    DLOG(ERROR) << "Decoded frame is not a CVPixelBuffer";
    NotifyError(PLATFORM_FAILURE, SFT_DECODE_ERROR);
    return;
  }

  Frame* frame = reinterpret_cast<Frame*>(source_frame_refcon);
  frame->image.reset(image_buffer, base::scoped_policy::RETAIN);
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VTVideoDecodeAccelerator::DecodeDone, weak_this_, frame));
}

void VTVideoDecodeAccelerator::DecodeDone(Frame* frame) {
  DVLOG(3) << __func__ << "(" << frame->bitstream_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  // pending_frames_.erase() will delete |frame|.
  int32_t bitstream_id = frame->bitstream_id;
  DCHECK_EQ(1u, pending_frames_.count(bitstream_id));

  if (state_ == STATE_ERROR || state_ == STATE_DESTROYING) {
    // Destroy() handles NotifyEndOfBitstreamBuffer().
    pending_frames_.erase(bitstream_id);
    return;
  }

  DCHECK_EQ(state_, STATE_DECODING);
  if (!frame->image.get()) {
    pending_frames_.erase(bitstream_id);
    assigned_bitstream_ids_.erase(bitstream_id);
    client_->NotifyEndOfBitstreamBuffer(bitstream_id);
    return;
  }

  Task task(TASK_FRAME);
  task.frame = pending_frames_[bitstream_id];
  pending_frames_.erase(bitstream_id);
  task_queue_.push(task);
  ProcessWorkQueues();
}

void VTVideoDecodeAccelerator::FlushTask(TaskType type) {
  DVLOG(3) << __func__;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  FinishDelayedFrames();

  if (type == TASK_DESTROY && session_) {
    // Destroy the decoding session before returning from the decoder thread.
    VTDecompressionSessionInvalidate(session_);
    session_.reset();
  }

  // Queue a task even if flushing fails, so that destruction always completes.
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VTVideoDecodeAccelerator::FlushDone, weak_this_, type));
}

void VTVideoDecodeAccelerator::FlushDone(TaskType type) {
  DVLOG(3) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  task_queue_.push(Task(type));
  ProcessWorkQueues();
}

void VTVideoDecodeAccelerator::Decode(const BitstreamBuffer& bitstream) {
  Decode(bitstream.ToDecoderBuffer(), bitstream.id());
}

void VTVideoDecodeAccelerator::Decode(scoped_refptr<DecoderBuffer> buffer,
                                      int32_t bitstream_id) {
  DVLOG(2) << __func__ << "(" << bitstream_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  if (bitstream_id < 0) {
    DLOG(ERROR) << "Invalid bitstream, id: " << bitstream_id;
    NotifyError(INVALID_ARGUMENT, SFT_INVALID_STREAM);
    return;
  }

  if (!buffer) {
    client_->NotifyEndOfBitstreamBuffer(bitstream_id);
    return;
  }

  DCHECK_EQ(0u, assigned_bitstream_ids_.count(bitstream_id));
  assigned_bitstream_ids_.insert(bitstream_id);

  Frame* frame = new Frame(bitstream_id);
  pending_frames_[bitstream_id] = make_linked_ptr(frame);
  decoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VTVideoDecodeAccelerator::DecodeTask,
                     base::Unretained(this), std::move(buffer), frame));
}

void VTVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& pictures) {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  for (const PictureBuffer& picture : pictures) {
    DVLOG(3) << "AssignPictureBuffer(" << picture.id() << ")";
    DCHECK(!picture_info_map_.count(picture.id()));
    assigned_picture_ids_.insert(picture.id());
    available_picture_ids_.push_back(picture.id());
    DCHECK_LE(1u, picture.client_texture_ids().size());
    DCHECK_LE(1u, picture.service_texture_ids().size());
    picture_info_map_.insert(std::make_pair(
        picture.id(),
        std::make_unique<PictureInfo>(picture.client_texture_ids()[0],
                                      picture.service_texture_ids()[0])));
  }

  // Pictures are not marked as uncleared until after this method returns, and
  // they will be broken if they are used before that happens. So, schedule
  // future work after that happens.
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VTVideoDecodeAccelerator::ProcessWorkQueues, weak_this_));
}

void VTVideoDecodeAccelerator::ReusePictureBuffer(int32_t picture_id) {
  DVLOG(2) << __func__ << "(" << picture_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  // It's possible there was a ReusePictureBuffer() request in flight when we
  // called DismissPictureBuffer(), in which case we won't find it. In that case
  // we should just drop the ReusePictureBuffer() request.
  auto it = picture_info_map_.find(picture_id);
  if (it == picture_info_map_.end())
    return;

  // Drop references to allow the underlying buffer to be released.
  PictureInfo* picture_info = it->second.get();
  bind_image_cb_.Run(picture_info->client_texture_id, GL_TEXTURE_RECTANGLE_ARB,
                     nullptr, false);
  picture_info->gl_image = nullptr;
  picture_info->bitstream_id = 0;

  // Mark the picture as available and try to complete pending output work.
  DCHECK(assigned_picture_ids_.count(picture_id));
  available_picture_ids_.push_back(picture_id);
  ProcessWorkQueues();
}

void VTVideoDecodeAccelerator::ProcessWorkQueues() {
  DVLOG(3) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  switch (state_) {
    case STATE_DECODING:
      // TODO(sandersd): Batch where possible.
      while (state_ == STATE_DECODING) {
        if (!ProcessReorderQueue() && !ProcessTaskQueue())
          break;
      }
      return;

    case STATE_ERROR:
      // Do nothing until Destroy() is called.
      return;

    case STATE_DESTROYING:
      // Drop tasks until we are ready to destruct.
      while (!task_queue_.empty()) {
        if (task_queue_.front().type == TASK_DESTROY) {
          delete this;
          return;
        }
        task_queue_.pop();
      }
      return;
  }
}

bool VTVideoDecodeAccelerator::ProcessTaskQueue() {
  DVLOG(3) << __func__ << " size=" << task_queue_.size();
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_DECODING);

  if (task_queue_.empty())
    return false;

  const Task& task = task_queue_.front();
  switch (task.type) {
    case TASK_FRAME: {
      bool reorder_queue_has_space =
          reorder_queue_.size() < kMaxReorderQueueSize;
      bool reorder_queue_flush_needed =
          task.frame->is_idr || task.frame->has_mmco5;
      bool reorder_queue_flush_done = reorder_queue_.empty();
      if (reorder_queue_has_space &&
          (!reorder_queue_flush_needed || reorder_queue_flush_done)) {
        DVLOG(2) << "Decode(" << task.frame->bitstream_id << ") complete";
        assigned_bitstream_ids_.erase(task.frame->bitstream_id);
        client_->NotifyEndOfBitstreamBuffer(task.frame->bitstream_id);
        reorder_queue_.push(task.frame);
        task_queue_.pop();
        return true;
      }
      return false;
    }

    case TASK_FLUSH:
      DCHECK_EQ(task.type, pending_flush_tasks_.front());
      if (reorder_queue_.size() == 0) {
        DVLOG(1) << "Flush complete";
        pending_flush_tasks_.pop();
        client_->NotifyFlushDone();
        task_queue_.pop();
        return true;
      }
      return false;

    case TASK_RESET:
      DCHECK_EQ(task.type, pending_flush_tasks_.front());
      if (reorder_queue_.size() == 0) {
        DVLOG(1) << "Reset complete";
        waiting_for_idr_ = true;
        pending_flush_tasks_.pop();
        client_->NotifyResetDone();
        task_queue_.pop();
        return true;
      }
      return false;

    case TASK_DESTROY:
      NOTREACHED() << "Can't destroy while in STATE_DECODING";
      NotifyError(ILLEGAL_STATE, SFT_PLATFORM_ERROR);
      return false;
  }
}

bool VTVideoDecodeAccelerator::ProcessReorderQueue() {
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_DECODING);

  if (reorder_queue_.empty())
    return false;

  // If the next task is a flush (because there is a pending flush or becuase
  // the next frame is an IDR), then we don't need a full reorder buffer to send
  // the next frame.
  bool flushing =
      !task_queue_.empty() && (task_queue_.front().type != TASK_FRAME ||
                               task_queue_.front().frame->is_idr ||
                               task_queue_.front().frame->has_mmco5);

  size_t reorder_window = std::max(0, reorder_queue_.top()->reorder_window);
  DVLOG(3) << __func__ << " size=" << reorder_queue_.size()
           << " window=" << reorder_window << " flushing=" << flushing;
  if (flushing || reorder_queue_.size() > reorder_window) {
    if (ProcessFrame(*reorder_queue_.top())) {
      reorder_queue_.pop();
      return true;
    }
  }

  return false;
}

bool VTVideoDecodeAccelerator::ProcessFrame(const Frame& frame) {
  DVLOG(3) << __func__ << "(" << frame.bitstream_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_DECODING);

  // If the next pending flush is for a reset, then the frame will be dropped.
  bool resetting = !pending_flush_tasks_.empty() &&
                   pending_flush_tasks_.front() == TASK_RESET;

  if (!resetting) {
    DCHECK(frame.image.get());
    // If the |image_size| has changed, request new picture buffers and then
    // wait for them.
    //
    // TODO(sandersd): When used by GpuVideoDecoder, we don't need to bother
    // with this. We can tell that is the case when we also have a timestamp.
    if (picture_size_ != frame.image_size) {
      // Dismiss current pictures.
      for (int32_t picture_id : assigned_picture_ids_) {
        DVLOG(3) << "DismissPictureBuffer(" << picture_id << ")";
        client_->DismissPictureBuffer(picture_id);
      }
      assigned_picture_ids_.clear();
      picture_info_map_.clear();
      available_picture_ids_.clear();

      // Request new pictures.
      picture_size_ = frame.image_size;
      DVLOG(3) << "ProvidePictureBuffers(" << kNumPictureBuffers
               << frame.image_size.ToString() << ")";
      client_->ProvidePictureBuffers(kNumPictureBuffers, PIXEL_FORMAT_UNKNOWN,
                                     1, frame.image_size,
                                     GL_TEXTURE_RECTANGLE_ARB);
      return false;
    }
    if (!SendFrame(frame))
      return false;
  }

  return true;
}

bool VTVideoDecodeAccelerator::SendFrame(const Frame& frame) {
  DVLOG(2) << __func__ << "(" << frame.bitstream_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(state_, STATE_DECODING);
  DCHECK(frame.image.get());

  if (available_picture_ids_.empty())
    return false;

  int32_t picture_id = available_picture_ids_.back();
  auto it = picture_info_map_.find(picture_id);
  DCHECK(it != picture_info_map_.end());
  PictureInfo* picture_info = it->second.get();
  DCHECK(!picture_info->gl_image);

  scoped_refptr<gl::GLImageIOSurface> gl_image(
      gl::GLImageIOSurface::Create(frame.image_size, GL_BGRA_EXT));
  if (!gl_image->InitializeWithCVPixelBuffer(
          frame.image.get(),
          gfx::GenericSharedMemoryId(g_cv_pixel_buffer_ids.GetNext()),
          gfx::BufferFormat::YUV_420_BIPLANAR)) {
    NOTIFY_STATUS("Failed to initialize GLImageIOSurface", PLATFORM_FAILURE,
                  SFT_PLATFORM_ERROR);
  }
  gfx::ColorSpace color_space = GetImageBufferColorSpace(frame.image);
  gl_image->SetColorSpaceForYUVToRGBConversion(color_space);

  if (!bind_image_cb_.Run(picture_info->client_texture_id,
                          GL_TEXTURE_RECTANGLE_ARB, gl_image, false)) {
    DLOG(ERROR) << "Failed to bind image";
    NotifyError(PLATFORM_FAILURE, SFT_PLATFORM_ERROR);
    return false;
  }
  picture_info->gl_image = gl_image;
  picture_info->bitstream_id = frame.bitstream_id;
  available_picture_ids_.pop_back();

  DVLOG(3) << "PictureReady(picture_id=" << picture_id << ", "
           << "bitstream_id=" << frame.bitstream_id << ")";
  client_->PictureReady(Picture(picture_id, frame.bitstream_id,
                                gfx::Rect(frame.image_size), color_space,
                                true));
  return true;
}

void VTVideoDecodeAccelerator::NotifyError(
    Error vda_error_type,
    VTVDASessionFailureType session_failure_type) {
  DCHECK_LT(session_failure_type, SFT_MAX + 1);
  if (!gpu_task_runner_->BelongsToCurrentThread()) {
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&VTVideoDecodeAccelerator::NotifyError, weak_this_,
                   vda_error_type, session_failure_type));
  } else if (state_ == STATE_DECODING) {
    state_ = STATE_ERROR;
    UMA_HISTOGRAM_ENUMERATION("Media.VTVDA.SessionFailureReason",
                              session_failure_type, SFT_MAX + 1);
    client_->NotifyError(vda_error_type);
  }
}

void VTVideoDecodeAccelerator::WriteToMediaLog(MediaLog::MediaLogLevel level,
                                               const std::string& message) {
  if (!gpu_task_runner_->BelongsToCurrentThread()) {
    gpu_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VTVideoDecodeAccelerator::WriteToMediaLog,
                                  weak_this_, level, message));
    return;
  }

  DVLOG(1) << __func__ << "(" << level << ") " << message;

  if (media_log_)
    media_log_->AddLogEvent(level, message);
}

void VTVideoDecodeAccelerator::QueueFlush(TaskType type) {
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  pending_flush_tasks_.push(type);
  decoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VTVideoDecodeAccelerator::FlushTask,
                                base::Unretained(this), type));

  // If this is a new flush request, see if we can make progress.
  if (pending_flush_tasks_.size() == 1)
    ProcessWorkQueues();
}

void VTVideoDecodeAccelerator::Flush() {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  QueueFlush(TASK_FLUSH);
}

void VTVideoDecodeAccelerator::Reset() {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  QueueFlush(TASK_RESET);
}

void VTVideoDecodeAccelerator::Destroy() {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  // In a forceful shutdown, the decoder thread may be dead already.
  if (!decoder_thread_.IsRunning()) {
    delete this;
    return;
  }

  // For a graceful shutdown, return assigned buffers and flush before
  // destructing |this|.
  for (int32_t bitstream_id : assigned_bitstream_ids_)
    client_->NotifyEndOfBitstreamBuffer(bitstream_id);
  assigned_bitstream_ids_.clear();
  state_ = STATE_DESTROYING;
  QueueFlush(TASK_DESTROY);

  // Prevent calling into a deleted MediaLog.
  media_log_ = nullptr;
}

bool VTVideoDecodeAccelerator::TryToSetupDecodeOnSeparateThread(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner) {
  return false;
}

// static
VideoDecodeAccelerator::SupportedProfiles
VTVideoDecodeAccelerator::GetSupportedProfiles() {
  SupportedProfiles profiles;
  if (!InitializeVideoToolbox())
    return profiles;

  for (const auto& supported_profile : kSupportedProfiles) {
    SupportedProfile profile;
    profile.profile = supported_profile;
    profile.min_resolution.SetSize(16, 16);
    profile.max_resolution.SetSize(4096, 2160);
    profiles.push_back(profile);
  }
  return profiles;
}

}  // namespace media
