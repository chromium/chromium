// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_NATIVE_EVENT_OBSERVER_H_
#define CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_NATIVE_EVENT_OBSERVER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if BUILDFLAG(IS_MAC)
#include "content/public/browser/native_event_processor_observer_mac.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "ui/aura/window_event_dispatcher_observer.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/message_loop/message_pump_win.h"
#endif

namespace content {
namespace responsiveness {

// This class must only be used from the UI thread.
//
// This class hooks itself into the native event processor for the platform and
// forwards will_run/did_run callbacks to Watcher. Native events are processed
// at different layers for each platform, so the interface for this class is
// necessarily messy.
//
// On macOS, the hook should be in -[BrowserCrApplication sendEvent:].
// On Linux, the hook should be in ui::PlatformEventSource::DispatchEvent.
// On Windows, the hook should be in MessagePumpForUI::ProcessMessageHelper.
// On Android, the hook should be in <TBD>.
class CONTENT_EXPORT NativeEventObserver
#if BUILDFLAG(IS_MAC)
    : public NativeEventProcessorObserver
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    : public aura::WindowEventDispatcherObserver
#elif BUILDFLAG(IS_WIN)
    : public base::MessagePumpForUI::Observer
#endif
{
 public:
  using WillRunEventCallback =
      base::RepeatingCallback<void(const void* opaque_identifier)>;
  using DidRunEventCallback =
      base::RepeatingCallback<void(const void* opaque_identifier)>;

  // The constructor will register the object as an observer of the native event
  // processor. The destructor will unregister the object.
  NativeEventObserver(WillRunEventCallback will_run_event_callback,
                      DidRunEventCallback did_run_event_callback);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  NativeEventObserver(const NativeEventObserver&) = delete;
  NativeEventObserver& operator=(const NativeEventObserver&) = delete;

  ~NativeEventObserver() override;
#else
  virtual ~NativeEventObserver();
#endif

 protected:
#if BUILDFLAG(IS_MAC)
  // NativeEventProcessorObserver overrides:
  // Exposed for tests.
  void WillRunNativeEvent(const void* opaque_identifier) override;
  void DidRunNativeEvent(const void* opaque_identifier) override;
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // aura::WindowEventDispatcherObserver overrides:
  void OnWindowEventDispatcherStartedProcessing(
      aura::WindowEventDispatcher* dispatcher,
      const ui::Event& event) override;
  void OnWindowEventDispatcherFinishedProcessingEvent(
      aura::WindowEventDispatcher* dispatcher) override;
#elif BUILDFLAG(IS_WIN)
  // base::MessagePumpForUI::Observer overrides:
  void WillDispatchMSG(const MSG& msg) override;
  void DidDispatchMSG(const MSG& msg) override;
#endif

 private:
  void RegisterObserver();
  void DeregisterObserver();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  struct EventInfo {
    raw_ptr<const void> unique_id;
  };
  std::vector<EventInfo> events_being_processed_;
#endif

  WillRunEventCallback will_run_event_callback_;
  DidRunEventCallback did_run_event_callback_;
};

}  // namespace responsiveness
}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_NATIVE_EVENT_OBSERVER_H_
