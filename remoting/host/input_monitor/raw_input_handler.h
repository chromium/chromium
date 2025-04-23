// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_INPUT_MONITOR_RAW_INPUT_HANDLER_H_
#define REMOTING_HOST_INPUT_MONITOR_RAW_INPUT_HANDLER_H_

#include <windows.h>

#include <cstdint>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/observer_list_threadsafe.h"
#include "base/threading/sequence_bound.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/event.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

// This interface provides the per-input type logic for registering and
// receiving raw input for the underlying input mode. Note that only one HWND
// per process can register for RawInput events so subclasses must ensure they
// do not register for the same event types.
class RawInputHandler {
 public:
  class Observer : base::CheckedObserver {
   public:
    virtual void OnMouseMove(const webrtc::DesktopVector&, ui::EventType) = 0;
    virtual void OnKeyboardInput(std::uint32_t usb_keycode) = 0;
    virtual void OnError() = 0;

   protected:
    Observer() = default;
    ~Observer() override = default;
  };
  using ObserverList = base::ObserverListThreadSafe<
      Observer,
      base::RemoveObserverPolicy::kAddingSequenceOnly>;

  RawInputHandler(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
                  std::uint16_t hid_usage);
  virtual ~RawInputHandler();

  // AddObserver()/RemoveObserver() are thread safe however `RemoveObserver()`
  // must be called on the same sequence that was used for `AddObserver()`.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  // Called for each raw input event.
  virtual void OnInputEvent(const RAWINPUT& event) = 0;

  // Notify observers when an event has been processed or an error has occurred.
  void NotifyMouseMove(const base::Location& from_here,
                       const webrtc::DesktopVector&);
  void NotifyKeyboardInput(const base::Location& from_here,
                           std::uint32_t usb_keycode);
  void NotifyError(const base::Location& from_here);

 private:
  void StartListening();
  void StopListening();

  class Core;
  base::SequenceBound<Core> core_;

  // Observers are bound to the sequence they registered on meaning they are
  // notified on that sequence and must unregister from that sequence as well.
  const scoped_refptr<ObserverList> observer_list_ =
      base::MakeRefCounted<ObserverList>(
          base::ObserverListPolicy::EXISTING_ONLY);

  base::WeakPtrFactory<RawInputHandler> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_INPUT_MONITOR_RAW_INPUT_HANDLER_H_
