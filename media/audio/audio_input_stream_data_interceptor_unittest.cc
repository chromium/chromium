// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_input_stream_data_interceptor.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "media/audio/audio_debug_recording_helper.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_glitch_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

using testing::Return;
using testing::StrictMock;
using testing::Mock;

const double kMaxVolume = 0.1234;
const double kNewVolume = 0.2345;
const double kVolume = 0.3456;

class MockStream : public AudioInputStream {
 public:
  MockStream() = default;
  ~MockStream() override = default;
  MOCK_METHOD0(Open, AudioInputStream::OpenOutcome());
  MOCK_METHOD1(Start, void(AudioInputStream::AudioInputCallback*));
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Close, void());
  MOCK_METHOD0(GetMaxVolume, double());
  MOCK_METHOD1(SetVolume, void(double));
  MOCK_METHOD0(GetVolume, double());
  MOCK_METHOD1(SetAutomaticGainControl, bool(bool));
  MOCK_METHOD0(GetAutomaticGainControl, bool());
  MOCK_METHOD0(IsMuted, bool());
  MOCK_METHOD1(SetOutputDeviceForAec, void(const std::string&));
};

class MockDebugRecorder : public AudioDebugRecorder {
 public:
  MockDebugRecorder() = default;
  ~MockDebugRecorder() override = default;
  MOCK_METHOD1(OnData, void(const AudioBus* source));
};

class MockCallback : public AudioInputStream::AudioInputCallback {
 public:
  MockCallback() = default;
  ~MockCallback() override = default;

  MOCK_METHOD4(
      OnData,
      void(const AudioBus*, base::TimeTicks, double, const AudioGlitchInfo&));
  MOCK_METHOD0(OnError, void());
};

class MockDebugRecorderFactory {
 public:
  MockDebugRecorderFactory() = default;
  ~MockDebugRecorderFactory() { DCHECK(!prepared_recorder_); }

  std::unique_ptr<AudioDebugRecorder> CreateDebugRecorder() {
    DCHECK(prepared_recorder_);
    return std::move(prepared_recorder_);
  }

  void ExpectRecorderCreation(
      std::unique_ptr<AudioDebugRecorder> recorder_ptr) {
    DCHECK(!prepared_recorder_);
    prepared_recorder_ = std::move(recorder_ptr);
    DCHECK(prepared_recorder_);
  }

 private:
  std::unique_ptr<AudioDebugRecorder> prepared_recorder_;
};

void TestSetAutomaticGainControl(bool enable, bool agc_is_supported) {
  MockDebugRecorderFactory factory;
  StrictMock<MockStream> stream;
  AudioInputStream* interceptor = new AudioInputStreamDataInterceptor(
      base::BindRepeating(&MockDebugRecorderFactory::CreateDebugRecorder,
                          base::Unretained(&factory)),
      &stream);

  EXPECT_CALL(stream, SetAutomaticGainControl(enable))
      .WillOnce(Return(agc_is_supported));
  EXPECT_EQ(interceptor->SetAutomaticGainControl(enable), agc_is_supported);

  Mock::VerifyAndClearExpectations(&stream);
  EXPECT_CALL(stream, Close());
  interceptor->Close();
}

}  // namespace

TEST(AudioInputStreamDataInterceptorTest, Open) {
  MockDebugRecorderFactory factory;
  StrictMock<MockStream> stream;
  AudioInputStream* interceptor = new AudioInputStreamDataInterceptor(
      base::BindRepeating(&MockDebugRecorderFactory::CreateDebugRecorder,
                          base::Unretained(&factory)),
      &stream);
  EXPECT_CALL(stream, Open());
  interceptor->Open();

  Mock::VerifyAndClearExpectations(&stream);
  EXPECT_CALL(stream, Close());
  interceptor->Close();
}

TEST(AudioInputStreamDataInterceptorTest, Start) {
  MockDebugRecorderFactory factory;
  StrictMock<MockStream> stream;
  StrictMock<MockCallback> callback;
  auto* recorder = new StrictMock<MockDebugRecorder>();
  std::unique_ptr<AudioBus> audio_bus = AudioBus::Create(1, 1);
  AudioInputStreamDataInterceptor* interceptor =
      new AudioInputStreamDataInterceptor(
          base::BindRepeating(&MockDebugRecorderFactory::CreateDebugRecorder,
                              base::Unretained(&factory)),
          &stream);

  EXPECT_CALL(stream, Start(interceptor));
  factory.ExpectRecorderCreation(base::WrapUnique(recorder));
  interceptor->Start(&callback);

  Mock::VerifyAndClearExpectations(&stream);

  base::TimeTicks time = base::TimeTicks::Now();
  AudioGlitchInfo glitch_info{.duration = base::Milliseconds(123), .count = 5};

  // Audio data should be passed to both callback and recorder.
  EXPECT_CALL(callback, OnData(audio_bus.get(), time, kVolume, glitch_info));
  EXPECT_CALL(*recorder, OnData(audio_bus.get()));
  interceptor->OnData(audio_bus.get(), time, kVolume, glitch_info);

  Mock::VerifyAndClearExpectations(&callback);
  Mock::VerifyAndClearExpectations(recorder);

  // Errors should be propagated to the renderer
  EXPECT_CALL(callback, OnError());
  interceptor->OnError();

  Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(stream, Close());
  interceptor->Close();
}

TEST(AudioInputStreamDataInterceptorTest, Stop) {
  MockDebugRecorderFactory factory;
  StrictMock<MockStream> stream;
  AudioInputStream* interceptor = new AudioInputStreamDataInterceptor(
      base::BindRepeating(&MockDebugRecorderFactory::CreateDebugRecorder,
                          base::Unretained(&factory)),
      &stream);
  EXPECT_CALL(stream, Stop());
  interceptor->Stop();

  Mock::VerifyAndClearExpectations(&stream);
  EXPECT_CALL(stream, Close());
  interceptor->Close();
}

TEST(AudioInputStreamDataInterceptorTest, Close) {
  MockDebugRecorderFactory factory;
  StrictMock<MockStream> stream;
  AudioInputStream* interceptor = new AudioInputStreamDataInterceptor(
      base::BindRepeating(&MockDebugRecorderFactory::CreateDebugRecorder,
                          base::Unretained(&factory)),
      &stream);

  EXPECT_CALL(stream, Close());
  interceptor->Close();
}

TEST(AudioInputStreamDataInterceptorTest, GetMaxVolume) {
  MockDebugRecorderFactory factory;
  StrictMock<MockStream> stream;
  AudioInputStream* interceptor = new AudioInputStreamDataInterceptor(
      base::BindRepeating(&MockDebugRecorderFactory::CreateDebugRecorder,
                          base::Unretained(&factory)),
      &stream);

  EXPECT_CALL(stream, GetMaxVolume()).WillOnce(Return(kMaxVolume));
  EXPECT_EQ(interceptor->GetMaxVolume(), kMaxVolume);

  Mock::VerifyAndClearExpectations(&stream);
  EXPECT_CALL(stream, Close());
  interceptor->Close();
}

TEST(AudioInputStreamDataInterceptorTest, SetVolume) {
  MockDebugRecorderFactory factory;
  StrictMock<MockStream> stream;
  AudioInputStream* interceptor = new AudioInputStreamDataInterceptor(
      base::BindRepeating(&MockDebugRecorderFactory::CreateDebugRecorder,
                          base::Unretained(&factory)),
      &stream);

  EXPECT_CALL(stream, SetVolume(kNewVolume));
  interceptor->SetVolume(kNewVolume);

  Mock::VerifyAndClearExpectations(&stream);
  EXPECT_CALL(stream, Close());
  interceptor->Close();
}

TEST(AudioInputStreamDataInterceptorTest, GetVolume) {
  MockDebugRecorderFactory factory;
  StrictMock<MockStream> stream;
  AudioInputStream* interceptor = new AudioInputStreamDataInterceptor(
      base::BindRepeating(&MockDebugRecorderFactory::CreateDebugRecorder,
                          base::Unretained(&factory)),
      &stream);

  EXPECT_CALL(stream, GetVolume()).WillOnce(Return(kVolume));
  EXPECT_EQ(interceptor->GetVolume(), kVolume);

  Mock::VerifyAndClearExpectations(&stream);
  EXPECT_CALL(stream, Close());
  interceptor->Close();
}

TEST(AudioInputStreamDataInterceptorTest,
     SetAutomaticGainControlTrueWhenSupported) {
  TestSetAutomaticGainControl(true, true);
}

TEST(AudioInputStreamDataInterceptorTest,
     SetAutomaticGainControlFalseWhenSupported) {
  TestSetAutomaticGainControl(false, true);
}

TEST(AudioInputStreamDataInterceptorTest,
     SetAutomaticGainControlTrueWhenNotSupported) {
  TestSetAutomaticGainControl(true, false);
}

TEST(AudioInputStreamDataInterceptorTest,
     SetAutomaticGainControlFalseWhenNotSupported) {
  TestSetAutomaticGainControl(false, false);
}

TEST(AudioInputStreamDataInterceptorTest, GetAutomaticGainControl_True) {
  MockDebugRecorderFactory factory;
  StrictMock<MockStream> stream;
  AudioInputStream* interceptor = new AudioInputStreamDataInterceptor(
      base::BindRepeating(&MockDebugRecorderFactory::CreateDebugRecorder,
                          base::Unretained(&factory)),
      &stream);

  EXPECT_CALL(stream, GetAutomaticGainControl()).WillOnce(Return(true));
  EXPECT_EQ(interceptor->GetAutomaticGainControl(), true);

  Mock::VerifyAndClearExpectations(&stream);
  EXPECT_CALL(stream, Close());
  interceptor->Close();
}

TEST(AudioInputStreamDataInterceptorTest, GetAutomaticGainControl_False) {
  MockDebugRecorderFactory factory;
  StrictMock<MockStream> stream;
  AudioInputStream* interceptor = new AudioInputStreamDataInterceptor(
      base::BindRepeating(&MockDebugRecorderFactory::CreateDebugRecorder,
                          base::Unretained(&factory)),
      &stream);

  EXPECT_CALL(stream, GetAutomaticGainControl()).WillOnce(Return(false));
  EXPECT_EQ(interceptor->GetAutomaticGainControl(), false);

  Mock::VerifyAndClearExpectations(&stream);
  EXPECT_CALL(stream, Close());
  interceptor->Close();
}

TEST(AudioInputStreamDataInterceptorTest, IsMuted_True) {
  MockDebugRecorderFactory factory;
  StrictMock<MockStream> stream;
  AudioInputStream* interceptor = new AudioInputStreamDataInterceptor(
      base::BindRepeating(&MockDebugRecorderFactory::CreateDebugRecorder,
                          base::Unretained(&factory)),
      &stream);

  EXPECT_CALL(stream, IsMuted()).WillOnce(Return(true));
  EXPECT_EQ(interceptor->IsMuted(), true);

  Mock::VerifyAndClearExpectations(&stream);
  EXPECT_CALL(stream, Close());
  interceptor->Close();
}

TEST(AudioInputStreamDataInterceptorTest, IsMuted_False) {
  MockDebugRecorderFactory factory;
  StrictMock<MockStream> stream;
  AudioInputStream* interceptor = new AudioInputStreamDataInterceptor(
      base::BindRepeating(&MockDebugRecorderFactory::CreateDebugRecorder,
                          base::Unretained(&factory)),
      &stream);

  EXPECT_CALL(stream, IsMuted()).WillOnce(Return(false));
  EXPECT_EQ(interceptor->IsMuted(), false);

  Mock::VerifyAndClearExpectations(&stream);
  EXPECT_CALL(stream, Close());
  interceptor->Close();
}

}  // namespace media
