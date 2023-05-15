// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_DISPLAY_ROTATION_OBSERVER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_DISPLAY_ROTATION_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

namespace media {

class DisplayRotationObserver {
 public:
  virtual void SetDisplayRotation(const display::Display& display) = 0;
};

// Registers itself as an observer at |display::Screen::GetScreen()| and
// forwards rotation change events to the given DisplayRotationObserver on the
// thread where ScreenObserverDelegate was created.
class ScreenObserverDelegate
    : public display::DisplayObserver,
      public base::RefCountedThreadSafe<ScreenObserverDelegate> {
 public:
  static scoped_refptr<ScreenObserverDelegate> Create(
      DisplayRotationObserver* observer,
      scoped_refptr<base::SingleThreadTaskRunner> display_task_runner);

  ScreenObserverDelegate() = delete;
  ScreenObserverDelegate(const ScreenObserverDelegate&) = delete;
  ScreenObserverDelegate& operator=(const ScreenObserverDelegate&) = delete;

  // The user must call RemoveObserver() to drop the reference to |observer_| in
  // ScreenObserverDelegate before deleting |observer_|.
  void RemoveObserver();

 private:
  friend class base::RefCountedThreadSafe<ScreenObserverDelegate>;

  ScreenObserverDelegate(
      DisplayRotationObserver* observer,
      scoped_refptr<base::SingleThreadTaskRunner> display_task_runner);
  ~ScreenObserverDelegate() override;

  // DisplayObserver implementations.
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  void AddObserverOnDisplayThread();
  void RemoveObserverOnDisplayThread();

  // Post the screen rotation change from the display thread to capture thread
  void SendDisplayRotation(const display::Display& display);
  void SendDisplayRotationOnCaptureThread(const display::Display& display);

  raw_ptr<DisplayRotationObserver, ExperimentalAsh> observer_;

  absl::optional<display::ScopedDisplayObserver> display_observer_;

  // The task runner where the calls to display::Display must be serialized on.
  const scoped_refptr<base::SingleThreadTaskRunner> display_task_runner_;
  // The task runner on which the ScreenObserverDelegate is created.
  const scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_DISPLAY_ROTATION_OBSERVER_H_
