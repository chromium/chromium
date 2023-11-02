// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_JANK_MONITOR_H_
#define CONTENT_PUBLIC_BROWSER_JANK_MONITOR_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

namespace content {
// This class monitors the responsiveness of the browser to notify the presence
// of janks to its observers. A jank is defined as a task or native event
// running for longer than a threshold on the UI or IO thread. An observer of
// this class is notified through the Observer interface on jank starts/stops so
// the observer can take actions (e.g. gather system-wide profile to capture the
// jank) *before* the janky task finishes execution. Notifications are sent on a
// dedicated sequence internal to this class so the observer needs to be careful
// with threading. For example, access to browser-related objects requires
// posting a task to the UI thread.
//
// Internally, a timer (bound to the monitor sequence) is used to perform
// periodic checks to decide the presence of janks. When a jank is detected, the
// monitor notifies its observers that a jank has started (through the
// Observer::OnJankStarted() method). The start of a jank is imprecise w.r.t.
// the jank threshold. When a janky task has finished execution, the monitor
// notifies the observers ASAP (through the Observer::OnJankStopped() method).
//
// Usage example:
//
// class Profiler : public Observer {
//  public:
//   void OnJankStarted() override; // Start the profiler.
//   void OnJankStopped() override; // Stop the profiler.
// }
// Profiler* profiler = ...;
//
// scoped_refptr<JankMonitor> monitor = JankMonitor::Create();
// monitor->SetUp();
// monitor->AddObserver(profiler);
//
// (Then start receiving notifications in Profiler::OnJankStarted() and
// Profiler::OnJankStopped()).
class CONTENT_EXPORT JankMonitor
    : public base::RefCountedThreadSafe<JankMonitor> {
 public:
  // Interface for observing janky tasks from the monitor. Note that the
  // callbacks are called *off* the UI thread. Post a task to the UI thread is
  // necessary if you need to access browser-related objects.
  class CONTENT_EXPORT Observer {
   public:
    virtual ~Observer();

    virtual void OnJankStarted() = 0;
    virtual void OnJankStopped() = 0;
  };

  static scoped_refptr<JankMonitor> Create();

  // AddObserver() and RemoveObserver() can be called on any sequence, but the
  // notifications only take place on the monitor sequence. Note: do *not* call
  // AddObserver() or RemoveObserver() synchronously in the observer callbacks,
  // or undefined behavior will result.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual void SetUp() = 0;
  virtual void Destroy() = 0;

 protected:
  friend class base::RefCountedThreadSafe<JankMonitor>;

  virtual ~JankMonitor();
};

}  // namespace content.

#endif  // CONTENT_PUBLIC_BROWSER_JANK_MONITOR_H_
