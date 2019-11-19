// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_UIA_WIN_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_EVENT_RECORDER_UIA_WIN_H_

#include <ole2.h>
#include <stdint.h>
#include <uiautomation.h>
#include <wrl/client.h>
#include <map>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/win/atl.h"
#include "content/browser/accessibility/accessibility_event_recorder.h"

namespace content {

class AccessibilityEventRecorderUia : public AccessibilityEventRecorder {
 public:
  AccessibilityEventRecorderUia(
      BrowserAccessibilityManager* manager,
      base::ProcessId pid,
      const base::StringPiece& application_name_match_pattern);
  ~AccessibilityEventRecorderUia() override;

  static std::unique_ptr<AccessibilityEventRecorder> CreateUia(
      BrowserAccessibilityManager* manager,
      base::ProcessId pid,
      const base::StringPiece& application_name_match_pattern);

  // Called to ensure the event recorder has finished recording async events.
  void FlushAsyncEvents() override;

 private:
  // Used to prevent creation of multiple instances simultaneously
  static volatile base::subtle::Atomic32 instantiated_;

  // All UIA calls need to be made on a secondary MTA thread to avoid sporadic
  // test hangs / timeouts.
  class Thread : public base::PlatformThread::Delegate {
   public:
    Thread();
    ~Thread() override;

    void Init(AccessibilityEventRecorderUia* owner,
              HWND hwnd,
              base::RunLoop& initialization_loop,
              base::RunLoop& shutdown_loop);

    void SendShutdownSignal();

    void ThreadMain() override;

   private:
    AccessibilityEventRecorderUia* owner_ = nullptr;
    HWND hwnd_ = NULL;
    EVENTID shutdown_sentinel_ = 0;

    Microsoft::WRL::ComPtr<IUIAutomation> uia_;
    Microsoft::WRL::ComPtr<IUIAutomationElement> root_;
    Microsoft::WRL::ComPtr<IUIAutomationCacheRequest> cache_request_;

    // Thread synchronization members
    base::OnceClosure initialization_complete_;
    base::OnceClosure shutdown_complete_;
    base::WaitableEvent shutdown_signal_;
    bool shutdown_sentinel_received_ = false;

    // Thread-specific wrapper for OnEvent to handle necessary locking
    void OnEvent(const std::string& event);
    base::Lock on_event_lock_;
    std::map<base::PlatformThreadId, std::vector<std::string>> event_logs_;

    // An implementation of various UIA interfaces that forward event
    // notifications to the owning event recorder.
    class EventHandler : public CComObjectRootEx<CComMultiThreadModel>,
                         public IUIAutomationFocusChangedEventHandler,
                         public IUIAutomationPropertyChangedEventHandler,
                         public IUIAutomationStructureChangedEventHandler,
                         public IUIAutomationEventHandler {
     public:
      EventHandler();
      virtual ~EventHandler();

      void Init(AccessibilityEventRecorderUia::Thread* owner,
                Microsoft::WRL::ComPtr<IUIAutomationElement> root);
      void CleanUp();

      BEGIN_COM_MAP(EventHandler)
      COM_INTERFACE_ENTRY(IUIAutomationFocusChangedEventHandler)
      COM_INTERFACE_ENTRY(IUIAutomationPropertyChangedEventHandler)
      COM_INTERFACE_ENTRY(IUIAutomationStructureChangedEventHandler)
      COM_INTERFACE_ENTRY(IUIAutomationEventHandler)
      END_COM_MAP()

      // IUIAutomationFocusChangedEventHandler interface.
      STDMETHOD(HandleFocusChangedEvent)(IUIAutomationElement* sender) override;

      // IUIAutomationPropertyChangedEventHandler interface.
      STDMETHOD(HandlePropertyChangedEvent)
      (IUIAutomationElement* sender,
       PROPERTYID property_id,
       VARIANT new_value) override;

      // IUIAutomationStructureChangedEventHandler interface.
      STDMETHOD(HandleStructureChangedEvent)
      (IUIAutomationElement* sender,
       StructureChangeType change_type,
       SAFEARRAY* runtime_id) override;

      // IUIAutomationEventHandler interface.
      STDMETHOD(HandleAutomationEvent)
      (IUIAutomationElement* sender, EVENTID event_id) override;

      // Points to the event recorder to receive notifications.
      AccessibilityEventRecorderUia::Thread* owner_ = nullptr;

     private:
      std::string GetSenderInfo(IUIAutomationElement* sender);

      Microsoft::WRL::ComPtr<IUIAutomationElement> root_;

      DISALLOW_COPY_AND_ASSIGN(EventHandler);
    };
    Microsoft::WRL::ComPtr<CComObject<EventHandler>> uia_event_handler_;
  };

  Thread thread_;
  base::RunLoop shutdown_loop_;
  base::PlatformThreadHandle thread_handle_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityEventRecorderUia);
};

}  // namespace content

#endif
