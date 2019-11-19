// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_debug_recording_session_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "media/audio/audio_debug_recording_manager.h"
#include "media/audio/audio_manager.h"

namespace media {

// Posting AudioManager::Get() as unretained is safe because
// AudioManager::Shutdown() (called before AudioManager destruction) shuts down
// AudioManager thread.

// TODO(https://crbug.com/788657) remove this file after switching to mojo
// implementation.

namespace {

#if defined(OS_WIN)
#define NumberToStringType base::NumberToString16
#else
#define NumberToStringType base::NumberToString
#endif

bool StreamTypeToStringType(AudioDebugRecordingStreamType stream_type,
                            base::FilePath::StringType* out) {
  switch (stream_type) {
    case AudioDebugRecordingStreamType::kInput:
      *out = FILE_PATH_LITERAL("input");
      return true;
    case AudioDebugRecordingStreamType::kOutput:
      *out = FILE_PATH_LITERAL("output");
      return true;
  }
  NOTREACHED();
  return false;
}

void CreateWavFile(const base::FilePath& debug_recording_file_path,
                   AudioDebugRecordingStreamType stream_type,
                   uint32_t id,
                   base::OnceCallback<void(base::File)> reply_callback) {
  base::FilePath::StringType stream_type_str;
  if (!StreamTypeToStringType(stream_type, &stream_type_str)) {
    std::move(reply_callback).Run(base::File());
    return;
  }

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          [](const base::FilePath& file_name) {
            return base::File(file_name, base::File::FLAG_CREATE_ALWAYS |
                                             base::File::FLAG_WRITE);
          },
          debug_recording_file_path.AddExtension(stream_type_str)
              .AddExtension(NumberToStringType(id))
              .AddExtension(FILE_PATH_LITERAL("wav"))),
      std::move(reply_callback));
}

}  // namespace

AudioDebugRecordingSessionImpl::AudioDebugRecordingSessionImpl(
    const base::FilePath& debug_recording_file_path) {
  AudioManager* audio_manager = AudioManager::Get();
  if (audio_manager == nullptr)
    return;

  audio_manager->GetTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](AudioManager* manager,
             AudioDebugRecordingManager::CreateWavFileCallback
                 create_file_callback) {
            AudioDebugRecordingManager* debug_recording_manager =
                manager->GetAudioDebugRecordingManager();
            if (debug_recording_manager == nullptr)
              return;
            debug_recording_manager->EnableDebugRecording(
                std::move(create_file_callback));
          },
          base::Unretained(audio_manager),
          base::BindRepeating(&CreateWavFile, debug_recording_file_path)));
}

AudioDebugRecordingSessionImpl::~AudioDebugRecordingSessionImpl() {
  AudioManager* audio_manager = AudioManager::Get();
  if (audio_manager == nullptr)
    return;

  audio_manager->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](AudioManager* manager) {
                       AudioDebugRecordingManager* debug_recording_manager =
                           manager->GetAudioDebugRecordingManager();
                       if (debug_recording_manager == nullptr)
                         return;
                       debug_recording_manager->DisableDebugRecording();
                     },
                     base::Unretained(audio_manager)));
}

}  // namespace media
