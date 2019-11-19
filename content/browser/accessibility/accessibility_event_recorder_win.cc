// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_event_recorder.h"

#include <oleacc.h>
#include <stdint.h>
#include <wrl/client.h>

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/accessibility_event_recorder_uia_win.h"
#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/base/win/atl_module.h"
#include "ui/gfx/win/hwnd_util.h"

namespace content {

namespace {

std::string RoleVariantToString(const base::win::ScopedVariant& role) {
  if (role.type() == VT_I4) {
    return base::UTF16ToUTF8(IAccessibleRoleToString(V_I4(role.ptr())));
  } else if (role.type() == VT_BSTR) {
    return base::UTF16ToUTF8(
        base::string16(V_BSTR(role.ptr()), SysStringLen(V_BSTR(role.ptr()))));
  }
  return std::string();
}

HRESULT QueryIAccessible2(IAccessible* accessible, IAccessible2** accessible2) {
  Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.GetAddressOf());
  return SUCCEEDED(hr)
             ? service_provider->QueryService(IID_IAccessible2, accessible2)
             : hr;
}

HRESULT QueryIAccessibleText(IAccessible* accessible,
                             IAccessibleText** accessible_text) {
  Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
  HRESULT hr = accessible->QueryInterface(service_provider.GetAddressOf());
  return SUCCEEDED(hr) ? service_provider->QueryService(IID_IAccessibleText,
                                                        accessible_text)
                       : hr;
}

std::string BstrToPrettyUTF8(BSTR bstr) {
  base::string16 str16(bstr, SysStringLen(bstr));

  // IAccessibleText returns the text you get by appending all static text
  // children, with an "embedded object character" for each non-text child.
  // Pretty-print the embedded object character as <obj> so that test output
  // is human-readable.
  base::StringPiece16 embedded_character(
      &BrowserAccessibilityComWin::kEmbeddedCharacter, 1);
  base::ReplaceChars(str16, embedded_character, L"<obj>", &str16);

  return base::UTF16ToUTF8(str16);
}

std::string AccessibilityEventToStringUTF8(int32_t event_id) {
  return base::UTF16ToUTF8(AccessibilityEventToString(event_id));
}

}  // namespace

class AccessibilityEventRecorderWin : public AccessibilityEventRecorder {
 public:
  AccessibilityEventRecorderWin(
      BrowserAccessibilityManager* manager,
      base::ProcessId pid,
      const base::StringPiece& application_name_match_pattern);
  ~AccessibilityEventRecorderWin() override;

  // Callback registered by SetWinEventHook. Just calls OnWinEventHook.
  static CALLBACK void WinEventHookThunk(HWINEVENTHOOK handle,
                                         DWORD event,
                                         HWND hwnd,
                                         LONG obj_id,
                                         LONG child_id,
                                         DWORD event_thread,
                                         DWORD event_time);

 private:
  // Called by the thunk registered by SetWinEventHook. Retrieves accessibility
  // info about the node the event was fired on and appends a string to
  // the event log.
  void OnWinEventHook(HWINEVENTHOOK handle,
                      DWORD event,
                      HWND hwnd,
                      LONG obj_id,
                      LONG child_id,
                      DWORD event_thread,
                      DWORD event_time);

  // Wrapper around AccessibleObjectFromWindow because the function call
  // inexplicably flakes sometimes on build/trybots.
  HRESULT AccessibleObjectFromWindowWrapper(HWND hwnd,
                                            DWORD dwId,
                                            REFIID riid,
                                            void** ppvObject);

  HWINEVENTHOOK win_event_hook_handle_;
  static AccessibilityEventRecorderWin* instance_;

  DISALLOW_COPY_AND_ASSIGN(AccessibilityEventRecorderWin);
};

// static
AccessibilityEventRecorderWin* AccessibilityEventRecorderWin::instance_ =
    nullptr;

// static
std::unique_ptr<AccessibilityEventRecorder> AccessibilityEventRecorder::Create(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const base::StringPiece& application_name_match_pattern) {
  if (!application_name_match_pattern.empty()) {
    LOG(FATAL) << "Recording accessibility events from an application name "
                  "match pattern not supported on this platform yet.";
  }

  return std::make_unique<AccessibilityEventRecorderWin>(
      manager, pid, application_name_match_pattern);
}

std::vector<AccessibilityEventRecorder::TestPass>
AccessibilityEventRecorder::GetTestPasses() {
  // In addition to the 'Blink' pass, Windows includes two accessibility APIs
  // that need to be tested independently (MSAA & UIA); the Blink pass uses the
  // same recorder as the MSAA pass.
  return {
      {"blink", &AccessibilityEventRecorder::Create},
      {"win", &AccessibilityEventRecorder::Create},
      {"uia", &AccessibilityEventRecorderUia::CreateUia},
  };
}

// static
CALLBACK void AccessibilityEventRecorderWin::WinEventHookThunk(
    HWINEVENTHOOK handle,
    DWORD event,
    HWND hwnd,
    LONG obj_id,
    LONG child_id,
    DWORD event_thread,
    DWORD event_time) {
  if (instance_) {
    instance_->OnWinEventHook(handle, event, hwnd, obj_id, child_id,
                              event_thread, event_time);
  }
}

AccessibilityEventRecorderWin::AccessibilityEventRecorderWin(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const base::StringPiece& application_name_match_pattern)
    : AccessibilityEventRecorder(manager) {
  CHECK(!instance_) << "There can be only one instance of"
                    << " AccessibilityEventRecorder at a time.";
  // For now, just use out of context events when running as a utility to watch
  // events (no BrowserAccessibilityManager), because otherwise Chrome events
  // are not getting reported. Being in context is better so that for
  // TEXT_REMOVED and TEXT_INSERTED events, we can query the text that was
  // inserted or removed and include that in the log.
  int context = manager ? WINEVENT_INCONTEXT : WINEVENT_OUTOFCONTEXT;
  win_event_hook_handle_ =
      SetWinEventHook(EVENT_MIN, EVENT_MAX, GetModuleHandle(NULL),
                      &AccessibilityEventRecorderWin::WinEventHookThunk, pid,
                      0,  // Hook all threads
                      context);
  CHECK(win_event_hook_handle_);
  instance_ = this;
}

AccessibilityEventRecorderWin::~AccessibilityEventRecorderWin() {
  UnhookWinEvent(win_event_hook_handle_);
  instance_ = nullptr;
}

void AccessibilityEventRecorderWin::OnWinEventHook(HWINEVENTHOOK handle,
                                                   DWORD event,
                                                   HWND hwnd,
                                                   LONG obj_id,
                                                   LONG child_id,
                                                   DWORD event_thread,
                                                   DWORD event_time) {
  Microsoft::WRL::ComPtr<IAccessible> browser_accessible;
  HRESULT hr = AccessibleObjectFromWindowWrapper(
      hwnd, obj_id, IID_IAccessible,
      reinterpret_cast<void**>(browser_accessible.GetAddressOf()));
  if (FAILED(hr)) {
    // Note: our event hook will pick up some superfluous events we
    // don't care about, so it's safe to just ignore these failures.
    // Same below for other HRESULT checks.
    VLOG(1) << "Ignoring result " << hr << " from AccessibleObjectFromWindow";
    return;
  }

  base::win::ScopedVariant childid_variant(child_id);
  Microsoft::WRL::ComPtr<IDispatch> dispatch;
  hr = browser_accessible->get_accChild(childid_variant,
                                        dispatch.GetAddressOf());
  if (hr != S_OK || !dispatch) {
    VLOG(1) << "Ignoring result " << hr << " and result " << dispatch.Get()
            << " from get_accChild";
    return;
  }

  Microsoft::WRL::ComPtr<IAccessible> iaccessible;
  hr = dispatch.CopyTo(iaccessible.GetAddressOf());
  if (FAILED(hr)) {
    VLOG(1) << "Ignoring result " << hr << " from QueryInterface";
    return;
  }

  std::string event_str = AccessibilityEventToStringUTF8(event);
  if (event_str.empty()) {
    VLOG(1) << "Ignoring event " << event;
    return;
  }

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedVariant role;
  iaccessible->get_accRole(childid_self, role.Receive());
  base::win::ScopedVariant state;
  iaccessible->get_accState(childid_self, state.Receive());
  int ia_state = V_I4(state.ptr());
  std::string hwnd_class_name = base::UTF16ToUTF8(gfx::GetClassName(hwnd));

  // Caret is special:
  // Log all caret events  that occur, with their window class, so that we can
  // test to make sure they are only occurring on the desired window class.
  if (ROLE_SYSTEM_CARET == V_I4(role.ptr())) {
    base::string16 state_str = IAccessibleStateToString(ia_state);
    std::string log = base::StringPrintf(
        "%s role=ROLE_SYSTEM_CARET %ls window_class=%s", event_str.c_str(),
        state_str.c_str(), hwnd_class_name.c_str());
    OnEvent(log);
    return;
  }

  if (only_web_events_) {
    if (hwnd_class_name != "Chrome_RenderWidgetHostHWND")
      return;

    Microsoft::WRL::ComPtr<IServiceProvider> service_provider;
    hr = iaccessible->QueryInterface(service_provider.GetAddressOf());
    if (FAILED(hr))
      return;

    Microsoft::WRL::ComPtr<IAccessible> content_document;
    hr = service_provider->QueryService(GUID_IAccessibleContentDocument,
                                        content_document.GetAddressOf());
    if (FAILED(hr))
      return;
  }

  base::win::ScopedBstr name_bstr;
  iaccessible->get_accName(childid_self, name_bstr.Receive());
  base::win::ScopedBstr value_bstr;
  iaccessible->get_accValue(childid_self, value_bstr.Receive());

  // Avoid flakiness. Events fired on a WINDOW are out of the control
  // of a test.
  if (role.type() == VT_I4 && ROLE_SYSTEM_WINDOW == V_I4(role.ptr())) {
    VLOG(1) << "Ignoring event " << event << " on ROLE_SYSTEM_WINDOW";
    return;
  }

  // Avoid flakiness. The "offscreen" state depends on whether the browser
  // window is frontmost or not, and "hottracked" depends on whether the
  // mouse cursor happens to be over the element.
  ia_state &= (~STATE_SYSTEM_OFFSCREEN & ~STATE_SYSTEM_HOTTRACKED);

  // The "readonly" state is set on almost every node and doesn't typically
  // change, so filter it out to keep the output less verbose.
  ia_state &= ~STATE_SYSTEM_READONLY;

  AccessibleStates ia2_state = 0;
  Microsoft::WRL::ComPtr<IAccessible2> iaccessible2;
  hr = QueryIAccessible2(iaccessible.Get(), iaccessible2.GetAddressOf());
  bool has_ia2 = SUCCEEDED(hr) && iaccessible2;

  base::string16 html_tag;
  base::string16 obj_class;
  base::string16 html_id;

  if (has_ia2) {
    iaccessible2->get_states(&ia2_state);
    base::win::ScopedBstr attributes_bstr;
    if (S_OK == iaccessible2->get_attributes(attributes_bstr.Receive())) {
      std::vector<base::string16> ia2_attributes = base::SplitString(
          base::string16(attributes_bstr, attributes_bstr.Length()),
          base::string16(1, ';'), base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      for (base::string16& attr : ia2_attributes) {
        if (base::StringPiece16(attr).starts_with(L"class:"))
          obj_class = attr.substr(6);  // HTML or view class
        if (base::StringPiece16(attr).starts_with(L"id:")) {
          html_id = base::string16(L"#");
          html_id += attr.substr(3);
        }
        if (base::StringPiece16(attr).starts_with(L"tag:")) {
          html_tag = attr.substr(4);
        }
      }
    }
  }

  std::string log = base::StringPrintf("%s on", event_str.c_str());
  if (!html_tag.empty()) {
    // HTML node with tag
    log += base::StringPrintf(
        " <%s%s%s%s>", base::UTF16ToUTF8(html_tag).c_str(),
        base::UTF16ToUTF8(html_id).c_str(), obj_class.empty() ? "" : ".",
        base::UTF16ToUTF8(obj_class).c_str());
  } else if (!obj_class.empty()) {
    // Non-HTML node with class
    log +=
        base::StringPrintf(" class=%s", base::UTF16ToUTF8(obj_class).c_str());
  }

  log += base::StringPrintf(" role=%s", RoleVariantToString(role).c_str());
  if (name_bstr.Length() > 0)
    log +=
        base::StringPrintf(" name=\"%s\"", BstrToPrettyUTF8(name_bstr).c_str());
  if (value_bstr.Length() > 0) {
    bool is_document =
        role.type() == VT_I4 && ROLE_SYSTEM_DOCUMENT == V_I4(role.ptr());
    // Don't show actual document value, which is a URL, in order to avoid
    // machine-based differences in tests.
    log += is_document
               ? " value~=[doc-url]"
               : base::StringPrintf(" value=\"%s\"",
                                    BstrToPrettyUTF8(value_bstr).c_str());
  }
  log += " ";
  log += base::UTF16ToUTF8(IAccessibleStateToString(ia_state));
  log += " ";
  log += base::UTF16ToUTF8(IAccessible2StateToString(ia2_state));

  // Group position, e.g. L3, 5 of 7
  LONG group_level, similar_items_in_group, position_in_group;
  if (has_ia2 &&
      iaccessible2->get_groupPosition(&group_level, &similar_items_in_group,
                                      &position_in_group) == S_OK) {
    if (group_level)
      log += base::StringPrintf(" level=%ld", group_level);
    if (position_in_group)
      log += base::StringPrintf(" PosInSet=%ld", position_in_group);
    if (similar_items_in_group)
      log += base::StringPrintf(" SetSize=%ld", similar_items_in_group);
  }

  // For TEXT_REMOVED and TEXT_INSERTED events, query the text that was
  // inserted or removed and include that in the log.
  Microsoft::WRL::ComPtr<IAccessibleText> accessible_text;
  hr = QueryIAccessibleText(iaccessible.Get(), accessible_text.GetAddressOf());
  if (SUCCEEDED(hr)) {
    if (event == IA2_EVENT_TEXT_REMOVED) {
      IA2TextSegment old_text;
      if (SUCCEEDED(accessible_text->get_oldText(&old_text))) {
        log += base::StringPrintf(" old_text={'%s' start=%ld end=%ld}",
                                  BstrToPrettyUTF8(old_text.text).c_str(),
                                  old_text.start, old_text.end);
      }
    }
    if (event == IA2_EVENT_TEXT_INSERTED) {
      IA2TextSegment new_text;
      if (SUCCEEDED(accessible_text->get_newText(&new_text))) {
        log += base::StringPrintf(" new_text={'%s' start=%ld end=%ld}",
                                  BstrToPrettyUTF8(new_text.text).c_str(),
                                  new_text.start, new_text.end);
      }
    }
  }

  log =
      base::UTF16ToUTF8(base::CollapseWhitespace(base::UTF8ToUTF16(log), true));
  OnEvent(log);
}

HRESULT AccessibilityEventRecorderWin::AccessibleObjectFromWindowWrapper(
    HWND hwnd,
    DWORD dw_id,
    REFIID riid,
    void** ppv_object) {
  HRESULT hr = ::AccessibleObjectFromWindow(hwnd, dw_id, riid, ppv_object);
  if (SUCCEEDED(hr))
    return hr;

  if (!manager_)  // No manager when outside of Chrome tests.
    return E_FAIL;

  // The above call to ::AccessibleObjectFromWindow fails for unknown
  // reasons every once in a while on the bots.  Work around it by grabbing
  // the object directly from the BrowserAccessibilityManager.
  HWND accessibility_hwnd =
      manager_->delegate()->AccessibilityGetAcceleratedWidget();
  if (accessibility_hwnd != hwnd)
    return E_FAIL;

  IAccessible* obj = ToBrowserAccessibilityComWin(manager_->GetRoot());
  obj->AddRef();
  *ppv_object = obj;
  return S_OK;
}

}  // namespace content
