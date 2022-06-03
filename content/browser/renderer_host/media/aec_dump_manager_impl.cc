// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/aec_dump_manager_impl.h"

#include "base/files/file.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "mojo/public/cpp/base/file_mojom_traits.h"

namespace content {
namespace {

constexpr char kAecDumpFileNameAddition[] = "aec_dump";

base::File CreateDumpFile(const base::FilePath& file_path) {
  return base::File(file_path,
                    base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
}

}  // namespace

AecDumpManagerImpl::AecDumpManagerImpl() = default;

AecDumpManagerImpl::~AecDumpManagerImpl() = default;

void AecDumpManagerImpl::AddReceiver(
    mojo::PendingReceiver<blink::mojom::AecDumpManager> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

void AecDumpManagerImpl::AutoStart() {
  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals->IsAudioDebugRecordingsEnabled())
    Start(webrtc_internals->GetAudioDebugRecordingsFilePath());
}

void AecDumpManagerImpl::Start(const base::FilePath& file_path) {
  for (auto& it : agents_)
    CreateFileAndStartDump(file_path, it.first);
}

void AecDumpManagerImpl::Stop() {
  for (auto& it : agents_)
    it.second->Stop();
}

void AecDumpManagerImpl::Add(
    mojo::PendingRemote<blink::mojom::AecDumpAgent> agent) {
  int id = ++id_counter_;

  agents_.emplace(std::make_pair(id, std::move(agent)));

  agents_[id].set_disconnect_handler(
      base::BindOnce(&AecDumpManagerImpl::OnAgentDisconnected,
                     weak_factory_.GetWeakPtr(), id));

  WebRTCInternals* webrtc_internals = WebRTCInternals::GetInstance();
  if (webrtc_internals->IsAudioDebugRecordingsEnabled()) {
    CreateFileAndStartDump(webrtc_internals->GetAudioDebugRecordingsFilePath(),
                           id);
  }
}

void AecDumpManagerImpl::CreateFileAndStartDump(const base::FilePath& file_path,
                                                int id) {
  base::FilePath file_path_extended =
      file_path.AddExtensionASCII(base::NumberToString(pid_))
          .AddExtensionASCII(kAecDumpFileNameAddition)
          .AddExtensionASCII(base::NumberToString(id));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
       base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&CreateDumpFile, file_path_extended),
      base::BindOnce(&AecDumpManagerImpl::StartDump, weak_factory_.GetWeakPtr(),
                     id));
}

void AecDumpManagerImpl::StartDump(int id, base::File file) {
  if (!file.IsValid()) {
    VLOG(1) << "Could not open AEC dump file, error=" << file.error_details();
    return;
  }

  auto it = agents_.find(id);
  if (it == agents_.end()) {
    // Post the file close to avoid blocking the current thread.
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::LOWEST, base::MayBlock()},
        base::BindOnce([](base::File) {}, std::move(file)));
    return;
  }

  it->second->Start(std::move(file));
}

void AecDumpManagerImpl::OnAgentDisconnected(int id) {
  agents_.erase(id);
}

}  // namespace content
