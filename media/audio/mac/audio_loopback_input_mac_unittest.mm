// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/audio/mac/audio_loopback_input_mac.h"
#include "media/audio/mac/audio_loopback_input_mac_impl.h"

#include <ScreenCaptureKit/ScreenCaptureKit.h>

#include <cstdint>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/apple/scoped_objc_class_swizzler.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "media/base/mac/audio_latency_mac.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#include "ui/gfx/geometry/rect.h"

using ::testing::_;

namespace media {

constexpr int kSampleRate = 48000;
constexpr int kFramesPerBuffer = 480;

constexpr gfx::Rect kDisplayPrimary(0, 0, 1920, 1080);
constexpr gfx::Rect kDisplaySecondary(-1920, 10, 1920, 1080);

class SCKAudioInputStreamTest : public PlatformTest {
 protected:
  static SCDisplay* API_AVAILABLE(macos(13.0)) CreateSCDisplay(CGRect frame) {
    id display = OCMClassMock([SCDisplay class]);
    OCMStub([display frame]).andReturn(frame);
    return display;
  }

  // Reports 2 displays when enumerating shareable content.
  static void API_AVAILABLE(macos(13.0))
      ShareableContentSuccess(NSInvocation* invocation) {
    void (^handler)(SCShareableContent* _Nullable, NSError* _Nullable);
    [invocation getArgument:&handler atIndex:2];

    NSArray* displays = @[
      CreateSCDisplay(kDisplayPrimary.ToCGRect()),
      CreateSCDisplay(kDisplaySecondary.ToCGRect())
    ];

    id content = OCMClassMock([SCShareableContent class]);
    OCMStub([content displays]).andReturn(displays);

    handler(content, nil);
  }

  SCKAudioInputStreamTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {}

  ~SCKAudioInputStreamTest() override = default;

  void API_AVAILABLE(macos(13.0)) SetUp() override {
    stream_delegates_.clear();
    playing_stream_count_ = 0;
  }

  void API_AVAILABLE(macos(13.0))
      SetUpShareableContentMock(void (^handler)(NSInvocation* invocation)) {
    shareable_content_mock_ = OCMClassMock([SCShareableContent class]);
    OCMStub([shareable_content_mock_
                getShareableContentWithCompletionHandler:[OCMArg any]])
        .andDo(handler);
  }

  // Mocks instance methods of an SCStream.
  API_AVAILABLE(macos(13.0))
  void StartSCStreamMocking(SCStream* stream,
                            SCContentFilter* filter,
                            SCStreamConfiguration* config,
                            id<SCStreamDelegate> delegate) {
    if (@available(macOS 13.0, *)) {
      EXPECT_TRUE(stream);
      EXPECT_TRUE(filter);
      EXPECT_TRUE(config);
      EXPECT_TRUE(delegate);

      stream_delegates_.emplace_back(delegate);

      scstream_mock_ = OCMPartialMock(stream);

      OCMStub([scstream_mock_ addStreamOutput:[OCMArg any]
                                         type:SCStreamOutputTypeAudio
                           sampleHandlerQueue:[OCMArg any]
                                        error:[OCMArg anyObjectRef]])
          .andDo(^(NSInvocation* invocation) {
            __unsafe_unretained id<SCStreamOutput> stream_output;
            [invocation getArgument:&stream_output atIndex:2];
            stream_outputs_.emplace_back(stream_output);
          })
          .andReturn(TRUE);

      OCMStub([scstream_mock_ removeStreamOutput:[OCMArg any]
                                            type:SCStreamOutputTypeAudio
                                           error:[OCMArg anyObjectRef]])
          .andDo(^(NSInvocation* invocation) {
            __unsafe_unretained id<SCStreamOutput> stream_output;
            [invocation getArgument:&stream_output atIndex:2];
            stream_outputs_.erase(
                std::remove(stream_outputs_.begin(), stream_outputs_.end(),
                            stream_output),
                stream_outputs_.end());
          })
          .andReturn(TRUE);

      OCMStub([scstream_mock_ startCaptureWithCompletionHandler:[OCMArg any]])
          .andDo(^(NSInvocation* invocation) {
            playing_stream_count_++;
          });

      OCMStub([scstream_mock_ stopCaptureWithCompletionHandler:[OCMArg any]])
          .andDo(^(NSInvocation* invocation) {
            playing_stream_count_--;
          });
    }
  }

  // Create an instance of SCKAudioInputStream with default parameters.
  API_AVAILABLE(macos(13.0))
  SCKAudioInputStream* CreateAudioInputStream() {
    const auto params = AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                        ChannelLayoutConfig::Stereo(),
                                        kSampleRate, kFramesPerBuffer);
    auto* stream = new SCKAudioInputStream(
        params, AudioDeviceDescription::kLoopbackInputDeviceId,
        base::BindRepeating(&SCKAudioInputStreamTest::OnLogMessage,
                            base::Unretained(this)),
        base::BindRepeating([](AudioInputStream* stream) { delete stream; }),
        base::BindRepeating(&SCKAudioInputStreamTest::StartSCStreamMocking,
                            base::Unretained(this)),
        base::Milliseconds(100));

    return stream;
  }

  // Create a CMSampleBuffer from a given stereo audio buffer with default
  // parameters. `buffer` must outlive the returned CMSampleBufferRef.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> CreateStereoAudioSampleBuffer(
      std::array<float, 2 * kFramesPerBuffer>& buffer) {
    CMBlockBufferCustomBlockSource custom_block_source{};
    custom_block_source.FreeBlock = [](void* refcon, void* doomedMemoryBlock,
                                       size_t sizeInBytes) {
      // Memory block is allocated and deallocated by the caller, which must
      // guarantee it outlives `block_buffer`, and should not be freed during
      // `block_buffer` release.
    };

    base::apple::ScopedCFTypeRef<CMBlockBufferRef> block_buffer;
    CMBlockBufferCreateWithMemoryBlock(
        NULL, buffer.data(), buffer.size() * sizeof(float), NULL,
        &custom_block_source, 0, buffer.size() * sizeof(float), 0,
        block_buffer.InitializeInto());

    AudioStreamBasicDescription asbd;
    asbd.mFormatID = kAudioFormatLinearPCM;
    asbd.mFormatFlags = 0;  // Raw.
    asbd.mSampleRate = kSampleRate;
    asbd.mBitsPerChannel = 8 * sizeof(float);
    asbd.mBytesPerFrame = sizeof(float);  // Non-interleaved data.
    asbd.mChannelsPerFrame = 2;           // Stereo.
    asbd.mBytesPerPacket =
        2 * kFramesPerBuffer *
        sizeof(float);  // 2 channels, |kFramesPerBuffer| frames each.
    asbd.mFramesPerPacket = kFramesPerBuffer;

    base::apple::ScopedCFTypeRef<CMAudioFormatDescriptionRef>
        format_description;
    CMAudioFormatDescriptionCreate(kCFAllocatorDefault, &asbd, 0, NULL, 0, NULL,
                                   NULL, format_description.InitializeInto());

    base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample_buffer;
    CMAudioSampleBufferCreateReadyWithPacketDescriptions(
        kCFAllocatorDefault, block_buffer.get(), format_description.get(),
        kFramesPerBuffer,
        CMTimeMakeWithSeconds(
            base::TimeTicks::Now().since_origin().InMicrosecondsF(), 1000000),
        NULL, sample_buffer.InitializeInto());

    return sample_buffer;
  }

  // Send an audio sample packet to the registered SCStreamOutput.
  void SendAudioSample(std::array<float, 2 * kFramesPerBuffer> buffer) {
    if (@available(macOS 13.0, *)) {
      for (auto& stream_output : stream_outputs_) {
        EXPECT_TRUE(stream_output);

        // Pass |stream| as a variable to bypass the nullability check as
        // |stream| is not needed.
        SCStream* stream = nil;
        [stream_output stream:stream
            didOutputSampleBuffer:CreateStereoAudioSampleBuffer(buffer).get()
                           ofType:SCStreamOutputTypeAudio];
      }
    }
  }

  // Send an audio sample with dummy data.
  void SendAudioSample() {
    // Buffer must be 16-bit aligned.
    alignas(16) std::array<float, 2 * kFramesPerBuffer> buffer;
    for (size_t i = 0; i < buffer.size(); i++) {
      buffer[i] = i;
    }

    SendAudioSample(buffer);
  }

  // Send an error to the registered SCStreamDelegate.
  void SendError() {
    if (@available(macOS 13.0, *)) {
      for (auto& stream_delegate : stream_delegates_) {
        // Pass |stream| as a variable to bypass the nullability check as
        // |stream| is not needed.
        SCStream* stream = nil;
        [stream_delegate
                      stream:stream
            didStopWithError:[NSError errorWithDomain:SCStreamErrorDomain
                                                 code:SCStreamErrorInternalError
                                             userInfo:nil]];
      }
    }
  }

  // Fake log callback.
  void OnLogMessage(const std::string& message) {}

  base::test::SingleThreadTaskEnvironment task_environment_;

  // Mock SCShareableContent.
  id shareable_content_mock_;

  // Mock SCStream.
  id scstream_mock_;

  // Keep track of open SCStream related objects.
  // Must be __unsafe_unretained as they come from an NSInvocation.
  API_AVAILABLE(macos(13.0))
  std::vector<__unsafe_unretained id<SCStreamDelegate>> stream_delegates_;
  API_AVAILABLE(macos(13.0))
  std::vector<__unsafe_unretained id<SCStreamOutput>> stream_outputs_;

  // Number of currently playing streams; incremented on a successful start of
  // SCK stream and decremented on stop.
  int playing_stream_count_;
};

class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MOCK_METHOD4(OnData,
               void(const AudioBus* src,
                    base::TimeTicks capture_time,
                    double volume,
                    const AudioGlitchInfo& glitch_info));
  MOCK_METHOD0(OnError, void());
};

class FakeAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  FakeAudioInputCallback() = default;

  FakeAudioInputCallback(const FakeAudioInputCallback&) = delete;
  FakeAudioInputCallback& operator=(const FakeAudioInputCallback&) = delete;

  void OnData(const AudioBus* src,
              base::TimeTicks capture_time,
              double volume,
              const AudioGlitchInfo& glitch_info) override {
    EXPECT_GE(capture_time, base::TimeTicks());
    for (int i = 0; i < src->channels(); i++) {
      channel_data_.insert(channel_data_.end(), src->channel(i),
                           src->channel(i) + src->frames());
    }
  }

  void OnError() override {}

  std::vector<float> channel_data() const { return channel_data_; }

 private:
  std::vector<float> channel_data_;
};

// Test starting a single stream.
TEST_F(SCKAudioInputStreamTest, StartOneStream) {
  if (@available(macOS 13.0, *)) {
    SetUpShareableContentMock(^(NSInvocation* invocation) {
      ShareableContentSuccess(invocation);
    });

    SCKAudioInputStream* stream = CreateAudioInputStream();

    EXPECT_EQ(stream->Open(), AudioInputStream::OpenOutcome::kSuccess);
    EXPECT_EQ(stream_delegates_.size(), 1u);
    EXPECT_EQ(stream_outputs_.size(), 1u);
    EXPECT_EQ(playing_stream_count_, 0);

    MockAudioInputCallback sink;
    stream->Start(&sink);
    EXPECT_EQ(playing_stream_count_, 1);

    stream->Stop();
    EXPECT_EQ(playing_stream_count_, 0);

    stream->Close();
    EXPECT_TRUE(stream_outputs_.empty());

    // Remove dangling references to stream delegates.
    stream_delegates_.clear();
  }
}

// Test opening and starting two streams simultaneously.
TEST_F(SCKAudioInputStreamTest, StartTwoStreams) {
  if (@available(macOS 13.0, *)) {
    SetUpShareableContentMock(^(NSInvocation* invocation) {
      ShareableContentSuccess(invocation);
    });

    SCKAudioInputStream* first_stream = CreateAudioInputStream();
    SCKAudioInputStream* second_stream = CreateAudioInputStream();

    EXPECT_EQ(first_stream->Open(), AudioInputStream::OpenOutcome::kSuccess);

    // For some reason, only the first call to [SCShareableContent
    // getShareableContentWithCompletionHandler:] is getting mocked. As a
    // workaround, destroy the existing mock object and create a new one.
    [shareable_content_mock_ stopMocking];
    SetUpShareableContentMock(^(NSInvocation* invocation) {
      ShareableContentSuccess(invocation);
    });

    EXPECT_EQ(second_stream->Open(), AudioInputStream::OpenOutcome::kSuccess);
    EXPECT_EQ(stream_delegates_.size(), 2u);
    EXPECT_EQ(stream_outputs_.size(), 2u);

    MockAudioInputCallback first_sink;
    MockAudioInputCallback second_sink;
    first_stream->Start(&first_sink);
    second_stream->Start(&second_sink);

    EXPECT_EQ(playing_stream_count_, 2);

    first_stream->Stop();
    second_stream->Stop();

    EXPECT_EQ(playing_stream_count_, 0);

    first_stream->Close();
    second_stream->Close();

    EXPECT_TRUE(stream_outputs_.empty());

    // Remove dangling references to stream delegates.
    stream_delegates_.clear();
  }
}

// Test Start(), Stop(), Start(), Stop().
TEST_F(SCKAudioInputStreamTest, StreamPausing) {
  if (@available(macOS 13.0, *)) {
    SetUpShareableContentMock(^(NSInvocation* invocation) {
      ShareableContentSuccess(invocation);
    });

    SCKAudioInputStream* stream = CreateAudioInputStream();

    EXPECT_EQ(stream->Open(), AudioInputStream::OpenOutcome::kSuccess);

    MockAudioInputCallback sink;
    stream->Start(&sink);
    EXPECT_EQ(playing_stream_count_, 1);
    stream->Stop();
    EXPECT_EQ(playing_stream_count_, 0);
    stream->Start(&sink);
    EXPECT_EQ(playing_stream_count_, 1);
    stream->Stop();
    EXPECT_EQ(playing_stream_count_, 0);

    stream->Close();
    EXPECT_TRUE(stream_outputs_.empty());

    // Remove dangling references to stream delegates.
    stream_delegates_.clear();
  }
}

// Test that the stream can only be opened once.
TEST_F(SCKAudioInputStreamTest, DoubleOpenStart) {
  if (@available(macOS 13.0, *)) {
    SetUpShareableContentMock(^(NSInvocation* invocation) {
      ShareableContentSuccess(invocation);
    });

    SCKAudioInputStream* stream = CreateAudioInputStream();

    EXPECT_EQ(stream->Open(), AudioInputStream::OpenOutcome::kSuccess);
    EXPECT_EQ(stream->Open(), AudioInputStream::OpenOutcome::kAlreadyOpen);

    MockAudioInputCallback sink;
    stream->Start(&sink);
    stream->Start(&sink);
    EXPECT_EQ(playing_stream_count_, 1);

    stream->Stop();
    stream->Close();
    EXPECT_TRUE(stream_outputs_.empty());

    // Remove dangling references to stream delegates.
    stream_delegates_.clear();
  }
}

// Test that Open() fails if shareable content enumeration times out.
TEST_F(SCKAudioInputStreamTest, OpenTimeout) {
  if (@available(macOS 13.0, *)) {
    SetUpShareableContentMock(^(NSInvocation* invocation){
        // Don't invoke the handler.
    });

    SCKAudioInputStream* stream = CreateAudioInputStream();

    EXPECT_EQ(stream->Open(), AudioInputStream::OpenOutcome::kFailed);
    stream->Close();

    EXPECT_TRUE(stream_outputs_.empty());
    // Remove dangling references to stream delegates.
    stream_delegates_.clear();
  }
}

// Test Open() with system screen capture permissions denied.
TEST_F(SCKAudioInputStreamTest, ScreenCapturePermissionsDenied) {
  if (@available(macOS 13.0, *)) {
    SetUpShareableContentMock(^(NSInvocation* invocation) {
      void (^handler)(SCShareableContent* _Nullable, NSError* _Nullable);
      [invocation getArgument:&handler atIndex:2];

      // Error reported by the API in case screen capture permissions
      // haven't been granted.
      NSError* error = [NSError errorWithDomain:SCStreamErrorDomain
                                           code:SCStreamErrorUserDeclined
                                       userInfo:nil];
      handler(nil, error);
    });

    SCKAudioInputStream* stream = CreateAudioInputStream();

    EXPECT_EQ(stream->Open(),
              AudioInputStream::OpenOutcome::kFailedSystemPermissions);
    stream->Close();

    EXPECT_TRUE(stream_outputs_.empty());
    // Remove dangling references to stream delegates.
    stream_delegates_.clear();
  }
}

// Test that no samples and errors are received by the callbacks after the
// stream is stopped.
TEST_F(SCKAudioInputStreamTest, NoStreamSamplesAfterStop) {
  if (@available(macOS 13.0, *)) {
    SetUpShareableContentMock(^(NSInvocation* invocation) {
      ShareableContentSuccess(invocation);
    });

    SCKAudioInputStream* stream = CreateAudioInputStream();

    EXPECT_EQ(stream->Open(), AudioInputStream::OpenOutcome::kSuccess);

    MockAudioInputCallback sink;
    EXPECT_CALL(sink, OnData(_, _, _, _)).Times(0);

    stream->Start(&sink);
    stream->Stop();
    SendAudioSample();
    stream->Close();

    EXPECT_TRUE(stream_outputs_.empty());
    // Remove dangling references to stream delegates.
    stream_delegates_.clear();
  }
}

TEST_F(SCKAudioInputStreamTest, CaptureSamples) {
  if (@available(macOS 13.0, *)) {
    SetUpShareableContentMock(^(NSInvocation* invocation) {
      ShareableContentSuccess(invocation);
    });

    SCKAudioInputStream* stream = CreateAudioInputStream();

    EXPECT_EQ(stream->Open(), AudioInputStream::OpenOutcome::kSuccess);

    FakeAudioInputCallback sink;
    stream->Start(&sink);

    // Buffer must be 16-bit aligned.
    alignas(16) std::array<float, 2 * kFramesPerBuffer> buffer;
    for (size_t i = 0; i < buffer.size(); i++) {
      buffer[i] = i;
    }

    SendAudioSample(buffer);

    stream->Stop();
    stream->Close();

    // Verify sample data matches.
    EXPECT_EQ(sink.channel_data().size(), buffer.size());
    for (size_t i = 0; i < buffer.size(); i++) {
      EXPECT_EQ(sink.channel_data()[i], buffer[i]);
    }

    EXPECT_TRUE(stream_outputs_.empty());
    // Remove dangling references to stream delegates.
    stream_delegates_.clear();
  }
}

TEST_F(SCKAudioInputStreamTest, ReportErrorToClient) {
  if (@available(macOS 13.0, *)) {
    SetUpShareableContentMock(^(NSInvocation* invocation) {
      ShareableContentSuccess(invocation);
    });

    SCKAudioInputStream* stream = CreateAudioInputStream();

    EXPECT_EQ(stream->Open(), AudioInputStream::OpenOutcome::kSuccess);

    MockAudioInputCallback sink;
    EXPECT_CALL(sink, OnError()).Times(1);

    stream->Start(&sink);
    SendError();
    stream->Stop();
    stream->Close();

    EXPECT_TRUE(stream_outputs_.empty());
    // Remove dangling references to stream delegates.
    stream_delegates_.clear();
  }
}

}  // namespace media
