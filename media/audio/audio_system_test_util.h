// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_SYSTEM_TEST_UTIL_H_
#define MEDIA_AUDIO_AUDIO_SYSTEM_TEST_UTIL_H_

#include <optional>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_system.h"
#include "media/audio/mock_audio_manager.h"
#include "media/base/audio_parameters.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// For tests only. Creates AudioSystem callbacks to be passed to AudioSystem
// methods. When AudioSystem calls such a callback, it verifies treading
// expectations and checks received parameters against expected values passed
// during its creation. After that it calls |on_cb_received| closure.
// Note AudioSystemCallbackExpectations object must outlive all the callbacks
// it produced, since they contain raw pointers to it.
class AudioSystemCallbackExpectations {
 public:
  AudioSystemCallbackExpectations() = default;

  AudioSystemCallbackExpectations(const AudioSystemCallbackExpectations&) =
      delete;
  AudioSystemCallbackExpectations& operator=(
      const AudioSystemCallbackExpectations&) = delete;

  AudioSystem::OnAudioParamsCallback GetAudioParamsCallback(
      const base::Location& location,
      base::OnceClosure on_cb_received,
      const std::optional<AudioParameters>& expected_params);

  AudioSystem::OnBoolCallback GetBoolCallback(const base::Location& location,
                                              base::OnceClosure on_cb_received,
                                              bool expected);

  AudioSystem::OnDeviceDescriptionsCallback GetDeviceDescriptionsCallback(
      const base::Location& location,
      base::OnceClosure on_cb_received,
      const AudioDeviceDescriptions& expected_descriptions);

  AudioSystem::OnInputDeviceInfoCallback GetInputDeviceInfoCallback(
      const base::Location& location,
      base::OnceClosure on_cb_received,
      const std::optional<AudioParameters>& expected_input,
      const std::optional<std::string>& expected_associated_device_id);

  AudioSystem::OnDeviceIdCallback GetDeviceIdCallback(
      const base::Location& location,
      base::OnceClosure on_cb_received,
      const std::optional<std::string>& expected_id);

 private:
  // Methods to verify correctness of received data.
  void OnAudioParams(const std::string& from_here,
                     base::OnceClosure on_cb_received,
                     const std::optional<AudioParameters>& expected,
                     const std::optional<AudioParameters>& received);

  void OnBool(const std::string& from_here,
              base::OnceClosure on_cb_received,
              bool expected,
              bool result);

  void OnDeviceDescriptions(
      const std::string& from_here,
      base::OnceClosure on_cb_received,
      const AudioDeviceDescriptions& expected_descriptions,
      AudioDeviceDescriptions descriptions);

  void OnInputDeviceInfo(
      const std::string& from_here,
      base::OnceClosure on_cb_received,
      const std::optional<AudioParameters>& expected_input,
      const std::optional<std::string>& expected_associated_device_id,
      const std::optional<AudioParameters>& input,
      const std::optional<std::string>& associated_device_id);

  void OnDeviceId(const std::string& from_here,
                  base::OnceClosure on_cb_received,
                  const std::optional<std::string>& expected_id,
                  const std::optional<std::string>& result_id);

  THREAD_CHECKER(thread_checker_);
};

// Template test case to test AudioSystem implementations.
template <class T>
class AudioSystemTestTemplate : public T {
 public:
  AudioSystemTestTemplate() {}

  AudioSystemTestTemplate(const AudioSystemTestTemplate&) = delete;
  AudioSystemTestTemplate& operator=(const AudioSystemTestTemplate&) = delete;

  ~AudioSystemTestTemplate() override {}

  void SetUp() override {
    T::SetUp();
    input_params_ = AudioParameters(AudioParameters::AUDIO_PCM_LINEAR,
                                    ChannelLayoutConfig::Mono(),
                                    AudioParameters::kTelephoneSampleRate,
                                    AudioParameters::kTelephoneSampleRate / 10);
    output_params_ = AudioParameters(
        AudioParameters::AUDIO_PCM_LINEAR, ChannelLayoutConfig::Mono(),
        AudioParameters::kTelephoneSampleRate,
        AudioParameters::kTelephoneSampleRate / 20);
    default_output_params_ = AudioParameters(
        AudioParameters::AUDIO_PCM_LINEAR, ChannelLayoutConfig::Mono(),
        AudioParameters::kTelephoneSampleRate,
        AudioParameters::kTelephoneSampleRate / 30);
    audio_manager()->SetInputStreamParameters(input_params_);
    audio_manager()->SetOutputStreamParameters(output_params_);
    audio_manager()->SetDefaultOutputStreamParameters(default_output_params_);

    auto get_device_descriptions = [](const AudioDeviceDescriptions* source,
                                      AudioDeviceDescriptions* destination) {
      destination->insert(destination->end(), source->begin(), source->end());
    };

    audio_manager()->SetInputDeviceDescriptionsCallback(
        base::BindRepeating(get_device_descriptions,
                            base::Unretained(&input_device_descriptions_)));
    audio_manager()->SetOutputDeviceDescriptionsCallback(
        base::BindRepeating(get_device_descriptions,
                            base::Unretained(&output_device_descriptions_)));
  }

 protected:
  MockAudioManager* audio_manager() { return T::audio_manager(); }
  AudioSystem* audio_system() { return T::audio_system(); }

  AudioSystemCallbackExpectations expectations_;
  AudioParameters input_params_;
  AudioParameters output_params_;
  AudioParameters default_output_params_;
  AudioDeviceDescriptions input_device_descriptions_;
  AudioDeviceDescriptions output_device_descriptions_;
};

TYPED_TEST_SUITE_P(AudioSystemTestTemplate);

TYPED_TEST_P(AudioSystemTestTemplate, GetInputStreamParametersNormal) {
  base::RunLoop wait_loop;
  this->audio_system()->GetInputStreamParameters(
      AudioDeviceDescription::kDefaultDeviceId,
      this->expectations_.GetAudioParamsCallback(
          FROM_HERE, wait_loop.QuitClosure(), this->input_params_));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate, GetInputStreamParametersNoDevice) {
  this->audio_manager()->SetHasInputDevices(false);

  base::RunLoop wait_loop;
  this->audio_system()->GetInputStreamParameters(
      AudioDeviceDescription::kDefaultDeviceId,
      this->expectations_.GetAudioParamsCallback(
          FROM_HERE, wait_loop.QuitClosure(),
          std::optional<AudioParameters>()));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate, GetOutputStreamParameters) {
  base::RunLoop wait_loop;
  this->audio_system()->GetOutputStreamParameters(
      "non-default-device-id",
      this->expectations_.GetAudioParamsCallback(
          FROM_HERE, wait_loop.QuitClosure(), this->output_params_));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate,
             GetOutputStreamParametersForDefaultDeviceNoDevices) {
  this->audio_manager()->SetHasOutputDevices(false);
  base::RunLoop wait_loop;
  this->audio_system()->GetOutputStreamParameters(
      AudioDeviceDescription::kDefaultDeviceId,
      this->expectations_.GetAudioParamsCallback(
          FROM_HERE, wait_loop.QuitClosure(),
          std::optional<AudioParameters>()));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate,
             GetOutputStreamParametersForNonDefaultDeviceNoDevices) {
  this->audio_manager()->SetHasOutputDevices(false);
  base::RunLoop wait_loop;
  this->audio_system()->GetOutputStreamParameters(
      "non-default-device-id", this->expectations_.GetAudioParamsCallback(
                                   FROM_HERE, wait_loop.QuitClosure(),
                                   std::optional<AudioParameters>()));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate, HasInputDevices) {
  base::RunLoop wait_loop;
  this->audio_system()->HasInputDevices(this->expectations_.GetBoolCallback(
      FROM_HERE, wait_loop.QuitClosure(), true));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate, HasNoInputDevices) {
  this->audio_manager()->SetHasInputDevices(false);
  base::RunLoop wait_loop;
  this->audio_system()->HasInputDevices(this->expectations_.GetBoolCallback(
      FROM_HERE, wait_loop.QuitClosure(), false));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate, HasOutputDevices) {
  base::RunLoop wait_loop;
  this->audio_system()->HasOutputDevices(this->expectations_.GetBoolCallback(
      FROM_HERE, wait_loop.QuitClosure(), true));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate, HasNoOutputDevices) {
  this->audio_manager()->SetHasOutputDevices(false);
  base::RunLoop wait_loop;
  this->audio_system()->HasOutputDevices(this->expectations_.GetBoolCallback(
      FROM_HERE, wait_loop.QuitClosure(), false));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate,
             GetInputDeviceDescriptionsNoInputDevices) {
  this->output_device_descriptions_.emplace_back(
      "output_device_name", "output_device_id", "group_id");
  EXPECT_EQ(0, static_cast<int>(this->input_device_descriptions_.size()));
  EXPECT_EQ(1, static_cast<int>(this->output_device_descriptions_.size()));

  base::RunLoop wait_loop;
  this->audio_system()->GetDeviceDescriptions(
      true, this->expectations_.GetDeviceDescriptionsCallback(
                FROM_HERE, wait_loop.QuitClosure(),
                this->input_device_descriptions_));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate, GetInputDeviceDescriptions) {
  this->output_device_descriptions_.emplace_back(
      "output_device_name", "output_device_id", "group_id");
  this->input_device_descriptions_.emplace_back(
      "input_device_name1", "input_device_id1", "group_id1");
  this->input_device_descriptions_.emplace_back(
      "input_device_name2", "input_device_id2", "group_id2");
  EXPECT_EQ(2, static_cast<int>(this->input_device_descriptions_.size()));
  EXPECT_EQ(1, static_cast<int>(this->output_device_descriptions_.size()));

  base::RunLoop wait_loop;
  this->audio_system()->GetDeviceDescriptions(
      true, this->expectations_.GetDeviceDescriptionsCallback(
                FROM_HERE, wait_loop.QuitClosure(),
                this->input_device_descriptions_));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate,
             GetOutputDeviceDescriptionsNoInputDevices) {
  this->input_device_descriptions_.emplace_back("input_device_name",
                                                "input_device_id", "group_id");
  EXPECT_EQ(0, static_cast<int>(this->output_device_descriptions_.size()));
  EXPECT_EQ(1, static_cast<int>(this->input_device_descriptions_.size()));

  base::RunLoop wait_loop;
  this->audio_system()->GetDeviceDescriptions(
      false, this->expectations_.GetDeviceDescriptionsCallback(
                 FROM_HERE, wait_loop.QuitClosure(),
                 this->output_device_descriptions_));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate, GetOutputDeviceDescriptions) {
  this->input_device_descriptions_.emplace_back("input_device_name",
                                                "input_device_id", "group_id");
  this->output_device_descriptions_.emplace_back(
      "output_device_name1", "output_device_id1", "group_id1");
  this->output_device_descriptions_.emplace_back(
      "output_device_name2", "output_device_id2", "group_id2");
  EXPECT_EQ(2, static_cast<int>(this->output_device_descriptions_.size()));
  EXPECT_EQ(1, static_cast<int>(this->input_device_descriptions_.size()));

  base::RunLoop wait_loop;
  this->audio_system()->GetDeviceDescriptions(
      false, this->expectations_.GetDeviceDescriptionsCallback(
                 FROM_HERE, wait_loop.QuitClosure(),
                 this->output_device_descriptions_));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate, GetAssociatedOutputDeviceID) {
  const std::string associated_id("associated_id");
  this->audio_manager()->SetAssociatedOutputDeviceIDCallback(
      base::BindRepeating([](const std::string& result, const std::string&)
                              -> std::string { return result; },
                          associated_id));

  base::RunLoop wait_loop;
  this->audio_system()->GetAssociatedOutputDeviceID(
      std::string(), this->expectations_.GetDeviceIdCallback(
                         FROM_HERE, wait_loop.QuitClosure(), associated_id));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate, GetInputDeviceInfoNoAssociation) {
  base::RunLoop wait_loop;
  this->audio_system()->GetInputDeviceInfo(
      "non-default-device-id",
      this->expectations_.GetInputDeviceInfoCallback(
          FROM_HERE, wait_loop.QuitClosure(), this->input_params_,
          std::optional<std::string>()));
  wait_loop.Run();
}

TYPED_TEST_P(AudioSystemTestTemplate, GetInputDeviceInfoWithAssociation) {
  const std::string associated_id("associated_id");
  this->audio_manager()->SetAssociatedOutputDeviceIDCallback(
      base::BindRepeating([](const std::string& result, const std::string&)
                              -> std::string { return result; },
                          associated_id));

  base::RunLoop wait_loop;
  this->audio_system()->GetInputDeviceInfo(
      "non-default-device-id", this->expectations_.GetInputDeviceInfoCallback(
                                   FROM_HERE, wait_loop.QuitClosure(),
                                   this->input_params_, associated_id));
  wait_loop.Run();
}

REGISTER_TYPED_TEST_SUITE_P(
    AudioSystemTestTemplate,
    GetInputStreamParametersNormal,
    GetInputStreamParametersNoDevice,
    GetOutputStreamParameters,
    GetOutputStreamParametersForDefaultDeviceNoDevices,
    GetOutputStreamParametersForNonDefaultDeviceNoDevices,
    HasInputDevices,
    HasNoInputDevices,
    HasOutputDevices,
    HasNoOutputDevices,
    GetInputDeviceDescriptionsNoInputDevices,
    GetInputDeviceDescriptions,
    GetOutputDeviceDescriptionsNoInputDevices,
    GetOutputDeviceDescriptions,
    GetAssociatedOutputDeviceID,
    GetInputDeviceInfoNoAssociation,
    GetInputDeviceInfoWithAssociation);

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_SYSTEM_TEST_UTIL_H_
