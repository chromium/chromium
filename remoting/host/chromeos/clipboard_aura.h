// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_CLIPBOARD_AURA_H_
#define REMOTING_HOST_CHROMEOS_CLIPBOARD_AURA_H_

#include <stdint.h>

#include <memory>

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/host/clipboard.h"
#include "ui/base/clipboard/clipboard.h"

namespace remoting {

namespace protocol {
class ClipboardStub;
}  // namespace protocol

// On Chrome OS, the clipboard is managed by aura instead of the underlying
// native platform (e.g. x11, ozone, etc).
//
// This class (1) monitors the aura clipboard for changes and notifies the
// |client_clipboard|, and (2) provides an interface to inject clipboard event
// into aura.
//
// The public API of this class can be called in any thread as internally it
// always posts the call to the |ui_task_runner|.  On ChromeOS, that should
// be the UI thread of the browser process.
class ClipboardAura : public Clipboard {
 public:
  explicit ClipboardAura();

  ClipboardAura(const ClipboardAura&) = delete;
  ClipboardAura& operator=(const ClipboardAura&) = delete;

  ~ClipboardAura() override;

  // Clipboard interface.
  void Start(
      std::unique_ptr<protocol::ClipboardStub> client_clipboard) override;
  void InjectClipboardEvent(const protocol::ClipboardEvent& event) override;

  // Overrides the clipboard polling interval for unit test.
  void SetPollingIntervalForTesting(base::TimeDelta polling_interval);

 private:
  void CheckClipboardForChanges();

  base::ThreadChecker thread_checker_;
  std::unique_ptr<protocol::ClipboardStub> client_clipboard_;
  base::RepeatingTimer clipboard_polling_timer_;
  ui::ClipboardSequenceNumberToken current_change_token_;
  base::TimeDelta polling_interval_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_CLIPBOARD_AURA_H_
