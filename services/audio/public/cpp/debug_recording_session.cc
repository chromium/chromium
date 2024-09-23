// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/debug_recording_session.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "media/audio/audio_debug_recording_manager.h"

namespace audio {

namespace {

#if BUILDFLAG(IS_WIN)
#define NumberToStringType base::NumberToWString
#else
#define NumberToStringType base::NumberToString
#endif

const base::FilePath::CharType* StreamTypeToStringType(
    media::AudioDebugRecordingStreamType stream_type) {
  switch (stream_type) {
    case media::AudioDebugRecordingStreamType::kInput:
      return FILE_PATH_LITERAL("input");
    case media::AudioDebugRecordingStreamType::kOutput:
      return FILE_PATH_LITERAL("output");
  }
  NOTREACHED_IN_MIGRATION();
  return FILE_PATH_LITERAL("output");
}

// Asynchronously creates a file and passes it to |reply_callback|.
void CreateFile(base::FilePath file_path,
                base::OnceCallback<void(base::File)> reply_callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          [](const base::FilePath& file_name) {
            return base::File(file_name, base::File::FLAG_CREATE_ALWAYS |
                                             base::File::FLAG_WRITE);
          },
          file_path),
      std::move(reply_callback));
}

}  // namespace

DebugRecordingSession::DebugRecordingFileProvider::DebugRecordingFileProvider(
    mojo::PendingReceiver<mojom::DebugRecordingFileProvider> receiver,
    const base::FilePath& file_name_base)
    : receiver_(this, std::move(receiver)), file_name_base_(file_name_base) {}

DebugRecordingSession::DebugRecordingFileProvider::
    ~DebugRecordingFileProvider() = default;

void DebugRecordingSession::DebugRecordingFileProvider::CreateWavFile(
    media::AudioDebugRecordingStreamType stream_type,
    uint32_t id,
    CreateWavFileCallback reply_callback) {
  CreateFile(file_name_base_.AddExtension(StreamTypeToStringType(stream_type))
                 .AddExtension(NumberToStringType(id))
                 .AddExtension(FILE_PATH_LITERAL("wav")),
             std::move(reply_callback));
}

void DebugRecordingSession::DebugRecordingFileProvider::CreateAecdumpFile(
    uint32_t id,
    CreateAecdumpFileCallback reply_callback) {
  CreateFile(file_name_base_.AddExtension(NumberToStringType(id))
                 .AddExtension(FILE_PATH_LITERAL("aecdump")),
             std::move(reply_callback));
}

DebugRecordingSession::DebugRecordingSession(
    const base::FilePath& file_name_base,
    mojo::PendingRemote<mojom::DebugRecording> debug_recording) {
  debug_recording_.Bind(std::move(debug_recording));

  mojo::PendingRemote<mojom::DebugRecordingFileProvider> remote_file_provider;
  file_provider_ = std::make_unique<DebugRecordingFileProvider>(
      remote_file_provider.InitWithNewPipeAndPassReceiver(), file_name_base);
  debug_recording_->Enable(std::move(remote_file_provider));
}

DebugRecordingSession::~DebugRecordingSession() {}

}  // namespace audio
