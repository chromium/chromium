// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Needed for defined(OS_WIN)
#include "build/build_config.h"

// Windows headers must come first.
#if defined(OS_WIN)
#include <windows.h>
#endif

// Proceed with header includes in usual order.
#include "content/browser/scheduler/responsiveness/native_event_observer.h"

#include "ui/events/platform/platform_event_source.h"

#if defined(OS_LINUX)
#include "ui/aura/env.h"
#include "ui/events/event.h"
#endif

#if defined(OS_WIN)
#include "base/message_loop/message_loop_current.h"
#endif

namespace content {
namespace responsiveness {

NativeEventObserver::NativeEventObserver(
    WillRunEventCallback will_run_event_callback,
    DidRunEventCallback did_run_event_callback)
    : will_run_event_callback_(will_run_event_callback),
      did_run_event_callback_(did_run_event_callback) {
  RegisterObserver();
}

NativeEventObserver::~NativeEventObserver() {
  DeregisterObserver();
}

#if defined(OS_LINUX)
void NativeEventObserver::RegisterObserver() {
  aura::Env::GetInstance()->AddWindowEventDispatcherObserver(this);
}
void NativeEventObserver::DeregisterObserver() {
  aura::Env::GetInstance()->RemoveWindowEventDispatcherObserver(this);
}

void NativeEventObserver::OnWindowEventDispatcherStartedProcessing(
    aura::WindowEventDispatcher* dispatcher,
    const ui::Event& event) {
  EventInfo info{&event};
  events_being_processed_.push_back(info);
  will_run_event_callback_.Run(&event);
}

void NativeEventObserver::OnWindowEventDispatcherFinishedProcessingEvent(
    aura::WindowEventDispatcher* dispatcher) {
  EventInfo& info = events_being_processed_.back();
  did_run_event_callback_.Run(info.unique_id);
  events_being_processed_.pop_back();
}
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
void NativeEventObserver::RegisterObserver() {
  base::MessageLoopCurrentForUI::Get()->AddMessagePumpObserver(this);
}
void NativeEventObserver::DeregisterObserver() {
  base::MessageLoopCurrentForUI::Get()->RemoveMessagePumpObserver(this);
}
void NativeEventObserver::WillDispatchMSG(const MSG& msg) {
  will_run_event_callback_.Run(&msg);
}
void NativeEventObserver::DidDispatchMSG(const MSG& msg) {
  did_run_event_callback_.Run(&msg);
}
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID) || defined(OS_FUCHSIA)
void NativeEventObserver::RegisterObserver() {}
void NativeEventObserver::DeregisterObserver() {}
#endif  // defined(OS_ANDROID) || defined(OS_FUCHSIA)

}  // namespace responsiveness
}  // namespace content
