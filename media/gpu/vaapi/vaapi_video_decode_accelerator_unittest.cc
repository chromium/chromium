// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_decode_accelerator.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/gpu/accelerated_video_decoder.h"
#include "media/gpu/vaapi/vaapi_picture.h"
#include "media/gpu/vaapi/vaapi_picture_factory.h"
#include "media/gpu/vaapi/vaapi_video_decoder_delegate.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/hdr_metadata.h"

using base::test::RunClosure;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::TestWithParam;
using ::testing::ValuesIn;
using ::testing::WithArg;

namespace media {

namespace {

struct TestParams {
  VideoCodecProfile video_codec;
  bool decode_using_client_picture_buffers;
};

constexpr int32_t kBitstreamId = 123;
constexpr size_t kInputSize = 256;

constexpr size_t kNumPictures = 14;
const gfx::Size kPictureSize(64, 48);

constexpr size_t kNewNumPictures = 13;
const gfx::Size kNewPictureSize(64, 48);

MATCHER_P2(IsExpectedDecoderBuffer, data_size, decrypt_config, "") {
  return arg.data_size() == data_size && arg.decrypt_config() == decrypt_config;
}
}  // namespace

class MockAcceleratedVideoDecoder : public AcceleratedVideoDecoder {
 public:
  MockAcceleratedVideoDecoder() = default;
  ~MockAcceleratedVideoDecoder() override = default;

  MOCK_METHOD2(SetStream, void(int32_t id, const DecoderBuffer&));
  MOCK_METHOD0(Flush, bool());
  MOCK_METHOD0(Reset, void());
  MOCK_METHOD0(Decode, DecodeResult());
  MOCK_CONST_METHOD0(GetPicSize, gfx::Size());
  MOCK_CONST_METHOD0(GetProfile, VideoCodecProfile());
  MOCK_CONST_METHOD0(GetBitDepth, uint8_t());
  MOCK_CONST_METHOD0(GetChromaSampling, VideoChromaSampling());
  MOCK_CONST_METHOD0(GetHDRMetadata, absl::optional<gfx::HDRMetadata>());
  MOCK_CONST_METHOD0(GetVisibleRect, gfx::Rect());
  MOCK_CONST_METHOD0(GetRequiredNumOfPictures, size_t());
  MOCK_CONST_METHOD0(GetNumReferenceFrames, size_t());
};

class MockVaapiWrapper : public VaapiWrapper {
 public:
  MockVaapiWrapper(CodecMode mode) : VaapiWrapper(mode) {}
  MOCK_METHOD5(CreateContextAndSurfaces,
               bool(unsigned int,
                    const gfx::Size&,
                    const std::vector<SurfaceUsageHint>&,
                    size_t,
                    std::vector<VASurfaceID>*));
  MOCK_METHOD1(CreateContext, bool(const gfx::Size&));
  MOCK_METHOD0(DestroyContext, void());
  MOCK_METHOD1(DestroySurface, void(VASurfaceID));

 private:
  ~MockVaapiWrapper() override = default;
};

class MockVaapiPicture : public VaapiPicture {
 public:
  MockVaapiPicture(scoped_refptr<VaapiWrapper> vaapi_wrapper,
                   const MakeGLContextCurrentCallback& make_context_current_cb,
                   const BindGLImageCallback& bind_image_cb,
                   int32_t picture_buffer_id,
                   const gfx::Size& size,
                   const gfx::Size& visible_size,
                   uint32_t texture_id,
                   uint32_t client_texture_id,
                   uint32_t texture_target)
      : VaapiPicture(std::move(vaapi_wrapper),
                     make_context_current_cb,
                     bind_image_cb,
                     picture_buffer_id,
                     size,
                     visible_size,
                     texture_id,
                     client_texture_id,
                     texture_target) {}
  ~MockVaapiPicture() override = default;

  // VaapiPicture implementation.
  VaapiStatus Allocate(gfx::BufferFormat format) override {
    return VaapiStatus::Codes::kOk;
  }
  bool ImportGpuMemoryBufferHandle(
      gfx::BufferFormat format,
      gfx::GpuMemoryBufferHandle gpu_memory_buffer_handle) override {
    return true;
  }
  bool DownloadFromSurface(scoped_refptr<VASurface> va_surface) override {
    return true;
  }
  bool AllowOverlay() const override { return false; }
  VASurfaceID va_surface_id() const override {
    // Return any number different from VA_INVALID_ID and VaapiPicture specific.
    return static_cast<VASurfaceID>(texture_id_);
  }
};

class MockVaapiPictureFactory : public VaapiPictureFactory {
 public:
  MockVaapiPictureFactory() = default;
  ~MockVaapiPictureFactory() override = default;

  MOCK_METHOD3(MockCreateVaapiPicture,
               void(VaapiWrapper*, const gfx::Size&, const gfx::Size&));
  std::unique_ptr<VaapiPicture> Create(
      scoped_refptr<VaapiWrapper> vaapi_wrapper,
      const MakeGLContextCurrentCallback& make_context_current_cb,
      const BindGLImageCallback& bind_image_cb,
      const PictureBuffer& picture_buffer,
      const gfx::Size& visible_size) override {
    const uint32_t service_texture_id = picture_buffer.service_texture_ids()[0];
    const uint32_t client_texture_id = picture_buffer.client_texture_ids()[0];
    MockCreateVaapiPicture(vaapi_wrapper.get(), picture_buffer.size(),
                           visible_size);
    return std::make_unique<MockVaapiPicture>(
        std::move(vaapi_wrapper), make_context_current_cb, bind_image_cb,
        picture_buffer.id(), picture_buffer.size(), visible_size,
        service_texture_id, client_texture_id, picture_buffer.texture_target());
  }
};

class VaapiVideoDecodeAcceleratorTest : public TestWithParam<TestParams>,
                                        public VideoDecodeAccelerator::Client {
 public:
  VaapiVideoDecodeAcceleratorTest()
      : vda_(base::BindRepeating([] { return true; }),
             base::BindRepeating(
                 [](uint32_t client_texture_id,
                    uint32_t texture_target,
                    const scoped_refptr<gl::GLImage>& image) { return true; })),
        decoder_thread_("VaapiVideoDecodeAcceleratorTestThread"),
        mock_decoder_(new ::testing::StrictMock<MockAcceleratedVideoDecoder>),
        mock_vaapi_picture_factory_(new MockVaapiPictureFactory()),
        mock_vaapi_wrapper_(new MockVaapiWrapper(VaapiWrapper::kDecode)),
        mock_vpp_vaapi_wrapper_(new MockVaapiWrapper(VaapiWrapper::kDecode)),
        weak_ptr_factory_(this) {
    decoder_thread_.Start();

    // Don't want to go through a vda_->Initialize() because it binds too many
    // items of the environment. Instead, do all the necessary steps here.

    vda_.decoder_thread_task_runner_ = decoder_thread_.task_runner();

    decoder_delegate_ = std::make_unique<VaapiVideoDecoderDelegate>(
        &vda_, mock_vaapi_wrapper_, base::DoNothing(), nullptr);

    // Plug in all the mocks and ourselves as the |client_|.
    vda_.decoder_.reset(mock_decoder_);
    vda_.decoder_delegate_ = decoder_delegate_.get();
    vda_.client_ = weak_ptr_factory_.GetWeakPtr();
    vda_.vaapi_wrapper_ = mock_vaapi_wrapper_;
    vda_.vpp_vaapi_wrapper_ = mock_vpp_vaapi_wrapper_;
    vda_.vaapi_picture_factory_.reset(mock_vaapi_picture_factory_);

    // TODO(crbug.com/917999): add IMPORT mode to test variations.
    vda_.output_mode_ = VideoDecodeAccelerator::Config::OutputMode::ALLOCATE;
    vda_.profile_ = GetParam().video_codec;
    vda_.buffer_allocation_mode_ =
        GetParam().decode_using_client_picture_buffers
            ? VaapiVideoDecodeAccelerator::BufferAllocationMode::kNone
            : VaapiVideoDecodeAccelerator::BufferAllocationMode::kSuperReduced;

    vda_.state_ = VaapiVideoDecodeAccelerator::kIdle;
  }

  VaapiVideoDecodeAcceleratorTest(const VaapiVideoDecodeAcceleratorTest&) =
      delete;
  VaapiVideoDecodeAcceleratorTest& operator=(
      const VaapiVideoDecodeAcceleratorTest&) = delete;

  ~VaapiVideoDecodeAcceleratorTest() {}

  void SetUp() override {
    in_shm_ = base::UnsafeSharedMemoryRegion::Create(kInputSize);
  }

  void SetVdaStateToUnitialized() {
    base::AutoLock auto_lock(vda_.lock_);
    vda_.state_ = VaapiVideoDecodeAccelerator::kUninitialized;
  }

  void QueueInputBuffer(BitstreamBuffer bitstream_buffer) {
    auto id = bitstream_buffer.id();
    vda_.QueueInputBuffer(bitstream_buffer.ToDecoderBuffer(), id);
  }

  void AssignPictureBuffers(const std::vector<PictureBuffer>& picture_buffers) {
    vda_.AssignPictureBuffers(picture_buffers);
  }

  // Reset epilogue, needed to get |vda_| worker thread out of its Wait().
  void ResetSequence() {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_decoder_, Reset());
    EXPECT_CALL(*this, NotifyResetDone())
        .WillOnce(RunClosure(run_loop.QuitClosure()));
    vda_.Reset();
    run_loop.Run();
  }

  // Try and QueueInputBuffer()s, where we pretend that |mock_decoder_| requests
  // to kConfigChange: |vda_| will ping us to ProvidePictureBuffers().
  // If |expect_dismiss_picture_buffers| is signalled, then we expect as well
  // that |vda_| will emit |num_picture_buffers_to_dismiss| DismissPictureBuffer
  // calls.
  void QueueInputBufferSequence(size_t num_pictures,
                                const gfx::Size& picture_size,
                                int32_t bitstream_id,
                                bool expect_dismiss_picture_buffers = false,
                                size_t num_picture_buffers_to_dismiss = 0) {
    ::testing::InSequence s;
    EXPECT_CALL(*mock_decoder_,
                SetStream(_, IsExpectedDecoderBuffer(kInputSize, nullptr)))
        .WillOnce(Return());
    EXPECT_CALL(*mock_decoder_, Decode())
        .WillOnce(Return(AcceleratedVideoDecoder::kConfigChange));

    EXPECT_CALL(*mock_decoder_, GetBitDepth()).WillOnce(Return(8u));
    EXPECT_CALL(*mock_decoder_, GetPicSize()).WillOnce(Return(picture_size));
    EXPECT_CALL(*mock_decoder_, GetVisibleRect())
        .WillOnce(Return(gfx::Rect(picture_size)));
    EXPECT_CALL(*mock_decoder_, GetRequiredNumOfPictures())
        .WillOnce(Return(num_pictures));
    const size_t kNumReferenceFrames = num_pictures / 2;
    EXPECT_CALL(*mock_decoder_, GetNumReferenceFrames())
        .WillOnce(Return(kNumReferenceFrames));
    EXPECT_CALL(*mock_decoder_, GetProfile())
        .WillOnce(Return(GetParam().video_codec));
    EXPECT_CALL(*mock_vaapi_wrapper_, DestroyContext());

    if (expect_dismiss_picture_buffers) {
      EXPECT_CALL(*this, DismissPictureBuffer(_))
          .Times(num_picture_buffers_to_dismiss);
    }

    const size_t expected_num_picture_buffers_requested =
        vda_.buffer_allocation_mode_ ==
                VaapiVideoDecodeAccelerator::BufferAllocationMode::kSuperReduced
            ? num_pictures - kNumReferenceFrames
            : num_pictures;

    base::RunLoop run_loop;

    EXPECT_CALL(*this,
                ProvidePictureBuffers(expected_num_picture_buffers_requested, _,
                                      1, picture_size, _))
        .WillOnce(RunClosure(run_loop.QuitClosure()));

    BitstreamBuffer bitstream_buffer(bitstream_id, in_shm_.Duplicate(),
                                     kInputSize);

    QueueInputBuffer(std::move(bitstream_buffer));
    run_loop.Run();
  }

  // Calls AssignPictureBuffers(), expecting the corresponding mock calls; we
  // pretend |mock_decoder_| has kRanOutOfStreamData (i.e. it's finished
  // decoding) and expect |vda_| to emit a NotifyEndOfBitstreamBuffer().
  // QueueInputBufferSequence() must have been called beforehand.
  void AssignPictureBuffersSequence(size_t num_pictures,
                                    const gfx::Size& picture_size,
                                    int32_t bitstream_id) {
    ASSERT_TRUE(vda_.curr_input_buffer_)
        << "QueueInputBuffer() should have been called";

    // |decode_using_client_picture_buffers| determines the concrete method for
    // creation of context, surfaces and VaapiPictures.
    if (GetParam().decode_using_client_picture_buffers) {
      EXPECT_CALL(*mock_vaapi_wrapper_, CreateContext(picture_size))
          .WillOnce(Return(true));
      EXPECT_CALL(*mock_decoder_, GetVisibleRect())
          .WillRepeatedly(Return(gfx::Rect(picture_size)));
      EXPECT_CALL(*mock_vaapi_picture_factory_,
                  MockCreateVaapiPicture(mock_vaapi_wrapper_.get(),
                                         picture_size, picture_size))
          .Times(num_pictures);
    } else {
      EXPECT_EQ(
          vda_.buffer_allocation_mode_,
          VaapiVideoDecodeAccelerator::BufferAllocationMode::kSuperReduced);
      const size_t kNumReferenceFrames = 1 + num_pictures / 2;
      EXPECT_CALL(*mock_vaapi_wrapper_,
                  CreateContextAndSurfaces(
                      _, picture_size,
                      std::vector<VaapiWrapper::SurfaceUsageHint>{
                          VaapiWrapper::SurfaceUsageHint::kVideoDecoder},
                      kNumReferenceFrames, _))
          .WillOnce(DoAll(
              WithArg<4>(Invoke([kNumReferenceFrames](
                                    std::vector<VASurfaceID>* va_surface_ids) {
                va_surface_ids->resize(kNumReferenceFrames);
              })),
              Return(true)));
      EXPECT_CALL(*mock_vaapi_wrapper_, DestroySurface(_))
          .Times(kNumReferenceFrames);
      EXPECT_CALL(*mock_decoder_, GetVisibleRect())
          .WillRepeatedly(Return(gfx::Rect(picture_size)));
      EXPECT_CALL(*mock_vaapi_picture_factory_,
                  MockCreateVaapiPicture(_, picture_size, picture_size))
          .Times(num_pictures);
    }

    ::testing::InSequence s;
    base::RunLoop run_loop;

    EXPECT_CALL(*mock_decoder_, Decode())
        .WillOnce(Return(AcceleratedVideoDecoder::kRanOutOfStreamData));
    EXPECT_CALL(*this, NotifyEndOfBitstreamBuffer(bitstream_id))
        .WillOnce(RunClosure(run_loop.QuitClosure()));

    const auto tex_target = mock_vaapi_picture_factory_->GetGLTextureTarget();
    int32_t irrelevant_id = 2;
    std::vector<PictureBuffer> picture_buffers;
    for (size_t picture = 0; picture < num_pictures; ++picture) {
      // The picture buffer id, client id and service texture ids are
      // arbitrarily chosen.
      picture_buffers.push_back(
          {irrelevant_id++, picture_size,
           PictureBuffer::TextureIds{static_cast<uint32_t>(irrelevant_id++)},
           PictureBuffer::TextureIds{static_cast<uint32_t>(irrelevant_id++)},
           tex_target, PIXEL_FORMAT_XRGB});
    }

    AssignPictureBuffers(picture_buffers);
    run_loop.Run();
  }

  // Calls QueueInputBuffer(); we instruct from |mock_decoder_| that it has
  // kRanOutOfStreamData (i.e. it's finished decoding). This is a fast method
  // because the Decode() is (almost) immediate.
  void DecodeOneFrameFast(int32_t bitstream_id) {
    base::RunLoop run_loop;
    EXPECT_CALL(*mock_decoder_,
                SetStream(_, IsExpectedDecoderBuffer(kInputSize, nullptr)))
        .WillOnce(Return());
    EXPECT_CALL(*mock_decoder_, Decode())
        .WillOnce(Return(AcceleratedVideoDecoder::kRanOutOfStreamData));
    EXPECT_CALL(*this, NotifyEndOfBitstreamBuffer(bitstream_id))
        .WillOnce(RunClosure(run_loop.QuitClosure()));

    QueueInputBuffer(
        BitstreamBuffer(bitstream_id, in_shm_.Duplicate(), kInputSize));

    run_loop.Run();
  }

  // VideoDecodeAccelerator::Client methods.
  MOCK_METHOD1(NotifyInitializationComplete, void(DecoderStatus));
  MOCK_METHOD5(
      ProvidePictureBuffers,
      void(uint32_t, VideoPixelFormat, uint32_t, const gfx::Size&, uint32_t));
  MOCK_METHOD1(DismissPictureBuffer, void(int32_t));
  MOCK_METHOD1(PictureReady, void(const Picture&));
  MOCK_METHOD1(NotifyEndOfBitstreamBuffer, void(int32_t));
  MOCK_METHOD0(NotifyFlushDone, void());
  MOCK_METHOD0(NotifyResetDone, void());
  MOCK_METHOD1(NotifyError, void(VideoDecodeAccelerator::Error));

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<VaapiVideoDecoderDelegate> decoder_delegate_;

  // The class under test and a worker thread for it.
  VaapiVideoDecodeAccelerator vda_;
  base::Thread decoder_thread_;

  // Ownership passed to |vda_|, but we retain a pointer to it for MOCK checks.
  raw_ptr<MockAcceleratedVideoDecoder> mock_decoder_;
  raw_ptr<MockVaapiPictureFactory> mock_vaapi_picture_factory_;

  scoped_refptr<MockVaapiWrapper> mock_vaapi_wrapper_;
  scoped_refptr<MockVaapiWrapper> mock_vpp_vaapi_wrapper_;

  base::UnsafeSharedMemoryRegion in_shm_;

 private:
  base::WeakPtrFactory<VaapiVideoDecodeAcceleratorTest> weak_ptr_factory_;
};

TEST_P(VaapiVideoDecodeAcceleratorTest, SupportedPlatforms) {
  EXPECT_EQ(VaapiPictureFactory::kVaapiImplementationNone,
            mock_vaapi_picture_factory_->GetVaapiImplementation(
                gl::kGLImplementationNone));
  EXPECT_EQ(VaapiPictureFactory::kVaapiImplementationDrm,
            mock_vaapi_picture_factory_->GetVaapiImplementation(
                gl::kGLImplementationEGLGLES2));

#if BUILDFLAG(USE_VAAPI_X11)
  EXPECT_EQ(VaapiPictureFactory::kVaapiImplementationAngle,
            mock_vaapi_picture_factory_->GetVaapiImplementation(
                gl::kGLImplementationEGLANGLE));
#elif BUILDFLAG(IS_OZONE)
  EXPECT_EQ(VaapiPictureFactory::kVaapiImplementationDrm,
            mock_vaapi_picture_factory_->GetVaapiImplementation(
                gl::kGLImplementationEGLANGLE));
#endif
}

// This test checks that QueueInputBuffer() fails when state is kUnitialized.
TEST_P(VaapiVideoDecodeAcceleratorTest,
       QueueInputBufferAndErrorWhenVDAUninitialized) {
  SetVdaStateToUnitialized();

  BitstreamBuffer bitstream_buffer(kBitstreamId, in_shm_.Duplicate(),
                                   kInputSize);

  EXPECT_CALL(*this,
              NotifyError(VaapiVideoDecodeAccelerator::PLATFORM_FAILURE));
  QueueInputBuffer(std::move(bitstream_buffer));
}

// Verifies that Decode() returning kDecodeError ends up pinging NotifyError().
TEST_P(VaapiVideoDecodeAcceleratorTest, QueueInputBufferAndDecodeError) {
  BitstreamBuffer bitstream_buffer(kBitstreamId, in_shm_.Duplicate(),
                                   kInputSize);

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_decoder_,
              SetStream(_, IsExpectedDecoderBuffer(kInputSize, nullptr)))
      .WillOnce(Return());
  EXPECT_CALL(*mock_decoder_, Decode())
      .WillOnce(Return(AcceleratedVideoDecoder::kDecodeError));
  EXPECT_CALL(*this, NotifyError(VaapiVideoDecodeAccelerator::PLATFORM_FAILURE))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  QueueInputBuffer(std::move(bitstream_buffer));
  run_loop.Run();
}

TEST_P(VaapiVideoDecodeAcceleratorTest, QueueVP9Profile2AndError) {
  if (GetParam().video_codec != VP9PROFILE_PROFILE2)
    GTEST_SKIP() << "The test parameter is not vp9 profile 2";

  BitstreamBuffer bitstream_buffer(kBitstreamId, in_shm_.Duplicate(),
                                   kInputSize);
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_decoder_,
              SetStream(_, IsExpectedDecoderBuffer(kInputSize, nullptr)))
      .WillOnce(Return());
  EXPECT_CALL(*mock_decoder_, Decode())
      .WillOnce(Return(AcceleratedVideoDecoder::kConfigChange));
  EXPECT_CALL(*mock_decoder_, GetBitDepth()).WillOnce(Return(10u));
  EXPECT_CALL(*this, NotifyError(VaapiVideoDecodeAccelerator::PLATFORM_FAILURE))
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  QueueInputBuffer(std::move(bitstream_buffer));
  run_loop.Run();
}

// Verifies a single fast frame decoding..
TEST_P(VaapiVideoDecodeAcceleratorTest, DecodeOneFrame) {
  if (GetParam().video_codec == VP9PROFILE_PROFILE2)
    GTEST_SKIP() << "Decoding profile 2 is not supported";
  DecodeOneFrameFast(kBitstreamId);

  ResetSequence();
}

// Tests usual startup sequence: a BitstreamBuffer is enqueued for decode;
// |vda_| asks for PictureBuffers, that we provide via AssignPictureBuffers().
TEST_P(VaapiVideoDecodeAcceleratorTest,
       QueueInputBuffersAndAssignPictureBuffers) {
  if (GetParam().video_codec == VP9PROFILE_PROFILE2)
    GTEST_SKIP() << "Decoding profile 2 is not supported";
  QueueInputBufferSequence(kNumPictures, kPictureSize, kBitstreamId);

  AssignPictureBuffersSequence(kNumPictures, kPictureSize, kBitstreamId);

  ResetSequence();
}

// Tests a typical resolution change sequence: a BitstreamBuffer is enqueued;
// |vda_| asks for PictureBuffers, we them provide via AssignPictureBuffers().
// We then try to enqueue a few BitstreamBuffers of a different resolution: we
// then expect the old ones to be dismissed and new ones provided.This sequence
// is purely ingress-wise, i.e. there's no decoded output checks.
TEST_P(VaapiVideoDecodeAcceleratorTest,
       QueueInputBuffersAndAssignPictureBuffersAndReallocate) {
  if (GetParam().video_codec == VP9PROFILE_PROFILE2)
    GTEST_SKIP() << "Decoding profile 2 is not supported";
  QueueInputBufferSequence(kNumPictures, kPictureSize, kBitstreamId);

  AssignPictureBuffersSequence(kNumPictures, kPictureSize, kBitstreamId);

  // Decode a few frames. This step is not necessary.
  for (int i = 0; i < 5; ++i)
    DecodeOneFrameFast(kBitstreamId + i);

  QueueInputBufferSequence(kNewNumPictures, kNewPictureSize, kBitstreamId,
                           true /* expect_dismiss_picture_buffers */,
                           kNumPictures /* num_picture_buffers_to_dismiss */);

  AssignPictureBuffersSequence(kNewNumPictures, kNewPictureSize, kBitstreamId);

  ResetSequence();
}

constexpr TestParams kTestCases[] = {
    {H264PROFILE_MIN, false /* decode_using_client_picture_buffers */},
    {H264PROFILE_MIN, true /* decode_using_client_picture_buffers */},
    {VP8PROFILE_MIN, false /* decode_using_client_picture_buffers */},
    {VP9PROFILE_MIN, false /* decode_using_client_picture_buffers */},
    {VP9PROFILE_MIN, true /* decode_using_client_picture_buffers */},
    {VP9PROFILE_PROFILE2, false /* decode_using_client_picture_buffers */},
};

INSTANTIATE_TEST_SUITE_P(All,
                         VaapiVideoDecodeAcceleratorTest,
                         ValuesIn(kTestCases));

}  // namespace media
