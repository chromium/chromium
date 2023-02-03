// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Needed for BUILDFLAG(IS_WIN)
#include "build/build_config.h"

// Windows headers must come first.
#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

// Proceed with header includes in usual order.
#include "content/browser/scheduler/responsiveness/native_event_observer.h"

#include "ui/events/platform/platform_event_source.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "ui/aura/env.h"
#include "ui/events/event.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/task/current_thread.h"
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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
  did_run_event_callback_.Run(info.unique_id.get());
  events_being_processed_.pop_back();
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
void NativeEventObserver::RegisterObserver() {
  base::CurrentUIThread::Get()->AddMessagePumpObserver(this);
}
void NativeEventObserver::DeregisterObserver() {
  base::CurrentUIThread::Get()->RemoveMessagePumpObserver(this);
}
void NativeEventObserver::WillDispatchMSG(const MSG& msg) {
  will_run_event_callback_.Run(&msg);
}
void NativeEventObserver::DidDispatchMSG(const MSG& msg) {
  did_run_event_callback_.Run(&msg);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
void NativeEventObserver::RegisterObserver() {}
void NativeEventObserver::DeregisterObserver() {}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA)

}  // namespace responsiveness
}  // namespace content
