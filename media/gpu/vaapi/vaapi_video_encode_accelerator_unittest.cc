// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/vaapi/vaapi_video_encode_accelerator.h"

#include <memory>
#include <numeric>
#include <vector>

#include "base/bits.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "build/chromeos_buildflags.h"
#include "media/base/media_util.h"
#include "media/base/mock_media_log.h"
#include "media/base/video_frame.h"
#include "media/gpu/gpu_video_encode_accelerator_helpers.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_video_encoder_delegate.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/gpu/vaapi/vp9_vaapi_video_encoder_delegate.h"
#include "media/gpu/vp9_picture.h"
#include "media/video/fake_gpu_memory_buffer.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_pixmap.h"

using base::test::RunClosure;
using ::testing::_;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Return;
using ::testing::WithArgs;

namespace media {
namespace {

constexpr gfx::Size kDefaultEncodeSize(1280, 720);
constexpr uint32_t kDefaultBitrateBps = 4 * 1000 * 1000;
constexpr Bitrate kDefaultBitrate =
    Bitrate::ConstantBitrate(kDefaultBitrateBps);
constexpr uint32_t kDefaultFramerate = 30;
constexpr size_t kMaxNumOfRefFrames = 3u;

constexpr int kSpatialLayersResolutionDenom[][3] = {
    {1, 0, 0},  // For one spatial layer.
    {2, 1, 0},  // For two spatial layers.
    {4, 2, 1},  // For three spatial layers.
};

VideoEncodeAccelerator::Config DefaultVideoEncodeAcceleratorConfig() {
  VideoEncodeAccelerator::Config vea_config(
      PIXEL_FORMAT_I420, kDefaultEncodeSize, VP9PROFILE_PROFILE0,
      kDefaultBitrate, kDefaultFramerate,
      VideoEncodeAccelerator::Config::StorageType::kShmem,
      VideoEncodeAccelerator::Config::ContentType::kCamera);

  return vea_config;
}

std::vector<VideoEncodeAccelerator::Config::SpatialLayer> GetDefaultSVCLayers(
    size_t num_spatial_layers,
    size_t num_temporal_layers) {
  std::vector<VideoEncodeAccelerator::Config::SpatialLayer> spatial_layers;

  // Return empty |spatial_layers| for simple stream.
  if (num_spatial_layers == 1 && num_temporal_layers == 1)
    return spatial_layers;

  for (uint8_t i = 0; i < num_spatial_layers; ++i) {
    VideoEncodeAccelerator::Config::SpatialLayer spatial_layer;
    const int denom = kSpatialLayersResolutionDenom[num_spatial_layers - 1][i];
    spatial_layer.width = kDefaultEncodeSize.width() / denom;
    spatial_layer.height = kDefaultEncodeSize.height() / denom;
    spatial_layer.bitrate_bps = kDefaultBitrateBps / denom;
    spatial_layer.framerate = kDefaultFramerate;
    spatial_layer.max_qp = 30;
    spatial_layer.num_of_temporal_layers = num_temporal_layers;
    spatial_layers.push_back(spatial_layer);
  }

  return spatial_layers;
}

std::vector<gfx::Size> GetDefaultSVCResolutions(size_t num_spatial_layers) {
  std::vector<gfx::Size> spatial_layer_resolutions;
  for (size_t i = 0; i < num_spatial_layers; ++i) {
    const int denom = kSpatialLayersResolutionDenom[num_spatial_layers - 1][i];
    spatial_layer_resolutions.emplace_back(
        gfx::Size(kDefaultEncodeSize.width() / denom,
                  kDefaultEncodeSize.height() / denom));
  }

  return spatial_layer_resolutions;
}

bool IsSVCSupported(const VideoEncodeAccelerator::Config& config) {
  // k-SVC encoding only supported on NV12/GMB/VP9.
  return config.input_format == PIXEL_FORMAT_NV12 &&
         config.storage_type ==
             VideoEncodeAccelerator::Config::StorageType::kGpuMemoryBuffer &&
         config.output_profile == VP9PROFILE_PROFILE0;
}

MATCHER_P(CheckEncodeData, payload_size_bytes, "") {
  return arg.payload_size_bytes == payload_size_bytes;
}

MATCHER_P3(MatchesBitstreamBufferMetadata,
           payload_size_bytes,
           key_frame,
           has_vp9_metadata,
           "") {
  return arg.payload_size_bytes == payload_size_bytes &&
         arg.key_frame == key_frame && arg.vp9.has_value() == has_vp9_metadata;
}

MATCHER_P2(MatchesEncoderInfo,
           num_of_spatial_layers,
           num_of_temporal_layers,
           "") {
  for (size_t i = 0; i < num_of_spatial_layers; ++i) {
    const auto& fps_allocation = arg.fps_allocation[i];
    if (fps_allocation.size() != num_of_temporal_layers)
      return false;
    constexpr uint8_t kFullFramerate = 255;
    if (fps_allocation.back() != kFullFramerate)
      return false;
    if (fps_allocation.size() != 1 &&
        fps_allocation != GetFpsAllocation(num_of_temporal_layers)) {
      return false;
    }
  }
  return arg.implementation_name == "VaapiVideoEncodeAccelerator" &&
         arg.supports_native_handle && !arg.has_trusted_rate_controller &&
         arg.is_hardware_accelerated && !arg.supports_simulcast;
}

MATCHER(ContainsTooManyEncoderInstances, "") {
  return CONTAINS_STRING(arg, "Too many encoders are allocated");
}

class MockVideoEncodeAcceleratorClient : public VideoEncodeAccelerator::Client {
 public:
  MockVideoEncodeAcceleratorClient() = default;
  ~MockVideoEncodeAcceleratorClient() override = default;

  MOCK_METHOD3(RequireBitstreamBuffers,
               void(unsigned int, const gfx::Size&, size_t));
  MOCK_METHOD2(BitstreamBufferReady,
               void(int32_t, const BitstreamBufferMetadata&));
  MOCK_METHOD1(NotifyErrorStatus, void(const EncoderStatus&));
  MOCK_METHOD1(NotifyEncoderInfoChange, void(const VideoEncoderInfo&));
};

class MockVaapiWrapper : public VaapiWrapper {
 public:
  explicit MockVaapiWrapper(CodecMode mode)
      : VaapiWrapper(VADisplayStateHandle(), mode) {}

  MOCK_METHOD2(GetVAEncMaxNumOfRefFrames, bool(VideoCodecProfile, size_t*));
  MOCK_METHOD1(CreateContext, bool(const gfx::Size&));
  MOCK_METHOD6(CreateScopedVASurfaces,
               std::vector<std::unique_ptr<ScopedVASurface>>(
                   unsigned int,
                   const gfx::Size&,
                   const std::vector<SurfaceUsageHint>&,
                   size_t,
                   const std::optional<gfx::Size>&,
                   const std::optional<uint32_t>&));
  MOCK_METHOD2(CreateVABuffer,
               std::unique_ptr<ScopedVABuffer>(VABufferType, size_t));
  MOCK_METHOD2(
      CreateVASurfaceForPixmap,
      std::unique_ptr<ScopedVASurface>(scoped_refptr<const gfx::NativePixmap>,
                                       bool));
  MOCK_METHOD2(GetEncodedChunkSize, uint64_t(VABufferID, VASurfaceID));
  MOCK_METHOD5(
      DownloadFromVABuffer,
      bool(VABufferID, std::optional<VASurfaceID>, uint8_t*, size_t, size_t*));
  MOCK_METHOD3(UploadVideoFrameToSurface,
               bool(const VideoFrame&, VASurfaceID, const gfx::Size&));
  MOCK_METHOD1(ExecuteAndDestroyPendingBuffers, bool(VASurfaceID));
  MOCK_METHOD0(DestroyContext, void());
  MOCK_METHOD1(DestroySurface, void(VASurfaceID));

  MOCK_METHOD2(DoBlitSurface,
               bool(std::optional<gfx::Rect>, std::optional<gfx::Rect>));
  bool BlitSurface(VASurfaceID va_surface_src_id,
                   const gfx::Size& va_surface_src_size,
                   VASurfaceID va_surface_dst_id,
                   const gfx::Size& va_surface_dst_size,
                   std::optional<gfx::Rect> src_rect = std::nullopt,
                   std::optional<gfx::Rect> dest_rect = std::nullopt
#if BUILDFLAG(IS_CHROMEOS_ASH)
                   ,
                   VAProtectedSessionID va_protected_session_id = VA_INVALID_ID
#endif
                   ) override {
    return DoBlitSurface(src_rect, dest_rect);
  }

 private:
  ~MockVaapiWrapper() override = default;
};

class MockVaapiVideoEncoderDelegate : public VaapiVideoEncoderDelegate {
 public:
  MockVaapiVideoEncoderDelegate(scoped_refptr<VaapiWrapper> vaapi_wrapper,
                                base::RepeatingClosure error_cb)
      : VaapiVideoEncoderDelegate(vaapi_wrapper, error_cb) {}
  MOCK_METHOD2(Initialize,
               bool(const VideoEncodeAccelerator::Config&,
                    const VaapiVideoEncoderDelegate::Config&));
  MOCK_CONST_METHOD0(GetCodedSize, gfx::Size());
  MOCK_CONST_METHOD0(GetMaxNumOfRefFrames, size_t());
  MOCK_METHOD0(GetSVCLayerResolutions, std::vector<gfx::Size>());
  MOCK_METHOD2(GetMetadata, BitstreamBufferMetadata(const EncodeJob&, size_t));
  MOCK_METHOD1(PrepareEncodeJob, PrepareEncodeJobResult(EncodeJob&));
  MOCK_METHOD2(UpdateRates, bool(const VideoBitrateAllocation&, uint32_t));
  MOCK_METHOD1(BitrateControlUpdate, void(const BitstreamBufferMetadata&));
};

class MockVP9VaapiVideoEncoderDelegate : public VP9VaapiVideoEncoderDelegate {
 public:
  MockVP9VaapiVideoEncoderDelegate(
      const scoped_refptr<VaapiWrapper>& vaapi_wrapper,
      base::RepeatingClosure error_cb)
      : VP9VaapiVideoEncoderDelegate(vaapi_wrapper, error_cb) {}
  MOCK_METHOD2(Initialize,
               bool(const VideoEncodeAccelerator::Config&,
                    const VaapiVideoEncoderDelegate::Config&));
  MOCK_CONST_METHOD0(GetCodedSize, gfx::Size());
  MOCK_CONST_METHOD0(GetBitstreamBufferSize, size_t());
  MOCK_CONST_METHOD0(GetMaxNumOfRefFrames, size_t());
  MOCK_METHOD2(GetMetadata, BitstreamBufferMetadata(const EncodeJob&, size_t));
  MOCK_METHOD1(PrepareEncodeJob, PrepareEncodeJobResult(EncodeJob&));
  MOCK_METHOD1(BitrateControlUpdate, void(const BitstreamBufferMetadata&));
  MOCK_METHOD0(GetSVCLayerResolutions, std::vector<gfx::Size>());
  bool UpdateRates(const VideoBitrateAllocation&, uint32_t) override {
    return false;
  }
};
}  // namespace

struct VaapiVideoEncodeAcceleratorTestParam;

class VaapiVideoEncodeAcceleratorTest
    : public ::testing::TestWithParam<VaapiVideoEncodeAcceleratorTestParam> {
 public:
  // Populate meaningful test suffixes instead of /0, /1, etc.
  struct PrintToStringParamName {
    template <class ParamType>
    std::string operator()(
        const testing::TestParamInfo<ParamType>& info) const {
      // Naming according to
      // https://www.w3.org/TR/webrtc-svc/#dependencydiagrams*
      return base::StringPrintf(
          "L%dT%d%s", info.param.num_of_spatial_layers,
          info.param.num_of_temporal_layers,
          (info.param.inter_layer_pred == SVCInterLayerPredMode::kOnKeyPic
               ? "_KEY"
               : ""));
    }
  };

 protected:
  VaapiVideoEncodeAcceleratorTest() = default;
  ~VaapiVideoEncodeAcceleratorTest() override = default;

  MOCK_METHOD0(OnError, void());

  void SetUp() override {
    mock_vaapi_wrapper_ = base::MakeRefCounted<MockVaapiWrapper>(
        VaapiWrapper::kEncodeConstantBitrate);

    // In real usage, the VaapiWrapper expects to be constructed, used, and
    // destroyed on the same sequence. For testing, however, we create it in the
    // main thread of the test and inject it into the VaapiVideoEncodeAccelerator
    // where it will be used and destroyed on the encoder thread. Therefore, we
    // detach the VaapiWrapper from the construction sequence just for testing.
    mock_vaapi_wrapper_->sequence_checker_.DetachFromSequence();
  }

  void ResetEncoder() {
    encoder_.reset(new VaapiVideoEncodeAccelerator);
    mock_encoder_delegate_ = nullptr;
    auto* vaapi_encoder =
        reinterpret_cast<VaapiVideoEncodeAccelerator*>(encoder_.get());
    base::WaitableEvent event;
    auto on_error_cb = base::BindRepeating(
        &VaapiVideoEncodeAcceleratorTest::OnError, base::Unretained(this));
    // Set |encoder_| and |vaapi_wrapper_| of |vaapi_encoder| in the encoder
    // sequence.
    vaapi_encoder->encoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](VaapiVideoEncodeAccelerator* vaapi_encoder,
               scoped_refptr<VaapiWrapper> vaapi_wrapper,
               base::RepeatingClosure on_error_cb,
               raw_ptr<MockVaapiVideoEncoderDelegate,
                       AcrossTasksDanglingUntriaged>* mock_encoder_delegate,
               base::WaitableEvent* event) {
              DCHECK_CALLED_ON_VALID_SEQUENCE(
                  vaapi_encoder->encoder_sequence_checker_);
              vaapi_encoder->vaapi_wrapper_ = vaapi_wrapper;
              vaapi_encoder->encoder_ =
                  std::make_unique<MockVaapiVideoEncoderDelegate>(
                      vaapi_wrapper, std::move(on_error_cb));
              *mock_encoder_delegate =
                  reinterpret_cast<MockVaapiVideoEncoderDelegate*>(
                      vaapi_encoder->encoder_.get());
              event->Signal();
            },
            base::Unretained(vaapi_encoder), mock_vaapi_wrapper_, on_error_cb,
            base::Unretained(&mock_encoder_delegate_),
            base::Unretained(&event)));
    event.Wait();
    EXPECT_CALL(*this, OnError()).Times(0);
  }

  void ResetVp9Encoder() {
    encoder_.reset(new VaapiVideoEncodeAccelerator);
    mock_encoder_delegate_ = nullptr;
    auto* vaapi_encoder =
        reinterpret_cast<VaapiVideoEncodeAccelerator*>(encoder_.get());
    base::WaitableEvent event;
    auto on_error_cb = base::BindRepeating(
        &VaapiVideoEncodeAcceleratorTest::OnError, base::Unretained(this));
    // Set |encoder_| of |vaapi_encoder| to be a
    // MockVP9VaapiVideoEncoderDelegate in the encoder sequence.
    vaapi_encoder->encoder_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](VaapiVideoEncodeAccelerator* vaapi_encoder,
               scoped_refptr<VaapiWrapper> vaapi_wrapper,
               base::RepeatingClosure on_error_cb,
               raw_ptr<MockVP9VaapiVideoEncoderDelegate,
                       AcrossTasksDanglingUntriaged>* mock_encoder,
               base::WaitableEvent* event) {
              DCHECK_CALLED_ON_VALID_SEQUENCE(
                  vaapi_encoder->encoder_sequence_checker_);
              vaapi_encoder->vaapi_wrapper_ = vaapi_wrapper;
              vaapi_encoder->encoder_ =
                  std::make_unique<MockVP9VaapiVideoEncoderDelegate>(
                      vaapi_wrapper, std::move(on_error_cb));
              *mock_encoder =
                  reinterpret_cast<MockVP9VaapiVideoEncoderDelegate*>(
                      vaapi_encoder->encoder_.get());
              event->Signal();
            },
            base::Unretained(vaapi_encoder), mock_vaapi_wrapper_, on_error_cb,
            base::Unretained(&mock_encoder_), base::Unretained(&event)));
    event.Wait();
    EXPECT_CALL(*this, OnError()).Times(0);
  }

  void SetDefaultMocksBehavior(const VideoEncodeAccelerator::Config& config) {
    ASSERT_TRUE(mock_vaapi_wrapper_);
    ASSERT_TRUE(mock_encoder_);

    ON_CALL(*mock_vaapi_wrapper_, GetVAEncMaxNumOfRefFrames)
        .WillByDefault(WithArgs<1>([](size_t* max_ref_frames) {
          *max_ref_frames = kMaxNumOfRefFrames;
          return true;
        }));

    ON_CALL(*mock_encoder_, GetBitstreamBufferSize)
        .WillByDefault(Return(config.input_visible_size.GetArea()));
    ON_CALL(*mock_encoder_, GetCodedSize())
        .WillByDefault(Return(config.input_visible_size));
    ON_CALL(*mock_encoder_, GetMaxNumOfRefFrames())
        .WillByDefault(Return(kMaxNumOfRefFrames));
    std::vector<gfx::Size> svc_resolutions;
    if (config.spatial_layers.size() > 1) {
      svc_resolutions = GetDefaultSVCResolutions(config.spatial_layers.size());
    } else {
      svc_resolutions = {kDefaultEncodeSize};
    }
    ON_CALL(*mock_encoder_, GetSVCLayerResolutions())
        .WillByDefault(Return(svc_resolutions));
  }

  bool InitializeVideoEncodeAccelerator(
      const VideoEncodeAccelerator::Config& config) {
    VideoEncodeAccelerator::SupportedProfile profile(config.output_profile,
                                                     config.input_visible_size);
    auto* vaapi_encoder =
        reinterpret_cast<VaapiVideoEncodeAccelerator*>(encoder_.get());
    vaapi_encoder->supported_profiles_for_testing_.push_back(profile);
    if (config.input_visible_size.IsEmpty())
      return false;
    return encoder_->Initialize(config, &client_,
                                std::make_unique<media::NullMediaLog>());
  }

  static constexpr int GetMaxNumOfEncoderInstances() {
    return VaapiVideoEncodeAccelerator::kMaxNumOfInstances;
  }

  void InitializeSequenceForVP9(const VideoEncodeAccelerator::Config& config)
      NO_THREAD_SAFETY_ANALYSIS {
    base::RunLoop run_loop;
    ::testing::InSequence s;
    const size_t num_spatial_layers = config.spatial_layers.size();
    // Scaling is needed only for non highest spatial layer, so here the vpp
    // number is |num_spatial_layers - 1|.
    va_encode_surface_ids_.resize(num_spatial_layers);
    va_vpp_dest_surface_ids_.resize(num_spatial_layers - 1, VA_INVALID_ID);
    mock_vpp_vaapi_wrapper_ =
        base::MakeRefCounted<MockVaapiWrapper>(VaapiWrapper::kVideoProcess);
    // In real usage, the VaapiWrapper expects to be constructed, used, and
    // destroyed on the same sequence. For testing, however, we create it in the
    // main thread of the test and inject it into the VaapiVideoEncodeAccelerator
    // where it will be used and destroyed on the encoder thread. Therefore, we
    // detach the VaapiWrapper from the construction sequence just for testing.
    mock_vpp_vaapi_wrapper_->sequence_checker_.DetachFromSequence();
    auto* vaapi_encoder =
        reinterpret_cast<VaapiVideoEncodeAccelerator*>(encoder_.get());
    vaapi_encoder->vpp_vaapi_wrapper_ = mock_vpp_vaapi_wrapper_;

    EXPECT_CALL(
        *mock_encoder_,
        Initialize(_,
                   testing::Field(
                       &VaapiVideoEncoderDelegate::Config::max_num_ref_frames,
                       kMaxNumOfRefFrames)))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_vaapi_wrapper_, CreateContext(kDefaultEncodeSize))
        .WillOnce(Return(true));
    EXPECT_CALL(client_, RequireBitstreamBuffers(_, kDefaultEncodeSize, _))
        .WillOnce(WithArgs<2>([this](size_t output_buffer_size) {
          this->output_buffer_size_ = output_buffer_size;
        }));
    EXPECT_CALL(client_, NotifyEncoderInfoChange(MatchesEncoderInfo(
                             num_spatial_layers,
                             config.spatial_layers[0].num_of_temporal_layers)))
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    ASSERT_TRUE(InitializeVideoEncodeAccelerator(config));
    run_loop.Run();
  }

  void EncodeSequenceForVP9SingleSpatialLayer(
      bool use_temporal_layer_encoding) {
    ::testing::InSequence s;

    constexpr VASurfaceID kEncodeSurfaceId = 1234;
    EXPECT_CALL(*mock_vaapi_wrapper_,
                CreateScopedVASurfaces(
                    VA_RT_FORMAT_YUV420, kDefaultEncodeSize,
                    std::vector<VaapiWrapper::SurfaceUsageHint>{
                        VaapiWrapper::SurfaceUsageHint::kVideoEncoder},
                    _, std::optional<gfx::Size>(), std::optional<uint32_t>()))
        .WillOnce(
            WithArgs<0, 1, 3>([&surface_ids = this->va_encode_surface_ids_[0],
                               &vaapi_wrapper = this->mock_vaapi_wrapper_](
                                  unsigned int format, const gfx::Size& size,
                                  size_t num_surfaces) {
              surface_ids.resize(num_surfaces);
              std::iota(surface_ids.begin(), surface_ids.end(), 1);
              surface_ids.back() = kEncodeSurfaceId;
              std::vector<std::unique_ptr<ScopedVASurface>> va_surfaces;
              for (const VASurfaceID id : surface_ids) {
                va_surfaces.push_back(std::make_unique<ScopedVASurface>(
                    vaapi_wrapper, id, size, format));
              }
              return va_surfaces;
            }));

    constexpr VASurfaceID kInputSurfaceId = 1234;
    EXPECT_CALL(*mock_vaapi_wrapper_,
                CreateScopedVASurfaces(
                    VA_RT_FORMAT_YUV420, kDefaultEncodeSize,
                    std::vector<VaapiWrapper::SurfaceUsageHint>{
                        VaapiWrapper::SurfaceUsageHint::kVideoEncoder},
                    1, std::optional<gfx::Size>(), std::optional<uint32_t>()))
        .WillOnce(
            WithArgs<0, 1>([&vaapi_wrapper = this->mock_vaapi_wrapper_,
                            surface_id = kInputSurfaceId](
                               unsigned int format, const gfx::Size& size) {
              std::vector<std::unique_ptr<ScopedVASurface>> va_surfaces;
              va_surfaces.push_back(std::make_unique<ScopedVASurface>(
                  vaapi_wrapper, surface_id, size, format));
              return va_surfaces;
            }));

    EXPECT_CALL(
        *mock_vaapi_wrapper_,
        UploadVideoFrameToSurface(_, kInputSurfaceId, kDefaultEncodeSize))
        .WillOnce(Return(true));

    constexpr VABufferID kCodedBufferId = 123;
    EXPECT_CALL(*mock_vaapi_wrapper_,
                CreateVABuffer(VAEncCodedBufferType, output_buffer_size_))
        .WillOnce(WithArgs<1>([](size_t buffer_size) {
          return ScopedVABuffer::CreateForTesting(
              kCodedBufferId, VAEncCodedBufferType, buffer_size);
        }));

    EXPECT_CALL(*mock_encoder_, PrepareEncodeJob(_))
        .WillOnce(WithArgs<0>([use_temporal_layer_encoding](
                                  VaapiVideoEncoderDelegate::EncodeJob& job) {
          if (use_temporal_layer_encoding) {
            // Set Vp9Metadata on temporal layer encoding.
            CodecPicture* picture = job.picture().get();
            reinterpret_cast<VP9Picture*>(picture)->metadata_for_encoding =
                Vp9Metadata();
          }
          return VaapiVideoEncoderDelegate::PrepareEncodeJobResult::kSuccess;
        }));
    EXPECT_CALL(*mock_vaapi_wrapper_,
                ExecuteAndDestroyPendingBuffers(kInputSurfaceId))
        .WillOnce(Return(true));

    constexpr uint64_t kEncodedChunkSize = 1234;
    ASSERT_LE(kEncodedChunkSize, output_buffer_size_);
    EXPECT_CALL(*mock_vaapi_wrapper_,
                GetEncodedChunkSize(kCodedBufferId, kInputSurfaceId))
        .WillOnce(Return(kEncodedChunkSize));
    EXPECT_CALL(*mock_encoder_, GetMetadata(_, _))
        .WillOnce(
            WithArgs<0, 1>([](const VaapiVideoEncoderDelegate::EncodeJob& job,
                              size_t payload_size) {
              // Same implementation in VP9VaapiVideoEncoderDelegate.
              BitstreamBufferMetadata metadata(
                  payload_size, job.IsKeyframeRequested(), job.timestamp());
              CodecPicture* picture = job.picture().get();
              metadata.vp9 =
                  reinterpret_cast<VP9Picture*>(picture)->metadata_for_encoding;
              return metadata;
            }));
    EXPECT_CALL(*mock_encoder_,
                BitrateControlUpdate(CheckEncodeData(kEncodedChunkSize)))
        .WillOnce(Return());
    EXPECT_CALL(*mock_vaapi_wrapper_,
                DownloadFromVABuffer(kCodedBufferId, Eq(std::nullopt), _,
                                     output_buffer_size_, _))
        .WillOnce(WithArgs<4>([](size_t* coded_data_size) {
          *coded_data_size = kEncodedChunkSize;
          return true;
        }));

    constexpr int32_t kBitstreamId = 12;
    base::RunLoop run_loop;

    EXPECT_CALL(client_, BitstreamBufferReady(kBitstreamId,
                                              MatchesBitstreamBufferMetadata(
                                                  kEncodedChunkSize, false,
                                                  use_temporal_layer_encoding)))
        .WillOnce(RunClosure(run_loop.QuitClosure()));

    auto region = base::UnsafeSharedMemoryRegion::Create(output_buffer_size_);
    ASSERT_TRUE(region.IsValid());
    encoder_->UseOutputBitstreamBuffer(
        BitstreamBuffer(kBitstreamId, std::move(region), output_buffer_size_));

    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, kDefaultEncodeSize,
                                         gfx::Rect(kDefaultEncodeSize),
                                         kDefaultEncodeSize, base::TimeDelta());
    ASSERT_TRUE(frame);
    encoder_->Encode(std::move(frame), false /* force_keyframe */);
    run_loop.Run();
  }

  void EncodeSequenceForVP9MultipleSpatialLayers(size_t num_spatial_layers) {
    constexpr int32_t kBitstreamIds[] = {12, 13, 14};
    constexpr uint64_t kEncodedChunkSizes[] = {1234, 1235, 1236};
    ASSERT_LE(num_spatial_layers, std::size(kBitstreamIds));
    ASSERT_LE(num_spatial_layers, std::size(kEncodedChunkSizes));
    base::RunLoop run_loop;
    // BitstreamBufferReady() is called in |child_task_runner_|, which is the
    // different thread of executing other mock calls. Therefore, guaranteeing
    // EXPECT_CALLs in the order of sequence |s| is impossible. Calls the
    // expected BitstreamBufferReady()s before |s| to not care when they are
    // called while calls in the sequence |s| are invoked.
    for (size_t i = 0; i < num_spatial_layers; ++i) {
      const int32_t kBitstreamId = kBitstreamIds[i];
      const uint64_t kEncodedChunkSize = kEncodedChunkSizes[i];
      if (i < num_spatial_layers - 1) {
        EXPECT_CALL(client_,
                    BitstreamBufferReady(kBitstreamId,
                                         MatchesBitstreamBufferMetadata(
                                             kEncodedChunkSize, false, true)));
      } else {
        EXPECT_CALL(client_,
                    BitstreamBufferReady(kBitstreamId,
                                         MatchesBitstreamBufferMetadata(
                                             kEncodedChunkSize, false, true)))
            .WillOnce(RunClosure(run_loop.QuitClosure()));
      }

      auto region = base::UnsafeSharedMemoryRegion::Create(output_buffer_size_);
      ASSERT_TRUE(region.IsValid());
      encoder_->UseOutputBitstreamBuffer(BitstreamBuffer(
          kBitstreamId, std::move(region), output_buffer_size_));
    }

    ::testing::InSequence s;

    std::vector<gfx::Size> svc_resolutions =
        GetDefaultSVCResolutions(num_spatial_layers);

    constexpr VASurfaceID kEncodeSurfaceIds[] = {458, 459, 460};
    for (size_t i = 0; i < num_spatial_layers; i++) {
      // For reconstructed surface.
      if (va_encode_surface_ids_[i].empty()) {
        EXPECT_CALL(
            *mock_vaapi_wrapper_,
            CreateScopedVASurfaces(
                VA_RT_FORMAT_YUV420, svc_resolutions[i],
                std::vector<VaapiWrapper::SurfaceUsageHint>{
                    VaapiWrapper::SurfaceUsageHint::kVideoEncoder},
                _, std::optional<gfx::Size>(), std::optional<uint32_t>()))
            .WillOnce(WithArgs<0, 1, 3>(
                [&surface_ids = this->va_encode_surface_ids_[i],
                 &vaapi_wrapper = this->mock_vaapi_wrapper_,
                 va_encode_surface_id = kEncodeSurfaceIds[i]](
                    unsigned int format, const gfx::Size& size,
                    size_t num_surfaces) {
                  surface_ids.resize(num_surfaces);
                  std::iota(surface_ids.begin(), surface_ids.end(), 1);
                  surface_ids.back() = va_encode_surface_id;
                  std::vector<std::unique_ptr<ScopedVASurface>> va_surfaces;
                  for (const VASurfaceID id : surface_ids) {
                    va_surfaces.push_back(std::make_unique<ScopedVASurface>(
                        vaapi_wrapper, id, size, format));
                  }
                  return va_surfaces;
                }));
      }
    }
    // Create VASurface from GpuMemory-based VideoFrame.
    const VASurfaceID kSourceSurfaceId = 123456;
    EXPECT_CALL(*mock_vaapi_wrapper_, CreateVASurfaceForPixmap(_, _))
        .WillOnce(Return(std::make_unique<ScopedVASurface>(
            mock_vaapi_wrapper_, kSourceSurfaceId, kDefaultEncodeSize,
            VA_RT_FORMAT_YUV420)));

    constexpr VASurfaceID kVppDestSurfaceIds[] = {456, 457};

    // Create Surfaces.
    for (size_t i = 0; i < num_spatial_layers - 1; ++i) {
        if (va_vpp_dest_surface_ids_[i] == VA_INVALID_ID) {
          EXPECT_CALL(
              *mock_vpp_vaapi_wrapper_,
              CreateScopedVASurfaces(
                  VA_RT_FORMAT_YUV420, svc_resolutions[i],
                  std::vector<VaapiWrapper::SurfaceUsageHint>{
                      VaapiWrapper::SurfaceUsageHint::kVideoProcessWrite,
                      VaapiWrapper::SurfaceUsageHint::kVideoEncoder},
                  1, std::optional<gfx::Size>(), std::optional<uint32_t>()))
              .WillOnce(WithArgs<0, 1>(
                  [&surface_id = this->va_vpp_dest_surface_ids_[i],
                   &vaapi_wrapper = this->mock_vpp_vaapi_wrapper_,
                   vpp_dest_surface_id = kVppDestSurfaceIds[i]](
                      unsigned int format, const gfx::Size& size) {
                    surface_id = vpp_dest_surface_id;
                    std::vector<std::unique_ptr<ScopedVASurface>> va_surfaces;
                    va_surfaces.push_back(std::make_unique<ScopedVASurface>(
                        vaapi_wrapper, vpp_dest_surface_id, size, format));
                    return va_surfaces;
                  }));
        }
        std::optional<gfx::Rect> default_rect = gfx::Rect(kDefaultEncodeSize);
        std::optional<gfx::Rect> layer_rect = gfx::Rect(svc_resolutions[i]);
        EXPECT_CALL(*mock_vpp_vaapi_wrapper_,
                    DoBlitSurface(default_rect, layer_rect))
            .WillOnce(Return(true));
    }

    // Create CodedBuffers in creating EncodeJobs.
    constexpr VABufferID kCodedBufferIds[] = {123, 124, 125};
    for (size_t i = 0; i < num_spatial_layers; ++i) {
      const VABufferID kCodedBufferId = kCodedBufferIds[i];
      EXPECT_CALL(*mock_vaapi_wrapper_,
                  CreateVABuffer(VAEncCodedBufferType, output_buffer_size_))
          .WillOnce(WithArgs<1>([kCodedBufferId](size_t buffer_size) {
            return ScopedVABuffer::CreateForTesting(
                kCodedBufferId, VAEncCodedBufferType, buffer_size);
          }));
    }

    for (size_t i = 0; i < num_spatial_layers; ++i) {
      EXPECT_CALL(*mock_encoder_, PrepareEncodeJob(_))
          .WillOnce(WithArgs<0>([](VaapiVideoEncoderDelegate::EncodeJob& job) {
            // Set Vp9Metadata on spatial layer encoding.
            CodecPicture* picture = job.picture().get();
            reinterpret_cast<VP9Picture*>(picture)->metadata_for_encoding =
                Vp9Metadata();
            return VaapiVideoEncoderDelegate::PrepareEncodeJobResult::kSuccess;
          }));
      EXPECT_CALL(*mock_vaapi_wrapper_, ExecuteAndDestroyPendingBuffers(_))
          .WillOnce(Return(true));
    }
    for (size_t i = 0; i < num_spatial_layers; ++i) {
      const VABufferID kCodedBufferId = kCodedBufferIds[i];
      const uint64_t kEncodedChunkSize = kEncodedChunkSizes[i];
      ASSERT_LE(kEncodedChunkSize, output_buffer_size_);
      EXPECT_CALL(*mock_vaapi_wrapper_, GetEncodedChunkSize(kCodedBufferId, _))
          .WillOnce(Return(kEncodedChunkSize));
      EXPECT_CALL(*mock_encoder_, GetMetadata(_, _))
          .WillOnce(
              WithArgs<0, 1>([](const VaapiVideoEncoderDelegate::EncodeJob& job,
                                size_t payload_size) {
                // Same implementation in VP9VaapiVideoEncoderDelegate.
                BitstreamBufferMetadata metadata(
                    payload_size, job.IsKeyframeRequested(), job.timestamp());
                CodecPicture* picture = job.picture().get();
                metadata.vp9 = reinterpret_cast<VP9Picture*>(picture)
                                   ->metadata_for_encoding;
                return metadata;
              }));
      EXPECT_CALL(*mock_encoder_,
                  BitrateControlUpdate(CheckEncodeData(kEncodedChunkSize)))
          .WillOnce(Return());
    }

    for (size_t i = 0; i < num_spatial_layers; ++i) {
      const VABufferID kCodedBufferId = kCodedBufferIds[i];
      const uint64_t kEncodedChunkSize = kEncodedChunkSizes[i];
      EXPECT_CALL(*mock_vaapi_wrapper_,
                  DownloadFromVABuffer(kCodedBufferId, Eq(std::nullopt), _,
                                       output_buffer_size_, _))
          .WillOnce(WithArgs<4>([kEncodedChunkSize](size_t* coded_data_size) {
            *coded_data_size = kEncodedChunkSize;
            return true;
          }));
    }

    std::unique_ptr<gfx::GpuMemoryBuffer> gmb =
        std::make_unique<FakeGpuMemoryBuffer>(
            kDefaultEncodeSize, gfx::BufferFormat::YUV_420_BIPLANAR);
    auto frame = VideoFrame::WrapExternalGpuMemoryBuffer(
        gfx::Rect(kDefaultEncodeSize), kDefaultEncodeSize, std::move(gmb),
        base::TimeDelta());
    ASSERT_TRUE(frame);
    encoder_->Encode(std::move(frame), /*force_keyframe=*/false);
    run_loop.Run();
  }
  using Config = VideoEncodeAccelerator::Config;
  size_t output_buffer_size_ = 0;
  std::vector<VASurfaceID> va_vpp_dest_surface_ids_;
  std::vector<std::vector<VASurfaceID>> va_encode_surface_ids_;
  base::test::TaskEnvironment task_environment_;
  MockVideoEncodeAcceleratorClient client_;
  // |encoder_| is a VideoEncodeAccelerator to use its specialized Deleter that
  // calls Destroy() so that destruction threading is respected.
  std::unique_ptr<VideoEncodeAccelerator> encoder_;
  scoped_refptr<MockVaapiWrapper> mock_vaapi_wrapper_;
  scoped_refptr<MockVaapiWrapper> mock_vpp_vaapi_wrapper_;
  raw_ptr<MockVP9VaapiVideoEncoderDelegate, AcrossTasksDanglingUntriaged>
      mock_encoder_ = nullptr;
  raw_ptr<MockVaapiVideoEncoderDelegate, AcrossTasksDanglingUntriaged>
      mock_encoder_delegate_ = nullptr;
};

struct VaapiVideoEncodeAcceleratorTestParam {
  SVCInterLayerPredMode inter_layer_pred = SVCInterLayerPredMode::kOnKeyPic;
  uint8_t num_of_spatial_layers = 0;
  uint8_t num_of_temporal_layers = 0;
} kTestCases[]{
    // {inter_layer_pred, num_of_spatial_layers, num_of_temporal_layers}
    {SVCInterLayerPredMode::kOnKeyPic, 2u, 1u},
    {SVCInterLayerPredMode::kOnKeyPic, 2u, 2u},
    {SVCInterLayerPredMode::kOnKeyPic, 2u, 3u},
    {SVCInterLayerPredMode::kOnKeyPic, 3u, 1u},
    {SVCInterLayerPredMode::kOnKeyPic, 3u, 2u},
    {SVCInterLayerPredMode::kOnKeyPic, 3u, 3u},
    {SVCInterLayerPredMode::kOff, 1u, 1u},
    {SVCInterLayerPredMode::kOff, 1u, 2u},
    {SVCInterLayerPredMode::kOff, 1u, 3u},
    {SVCInterLayerPredMode::kOff, 2u, 1u},
    {SVCInterLayerPredMode::kOff, 2u, 2u},
    {SVCInterLayerPredMode::kOff, 2u, 3u},
    {SVCInterLayerPredMode::kOff, 3u, 1u},
    {SVCInterLayerPredMode::kOff, 3u, 2u},
    {SVCInterLayerPredMode::kOff, 3u, 3u},
};

TEST_P(VaapiVideoEncodeAcceleratorTest, Initialize) {
  ResetEncoder();
  const uint8_t num_of_spatial_layers = GetParam().num_of_spatial_layers;
  const SVCInterLayerPredMode inter_layer_pred = GetParam().inter_layer_pred;

  Config config = DefaultVideoEncodeAcceleratorConfig();
  config.inter_layer_pred = inter_layer_pred;
  const uint8_t num_of_temporal_layers = GetParam().num_of_temporal_layers;
  config.spatial_layers =
      GetDefaultSVCLayers(num_of_spatial_layers, num_of_temporal_layers);

  for (const VideoPixelFormat format : {PIXEL_FORMAT_I420, PIXEL_FORMAT_NV12}) {
    for (const Config::StorageType storage_type :
         {Config::StorageType::kShmem, Config::StorageType::kGpuMemoryBuffer}) {
      for (const VideoCodecProfile profile :
           {H264PROFILE_MAIN, VP9PROFILE_PROFILE0}) {
        config.input_format = format;
        config.storage_type = storage_type;
        config.output_profile = profile;
        if (storage_type == Config::StorageType::kGpuMemoryBuffer &&
            format != PIXEL_FORMAT_NV12) {
          // VaapiVEA doesn't support native input mode for non-NV12 format.
          EXPECT_EQ(InitializeVideoEncodeAccelerator(config), false);
        } else {
          EXPECT_EQ(InitializeVideoEncodeAccelerator(config),
                    !config.HasSpatialLayer() || IsSVCSupported(config));
        }
        // Since |encoder_->Initialize| needs be called many times, so here
        // manually reset the |encoder_|.
        ResetEncoder();
      }
    }
  }
}

// This test verifies VP9 single stream and temporal layer encoding in non
// native input mode.
TEST_P(VaapiVideoEncodeAcceleratorTest, EncodeVP9WithSingleSpatialLayer) {
  ResetVp9Encoder();
  if (GetParam().num_of_spatial_layers > 1u)
    GTEST_SKIP() << "Test only meant for single spatial layer";

  Config config = DefaultVideoEncodeAcceleratorConfig();
  const SVCInterLayerPredMode inter_layer_pred = GetParam().inter_layer_pred;
  config.inter_layer_pred = inter_layer_pred;
  Config::SpatialLayer spatial_layer;
  spatial_layer.width = kDefaultEncodeSize.width();
  spatial_layer.height = kDefaultEncodeSize.height();
  spatial_layer.bitrate_bps = kDefaultBitrateBps;
  spatial_layer.framerate = kDefaultFramerate;
  spatial_layer.max_qp = 30;
  spatial_layer.num_of_temporal_layers = GetParam().num_of_temporal_layers;
  config.spatial_layers.push_back(spatial_layer);
  SetDefaultMocksBehavior(config);

  InitializeSequenceForVP9(config);
  EncodeSequenceForVP9SingleSpatialLayer(spatial_layer.num_of_temporal_layers >
                                         1u);
}

// This test verifies VP9 multiple spaital layers encoding in native input mode.
TEST_P(VaapiVideoEncodeAcceleratorTest, EncodeVP9WithMultipleSpatialLayers) {
  ResetVp9Encoder();
  const uint8_t num_of_spatial_layers = GetParam().num_of_spatial_layers;
  if (num_of_spatial_layers <= 1)
    GTEST_SKIP() << "Test only meant for multiple spatial layers configuration";

  const uint8_t num_of_temporal_layers = GetParam().num_of_temporal_layers;
  Config config = DefaultVideoEncodeAcceleratorConfig();
  const SVCInterLayerPredMode inter_layer_pred = GetParam().inter_layer_pred;
  config.inter_layer_pred = inter_layer_pred;
  config.input_format = PIXEL_FORMAT_NV12;
  config.storage_type = Config::StorageType::kGpuMemoryBuffer;
  config.spatial_layers =
      GetDefaultSVCLayers(num_of_spatial_layers, num_of_temporal_layers);
  SetDefaultMocksBehavior(config);

  InitializeSequenceForVP9(config);
  EncodeSequenceForVP9MultipleSpatialLayers(num_of_spatial_layers);
}

// This test verifies Initialize() fails with correct corresponding error
// logging when the max number of encoder instances is reached. Once it happens,
// the encoder fails the rest of the Initialize() sequence, which requires
// setting up new |encoder_| and |vaapi_wrapper_|s to succeed. So this test
// creates and stores encoder instances within the threshold number without
// initializing them.
TEST_F(VaapiVideoEncodeAcceleratorTest, TooManyEncoderInstances) {
  Config config = DefaultVideoEncodeAcceleratorConfig();
  constexpr int kMaxNumOfInstances = GetMaxNumOfEncoderInstances();

  std::vector<std::unique_ptr<VaapiVideoEncodeAccelerator>> encoders(
      kMaxNumOfInstances);
  for (int i = 0; i <= kMaxNumOfInstances; i++) {
    auto encoder = std::make_unique<VaapiVideoEncodeAccelerator>();
    auto media_log = std::make_unique<MockMediaLog>();
    if (i == kMaxNumOfInstances) {
      EXPECT_MEDIA_LOG_ON(*media_log, ContainsTooManyEncoderInstances());
      EXPECT_FALSE(encoder->Initialize(config, &client_, std::move(media_log)));
    } else {
      encoders[i] = std::move(encoder);
    }
  }
}

// This test verifies Initialize() fails when the encoder is already
// Initialize()d.
TEST_F(VaapiVideoEncodeAcceleratorTest, AttemptedInitialization) {
  ResetEncoder();
  Config config = DefaultVideoEncodeAcceleratorConfig();
  EXPECT_TRUE(InitializeVideoEncodeAccelerator(config));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(InitializeVideoEncodeAccelerator(config));
  task_environment_.RunUntilIdle();
}

TEST_F(VaapiVideoEncodeAcceleratorTest, InitializeWithUnsupportedConfig) {
  const Bitrate kVariableBitrate = Bitrate::VariableBitrate(0u, 123456u);
  const Config unsupported_configs[] = {
      // VaapiVEA does not support HEVC encoding.
      Config(PIXEL_FORMAT_NV12, kDefaultEncodeSize, HEVCPROFILE_MAIN,
             kDefaultBitrate, kDefaultFramerate, Config::StorageType::kShmem,
             Config::ContentType::kCamera),
      // VaapiVEA only supports variable bitrate with H264 encoding.
      Config(PIXEL_FORMAT_NV12, kDefaultEncodeSize, VP9PROFILE_PROFILE0,
             kVariableBitrate, kDefaultFramerate, Config::StorageType::kShmem,
             Config::ContentType::kCamera),
      // VaapiVEA does not support PIXEL_FORMAT_YV12.
      Config(PIXEL_FORMAT_YV12, kDefaultEncodeSize, VP9PROFILE_PROFILE0,
             kDefaultBitrate, kDefaultFramerate, Config::StorageType::kShmem,
             Config::ContentType::kCamera)};

  for (const auto& config : unsupported_configs) {
    ResetEncoder();
    EXPECT_FALSE(InitializeVideoEncodeAccelerator(config));
  }
}

// This test verifies RequestEncodingParametersChange() succeeds.
TEST_F(VaapiVideoEncodeAcceleratorTest, EncodingParametersChange) {
  const uint32_t kNewFramerate = 60;
  const uint32_t kNewBitrate = 123123u;

  const Bitrate kConstantBitrate = Bitrate::ConstantBitrate(kNewBitrate);
  const Bitrate kVariableBitrate =
      Bitrate::VariableBitrate(kNewBitrate, 2 * kNewBitrate);

  for (const Bitrate bitrate : {kConstantBitrate, kVariableBitrate}) {
    ResetEncoder();
    Config config = DefaultVideoEncodeAcceleratorConfig();
    if (bitrate.mode() == Bitrate::Mode::kVariable) {
      // Variable bitrate is only supported with H264 encoding.
      config.output_profile = H264PROFILE_BASELINE;
      const uint32_t bitrate_bps = config.bitrate.target_bps();
      config.bitrate = Bitrate::VariableBitrate(bitrate_bps, 2u * bitrate_bps);
    }
    ASSERT_TRUE(InitializeVideoEncodeAccelerator(config));
    task_environment_.RunUntilIdle();

    VideoBitrateAllocation expected_bitrate_allocation(bitrate.mode());
    expected_bitrate_allocation.SetBitrate(0, 0, bitrate.target_bps());
    expected_bitrate_allocation.SetPeakBps(bitrate.peak_bps());
    EXPECT_CALL(*mock_encoder_delegate_,
                UpdateRates(expected_bitrate_allocation, kNewFramerate))
        .WillOnce(Return(true));
    encoder_->RequestEncodingParametersChange(bitrate, kNewFramerate,
                                              std::nullopt);
    task_environment_.RunUntilIdle();
  }
}

// This test verifies RequestEncodingParametersChange() succeeds with
// multi-dimensional bitrate allocation.
TEST_F(VaapiVideoEncodeAcceleratorTest,
       EncodingParametersChangeWithBitrateAllocation) {
  ResetEncoder();
  Config config = DefaultVideoEncodeAcceleratorConfig();
  ASSERT_TRUE(InitializeVideoEncodeAccelerator(config));
  task_environment_.RunUntilIdle();

  const uint32_t kNewFramerate = 60;
  // Verify translation of VideoBitrateAllocation into vector of bitrates for
  // everything from empty array up to max number of layers.
  VideoBitrateAllocation bitrate_allocation;
  for (size_t si = 0; si < VideoBitrateAllocation::kMaxSpatialLayers; si++) {
    for (size_t ti = 0; ti < VideoBitrateAllocation::kMaxTemporalLayers; ti++) {
      uint32_t layer_bitrate =
          std::max(si * ti * 1000, static_cast<size_t>(100));
      bitrate_allocation.SetBitrate(si, ti, layer_bitrate);
      EXPECT_CALL(*mock_encoder_delegate_,
                  UpdateRates(bitrate_allocation, kNewFramerate))
          .WillOnce(Return(true));
      encoder_->RequestEncodingParametersChange(bitrate_allocation,
                                                kNewFramerate, std::nullopt);
      task_environment_.RunUntilIdle();
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    VaapiVideoEncodeAcceleratorTest,
    ::testing::ValuesIn(kTestCases),
    VaapiVideoEncodeAcceleratorTest::PrintToStringParamName());
}  // namespace media
