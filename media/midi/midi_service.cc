// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/midi/midi_service.h"

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "media/midi/midi_manager.h"
#include "media/midi/midi_switches.h"
#include "media/midi/task_service.h"

namespace midi {

std::unique_ptr<MidiManager> MidiService::ManagerFactory::Create(
    MidiService* service) {
  return std::unique_ptr<MidiManager>(MidiManager::Create(service));
}

// static
base::TimeDelta MidiService::TimestampToTimeDeltaDelay(
    base::TimeTicks timestamp) {
  if (timestamp.is_null())
    return base::TimeDelta();
  return std::max(timestamp - base::TimeTicks::Now(), base::TimeDelta());
}

MidiService::MidiService() : MidiService(std::make_unique<ManagerFactory>()) {}

MidiService::MidiService(std::unique_ptr<ManagerFactory> factory)
    : manager_factory_(std::move(factory)),
      task_service_(std::make_unique<TaskService>()) {}

MidiService::~MidiService() {
  base::AutoLock lock(lock_);
  DCHECK(!manager_);
  base::AutoLock threads_lock(threads_lock_);
  threads_.clear();
}

void MidiService::Shutdown() {
  base::AutoLock lock(lock_);
  if (manager_) {
    manager_->EndAllSessions();
    DCHECK(manager_destructor_runner_);
    manager_destructor_runner_->DeleteSoon(FROM_HERE, std::move(manager_));
    manager_destructor_runner_ = nullptr;
  }
}

void MidiService::StartSession(MidiManagerClient* client) {
  base::AutoLock lock(lock_);
  if (!manager_) {
    manager_ = manager_factory_->Create(this);
    DCHECK(!manager_destructor_runner_);
    manager_destructor_runner_ =
        base::SingleThreadTaskRunner::GetCurrentDefault();
  }
  manager_->StartSession(client);
}

bool MidiService::EndSession(MidiManagerClient* client) {
  base::AutoLock lock(lock_);

  // |client| does not seem to be valid.
  if (!manager_ || !manager_->EndSession(client))
    return false;

// Do not destruct MidiManager on macOS to avoid a Core MIDI issue that
// MIDIClientCreate starts failing with the OSStatus -50 after repeated calls
// of MIDIClientDispose. It rarely happens, but once it starts, it will never
// get back to be sane. See https://crbug.com/718140.
#if !BUILDFLAG(IS_MAC)
  if (!manager_->HasOpenSession()) {
    // MidiManager for each platform should be able to shutdown correctly even
    // if following destruction happens in the middle of StartInitialization().
    manager_.reset();
    DCHECK(manager_destructor_runner_);
    DCHECK(manager_destructor_runner_->BelongsToCurrentThread());
    manager_destructor_runner_ = nullptr;
  }
#endif
  return true;
}

void MidiService::DispatchSendMidiData(MidiManagerClient* client,
                                       uint32_t port_index,
                                       const std::vector<uint8_t>& data,
                                       base::TimeTicks timestamp) {
  base::AutoLock lock(lock_);

  // MidiService needs to consider invalid DispatchSendMidiData calls without
  // an open session that could be sent from a broken renderer.
  if (manager_)
    manager_->DispatchSendMidiData(client, port_index, data, timestamp);
}

scoped_refptr<base::SingleThreadTaskRunner> MidiService::GetTaskRunner(
    size_t runner_id) {
  base::AutoLock lock(threads_lock_);
  if (threads_.size() <= runner_id)
    threads_.resize(runner_id + 1);
  if (!threads_[runner_id]) {
    threads_[runner_id] = std::make_unique<base::Thread>(
        base::StringPrintf("MidiServiceThread(%zu)", runner_id));
#if BUILDFLAG(IS_WIN)
    threads_[runner_id]->init_com_with_mta(true);
#endif
    threads_[runner_id]->Start();
  }
  return threads_[runner_id]->task_runner();
}

}  // namespace midi
