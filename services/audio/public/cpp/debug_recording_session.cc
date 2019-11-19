// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/public/cpp/debug_recording_session.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "media/audio/audio_debug_recording_manager.h"
#include "services/audio/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace audio {

namespace {

#if defined(OS_WIN)
#define NumberToStringType base::NumberToString16
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
  NOTREACHED();
  return FILE_PATH_LITERAL("output");
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
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          [](const base::FilePath& file_name) {
            return base::File(file_name, base::File::FLAG_CREATE_ALWAYS |
                                             base::File::FLAG_WRITE);
          },
          file_name_base_.AddExtension(StreamTypeToStringType(stream_type))
              .AddExtension(NumberToStringType(id))
              .AddExtension(FILE_PATH_LITERAL("wav"))),
      std::move(reply_callback));
}

DebugRecordingSession::DebugRecordingSession(
    const base::FilePath& file_name_base,
    std::unique_ptr<service_manager::Connector> connector) {
  DCHECK(connector);

  mojo::PendingRemote<mojom::DebugRecordingFileProvider> remote_file_provider;
  file_provider_ = std::make_unique<DebugRecordingFileProvider>(
      remote_file_provider.InitWithNewPipeAndPassReceiver(), file_name_base);

  connector->Connect(audio::mojom::kServiceName,
                     debug_recording_.BindNewPipeAndPassReceiver());
  if (debug_recording_.is_bound())
    debug_recording_->Enable(std::move(remote_file_provider));
}

DebugRecordingSession::~DebugRecordingSession() {}

}  // namespace audio
