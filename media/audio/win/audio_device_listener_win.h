// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_WIN_AUDIO_DEVICE_LISTENER_WIN_H_
#define MEDIA_AUDIO_WIN_AUDIO_DEVICE_LISTENER_WIN_H_

#include <MMDeviceAPI.h>
#include <wrl/client.h>

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "media/base/media_export.h"

namespace base {
class TickClock;
}

namespace media {

// IMMNotificationClient implementation for listening for default device changes
// and forwarding to AudioManagerWin so it can notify downstream clients.  Only
// output (eRender) device changes are supported currently.  Core Audio support
// is required to construct this object.  Must be constructed and destructed on
// a single COM initialized thread.
// TODO(dalecurtis, henrika): Support input device changes.
class MEDIA_EXPORT AudioDeviceListenerWin : public IMMNotificationClient {
 public:
  // The listener callback will be called from a system level multimedia thread,
  // thus the callee must be thread safe.  |listener_cb| is a permanent callback
  // and must outlive AudioDeviceListenerWin.
  explicit AudioDeviceListenerWin(base::RepeatingClosure listener_cb);

  AudioDeviceListenerWin(const AudioDeviceListenerWin&) = delete;
  AudioDeviceListenerWin& operator=(const AudioDeviceListenerWin&) = delete;

  virtual ~AudioDeviceListenerWin();

 private:
  friend class AudioDeviceListenerWinTest;

  // Minimum allowed time between device change notifications.
  static constexpr base::TimeDelta kDeviceChangeLimit = base::Milliseconds(250);

  // IMMNotificationClient implementation.
  IFACEMETHODIMP_(ULONG) AddRef() override;
  IFACEMETHODIMP_(ULONG) Release() override;
  IFACEMETHODIMP QueryInterface(REFIID iid, void** object) override;
  IFACEMETHODIMP OnPropertyValueChanged(LPCWSTR device_id,
                                        const PROPERTYKEY key) override;
  IFACEMETHODIMP OnDeviceAdded(LPCWSTR device_id) override;
  IFACEMETHODIMP OnDeviceRemoved(LPCWSTR device_id) override;
  IFACEMETHODIMP OnDeviceStateChanged(LPCWSTR device_id,
                                      DWORD new_state) override;
  IFACEMETHODIMP OnDefaultDeviceChanged(EDataFlow flow,
                                        ERole role,
                                        LPCWSTR new_default_device_id) override;

  const base::RepeatingClosure listener_cb_;
  Microsoft::WRL::ComPtr<IMMDeviceEnumerator> device_enumerator_;

  // Used to rate limit device change events.
  base::TimeTicks last_device_change_time_;
  std::string last_device_id_;

  // AudioDeviceListenerWin must be constructed and destructed on one thread.
  THREAD_CHECKER(thread_checker_);

  raw_ptr<const base::TickClock> tick_clock_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_WIN_AUDIO_DEVICE_LISTENER_WIN_H_
