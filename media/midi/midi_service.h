// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MIDI_MIDI_SERVICE_H_
#define MEDIA_MIDI_MIDI_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "media/midi/midi_export.h"
#include "media/midi/midi_manager.h"

namespace midi {

class TaskService;

// Manages MidiManager backends.  This class expects to be constructed and
// destructed on the browser main thread, but methods can be called on both
// the main thread and the I/O thread.
class MIDI_EXPORT MidiService final {
 public:
  class MIDI_EXPORT ManagerFactory {
   public:
    ManagerFactory() = default;

    ManagerFactory(const ManagerFactory&) = delete;
    ManagerFactory& operator=(const ManagerFactory&) = delete;

    virtual ~ManagerFactory() = default;
    virtual std::unique_ptr<MidiManager> Create(MidiService* service);
  };

  // Converts Web MIDI timestamp to base::TimeDelta delay for PostDelayedTask.
  static base::TimeDelta TimestampToTimeDeltaDelay(base::TimeTicks timestamp);

  MidiService();
  // Customized ManagerFactory can be specified in the constructor for testing.
  explicit MidiService(std::unique_ptr<ManagerFactory> factory);

  MidiService(const MidiService&) = delete;
  MidiService& operator=(const MidiService&) = delete;

  ~MidiService();

  // Called on the browser main thread to notify the I/O thread will stop and
  // the instance will be destructed on the main thread soon.
  void Shutdown();

  // A client calls StartSession() to receive and send MIDI data.
  void StartSession(MidiManagerClient* client);

  // A client calls EndSession() to stop receiving MIDI data.
  // Returns false if |client| did not start a session.
  bool EndSession(MidiManagerClient* client);

  // A client calls DispatchSendMidiData() to send MIDI data.
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            base::TimeTicks timestamp);

  // Returns a SingleThreadTaskRunner reference to serve MidiManager. Each
  // TaskRunner will be constructed on demand.
  // MidiManager that supports the dynamic instantiation feature will use this
  // method to post tasks that should not run on I/O. Since TaskRunners outlive
  // MidiManager, each task should ensure that MidiManager that posted the task
  // is still alive while accessing |this|. TaskRunners will be reused when
  // another MidiManager is instantiated.
  // TODO(toyoshim): Remove this interface.
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(size_t runner_id);

  // Obtains a TaskService that lives with MidiService.
  TaskService* task_service() { return task_service_.get(); }

 private:
  // ManagerFactory passed in the constructor.
  std::unique_ptr<ManagerFactory> manager_factory_;

  // Holds MidiManager instance. The MidiManager is constructed and destructed
  // on the I/O thread, and all MidiManager methods should be called on the I/O
  // thread.
  std::unique_ptr<MidiManager> manager_;

  // Holds TaskService instance.
  std::unique_ptr<TaskService> task_service_;

  // TaskRunner to destruct |manager_| on the right thread.
  scoped_refptr<base::SingleThreadTaskRunner> manager_destructor_runner_;

  // Protects all members above.
  base::Lock lock_;

  // Threads to host SingleThreadTaskRunners.
  std::vector<std::unique_ptr<base::Thread>> threads_;

  // Protects |threads_|.
  base::Lock threads_lock_;
};

}  // namespace midi

#endif  // MEDIA_MIDI_MIDI_SERVICE_H_
