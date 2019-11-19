// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_event_recorder_uia_win.h"

#include <algorithm>
#include <numeric>
#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"
#include "content/browser/accessibility/browser_accessibility_com_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "ui/base/win/atl_module.h"

namespace content {

namespace {

std::string UiaIdentifierToStringPretty(int32_t id) {
  auto str = base::UTF16ToUTF8(UiaIdentifierToString(id));
  // Remove UIA_ prefix, and EventId/PropertyId suffixes
  if (base::StartsWith(str, "UIA_", base::CompareCase::SENSITIVE))
    str = str.substr(base::size("UIA_") - 1);
  if (base::EndsWith(str, "EventId", base::CompareCase::SENSITIVE))
    str = str.substr(0, str.size() - base::size("EventId") + 1);
  if (base::EndsWith(str, "PropertyId", base::CompareCase::SENSITIVE))
    str = str.substr(0, str.size() - base::size("PropertyId") + 1);
  return str;
}

}  // namespace

// static
volatile base::subtle::Atomic32 AccessibilityEventRecorderUia::instantiated_ =
    0;

// static
std::unique_ptr<AccessibilityEventRecorder>
AccessibilityEventRecorderUia::CreateUia(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const base::StringPiece& application_name_match_pattern) {
  return std::make_unique<AccessibilityEventRecorderUia>(
      manager, pid, application_name_match_pattern);
}

AccessibilityEventRecorderUia::AccessibilityEventRecorderUia(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const base::StringPiece& application_name_match_pattern)
    : AccessibilityEventRecorder(manager) {
  CHECK(!base::subtle::NoBarrier_AtomicExchange(&instantiated_, 1))
      << "There can be only one instance at a time.";

  // Find the root content window
  HWND hwnd = manager->GetRoot()->GetTargetForNativeAccessibilityEvent();
  CHECK(hwnd);

  // Create the event thread, and pump messages via |initialization_loop| until
  // initialization is complete.
  base::RunLoop initialization_loop;
  base::PlatformThread::Create(0, &thread_, &thread_handle_);
  thread_.Init(this, hwnd, initialization_loop, shutdown_loop_);
  initialization_loop.Run();
}

AccessibilityEventRecorderUia::~AccessibilityEventRecorderUia() {
  base::subtle::NoBarrier_AtomicExchange(&instantiated_, 0);
}

void AccessibilityEventRecorderUia::FlushAsyncEvents() {
  // Pump messages via |shutdown_loop_| until the thread is complete.
  shutdown_loop_.Run();
  base::PlatformThread::Join(thread_handle_);
}

AccessibilityEventRecorderUia::Thread::Thread() {}

AccessibilityEventRecorderUia::Thread::~Thread() {}

void AccessibilityEventRecorderUia::Thread::Init(
    AccessibilityEventRecorderUia* owner,
    HWND hwnd,
    base::RunLoop& initialization,
    base::RunLoop& shutdown) {
  owner_ = owner;
  hwnd_ = hwnd;
  initialization_complete_ = initialization.QuitClosure();
  shutdown_complete_ = shutdown.QuitClosure();
}

void AccessibilityEventRecorderUia::Thread::ThreadMain() {
  // UIA calls must be made on an MTA thread to prevent random timeouts.
  base::win::ScopedCOMInitializer com_init{
      base::win::ScopedCOMInitializer::kMTA};

  // Create an instance of the CUIAutomation class.
  CoCreateInstance(CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
                   IID_IUIAutomation, &uia_);
  CHECK(uia_.Get());

  // Register the custom event to mark the end of the test.
  Microsoft::WRL::ComPtr<IUIAutomationRegistrar> registrar;
  CoCreateInstance(CLSID_CUIAutomationRegistrar, NULL, CLSCTX_INPROC_SERVER,
                   IID_IUIAutomationRegistrar, &registrar);
  CHECK(registrar.Get());
  UIAutomationEventInfo custom_event = {kUiaTestCompleteSentinelGuid,
                                        kUiaTestCompleteSentinel};
  CHECK(
      SUCCEEDED(registrar->RegisterEvent(&custom_event, &shutdown_sentinel_)));

  // Find the IUIAutomationElement for the root content window
  uia_->ElementFromHandle(hwnd_, &root_);
  CHECK(root_.Get());

  // Create the event handler
  ui::win::CreateATLModuleIfNeeded();
  CHECK(
      SUCCEEDED(CComObject<EventHandler>::CreateInstance(&uia_event_handler_)));
  uia_event_handler_->AddRef();
  uia_event_handler_->Init(this, root_);

  // Create a cache request to avoid cross-thread issues when logging.
  CHECK(SUCCEEDED(uia_->CreateCacheRequest(&cache_request_)));
  CHECK(cache_request_.Get());
  CHECK(SUCCEEDED(cache_request_->AddProperty(UIA_NamePropertyId)));
  CHECK(SUCCEEDED(cache_request_->AddProperty(UIA_AriaRolePropertyId)));

  // Subscribe to the shutdown sentinel event
  uia_->AddAutomationEventHandler(
      shutdown_sentinel_, root_.Get(), TreeScope::TreeScope_Subtree,
      cache_request_.Get(), uia_event_handler_.Get());

  // Subscribe to focus events
  uia_->AddFocusChangedEventHandler(cache_request_.Get(),
                                    uia_event_handler_.Get());

  // Subscribe to all property-change events
  static const PROPERTYID kMinProp = UIA_RuntimeIdPropertyId;
  static const PROPERTYID kMaxProp = UIA_HeadingLevelPropertyId;
  std::array<PROPERTYID, (kMaxProp - kMinProp) + 1> property_list;
  std::iota(property_list.begin(), property_list.end(), kMinProp);
  uia_->AddPropertyChangedEventHandlerNativeArray(
      root_.Get(), TreeScope::TreeScope_Subtree, cache_request_.Get(),
      uia_event_handler_.Get(), &property_list[0], property_list.size());

  // Subscribe to all structure-change events
  uia_->AddStructureChangedEventHandler(root_.Get(), TreeScope_Subtree,
                                        cache_request_.Get(),
                                        uia_event_handler_.Get());

  // Subscribe to all automation events (except structure-change events and
  // live-region events, which are handled elsewhere).
  static const EVENTID kMinEvent = UIA_ToolTipOpenedEventId;
  static const EVENTID kMaxEvent = UIA_NotificationEventId;
  for (EVENTID event_id = kMinEvent; event_id <= kMaxEvent; ++event_id) {
    if (event_id != UIA_StructureChangedEventId &&
        event_id != UIA_LiveRegionChangedEventId) {
      uia_->AddAutomationEventHandler(
          event_id, root_.Get(), TreeScope::TreeScope_Subtree,
          cache_request_.Get(), uia_event_handler_.Get());
    }
  }

  // Subscribe to live-region change events.  This must be the last event we
  // subscribe to, because |AXFragmentRootWin| will fire events when advised of
  // the subscription, and this can hang the test-process (on Windows 19H1+) if
  // we're simultaneously trying to subscribe to other events.
  uia_->AddAutomationEventHandler(
      UIA_LiveRegionChangedEventId, root_.Get(), TreeScope::TreeScope_Subtree,
      cache_request_.Get(), uia_event_handler_.Get());

  // Signal that initialization is complete; this will wake the main thread to
  // start executing the test.
  std::move(initialization_complete_).Run();

  // Wait for shutdown signal
  shutdown_signal_.Wait();

  // Due to a bug in Windows (fixed in Windows 10 19H1), events are raised
  // exactly twice for any in-proc off-thread event listeners. We filter out the
  // duplicate events here, and forward the remaining events to our owner.
  {
    base::AutoLock lock{on_event_lock_};
    if (event_logs_.size() == 1) {
      // Only received events on a single thread... perhaps the bug was fixed?
      // Forward all events.
      for (auto&& event : event_logs_.begin()->second)
        owner_->OnEvent(event);
    } else if (event_logs_.size() == 2) {
      // Events were raised on two threads, as expected.  Sort the lists and
      // forward events, eliminating duplicates that occur in both threads.
      auto&& events_thread1 = event_logs_.begin()->second;
      auto&& events_thread2 = (++event_logs_.begin())->second;

      std::sort(events_thread1.begin(), events_thread1.end());
      std::sort(events_thread2.begin(), events_thread2.end());

      auto it1 = events_thread1.begin();
      auto it2 = events_thread2.begin();
      while (it1 < events_thread1.end() && it2 < events_thread2.end()) {
        if (*it1 == *it2) {
          owner_->OnEvent(*it1);
          it1++;
          it2++;
        } else if (*it1 < *it2) {
          owner_->OnEvent(*it1);
          it1++;
        } else {
          owner_->OnEvent(*it2);
          it2++;
        }
      }
      while (it1 < events_thread1.end())
        owner_->OnEvent(*it1++);
      while (it2 < events_thread2.end())
        owner_->OnEvent(*it2++);
    } else {
      // Typically we'll get events on exactly two threads (one directly from
      // UIA, the second from RPC), but sometimes RPC will split its events
      // across different threads.
      //
      // Unfortunately, there is no robust method of eliminating duplicates in
      // this case.  Tests with intentional duplicates could run afoul of this
      // logic in rare scenarios; it is recommended that intentionally
      // duplicated events be avoided in tests, when possible.
      std::vector<std::string> combined;
      for (auto&& log : event_logs_)
        combined.insert(combined.end(), log.second.begin(), log.second.end());
      std::sort(combined.begin(), combined.end());

      std::string last_event;
      for (auto&& event : combined) {
        if (last_event != event)
          owner_->OnEvent(last_event = event);
        else
          last_event = {};
      }
    }
  }

  // Cleanup
  uia_->RemoveAllEventHandlers();
  uia_event_handler_->CleanUp();
  uia_event_handler_.Reset();
  cache_request_.Reset();
  root_.Reset();
  uia_.Reset();

  // Signal thread shutdown complete; this will wake the main thread to compile
  // test results and compare against the expected results.
  std::move(shutdown_complete_).Run();
}

void AccessibilityEventRecorderUia::Thread::SendShutdownSignal() {
  // We expect to see the shutdown sentinel exactly twice (due to the Windows
  // bug detailed in |ThreadMain| and fixed in 19H1), so don't actually shut
  // down the thread until the second call.
  if (shutdown_sentinel_received_ ||
      base::win::GetVersion() >= base::win::Version::WIN10_19H1)
    shutdown_signal_.Signal();
  else
    shutdown_sentinel_received_ = true;
}

void AccessibilityEventRecorderUia::Thread::OnEvent(const std::string& event) {
  // We need to synchronize event logging, since UIA event callbacks can be
  // coming from multiple threads.
  base::AutoLock lock{on_event_lock_};
  event_logs_[base::PlatformThread::CurrentId()].push_back(event);
}

AccessibilityEventRecorderUia::Thread::EventHandler::EventHandler() {}

AccessibilityEventRecorderUia::Thread::EventHandler::~EventHandler() {}

void AccessibilityEventRecorderUia::Thread::EventHandler::Init(
    AccessibilityEventRecorderUia::Thread* owner,
    Microsoft::WRL::ComPtr<IUIAutomationElement> root) {
  owner_ = owner;
  root_ = root;
}

void AccessibilityEventRecorderUia::Thread::EventHandler::CleanUp() {
  owner_ = nullptr;
  root_.Reset();
}

STDMETHODIMP
AccessibilityEventRecorderUia::Thread::EventHandler::HandleFocusChangedEvent(
    IUIAutomationElement* sender) {
  if (!owner_)
    return S_OK;

  base::win::ScopedSafearray id;
  sender->GetRuntimeId(id.Receive());
  base::win::ScopedVariant id_variant(id.Release());

  Microsoft::WRL::ComPtr<IUIAutomationElement> element_found;
  Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;

  owner_->uia_->CreatePropertyCondition(UIA_RuntimeIdPropertyId, id_variant,
                                        &condition);
  CHECK(condition);
  root_->FindFirst(TreeScope::TreeScope_Subtree, condition.Get(),
                   &element_found);
  if (!element_found) {
    VLOG(1) << "Ignoring UIA focus event outside our frame";
    return S_OK;
  }

  std::string log = base::StringPrintf("AutomationFocusChanged %s",
                                       GetSenderInfo(sender).c_str());
  owner_->OnEvent(log);

  return S_OK;
}

STDMETHODIMP
AccessibilityEventRecorderUia::Thread::EventHandler::HandlePropertyChangedEvent(
    IUIAutomationElement* sender,
    PROPERTYID property_id,
    VARIANT new_value) {
  if (owner_) {
    std::string prop_str = UiaIdentifierToStringPretty(property_id);
    if (prop_str.empty()) {
      VLOG(1) << "Ignoring UIA property-changed event " << property_id;
      return S_OK;
    }

    std::string log = base::StringPrintf("%s changed %s", prop_str.c_str(),
                                         GetSenderInfo(sender).c_str());
    owner_->OnEvent(log);
  }
  return S_OK;
}

STDMETHODIMP
AccessibilityEventRecorderUia::Thread::EventHandler::
    HandleStructureChangedEvent(IUIAutomationElement* sender,
                                StructureChangeType change_type,
                                SAFEARRAY* runtime_id) {
  if (owner_) {
    std::string type_str;
    switch (change_type) {
      case StructureChangeType_ChildAdded:
        type_str = "ChildAdded";
        break;
      case StructureChangeType_ChildRemoved:
        type_str = "ChildRemoved";
        break;
      case StructureChangeType_ChildrenInvalidated:
        type_str = "ChildrenInvalidated";
        break;
      case StructureChangeType_ChildrenBulkAdded:
        type_str = "ChildrenBulkAdded";
        break;
      case StructureChangeType_ChildrenBulkRemoved:
        type_str = "ChildrenBulkRemoved";
        break;
      case StructureChangeType_ChildrenReordered:
        type_str = "ChildrenReordered";
        break;
    }

    std::string log =
        base::StringPrintf("StructureChanged/%s %s", type_str.c_str(),
                           GetSenderInfo(sender).c_str());
    owner_->OnEvent(log);
  }
  return S_OK;
}

STDMETHODIMP
AccessibilityEventRecorderUia::Thread::EventHandler::HandleAutomationEvent(
    IUIAutomationElement* sender,
    EVENTID event_id) {
  if (owner_) {
    if (event_id == owner_->shutdown_sentinel_) {
      // This is a sentinel value that tells us the tests are finished.
      owner_->SendShutdownSignal();
    } else {
      std::string event_str = UiaIdentifierToStringPretty(event_id);
      if (event_str.empty()) {
        VLOG(1) << "Ignoring UIA automation event " << event_id;
        return S_OK;
      }

      // Remove duplicate menuclosed events with no event data.
      // The "duplicates" are benign. UIA currently duplicates *all* events for
      // in-process listeners, and the event-recorder tries to eliminate the
      // duplicates... but since the recorder sometimes isn't able to retrieve
      // the role, the duplicate-elimination logic doesn't see them as
      // duplicates in this case.
      std::string sender_info =
          event_id == UIA_MenuClosedEventId ? "" : GetSenderInfo(sender);
      std::string log =
          base::StringPrintf("%s %s", event_str.c_str(), sender_info.c_str());
      owner_->OnEvent(log);
    }
  }
  return S_OK;
}

std::string AccessibilityEventRecorderUia::Thread::EventHandler::GetSenderInfo(
    IUIAutomationElement* sender) {
  std::string sender_info = "";

  auto append_property = [&](const char* name, auto getter) {
    base::win::ScopedBstr bstr;
    (sender->*getter)(bstr.Receive());
    if (bstr.Length() > 0) {
      sender_info +=
          base::StringPrintf("%s%s=%s", sender_info.empty() ? "" : ", ", name,
                             BstrToUTF8(bstr).c_str());
    }
  };

  append_property("role", &IUIAutomationElement::get_CachedAriaRole);
  append_property("name", &IUIAutomationElement::get_CachedName);

  if (!sender_info.empty())
    sender_info = "on " + sender_info;
  return sender_info;
}

}  // namespace content
