// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/base/audio_parameters.h"
#include "media/base/fake_audio_render_callback.h"
#include "media/base/media_util.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_audio_source_provider_client.h"
#include "third_party/blink/public/platform/webaudiosourceprovider_impl.h"

using ::testing::_;

namespace blink {

namespace {
const float kTestVolume = 0.25;
const int kTestSampleRate = 48000;
}  // namespace

class WebAudioSourceProviderImplTest : public testing::Test,
                                       public WebAudioSourceProviderClient {
 public:
  WebAudioSourceProviderImplTest()
      : params_(media::AudioParameters::AUDIO_PCM_LINEAR,
                media::CHANNEL_LAYOUT_STEREO,
                kTestSampleRate,
                64),
        fake_callback_(0.1, kTestSampleRate),
        mock_sink_(new media::MockAudioRendererSink()),
        wasp_impl_(new WebAudioSourceProviderImpl(mock_sink_, &media_log_)) {}

  virtual ~WebAudioSourceProviderImplTest() = default;

  void CallAllSinkMethodsAndVerify(bool verify) {
    testing::InSequence s;

    EXPECT_CALL(*mock_sink_, Start()).Times(verify);
    wasp_impl_->Start();

    EXPECT_CALL(*mock_sink_, Play()).Times(verify);
    wasp_impl_->Play();

    EXPECT_CALL(*mock_sink_, Pause()).Times(verify);
    wasp_impl_->Pause();

    EXPECT_CALL(*mock_sink_, SetVolume(kTestVolume)).Times(verify);
    wasp_impl_->SetVolume(kTestVolume);

    EXPECT_CALL(*mock_sink_, Stop()).Times(verify);
    wasp_impl_->Stop();

    testing::Mock::VerifyAndClear(mock_sink_.get());
  }

  void SetClient(WebAudioSourceProviderClient* client) {
    testing::InSequence s;

    if (client) {
      EXPECT_CALL(*mock_sink_, Stop());
      EXPECT_CALL(*this, SetFormat(params_.channels(), params_.sample_rate()));
    }
    wasp_impl_->SetClient(client);
    base::RunLoop().RunUntilIdle();

    testing::Mock::VerifyAndClear(mock_sink_.get());
    testing::Mock::VerifyAndClear(this);
  }

  bool CompareBusses(const media::AudioBus* bus1, const media::AudioBus* bus2) {
    EXPECT_EQ(bus1->channels(), bus2->channels());
    EXPECT_EQ(bus1->frames(), bus2->frames());
    for (int ch = 0; ch < bus1->channels(); ++ch) {
      if (memcmp(bus1->channel(ch), bus2->channel(ch),
                 sizeof(*bus1->channel(ch)) * bus1->frames()) != 0) {
        return false;
      }
    }
    return true;
  }

  // WebAudioSourceProviderClient implementation.
  MOCK_METHOD2(SetFormat, void(uint32_t numberOfChannels, float sampleRate));

  // CopyAudioCB. Added forwarder method due to GMock troubles with scoped_ptr.
  MOCK_METHOD3(DoCopyAudioCB,
               void(media::AudioBus*,
                    uint32_t frames_delayed,
                    int sample_rate));
  void OnAudioBus(std::unique_ptr<media::AudioBus> bus,
                  uint32_t frames_delayed,
                  int sample_rate) {
    DoCopyAudioCB(bus.get(), frames_delayed, sample_rate);
  }

  int Render(media::AudioBus* audio_bus) {
    return wasp_impl_->RenderForTesting(audio_bus);
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  media::AudioParameters params_;
  media::FakeAudioRenderCallback fake_callback_;
  media::NullMediaLog media_log_;
  scoped_refptr<media::MockAudioRendererSink> mock_sink_;
  scoped_refptr<WebAudioSourceProviderImpl> wasp_impl_;

  DISALLOW_COPY_AND_ASSIGN(WebAudioSourceProviderImplTest);
};

TEST_F(WebAudioSourceProviderImplTest, SetClientBeforeInitialize) {
  // setClient() with a nullptr client should do nothing if no client is set.
  wasp_impl_->SetClient(nullptr);

  // If |mock_sink_| is not null, it should be stopped during setClient(this).
  if (mock_sink_)
    EXPECT_CALL(*mock_sink_.get(), Stop());

  wasp_impl_->SetClient(this);
  base::RunLoop().RunUntilIdle();

  wasp_impl_->SetClient(nullptr);
  base::RunLoop().RunUntilIdle();

  wasp_impl_->SetClient(this);
  base::RunLoop().RunUntilIdle();

  // When Initialize() is called after setClient(), the params should propagate
  // to the client via setFormat() during the call.
  EXPECT_CALL(*this, SetFormat(params_.channels(), params_.sample_rate()));
  wasp_impl_->Initialize(params_, &fake_callback_);
  base::RunLoop().RunUntilIdle();

  // setClient() with the same client should do nothing.
  wasp_impl_->SetClient(this);
  base::RunLoop().RunUntilIdle();
}

// Verify AudioRendererSink functionality w/ and w/o a client.
TEST_F(WebAudioSourceProviderImplTest, SinkMethods) {
  wasp_impl_->Initialize(params_, &fake_callback_);

  // Without a client all WASP calls should fall through to the underlying sink.
  CallAllSinkMethodsAndVerify(true);

  // With a client no calls should reach the Stop()'d sink.  Also, setClient()
  // should propagate the params provided during Initialize() at call time.
  SetClient(this);
  CallAllSinkMethodsAndVerify(false);

  // Removing the client should cause WASP to revert to the underlying sink;
  // this shouldn't crash, but shouldn't do anything either.
  SetClient(nullptr);
  CallAllSinkMethodsAndVerify(false);
}

// Test the AudioRendererSink state machine and its effects on provideInput().
TEST_F(WebAudioSourceProviderImplTest, ProvideInput) {
  auto bus1 = media::AudioBus::Create(params_);
  auto bus2 = media::AudioBus::Create(params_);

  // Point the WebVector into memory owned by |bus1|.
  WebVector<float*> audio_data(static_cast<size_t>(bus1->channels()));
  for (size_t i = 0; i < audio_data.size(); ++i)
    audio_data[i] = bus1->channel(static_cast<int>(i));

  // Verify provideInput() works before Initialize() and returns silence.
  bus1->channel(0)[0] = 1;
  bus2->Zero();
  wasp_impl_->ProvideInput(audio_data, params_.frames_per_buffer());
  ASSERT_TRUE(CompareBusses(bus1.get(), bus2.get()));

  wasp_impl_->Initialize(params_, &fake_callback_);
  SetClient(this);

  // Verify provideInput() is muted prior to Start() and no calls to the render
  // callback have occurred.
  bus1->channel(0)[0] = 1;
  bus2->Zero();
  wasp_impl_->ProvideInput(audio_data, params_.frames_per_buffer());
  ASSERT_TRUE(CompareBusses(bus1.get(), bus2.get()));
  ASSERT_EQ(fake_callback_.last_delay(), base::TimeDelta::Max());

  wasp_impl_->Start();

  // Ditto for Play().
  bus1->channel(0)[0] = 1;
  wasp_impl_->ProvideInput(audio_data, params_.frames_per_buffer());
  ASSERT_TRUE(CompareBusses(bus1.get(), bus2.get()));
  ASSERT_EQ(fake_callback_.last_delay(), base::TimeDelta::Max());

  wasp_impl_->Play();

  // Now we should get real audio data.
  wasp_impl_->ProvideInput(audio_data, params_.frames_per_buffer());
  ASSERT_FALSE(CompareBusses(bus1.get(), bus2.get()));

  // Ensure volume adjustment is working.
  fake_callback_.reset();
  fake_callback_.Render(base::TimeDelta(), base::TimeTicks::Now(), 0,
                        bus2.get());
  bus2->Scale(kTestVolume);

  fake_callback_.reset();
  wasp_impl_->SetVolume(kTestVolume);
  wasp_impl_->ProvideInput(audio_data, params_.frames_per_buffer());
  ASSERT_TRUE(CompareBusses(bus1.get(), bus2.get()));

  // Pause should return to silence.
  wasp_impl_->Pause();
  bus1->channel(0)[0] = 1;
  bus2->Zero();
  wasp_impl_->ProvideInput(audio_data, params_.frames_per_buffer());
  ASSERT_TRUE(CompareBusses(bus1.get(), bus2.get()));

  // Ensure if a renderer properly fill silence for partial Render() calls by
  // configuring the fake callback to return half the data.  After these calls
  // bus1 is full of junk data, and bus2 is partially filled.
  wasp_impl_->SetVolume(1);
  fake_callback_.Render(base::TimeDelta(), base::TimeTicks::Now(), 0,
                        bus1.get());
  fake_callback_.reset();
  fake_callback_.Render(base::TimeDelta(), base::TimeTicks::Now(), 0,
                        bus2.get());
  bus2->ZeroFramesPartial(bus2->frames() / 2,
                          bus2->frames() - bus2->frames() / 2);
  fake_callback_.reset();
  fake_callback_.set_half_fill(true);
  wasp_impl_->Play();

  // Play should return real audio data again, but the last half should be zero.
  wasp_impl_->ProvideInput(audio_data, params_.frames_per_buffer());
  ASSERT_TRUE(CompareBusses(bus1.get(), bus2.get()));

  // Stop() should return silence.
  wasp_impl_->Stop();
  bus1->channel(0)[0] = 1;
  bus2->Zero();
  wasp_impl_->ProvideInput(audio_data, params_.frames_per_buffer());
  ASSERT_TRUE(CompareBusses(bus1.get(), bus2.get()));
}

// Verify CopyAudioCB is called if registered.
TEST_F(WebAudioSourceProviderImplTest, CopyAudioCB) {
  testing::InSequence s;
  wasp_impl_->Initialize(params_, &fake_callback_);
  wasp_impl_->SetCopyAudioCallback(base::Bind(
      &WebAudioSourceProviderImplTest::OnAudioBus, base::Unretained(this)));

  const auto bus1 = media::AudioBus::Create(params_);
  EXPECT_CALL(*this, DoCopyAudioCB(_, 0, params_.sample_rate())).Times(1);
  Render(bus1.get());

  wasp_impl_->ClearCopyAudioCallback();
  EXPECT_CALL(*this, DoCopyAudioCB(_, _, _)).Times(0);
  Render(bus1.get());

  testing::Mock::VerifyAndClear(mock_sink_.get());
}

}  // namespace blink
