// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tool reads an input WAV file, processes it using the voice isolation
// feature to remove background noise, and writes the cleaned audio to an
// output WAV file. The output file is int16. This is intended for manual
// testing and debugging of the VoiceIsolation component with different audio
// samples and ML models.

#include <iostream>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/logging/logging_settings.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "components/optimization_guide/core/tflite_op_resolver.h"
#include "media/audio/audio_debug_file_writer.h"
#include "media/audio/wav_audio_handler.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_sample_types.h"
#include "media/webrtc/voice_isolation/voice_isolation.h"
#include "third_party/tflite/src/tensorflow/lite/model_builder.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_STDERR;
  logging::InitLogging(settings);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "voice_isolation_wav_tool");

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (!command_line.HasSwitch("in_file") ||
      !command_line.HasSwitch("out_file") || !command_line.HasSwitch("model")) {
    std::cerr << "Usage: " << argv[0]
              << " --in_file=<input.wav> --out_file=<output.wav> "
                 "--model=<model.tflite> --repeat=1"
              << std::endl;
    return 1;
  }

  const base::FilePath in_path = command_line.GetSwitchValuePath("in_file");
  const base::FilePath out_path = command_line.GetSwitchValuePath("out_file");
  const base::FilePath model_path = command_line.GetSwitchValuePath("model");

  int repetitions = 1;
  constexpr std::string repeat_flag = "repeat";
  if (command_line.HasSwitch(repeat_flag)) {
    std::string repetitions_value =
        command_line.GetSwitchValueASCII(repeat_flag);
    if (!base::StringToInt(repetitions_value, &repetitions)) {
      std::cerr << "Can't parse repeat value " << repetitions_value
                << std::endl;
    }
  }

  // Check input file.
  std::string wav_data;
  if (!base::ReadFileToString(in_path, &wav_data)) {
    std::cerr << "Failed to read input file: " << in_path.value() << std::endl;
    return 1;
  }

  auto input_handler =
      media::WavAudioHandler::Create(base::as_byte_span(wav_data));
  if (!input_handler) {
    std::cerr << "Failed to parse WAV file." << std::endl;
    return 1;
  }

  constexpr int kNumChannels = 1;
  if (input_handler->GetNumChannels() != kNumChannels) {
    std::cerr << "Input file must have 1 channel." << std::endl;
    return 1;
  }
  int input_sample_rate = input_handler->GetSampleRate();

  // Prepare model and voice isolation.
  auto model =
      tflite::FlatBufferModel::BuildFromFile(model_path.AsUTF8Unsafe().c_str());
  if (!model) {
    std::cerr << "Failed to load model from path: " << model_path << std::endl;
    return 1;
  }
  const int input_frame_size = input_sample_rate / 100;

  media::AudioParameters audio_params(
      media::AudioParameters::Format::AUDIO_PCM_LINEAR,
      media::ChannelLayoutConfig::Mono(), input_sample_rate, input_frame_size);

  std::unique_ptr<media::VoiceIsolation> voice_isolation =
      media::VoiceIsolation::Create(model.get(), audio_params);
  if (!voice_isolation) {
    std::cerr << "Failed to create VoiceIsolation" << std::endl;
    return 1;
  }

  // Prepare output file.
  base::File out_file(out_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  if (!out_file.IsValid()) {
    std::cerr << "Failed to open output file: " << out_path.value()
              << std::endl;
    return 1;
  }

  auto audio_debug_file_writer =
      media::AudioDebugFileWriter::Create(audio_params, std::move(out_file));

  // Consume audio in 10ms chunks.
  CHECK_EQ(input_sample_rate % 100, 0);
  std::unique_ptr<media::AudioBus> audio_bus =
      media::AudioBus::Create(kNumChannels, input_frame_size);

  for (int r = 0; r < repetitions; ++r) {
    size_t frames_written = input_frame_size;
    while (frames_written == static_cast<size_t>(audio_bus->frames())) {
      if (!input_handler->CopyTo(audio_bus.get(), &frames_written)) {
        std::cerr << "Failed to copy audio data to AudioBus." << std::endl;
        return 1;
      }
      if (frames_written != static_cast<size_t>(input_frame_size)) {
        // Skipping incomplete frames.
        input_handler->Reset();
        break;
      }
      voice_isolation->ProcessAudio(*audio_bus, *audio_bus);
      audio_debug_file_writer->Write(*audio_bus);
    }
  }

  // AudioDebugFileWriter automatically writes the correct WAV header sizes on
  // destruction. Wait for the background task runner to flush the file writes.
  audio_debug_file_writer.reset();
  base::ThreadPoolInstance::Get()->Shutdown();
  return 0;
}
