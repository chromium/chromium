// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/media_switches.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_metadata.h"
#include "media/capture/mojom/video_capture_buffer.mojom.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/video_capture/public/cpp/mock_video_frame_handler.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor.h"

// ImageTransportFactory::GetInstance is not available on all build configs.
#if defined(USE_AURA) || BUILDFLAG(IS_MAC)
#define CAN_USE_IMAGE_TRANSPORT_FACTORY 1
#endif

#if defined(CAN_USE_IMAGE_TRANSPORT_FACTORY)
#include "content/browser/compositor/image_transport_factory.h"

namespace content {

namespace {

static const char kVideoCaptureHtmlFile[] = "/media/video_capture_test.html";
static const char kStartVideoCaptureAndVerify[] =
    "startVideoCaptureFromVirtualDeviceAndVerifyUniformColorVideoWithSize(%d, "
    "%d)";

static const char kVirtualDeviceId[] = "/virtual/device";
static const char kVirtualDeviceName[] = "Virtual Device";

static const gfx::Size kDummyFrameCodedSize(320, 200);
static const gfx::Rect kDummyFrameVisibleRect(94, 36, 178, 150);
static const int kDummyFrameRate = 5;

}  // namespace

// Abstraction for logic that is different between exercising
// DeviceFactory.AddTextureVirtualDevice() and
// DeviceFactory.AddSharedMemoryVirtualDevice().
class VirtualDeviceExerciser {
 public:
  virtual ~VirtualDeviceExerciser() {}
  virtual void Initialize() = 0;
  virtual void RegisterVirtualDeviceAtVideoSourceProvider(
      mojo::Remote<video_capture::mojom::VideoSourceProvider>* video_source,
      const media::VideoCaptureDeviceInfo& info) = 0;
  virtual gfx::Size GetVideoSize() = 0;
  virtual void PushNextFrame(base::TimeDelta timestamp) = 0;
  virtual void ShutDown() = 0;
};

// A VirtualDeviceExerciser for exercising
// DeviceFactory.AddTextureVirtualDevice(). It alternates between two texture
// RGB dummy frames, one dark one and one light one.
class TextureDeviceExerciser : public VirtualDeviceExerciser {
 public:
  TextureDeviceExerciser() { DETACH_FROM_SEQUENCE(sequence_checker_); }

  void Initialize() override {
    ImageTransportFactory* factory = ImageTransportFactory::GetInstance();
    CHECK(factory);
    context_provider_ =
        factory->GetContextFactory()->SharedMainThreadRasterContextProvider();
    CHECK(context_provider_);
    gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
    CHECK(ri);

    gpu::SharedImageInterface* sii = context_provider_->SharedImageInterface();
    CHECK(sii);

    const SkColor4f kDarkFrameColor = SkColors::kBlack;
    const SkColor4f kLightFrameColor = SkColors::kGray;
    dummy_frame_0_shared_image_ = CreateDummyRgbFrame(
        ri, sii, kDarkFrameColor, dummy_frame_0_sync_token_);
    dummy_frame_1_shared_image_ = CreateDummyRgbFrame(
        ri, sii, kLightFrameColor, dummy_frame_1_sync_token_);
  }

  void RegisterVirtualDeviceAtVideoSourceProvider(
      mojo::Remote<video_capture::mojom::VideoSourceProvider>* video_source,
      const media::VideoCaptureDeviceInfo& info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    (*video_source)
        ->AddTextureVirtualDevice(info,
                                  virtual_device_.BindNewPipeAndPassReceiver());

    gpu::ExportedSharedImage dummy_frame_0_exported_shared_image =
        dummy_frame_0_shared_image_->Export();
    gpu::ExportedSharedImage dummy_frame_1_exported_shared_image =
        dummy_frame_1_shared_image_->Export();

    virtual_device_->OnNewSharedImageBufferHandle(
        0, media::mojom::SharedImageBufferHandleSet::New(
               std::move(dummy_frame_0_exported_shared_image),
               dummy_frame_0_sync_token_));
    virtual_device_->OnNewSharedImageBufferHandle(
        1, media::mojom::SharedImageBufferHandleSet::New(
               std::move(dummy_frame_1_exported_shared_image),
               dummy_frame_1_sync_token_));
    frame_being_consumed_[0] = false;
    frame_being_consumed_[1] = false;
  }

  gfx::Size GetVideoSize() override { return kDummyFrameCodedSize; }

  void PushNextFrame(base::TimeDelta timestamp) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (frame_being_consumed_[dummy_frame_index_]) {
      LOG(INFO) << "Frame " << dummy_frame_index_ << " is still being consumed";
      return;
    }

    if (!virtual_device_has_frame_access_handler_) {
      mojo::PendingRemote<video_capture::mojom::VideoFrameAccessHandler>
          pending_frame_access_handler;
      mojo::MakeSelfOwnedReceiver<
          video_capture::mojom::VideoFrameAccessHandler>(
          std::make_unique<video_capture::FakeVideoFrameAccessHandler>(
              base::BindRepeating(
                  &TextureDeviceExerciser::OnFrameConsumptionFinished,
                  weak_factory_.GetWeakPtr())),
          pending_frame_access_handler.InitWithNewPipeAndPassReceiver());
      virtual_device_->OnFrameAccessHandlerReady(
          std::move(pending_frame_access_handler));
      virtual_device_has_frame_access_handler_ = true;
    }

    media::VideoFrameMetadata metadata;
    metadata.frame_rate = kDummyFrameRate;
    metadata.reference_time = base::TimeTicks::Now();

    media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
    info->timestamp = timestamp;
    info->pixel_format = media::PIXEL_FORMAT_ARGB;
    info->coded_size = kDummyFrameCodedSize;
    info->visible_rect = gfx::Rect(kDummyFrameCodedSize);
    info->metadata = metadata;

    frame_being_consumed_[dummy_frame_index_] = true;
    virtual_device_->OnFrameReadyInBuffer(dummy_frame_index_, std::move(info));

    dummy_frame_index_ = (dummy_frame_index_ + 1) % 2;
  }

  void ShutDown() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    virtual_device_.reset();
    weak_factory_.InvalidateWeakPtrs();
  }

 private:
  scoped_refptr<gpu::ClientSharedImage> CreateDummyRgbFrame(
      gpu::raster::RasterInterface* ri,
      gpu::SharedImageInterface* sii,
      SkColor4f frame_color,
      gpu::SyncToken& ri_token) {
    SkBitmap frame_bitmap;
    frame_bitmap.allocPixels(SkImageInfo::Make(
        kDummyFrameCodedSize.width(), kDummyFrameCodedSize.height(),
        kRGBA_8888_SkColorType, kOpaque_SkAlphaType));
    frame_bitmap.eraseColor(frame_color);

    // This SharedImage is populated via the raster interface below and may
    // be read via the raster interface in normal VideoFrame usage exercised
    // by the tests.
    auto shared_image = sii->CreateSharedImage(
        {viz::SinglePlaneFormat::kRGBA_8888, kDummyFrameCodedSize,
         gfx::ColorSpace::CreateSRGB(), kTopLeft_GrSurfaceOrigin,
         kOpaque_SkAlphaType,
         gpu::SHARED_IMAGE_USAGE_RASTER_READ |
             gpu::SHARED_IMAGE_USAGE_RASTER_WRITE |
             gpu::SHARED_IMAGE_USAGE_OOP_RASTERIZATION,
         "TestLabel"},
        gpu::kNullSurfaceHandle);

    gpu::SyncToken sii_token = sii->GenVerifiedSyncToken();
    ri->WaitSyncTokenCHROMIUM(sii_token.GetConstData());
    ri->WritePixels(shared_image->mailbox(), 0, 0, GL_TEXTURE_2D,
                    frame_bitmap.pixmap());

    ri->GenSyncTokenCHROMIUM(ri_token.GetData());
    ri->ShallowFlushCHROMIUM();
    CHECK_EQ(ri->GetError(), static_cast<GLenum>(GL_NO_ERROR));

    return shared_image;
  }

  void OnFrameConsumptionFinished(int32_t frame_index) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    frame_being_consumed_[frame_index] = false;
  }

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<viz::RasterContextProvider> context_provider_;
  mojo::Remote<video_capture::mojom::TextureVirtualDevice> virtual_device_;
  bool virtual_device_has_frame_access_handler_ = false;
  int dummy_frame_index_ = 0;
  scoped_refptr<gpu::ClientSharedImage> dummy_frame_0_shared_image_;
  scoped_refptr<gpu::ClientSharedImage> dummy_frame_1_shared_image_;
  gpu::SyncToken dummy_frame_0_sync_token_;
  gpu::SyncToken dummy_frame_1_sync_token_;
  std::array<bool, 2> frame_being_consumed_;
  base::WeakPtrFactory<TextureDeviceExerciser> weak_factory_{this};
};

// A VirtualDeviceExerciser for exercising
// DeviceFactory.AddSharedMemoryVirtualDevice().
// It generates (dummy) I420 frame data by setting all bytes equal to the
// current frame count. Padding bytes are set to 0.
class SharedMemoryDeviceExerciser : public VirtualDeviceExerciser,
                                    public video_capture::mojom::Producer {
 public:
  explicit SharedMemoryDeviceExerciser(
      media::mojom::PlaneStridesPtr strides = nullptr)
      : strides_(std::move(strides)) {}

  // VirtualDeviceExerciser implementation.
  void Initialize() override {}
  void RegisterVirtualDeviceAtVideoSourceProvider(
      mojo::Remote<video_capture::mojom::VideoSourceProvider>* video_source,
      const media::VideoCaptureDeviceInfo& info) override {
    mojo::PendingRemote<video_capture::mojom::Producer> producer;
    producer_receiver_.Bind(producer.InitWithNewPipeAndPassReceiver());
    (*video_source)
        ->AddSharedMemoryVirtualDevice(
            info, std::move(producer),
            virtual_device_.BindNewPipeAndPassReceiver());
  }
  gfx::Size GetVideoSize() override {
    return gfx::Size(kDummyFrameVisibleRect.width(),
                     kDummyFrameVisibleRect.height());
  }
  void PushNextFrame(base::TimeDelta timestamp) override {
    virtual_device_->RequestFrameBuffer(
        kDummyFrameCodedSize, media::VideoPixelFormat::PIXEL_FORMAT_I420,
        strides_.Clone(),
        base::BindOnce(&SharedMemoryDeviceExerciser::OnFrameBufferReceived,
                       weak_factory_.GetWeakPtr(), timestamp));
  }
  void ShutDown() override {
    virtual_device_.reset();
    producer_receiver_.reset();
    weak_factory_.InvalidateWeakPtrs();
  }

  // video_capture::mojom::Producer implementation.
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle,
                   OnNewBufferCallback callback) override {
    CHECK(buffer_handle->is_unsafe_shmem_region());
    base::UnsafeSharedMemoryRegion region =
        std::move(buffer_handle->get_unsafe_shmem_region());
    CHECK(region.IsValid());
    base::WritableSharedMemoryMapping mapping = region.Map();
    CHECK(mapping.IsValid());
    outgoing_buffer_id_to_buffer_map_.insert(
        std::make_pair(buffer_id, std::move(mapping)));
    std::move(callback).Run();
  }
  void OnBufferRetired(int32_t buffer_id) override {
    outgoing_buffer_id_to_buffer_map_.erase(buffer_id);
  }

 private:
  void OnFrameBufferReceived(base::TimeDelta timestamp, int32_t buffer_id) {
    if (buffer_id == video_capture::mojom::kInvalidBufferId)
      return;

    media::VideoFrameMetadata metadata;
    metadata.frame_rate = kDummyFrameRate;
    metadata.reference_time = base::TimeTicks::Now();

    media::mojom::VideoFrameInfoPtr info = media::mojom::VideoFrameInfo::New();
    info->timestamp = timestamp;
    info->pixel_format = media::PIXEL_FORMAT_I420;
    info->coded_size = kDummyFrameCodedSize;
    info->visible_rect = kDummyFrameVisibleRect;
    info->metadata = metadata;
    info->strides = strides_.Clone();

    const base::WritableSharedMemoryMapping& outgoing_buffer =
        outgoing_buffer_id_to_buffer_map_.at(buffer_id);

    static int frame_count = 0;
    frame_count++;
    const uint8_t dummy_value = frame_count % 256;

    // Reset the whole buffer to 0
    memset(outgoing_buffer.memory(), 0, outgoing_buffer.size());

    // Set all bytes affecting |info->visible_rect| to |dummy_value|.
    const int kYStride = info->strides ? info->strides->stride_by_plane[0]
                                       : info->coded_size.width();
    const int kYColsToSkipAtStart = info->visible_rect.x();
    const int kYVisibleColCount = info->visible_rect.width();
    const int kYCodedRowCount = info->coded_size.height();
    const int kYRowsToSkipAtStart = info->visible_rect.y();
    const int kYVisibleRowCount = info->visible_rect.height();

    const int kUStride = info->strides ? info->strides->stride_by_plane[1]
                                       : info->coded_size.width() / 2;
    const int kUColsToSkipAtStart =
        cc::MathUtil::UncheckedRoundDown(info->visible_rect.x(), 2) / 2;
    const int kUVisibleColCount =
        (cc::MathUtil::UncheckedRoundUp(info->visible_rect.right(), 2) / 2) -
        kUColsToSkipAtStart;
    const int kUCodedRowCount = info->coded_size.height() / 2;
    const int kURowsToSkipAtStart =
        cc::MathUtil::UncheckedRoundDown(info->visible_rect.y(), 2) / 2;
    const int kUVisibleRowCount =
        (cc::MathUtil::UncheckedRoundUp(info->visible_rect.bottom(), 2) / 2) -
        kURowsToSkipAtStart;

    const int kVStride = info->strides ? info->strides->stride_by_plane[2]
                                       : info->coded_size.width() / 2;

    uint8_t* write_ptr = outgoing_buffer.GetMemoryAsSpan<uint8_t>().data();
    FillVisiblePortionOfPlane(&write_ptr, dummy_value, kYCodedRowCount,
                              kYRowsToSkipAtStart, kYVisibleRowCount, kYStride,
                              kYColsToSkipAtStart, kYVisibleColCount);
    FillVisiblePortionOfPlane(&write_ptr, dummy_value, kUCodedRowCount,
                              kURowsToSkipAtStart, kUVisibleRowCount, kUStride,
                              kUColsToSkipAtStart, kUVisibleColCount);
    FillVisiblePortionOfPlane(&write_ptr, dummy_value, kUCodedRowCount,
                              kURowsToSkipAtStart, kUVisibleRowCount, kVStride,
                              kUColsToSkipAtStart, kUVisibleColCount);

    virtual_device_->OnFrameReadyInBuffer(buffer_id, std::move(info));
  }

  void FillVisiblePortionOfPlane(uint8_t** write_ptr,
                                 uint8_t fill_value,
                                 int row_count,
                                 int rows_to_skip_at_start,
                                 int visible_row_count,
                                 int col_count,
                                 int cols_to_skip_at_start,
                                 int visible_col_count) {
    const int kColsToSkipAtEnd =
        col_count - visible_col_count - cols_to_skip_at_start;
    const int kRowsToSkipAtEnd =
        row_count - visible_row_count - rows_to_skip_at_start;

    // Skip rows at start
    (*write_ptr) += col_count * rows_to_skip_at_start;
    // Fill rows
    for (int i = 0; i < visible_row_count; i++) {
      // Skip cols at start
      (*write_ptr) += cols_to_skip_at_start;
      // Fill visible bytes
      memset(*write_ptr, fill_value, visible_col_count);
      (*write_ptr) += visible_col_count;
      // Skip cols at end
      (*write_ptr) += kColsToSkipAtEnd;
    }
    // Skip rows at end
    (*write_ptr) += col_count * kRowsToSkipAtEnd;
  }

  media::mojom::PlaneStridesPtr strides_;
  mojo::Receiver<video_capture::mojom::Producer> producer_receiver_{this};
  mojo::Remote<video_capture::mojom::SharedMemoryVirtualDevice> virtual_device_;
  std::map<int32_t /*buffer_id*/, base::WritableSharedMemoryMapping>
      outgoing_buffer_id_to_buffer_map_;
  base::WeakPtrFactory<SharedMemoryDeviceExerciser> weak_factory_{this};
};

// Integration test that obtains a connection to the video capture service. It
// It then registers a virtual device at the service and feeds frames to it. It
// opens the virtual device in a <video> element on a test page and verifies
// that the element plays in the expected dimensions and the pixel content on
// the element changes.
class WebRtcVideoCaptureServiceBrowserTest : public ContentBrowserTest {
 public:
  WebRtcVideoCaptureServiceBrowserTest()
      : virtual_device_thread_("Virtual Device Thread") {
    virtual_device_thread_.Start();
  }

  WebRtcVideoCaptureServiceBrowserTest(
      const WebRtcVideoCaptureServiceBrowserTest&) = delete;
  WebRtcVideoCaptureServiceBrowserTest& operator=(
      const WebRtcVideoCaptureServiceBrowserTest&) = delete;

  ~WebRtcVideoCaptureServiceBrowserTest() override {}

  void AddVirtualDeviceAndStartCapture(VirtualDeviceExerciser* device_exerciser,
                                       base::OnceClosure finish_test_cb) {
    DCHECK(virtual_device_thread_.task_runner()->RunsTasksInCurrentSequence());

    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](mojo::PendingReceiver<video_capture::mojom::VideoSourceProvider>
                   receiver) {
              GetVideoCaptureService().ConnectToVideoSourceProvider(
                  std::move(receiver));
            },
            video_source_provider_.BindNewPipeAndPassReceiver()));

    media::VideoCaptureDeviceInfo info;
    info.descriptor.device_id = kVirtualDeviceId;
    info.descriptor.set_display_name(kVirtualDeviceName);
    info.descriptor.capture_api = media::VideoCaptureApi::VIRTUAL_DEVICE;

    video_size_ = device_exerciser->GetVideoSize();
    device_exerciser->RegisterVirtualDeviceAtVideoSourceProvider(
        &video_source_provider_, info);

    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WebRtcVideoCaptureServiceBrowserTest::
                           OpenVirtualDeviceInRendererAndWaitForPlaying,
                       base::Unretained(this),
                       base::BindPostTaskToCurrentDefault(base::BindOnce(
                           &WebRtcVideoCaptureServiceBrowserTest::
                               ShutDownVirtualDeviceAndContinue,
                           base::Unretained(this), device_exerciser,
                           std::move(finish_test_cb)))));

    PushDummyFrameAndScheduleNextPush(device_exerciser);
  }

  void PushDummyFrameAndScheduleNextPush(
      VirtualDeviceExerciser* device_exerciser) {
    DCHECK(virtual_device_thread_.task_runner()->RunsTasksInCurrentSequence());
    device_exerciser->PushNextFrame(CalculateTimeSinceFirstInvocation());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WebRtcVideoCaptureServiceBrowserTest::
                           PushDummyFrameAndScheduleNextPush,
                       weak_factory_.GetWeakPtr(), device_exerciser),
        base::Milliseconds(1000 / kDummyFrameRate));
  }

  void ShutDownVirtualDeviceAndContinue(
      VirtualDeviceExerciser* device_exerciser,
      base::OnceClosure continuation) {
    DCHECK(virtual_device_thread_.task_runner()->RunsTasksInCurrentSequence());
    LOG(INFO) << "Shutting down virtual device";
    device_exerciser->ShutDown();
    video_source_provider_.reset();
    weak_factory_.InvalidateWeakPtrs();
    std::move(continuation).Run();
  }

  void OpenVirtualDeviceInRendererAndWaitForPlaying(
      base::OnceClosure finish_test_cb) {
    DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
    embedded_test_server()->StartAcceptingConnections();
    GURL url(embedded_test_server()->GetURL(kVideoCaptureHtmlFile));
    EXPECT_TRUE(NavigateToURL(shell(), url));

    std::string javascript_to_execute = base::StringPrintf(
        kStartVideoCaptureAndVerify, video_size_.width(), video_size_.height());
    // Start video capture and wait until it started rendering
    ASSERT_TRUE(ExecJs(shell(), javascript_to_execute));

    std::move(finish_test_cb).Run();
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Note: We are not planning to actually use the fake device, but we want
    // to avoid enumerating or otherwise calling into real capture devices.
    command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    EnablePixelOutput();
    ContentBrowserTest::SetUp();
  }

  void Initialize() {
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
    main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  }

  base::Thread virtual_device_thread_;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

 private:
  base::TimeDelta CalculateTimeSinceFirstInvocation() {
    if (first_frame_time_.is_null())
      first_frame_time_ = base::TimeTicks::Now();
    return base::TimeTicks::Now() - first_frame_time_;
  }

  mojo::Remote<video_capture::mojom::VideoSourceProvider>
      video_source_provider_;
  gfx::Size video_size_;
  base::TimeTicks first_frame_time_;
  base::WeakPtrFactory<WebRtcVideoCaptureServiceBrowserTest> weak_factory_{
      this};
};

// TODO(crbug.com/40835247): Fix and enable on Fuchsia.
// TODO(crbug.com/40781953): This test is flakey on macOS.
// TODO(crbug.com/41484083): This test is flakey on ChromeOS.
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_FramesSentThroughTextureVirtualDeviceGetDisplayedOnPage \
  DISABLED_FramesSentThroughTextureVirtualDeviceGetDisplayedOnPage
#else
#define MAYBE_FramesSentThroughTextureVirtualDeviceGetDisplayedOnPage \
  FramesSentThroughTextureVirtualDeviceGetDisplayedOnPage
#endif
IN_PROC_BROWSER_TEST_F(
    WebRtcVideoCaptureServiceBrowserTest,
    MAYBE_FramesSentThroughTextureVirtualDeviceGetDisplayedOnPage) {
  Initialize();
  auto device_exerciser = std::make_unique<TextureDeviceExerciser>();
  device_exerciser->Initialize();

  base::RunLoop run_loop;
  virtual_device_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcVideoCaptureServiceBrowserTest::
                                    AddVirtualDeviceAndStartCapture,
                                base::Unretained(this), device_exerciser.get(),
                                base::BindPostTaskToCurrentDefault(
                                    run_loop.QuitClosure())));
  run_loop.Run();
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40781953): This test is flakey on macOS.
#define MAYBE_FramesSentThroughSharedMemoryVirtualDeviceGetDisplayedOnPage \
  DISABLED_FramesSentThroughSharedMemoryVirtualDeviceGetDisplayedOnPage
#else
#define MAYBE_FramesSentThroughSharedMemoryVirtualDeviceGetDisplayedOnPage \
  FramesSentThroughSharedMemoryVirtualDeviceGetDisplayedOnPage
#endif
IN_PROC_BROWSER_TEST_F(
    WebRtcVideoCaptureServiceBrowserTest,
    MAYBE_FramesSentThroughSharedMemoryVirtualDeviceGetDisplayedOnPage) {
  Initialize();
  auto device_exerciser = std::make_unique<SharedMemoryDeviceExerciser>();
  device_exerciser->Initialize();

  base::RunLoop run_loop;
  virtual_device_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcVideoCaptureServiceBrowserTest::
                                    AddVirtualDeviceAndStartCapture,
                                base::Unretained(this), device_exerciser.get(),
                                base::BindPostTaskToCurrentDefault(
                                    run_loop.QuitClosure())));
  run_loop.Run();
}

#if BUILDFLAG(IS_MAC)
// TODO(crbug.com/40781953): This test is flakey on macOS.
#define MAYBE_PaddedI420FramesSentThroughSharedMemoryVirtualDeviceGetDisplayedOnPage \
  DISABLED_PaddedI420FramesSentThroughSharedMemoryVirtualDeviceGetDisplayedOnPage
#else
#define MAYBE_PaddedI420FramesSentThroughSharedMemoryVirtualDeviceGetDisplayedOnPage \
  PaddedI420FramesSentThroughSharedMemoryVirtualDeviceGetDisplayedOnPage
#endif
IN_PROC_BROWSER_TEST_F(
    WebRtcVideoCaptureServiceBrowserTest,
    MAYBE_PaddedI420FramesSentThroughSharedMemoryVirtualDeviceGetDisplayedOnPage) {
  Initialize();
  auto device_exerciser = std::make_unique<SharedMemoryDeviceExerciser>(
      media::mojom::PlaneStrides::New(
          std::vector<uint32_t>({1024u, 512u, 1024u, 0u})));
  device_exerciser->Initialize();

  base::RunLoop run_loop;
  virtual_device_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcVideoCaptureServiceBrowserTest::
                                    AddVirtualDeviceAndStartCapture,
                                base::Unretained(this), device_exerciser.get(),
                                base::BindPostTaskToCurrentDefault(
                                    run_loop.QuitClosure())));
  run_loop.Run();
}

}  // namespace content

#endif  // defined(CAN_USE_IMAGE_TRANSPORT_FACTORY)
