// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/page_type.h"
#include "ipc/message_filter.h"
#include "net/base/load_flags.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "storage/common/fileapi/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace gfx {
class Point;
}

namespace net {
class CanonicalCookie;
namespace test_server {
class EmbeddedTestServer;
}
// TODO(svaldez): Remove typedef once EmbeddedTestServer has been migrated
// out of net::test_server.
using test_server::EmbeddedTestServer;
}

// A collections of functions designed for use with content_browsertests and
// browser_tests.
// TO BE CLEAR: any function here must work against both binaries. If it only
// works with browser_tests, it should be in chrome\test\base\ui_test_utils.h.
// If it only works with content_browsertests, it should be in
// content\test\content_browser_test_utils.h.

namespace content {

class BrowserContext;
struct FrameVisualProperties;
class FrameTreeNode;
class InterstitialPage;
class NavigationHandle;
class RenderWidgetHost;
class RenderWidgetHostView;
class WebContents;

// Navigates |web_contents| to |url|, blocking until the navigation finishes.
// Returns true if the page was loaded successfully and the last committed URL
// matches |url|.  This is a browser-initiated navigation that simulates a user
// typing |url| into the address bar.
WARN_UNUSED_RESULT bool NavigateToURL(WebContents* web_contents,
                                      const GURL& url);

// Navigates |web_contents| to |url|, blocking until the given number of
// navigations finishes.
void NavigateToURLBlockUntilNavigationsComplete(WebContents* web_contents,
                                                const GURL& url,
                                                int number_of_navigations);

// Navigate a frame with ID |iframe_id| to |url|, blocking until the navigation
// finishes.  Uses a renderer-initiated navigation from script code in the
// main frame.
bool NavigateIframeToURL(WebContents* web_contents,
                         std::string iframe_id,
                         const GURL& url);

// Generate a URL for a file path including a query string.
GURL GetFileUrlWithQuery(const base::FilePath& path,
                         const std::string& query_string);

// Checks whether the page type of the last committed navigation entry matches
// |page_type|.
bool IsLastCommittedEntryOfPageType(WebContents* web_contents,
                                    content::PageType page_type);

// Waits for |web_contents| to stop loading.  If |web_contents| is not loading
// returns immediately.  Tests should use WaitForLoadStop instead and check that
// last navigation succeeds, and this function should only be used if the
// navigation leads to web_contents being destroyed.
void WaitForLoadStopWithoutSuccessCheck(WebContents* web_contents);

// Waits for |web_contents| to stop loading.  If |web_contents| is not loading
// returns immediately.  Returns true if the last navigation succeeded (resulted
// in a committed navigation entry of type PAGE_TYPE_NORMAL).
// TODO(alexmos): tests that use this function to wait for successful
// navigations should be refactored to do EXPECT_TRUE(WaitForLoadStop()).
bool WaitForLoadStop(WebContents* web_contents);

// If a test uses a beforeunload dialog, it must be prepared to avoid flakes.
// This function collects everything that needs to be done.
void PrepContentsForBeforeUnloadTest(WebContents* web_contents);

#if defined(USE_AURA) || defined(OS_ANDROID)
// If WebContent's view is currently being resized, this will wait for the ack
// from the renderer that the resize is complete and for the
// WindowEventDispatcher to release the pointer moves. If there's no resize in
// progress, the method will return right away.
void WaitForResizeComplete(WebContents* web_contents);
#endif  // defined(USE_AURA) || defined(OS_ANDROID)

// Allows tests to set the last committed origin of |render_frame_host|, to
// simulate a scenario that might happen with a compromised renderer or might
// not otherwise be possible.
void OverrideLastCommittedOrigin(RenderFrameHost* render_frame_host,
                                 const url::Origin& origin);

// Causes the specified web_contents to crash. Blocks until it is crashed.
void CrashTab(WebContents* web_contents);

// Sets up a commit interceptor to alter commits for |target_url| to change
// their commit URL to |new_url| and origin to |new_origin|. This will happen
// for all commits in |web_contents|.
void PwnCommitIPC(WebContents* web_contents,
                  const GURL& target_url,
                  const GURL& new_url,
                  const url::Origin& new_origin);

// Causes the specified web_contents to issue an OnUnresponsiveRenderer event
// to its observers.
void SimulateUnresponsiveRenderer(WebContents* web_contents,
                                  RenderWidgetHost* widget);

// Simulates clicking at the center of the given tab asynchronously; modifiers
// may contain bits from WebInputEvent::Modifiers.
void SimulateMouseClick(WebContents* web_contents,
                        int modifiers,
                        blink::WebMouseEvent::Button button);

// Simulates clicking at the point |point| of the given tab asynchronously;
// modifiers may contain bits from WebInputEvent::Modifiers.
void SimulateMouseClickAt(WebContents* web_contents,
                          int modifiers,
                          blink::WebMouseEvent::Button button,
                          const gfx::Point& point);

// Same as SimulateMouseClickAt() except it forces the mouse event to go through
// RenderWidgetHostInputEventRouter.
void SimulateRoutedMouseClickAt(WebContents* web_contents,
                                int modifiers,
                                blink::WebMouseEvent::Button button,
                                const gfx::Point& point);

// Simulates MouseDown at the center of the given RenderWidgetHost's area.
// This does not send a corresponding MouseUp.
void SendMouseDownToWidget(RenderWidgetHost* target,
                           int modifiers,
                           blink::WebMouseEvent::Button button);

// Simulates asynchronously a mouse enter/move/leave event.
void SimulateMouseEvent(WebContents* web_contents,
                        blink::WebInputEvent::Type type,
                        const gfx::Point& point);

// Same as SimulateMouseEvent() except it forces the mouse event to go through
// RenderWidgetHostInputEventRouter.
void SimulateRoutedMouseEvent(WebContents* web_contents,
                              blink::WebInputEvent::Type type,
                              const gfx::Point& point);

// Simulate a mouse wheel event.
void SimulateMouseWheelEvent(WebContents* web_contents,
                             const gfx::Point& point,
                             const gfx::Vector2d& delta,
                             const blink::WebMouseWheelEvent::Phase phase);

#if !defined(OS_MACOSX)
// Simulate a mouse wheel event with the ctrl modifier set.
void SimulateMouseWheelCtrlZoomEvent(WebContents* web_contents,
                                     const gfx::Point& point,
                                     bool zoom_in,
                                     blink::WebMouseWheelEvent::Phase phase);
#endif  // !defined(OS_MACOSX)

// Sends a GesturePinch Begin/Update/End sequence.
void SimulateGesturePinchSequence(WebContents* web_contents,
                                  const gfx::Point& point,
                                  float scale,
                                  blink::WebGestureDevice source_device);

// Sends a simple, three-event (Begin/Update/End) gesture scroll.
void SimulateGestureScrollSequence(WebContents* web_contents,
                                   const gfx::Point& point,
                                   const gfx::Vector2dF& delta);

void SimulateGestureFlingSequence(WebContents* web_contents,
                                  const gfx::Point& point,
                                  const gfx::Vector2dF& velocity);

void SimulateGestureEvent(WebContents* web_contents,
                          const blink::WebGestureEvent& gesture_event,
                          const ui::LatencyInfo& latency);

// Taps the screen at |point|, using gesture Tap or TapDown.
void SimulateTapAt(WebContents* web_contents, const gfx::Point& point);
void SimulateTapDownAt(WebContents* web_contents, const gfx::Point& point);

// A helper function for SimulateTap(Down)At.
void SimulateTouchGestureAt(WebContents* web_contents,
                            const gfx::Point& point,
                            blink::WebInputEvent::Type type);

#if defined(USE_AURA)
// Generates a TouchStart at |point|.
void SimulateTouchPressAt(WebContents* web_contents, const gfx::Point& point);

void SimulateLongTapAt(WebContents* web_contents, const gfx::Point& point);
#endif

// Taps the screen with modifires at |point|.
void SimulateTapWithModifiersAt(WebContents* web_contents,
                                unsigned Modifiers,
                                const gfx::Point& point);

// Sends a key press asynchronously.
// |key| specifies the UIEvents (aka: DOM4Events) value of the key.
// |code| specifies the UIEvents (aka: DOM4Events) value of the physical key.
// |key_code| alone is good enough for scenarios that only need the char
// value represented by a key event and not the physical key on the keyboard
// or the keyboard layout.
// If set to true, the modifiers |control|, |shift|, |alt|, and |command| are
// pressed down first before the key event, and released after.
void SimulateKeyPress(WebContents* web_contents,
                      ui::DomKey key,
                      ui::DomCode code,
                      ui::KeyboardCode key_code,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command);

// Like SimulateKeyPress(), but does not send the char (AKA keypress) event.
// This is useful for arrow keys and other key presses that do not generate
// characters.
void SimulateKeyPressWithoutChar(WebContents* web_contents,
                                 ui::DomKey key,
                                 ui::DomCode code,
                                 ui::KeyboardCode key_code,
                                 bool control,
                                 bool shift,
                                 bool alt,
                                 bool command);

// Reset touch action for the embedder of a BrowserPluginGuest.
void ResetTouchAction(RenderWidgetHost* host);

// In some cases when an event is send to guest view, it gets resent to the
// embedder.
void ResendGestureScrollUpdateToEmbedder(WebContents* guest_web_contents,
                                         const blink::WebInputEvent& event);

// When a guest view is pre-processing a mouse/touch event, send a synthetic
// tap gesture to its RenderWidgetHostView.
void MaybeSendSyntheticTapGesture(WebContents* guest_web_contents);

// Holds down modifier keys for the duration of its lifetime and releases them
// upon destruction. This allows simulating multiple input events without
// simulating modifier key releases in between.
class ScopedSimulateModifierKeyPress {
 public:
  ScopedSimulateModifierKeyPress(WebContents* web_contents,
                                 bool control,
                                 bool shift,
                                 bool alt,
                                 bool command);
  ~ScopedSimulateModifierKeyPress();

  // Similar to SimulateMouseClickAt().
  void MouseClickAt(int additional_modifiers,
                    blink::WebMouseEvent::Button button,
                    const gfx::Point& point);

  // Similar to SimulateKeyPress().
  void KeyPress(ui::DomKey key, ui::DomCode code, ui::KeyboardCode key_code);

  // Similar to SimulateKeyPressWithoutChar().
  void KeyPressWithoutChar(ui::DomKey key,
                           ui::DomCode code,
                           ui::KeyboardCode key_code);

 private:
  WebContents* const web_contents_;
  int modifiers_;
  const bool control_;
  const bool shift_;
  const bool alt_;
  const bool command_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSimulateModifierKeyPress);
};

// Method to check what devices we have on the system.
bool IsWebcamAvailableOnSystem(WebContents* web_contents);

// Allow ExecuteScript* methods to target either a WebContents or a
// RenderFrameHost.  Targetting a WebContents means executing the script in the
// RenderFrameHost returned by WebContents::GetMainFrame(), which is the main
// frame.  Pass a specific RenderFrameHost to target it. Embedders may declare
// additional ConvertToRenderFrameHost functions for convenience.
class ToRenderFrameHost {
 public:
  template <typename T>
  ToRenderFrameHost(T* frame_convertible_value)
      : render_frame_host_(ConvertToRenderFrameHost(frame_convertible_value)) {}

  // Extract the underlying frame.
  RenderFrameHost* render_frame_host() const { return render_frame_host_; }

 private:
  RenderFrameHost* render_frame_host_;
};

RenderFrameHost* ConvertToRenderFrameHost(RenderFrameHost* render_view_host);
RenderFrameHost* ConvertToRenderFrameHost(WebContents* web_contents);

// Semi-deprecated: in new code, prefer ExecJs() -- it works the same, but has
// better error handling. (Note: still use ExecuteScript() on pages with a
// Content Security Policy).
//
// Executes the passed |script| in the specified frame with the user gesture.
//
// Appends |domAutomationController.send(...)| to the end of |script| and waits
// until the response comes back (pumping the message loop while waiting).  The
// |script| itself should not invoke domAutomationController.send(); if you want
// to call domAutomationController.send(...) yourself and extract the result,
// then use one of ExecuteScriptAndExtract... functions).
//
// Returns true on success (if the renderer responded back with the expected
// value).  Returns false otherwise (e.g. if the script threw an exception
// before calling the appended |domAutomationController.send(...)|, or if the
// renderer died or if the renderer called |domAutomationController.send(...)|
// with a malformed or unexpected value).
//
// See also:
// - ExecJs (preferred replacement with better errror handling)
// - EvalJs (if you want to retrieve a value)
// - ExecuteScriptAsync (if you don't want to block for |script| completion)
// - DOMMessageQueue (to manually wait for domAutomationController.send(...))
bool ExecuteScript(const ToRenderFrameHost& adapter,
                   const std::string& script) WARN_UNUSED_RESULT;

// Same as content::ExecuteScript but doesn't send a user gesture to the
// renderer.
bool ExecuteScriptWithoutUserGesture(const ToRenderFrameHost& adapter,
                                     const std::string& script)
    WARN_UNUSED_RESULT;

// Similar to ExecuteScript above, but
// - Doesn't modify the |script|.
// - Kicks off execution of the |script| in the specified frame and returns
//   immediately (without waiting for a response from the renderer and/or
//   without checking that the script succeeded).
void ExecuteScriptAsync(const ToRenderFrameHost& adapter,
                        const std::string& script);

// The following methods execute the passed |script| in the specified frame and
// sets |result| to the value passed to "window.domAutomationController.send" by
// the executed script. They return true on success, false if the script
// execution failed or did not evaluate to the expected type.
//
// Semi-deprecated: Consider using EvalJs() or EvalJsWithManualReply() instead,
// which handle errors better and don't require an out-param. If the target
// document doesn't have a CSP. See the comment on EvalJs() for migration tips.
bool ExecuteScriptAndExtractDouble(const ToRenderFrameHost& adapter,
                                   const std::string& script,
                                   double* result) WARN_UNUSED_RESULT;
bool ExecuteScriptAndExtractInt(const ToRenderFrameHost& adapter,
                                const std::string& script,
                                int* result) WARN_UNUSED_RESULT;
bool ExecuteScriptAndExtractBool(const ToRenderFrameHost& adapter,
                                 const std::string& script,
                                 bool* result) WARN_UNUSED_RESULT;
bool ExecuteScriptAndExtractString(const ToRenderFrameHost& adapter,
                                   const std::string& script,
                                   std::string* result) WARN_UNUSED_RESULT;

// Same as above but the script executed without user gesture.
bool ExecuteScriptWithoutUserGestureAndExtractDouble(
    const ToRenderFrameHost& adapter,
    const std::string& script,
    double* result) WARN_UNUSED_RESULT;
bool ExecuteScriptWithoutUserGestureAndExtractInt(
    const ToRenderFrameHost& adapter,
    const std::string& script,
    int* result) WARN_UNUSED_RESULT;
bool ExecuteScriptWithoutUserGestureAndExtractBool(
    const ToRenderFrameHost& adapter,
    const std::string& script,
    bool* result) WARN_UNUSED_RESULT;
bool ExecuteScriptWithoutUserGestureAndExtractString(
    const ToRenderFrameHost& adapter,
    const std::string& script,
    std::string* result) WARN_UNUSED_RESULT;

// JsLiteralHelper is a helper class that determines what types are legal to
// pass to StringifyJsLiteral. Legal types include int, string, StringPiece,
// char*, bool, double, GURL, url::Origin, and base::Value&&.
template <typename T>
struct JsLiteralHelper {
  // This generic version enables passing any type from which base::Value can be
  // instantiated. This covers int, string, double, bool, base::Value&&, etc.
  template <typename U>
  static base::Value Convert(U&& arg) {
    return base::Value(std::forward<U>(arg));
  }

  static base::Value Convert(const base::Value& value) {
    return value.Clone();
  }

  static base::Value Convert(const base::ListValue& value) {
    return value.Clone();
  }
};

// Specialization allowing GURL to be passed to StringifyJsLiteral.
template <>
struct JsLiteralHelper<GURL> {
  static base::Value Convert(const GURL& url) {
    return base::Value(url.spec());
  }
};

// Specialization allowing url::Origin to be passed to StringifyJsLiteral.
template <>
struct JsLiteralHelper<url::Origin> {
  static base::Value Convert(const url::Origin& url) {
    return base::Value(url.Serialize());
  }
};

// Helper for variadic ListValueOf() -- zero-argument base case.
inline void ConvertToBaseValueList(base::Value::ListStorage* list) {}

// Helper for variadic ListValueOf() -- case with at least one argument.
//
// |first| can be any type explicitly convertible to base::Value
// (including int/string/StringPiece/char*/double/bool), or any type that
// JsLiteralHelper is specialized for -- like URL and url::Origin, which emit
// string literals.
template <typename T, typename... Args>
void ConvertToBaseValueList(base::Value::ListStorage* list,
                            T&& first,
                            Args&&... rest) {
  using ValueType = std::remove_cv_t<std::remove_reference_t<T>>;
  list->push_back(JsLiteralHelper<ValueType>::Convert(std::forward<T>(first)));
  ConvertToBaseValueList(list, std::forward<Args>(rest)...);
}

// Construct a list-type base::Value from a mix of arguments.
//
// Each |arg| can be any type explicitly convertible to base::Value
// (including int/string/StringPiece/char*/double/bool), or any type that
// JsLiteralHelper is specialized for -- like URL and url::Origin, which emit
// string literals. |args| can be a mix of different types.
template <typename... Args>
base::ListValue ListValueOf(Args&&... args) {
  base::ListValue result;
  ConvertToBaseValueList(&result.GetList(), std::forward<Args>(args)...);
  return result;
}

// Replaces $1, $2, $3, etc in |script_template| with JS literal values
// constructed from |args|, similar to base::ReplaceStringPlaceholders.
//
// Unlike StringPrintf or manual concatenation, this version will properly
// escape string content, even if it contains slashes or quotation marks.
//
// Each |arg| can be any type explicitly convertible to base::Value
// (including int/string/StringPiece/char*/double/bool), or any type that
// JsLiteralHelper is specialized for -- like URL and url::Origin, which emit
// string literals. |args| can be a mix of different types.
//
// Example 1:
//
//   GURL page_url("http://example.com");
//   EXPECT_TRUE(ExecuteScript(
//       shell(), JsReplace("window.open($1, '_blank');", page_url)));
//
// $1 is replaced with a double-quoted JS string literal:
// "http://example.com". Note that quotes around $1 are not required.
//
// Example 2:
//
//   bool forced_reload = true;
//   EXPECT_TRUE(ExecuteScript(
//       shell(), JsReplace("window.location.reload($1);", forced_reload)));
//
// This becomes "window.location.reload(true);" -- because bool values are
// supported by base::Value. Numbers, lists, and dicts also work.
template <typename... Args>
std::string JsReplace(base::StringPiece script_template, Args&&... args) {
  base::Value::ListStorage values;
  ConvertToBaseValueList(&values, std::forward<Args>(args)...);
  std::vector<std::string> replacements(values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    CHECK(base::JSONWriter::Write(values[i], &replacements[i]));
  }
  return base::ReplaceStringPlaceholders(script_template, replacements,
                                         nullptr);
}

// The return value of EvalJs. Captures the value (or the error) arising from
// script execution. When used with gtest assertions, EvalJsResult generally
// behaves like its wrapped value.
//
// An EvalJsResult can be consumed in two ways:
//
//  (1) [preferred] Pass it directly to an EXPECT_EQ() macro. It has
//      overloaded operator== against std::string, bool, int, double,
//      nullptr_t, and base::Value. This will produce readable assertion
//      failures if there is a type mismatch, or if an exception was thrown --
//      errors are never equal to anything.
//
//      For boolean results, note that EXPECT_TRUE(..) and EXPECT_FALSE()
//      won't compile; use EXPECT_EQ(true, ...) instead. This is intentional,
//      since EXPECT_TRUE() could be read ambiguously as either "expect
//      successful execution", "expect truthy value of any type", or "expect
//      boolean value 'true'".
//
//  (2) [use when necessary] Extract the underlying value of an expected type,
//      by calling ExtractString(), ExtractInt(), etc. This will produce a
//      CHECK failure if the execution didn't result in the appropriate type
//      of result, or if an exception was thrown.
struct EvalJsResult {
  const base::Value value;  // Value; if things went well.
  const std::string error;  // Error; if things went badly.

  // Creates an ExecuteScript result. If |error| is non-empty, |value| will be
  // ignored.
  EvalJsResult(base::Value value, const std::string& error);

  // Copy ctor.
  EvalJsResult(const EvalJsResult& value);

  // Extract a result value of the requested type, or die trying.
  //
  // If there was an error, or if returned value is of a different type, these
  // will fail with a CHECK. Use Extract methods only when accessing the
  // result value is necessary; prefer operator== and EXPECT_EQ() instead:
  // they don't CHECK, and give better error messages.
  const std::string& ExtractString() const WARN_UNUSED_RESULT;
  int ExtractInt() const WARN_UNUSED_RESULT;
  bool ExtractBool() const WARN_UNUSED_RESULT;
  double ExtractDouble() const WARN_UNUSED_RESULT;
  base::ListValue ExtractList() const WARN_UNUSED_RESULT;
};

// Enables EvalJsResult to be used directly in ASSERT/EXPECT macros:
//
//    ASSERT_EQ("ab", EvalJs(rfh, "'a' + 'b'"))
//    ASSERT_EQ(2, EvalJs(rfh, "1 + 1"))
//    ASSERT_EQ(nullptr, EvalJs(rfh, "var a = 1 + 1"))
//
// Error values never return true for any comparison operator.
template <typename T>
bool operator==(const T& a, const EvalJsResult& b) {
  return b.error.empty() && (JsLiteralHelper<T>::Convert(a) == b.value);
}

template <typename T>
bool operator!=(const T& a, const EvalJsResult& b) {
  return b.error.empty() && (JsLiteralHelper<T>::Convert(a) != b.value);
}

template <typename T>
bool operator>=(const T& a, const EvalJsResult& b) {
  return b.error.empty() && (JsLiteralHelper<T>::Convert(a) >= b.value);
}

template <typename T>
bool operator<=(const T& a, const EvalJsResult& b) {
  return b.error.empty() && (JsLiteralHelper<T>::Convert(a) <= b.value);
}

template <typename T>
bool operator<(const T& a, const EvalJsResult& b) {
  return b.error.empty() && (JsLiteralHelper<T>::Convert(a) < b.value);
}

template <typename T>
bool operator>(const T& a, const EvalJsResult& b) {
  return b.error.empty() && (JsLiteralHelper<T>::Convert(a) > b.value);
}

inline bool operator==(nullptr_t a, const EvalJsResult& b) {
  return b.error.empty() && (base::Value() == b.value);
}

// Provides informative failure messages when the result of EvalJs() is
// used in a failing ASSERT_EQ or EXPECT_EQ.
void PrintTo(const EvalJsResult& bar, ::std::ostream* os);

enum EvalJsOptions {
  EXECUTE_SCRIPT_DEFAULT_OPTIONS = 0,

  // By default, EvalJs runs with a user gesture. This bit flag disables
  // that.
  EXECUTE_SCRIPT_NO_USER_GESTURE = (1 << 0),

  // This bit controls how the result is obtained. By default, EvalJs's runner
  // script will call domAutomationController.send() with the completion
  // value. Setting this bit will disable that, requiring |script| to provide
  // its own call to domAutomationController.send() instead.
  EXECUTE_SCRIPT_USE_MANUAL_REPLY = (1 << 1),

  // By default, when the script passed to EvalJs evaluates to a Promise, the
  // execution continues until the Promise resolves, and the resolved value is
  // returned. Setting this bit disables such Promise resolution.
  EXECUTE_SCRIPT_NO_RESOLVE_PROMISES = (1 << 2),
};

// EvalJs() -- run |script| in |execution_target| and return its value or error.
//
// Example simple usage:
//
//   EXPECT_EQ("https://abcd.com", EvalJs(render_frame_host, "self.origin"));
//   EXPECT_EQ(5, EvalJs(render_frame_host, "history.length"));
//   EXPECT_EQ(false, EvalJs(render_frame_host, "history.length > 5"));
//
// The result value of |script| is its "statement completion value" -- the same
// semantics used by Javascript's own eval() function. If |script|
// raises exceptions, or is syntactically invalid, an error is captured instead,
// including a full stack trace.
//
// The return value of EvalJs() may be used directly in EXPECT_EQ()
// macros, and compared for against std::string, int, or any other type for
// which base::Value has a constructor.  If an error was thrown by the script,
// any comparison operators will always return false.
//
// If |script|'s captured completion value is a Promise, this function blocks
// until the Promise is resolved. This enables a usage pattern where |script|
// may call an async function, and use the await keyword to wait for
// events to fire. For example:
//
//   EXPECT_EQ(200, EvalJs(rfh, "(async () => { var resp = (await fetch(url));"
//                              "               return resp.status; })()");
//
// In the above example, the immediately-invoked function expression results in
// a Promise (that's what async functions do); EvalJs will continue blocking
// until the Promise resolves, which happens when the async function returns
// the HTTP status code -- which is expected, in this case, to be 200.
//
// Quick migration guide for users of the classic ExecuteScriptAndExtract*():
//  - If your page has a Content SecurityPolicy, don't migrate [yet]; CSP can
//    interfere with the internal mechanism used here.
//  - Get rid of the out-param. You call EvalJs no matter what your return
//    type is.
//  - If possible, pass the result of EvalJs() into the second argument of an
//    EXPECT_EQ macro. This will trigger failure (and a nice message) if an
//    error occurs.
//  - Eliminate calls to domAutomationController.send() in |script|. In simple
//    cases, |script| is just an expression you want the value of.
//  - When a script previously installed a callback or event listener that
//    invoked domAutomationController.send(x) asynchronously, there is a choice:
//     * Preferred, but more rewriting: Use EvalJs with a Promise which
//       resolves to the value you previously passed to send().
//     * Less rewriting of |script|, but with some drawbacks: Use
//       EXECUTE_SCRIPT_USE_MANUAL_REPLY in |options|, or EvalJsWithManualReply.
//       When specified, this means that |script| must continue to call
//       domAutomationController.send(). Note that this option option disables
//       some error-catching safeguards, but you still get the benefit of having
//       an EvalJsResult that can be passed to EXPECT.
//
// Why prefer EvalJs over ExecuteScriptAndExtractString(), etc? Because:
//
//  - It's one function, that does everything, and more succinctly.
//  - Can be used directly in EXPECT_EQ macros (no out- param pointers like
//    ExecuteScriptAndExtractBool()) -- no temporary variable is required,
//    usually resulting in fewer lines of code.
//  - JS exceptions are reliably captured and will appear as C++ assertion
//    failures.
//  - JS stack traces arising from exceptions are annotated with the
//    corresponding source code; this also appears in C++ assertion failures.
//  - Delayed response is supported via Promises and JS async/await.
//  - |script| doesn't need to call domAutomationController.send directly.
//  - When a script doesn't produce a result, it's likely an assertion
//    failure rather than a hang.  Doesn't get confused by crosstalk with
//    other callers of domAutomationController.send() -- script results carry
//    a GUID.
//  - Lists, dicts, null values, etc. can be returned as base::Values.
EvalJsResult EvalJs(const ToRenderFrameHost& execution_target,
                    const std::string& script,
                    int options = EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                    int world_id = ISOLATED_WORLD_ID_GLOBAL) WARN_UNUSED_RESULT;

// Like EvalJs(), except that |script| must call domAutomationController.send()
// itself. This is the same as specifying the EXECUTE_SCRIPT_USE_MANUAL_REPLY
// option to EvalJs.
EvalJsResult EvalJsWithManualReply(const ToRenderFrameHost& execution_target,
                                   const std::string& script,
                                   int options = EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                   int world_id = ISOLATED_WORLD_ID_GLOBAL)
    WARN_UNUSED_RESULT;

// Run a script exactly the same as EvalJs(), but ignore the resulting value.
//
// Returns AssertionSuccess() if |script| ran successfully, and
// AssertionFailure() if |script| contained a syntax error or threw an
// exception.
//
// Unlike ExecuteScript(), this catches syntax errors and uncaught exceptions,
// and gives more useful error messages when things go wrong. Prefer ExecJs to
// ExecuteScript(), unless your page has a CSP.
::testing::AssertionResult ExecJs(const ToRenderFrameHost& execution_target,
                                  const std::string& script,
                                  int options = EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                  int world_id = ISOLATED_WORLD_ID_GLOBAL)
    WARN_UNUSED_RESULT;

// Walks the frame tree of the specified WebContents and returns the sole
// frame that matches the specified predicate function. This function will
// DCHECK if no frames match the specified predicate, or if more than one
// frame matches.
RenderFrameHost* FrameMatchingPredicate(
    WebContents* web_contents,
    const base::Callback<bool(RenderFrameHost*)>& predicate);

// Predicates for use with FrameMatchingPredicate.
bool FrameMatchesName(const std::string& name, RenderFrameHost* frame);
bool FrameIsChildOfMainFrame(RenderFrameHost* frame);
bool FrameHasSourceUrl(const GURL& url, RenderFrameHost* frame);

// Finds the child frame at the specified |index| for |frame| and returns its
// RenderFrameHost.  Returns nullptr if such child frame does not exist.
RenderFrameHost* ChildFrameAt(RenderFrameHost* frame, size_t index);

// Executes the WebUI resource test runner injecting each resource ID in
// |js_resource_ids| prior to executing the tests.
//
// Returns true if tests ran successfully, false otherwise.
bool ExecuteWebUIResourceTest(WebContents* web_contents,
                              const std::vector<int>& js_resource_ids);

// Returns the serialized cookie string for the given url.
std::string GetCookies(BrowserContext* browser_context, const GURL& url);

// Returns the canonical cookies for the given url.
std::vector<net::CanonicalCookie> GetCanonicalCookies(
    BrowserContext* browser_context,
    const GURL& url);

// Sets a cookie for the given url. Returns true on success.
bool SetCookie(BrowserContext* browser_context,
               const GURL& url,
               const std::string& value);

// Fetch the histograms data from other processes. This should be called after
// the test code has been executed but before performing assertions.
void FetchHistogramsFromChildProcesses();

// Registers a request handler which redirects to a different host, based
// on the request path. The format of the path should be
// "/cross-site/hostname/rest/of/path" to redirect the request to
// "<scheme>://hostname:<port>/rest/of/path", where <scheme> and <port>
// are the values for the instance of EmbeddedTestServer.
//
// By default, redirection will be done using HTTP 302 response, but in some
// cases (e.g. to preserve HTTP method and POST body across redirects as
// prescribed by https://tools.ietf.org/html/rfc7231#section-6.4.7) a test might
// want to use HTTP 307 response instead.  This can be accomplished by replacing
// "/cross-site/" URL substring above with "/cross-site-307/".
//
// |embedded_test_server| should not be running when passing it to this function
// because adding the request handler won't be thread safe.
void SetupCrossSiteRedirector(net::EmbeddedTestServer* embedded_test_server);

// Waits for an interstitial page to attach to given web contents.
void WaitForInterstitialAttach(content::WebContents* web_contents);

// Waits for an interstitial page to detach from given web contents.
void WaitForInterstitialDetach(content::WebContents* web_contents);

// Runs task and waits for an interstitial page to detach from given web
// contents. Prefer this over WaitForInterstitialDetach if web_contents may be
// destroyed by the time WaitForInterstitialDetach is called (e.g. when waiting
// for an interstitial detach after closing a tab).
void RunTaskAndWaitForInterstitialDetach(content::WebContents* web_contents,
                                         const base::Closure& task);

// Waits until all resources have loaded in the given RenderFrameHost.
// When the load completes, this function sends a "pageLoadComplete" message
// via domAutomationController. The caller should make sure this extra
// message is handled properly.
bool WaitForRenderFrameReady(RenderFrameHost* rfh) WARN_UNUSED_RESULT;

// Enable accessibility support for all of the frames in this WebContents
void EnableAccessibilityForWebContents(WebContents* web_contents);

// Wait until the focused accessible node changes in any WebContents.
void WaitForAccessibilityFocusChange();

// Retrieve information about the node that's focused in the accessibility tree.
ui::AXNodeData GetFocusedAccessibilityNodeInfo(WebContents* web_contents);

// This is intended to be a robust way to assert that the accessibility
// tree eventually gets into the correct state, without worrying about
// the exact ordering of events received while getting there.
//
// Searches the accessibility tree to see if any node's accessible name
// is equal to the given name. If not, sets up a notification waiter
// that listens for any accessibility event in any frame, and checks again
// after each event. Keeps looping until the text is found (or the
// test times out).
void WaitForAccessibilityTreeToContainNodeWithName(WebContents* web_contents,
                                                   const std::string& name);

// Get a snapshot of a web page's accessibility tree.
ui::AXTreeUpdate GetAccessibilityTreeSnapshot(WebContents* web_contents);

// Find out if the BrowserPlugin for a guest WebContents is focused. Returns
// false if the WebContents isn't a guest with a BrowserPlugin.
bool IsWebContentsBrowserPluginFocused(content::WebContents* web_contents);

// Returns the RenderWidgetHost that holds the mouse lock.
RenderWidgetHost* GetMouseLockWidget(WebContents* web_contents);

// Returns the RenderWidgetHost that holds the keyboard lock.
RenderWidgetHost* GetKeyboardLockWidget(WebContents* web_contents);

// Returns the RenderWidgetHost that holds mouse capture, if any. This is
// distinct from MouseLock above in that it is a widget that has requested
// implicit capture, such as during a drag. MouseLock is explicitly gained
// through the JavaScript API.
RenderWidgetHost* GetMouseCaptureWidget(WebContents* web_contents);

// Allows tests to drive keyboard lock functionality without requiring access
// to the RenderWidgetHostImpl header or setting up an HTTP test server.
// |codes| represents the set of keys to lock.  If |codes| has no value, then
// all keys will be considered locked.  If |codes| has a value, then at least
// one key must be specified.
bool RequestKeyboardLock(WebContents* web_contents,
                         base::Optional<base::flat_set<ui::DomCode>> codes);
void CancelKeyboardLock(WebContents* web_contents);

// Returns true if inner |interstitial_page| is connected to an outer
// WebContents.
bool IsInnerInterstitialPageConnected(InterstitialPage* interstitial_page);

// Returns the screen orientation provider that's been set via
// WebContents::SetScreenOrientationDelegate(). May return null.
ScreenOrientationDelegate* GetScreenOrientationDelegate();

// Returns all the RenderWidgetHostViews inside the |web_contents| that are
// registered in the RenderWidgetHostInputEventRouter.
std::vector<RenderWidgetHostView*> GetInputEventRouterRenderWidgetHostViews(
    WebContents* web_contents);

// Returns the focused RenderWidgetHost.
RenderWidgetHost* GetFocusedRenderWidgetHost(WebContents* web_contents);

// Returns the focused WebContents.
WebContents* GetFocusedWebContents(WebContents* web_contents);

// Route the |event| through the RenderWidgetHostInputEventRouter. This allows
// correct targeting of events to out of process iframes.
void RouteMouseEvent(WebContents* web_contents, blink::WebMouseEvent* event);

#if defined(USE_AURA)
// The following two methods allow a test to send a touch tap sequence, and
// a corresponding gesture tap sequence, by sending it to the top-level
// WebContents for the page.

// Send a TouchStart/End sequence routed via the main frame's
// RenderWidgetHostViewAura.
void SendRoutedTouchTapSequence(content::WebContents* web_contents,
                                gfx::Point point);

// Send a GestureTapDown/GestureTap sequence routed via the main frame's
// RenderWidgetHostViewAura.
void SendRoutedGestureTapSequence(content::WebContents* web_contents,
                                  gfx::Point point);
#endif  // defined(USE_AURA)

// Watches title changes on a WebContents, blocking until an expected title is
// set.
class TitleWatcher : public WebContentsObserver {
 public:
  // |web_contents| must be non-NULL and needs to stay alive for the
  // entire lifetime of |this|. |expected_title| is the title that |this|
  // will wait for.
  TitleWatcher(WebContents* web_contents,
               const base::string16& expected_title);
  ~TitleWatcher() override;

  // Adds another title to watch for.
  void AlsoWaitForTitle(const base::string16& expected_title);

  // Waits until the title matches either expected_title or one of the titles
  // added with AlsoWaitForTitle. Returns the value of the most recently
  // observed matching title.
  const base::string16& WaitAndGetTitle() WARN_UNUSED_RESULT;

 private:
  // Overridden WebContentsObserver methods.
  void DidStopLoading() override;
  void TitleWasSet(NavigationEntry* entry) override;

  void TestTitle();

  std::vector<base::string16> expected_titles_;
  base::RunLoop run_loop_;

  // The most recently observed expected title, if any.
  base::string16 observed_title_;

  DISALLOW_COPY_AND_ASSIGN(TitleWatcher);
};

// Watches a RenderProcessHost and waits for specified destruction events.
class RenderProcessHostWatcher : public RenderProcessHostObserver {
 public:
  enum WatchType {
    WATCH_FOR_PROCESS_EXIT,
    WATCH_FOR_HOST_DESTRUCTION
  };

  RenderProcessHostWatcher(RenderProcessHost* render_process_host,
                           WatchType type);
  // Waits for the render process that contains the specified web contents.
  RenderProcessHostWatcher(WebContents* web_contents, WatchType type);
  ~RenderProcessHostWatcher() override;

  // Waits until the renderer process exits.
  void Wait();

  // Returns true if a renderer process exited cleanly (without hitting
  // RenderProcessExited with an abnormal TerminationStatus). This should be
  // called after Wait().
  bool did_exit_normally() { return did_exit_normally_; }

 private:
  // Overridden RenderProcessHost::LifecycleObserver methods.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  RenderProcessHost* render_process_host_;
  WatchType type_;
  bool did_exit_normally_;

  base::RunLoop run_loop_;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostWatcher);
};

// Watches for responses from the DOMAutomationController and keeps them in a
// queue. Useful for waiting for a message to be received.
class DOMMessageQueue : public NotificationObserver,
                        public WebContentsObserver {
 public:
  // Constructs a DOMMessageQueue and begins listening for messages from the
  // DOMAutomationController. Do not construct this until the browser has
  // started.
  DOMMessageQueue();

  // Same as the default constructor, but only listens for messages
  // sent from a particular |web_contents|.
  explicit DOMMessageQueue(WebContents* web_contents);

  ~DOMMessageQueue() override;

  // Removes all messages in the message queue.
  void ClearQueue();

  // Wait for the next message to arrive. |message| will be set to the next
  // message. Returns true on success.
  bool WaitForMessage(std::string* message) WARN_UNUSED_RESULT;

  // If there is a message in the queue, then copies it to |message| and returns
  // true.  Otherwise (if the queue is empty), returns false.
  bool PopMessage(std::string* message) WARN_UNUSED_RESULT;

  // Overridden NotificationObserver methods.
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // Overridden WebContentsObserver methods.
  void RenderProcessGone(base::TerminationStatus status) override;

 private:
  NotificationRegistrar registrar_;
  base::queue<std::string> message_queue_;
  base::OnceClosure quit_closure_;
  bool renderer_crashed_ = false;

  DISALLOW_COPY_AND_ASSIGN(DOMMessageQueue);
};

// Used to wait for a new WebContents to be created. Instantiate this object
// before the operation that will create the window.
class WebContentsAddedObserver {
 public:
  WebContentsAddedObserver();
  ~WebContentsAddedObserver();

  // Will run a message loop to wait for the new window if it hasn't been
  // created since the constructor
  WebContents* GetWebContents();

  // Will tell whether RenderViewCreated Callback has invoked
  bool RenderViewCreatedCalled();

 private:
  class RenderViewCreatedObserver;

  void WebContentsCreated(WebContents* web_contents);

  // Callback to WebContentCreated(). Cached so that we can unregister it.
  base::Callback<void(WebContents*)> web_contents_created_callback_;

  WebContents* web_contents_;
  std::unique_ptr<RenderViewCreatedObserver> child_observer_;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsAddedObserver);
};

// Watches a WebContents to check if it was destroyed.
class WebContentsDestroyedObserver : public WebContentsObserver {
 public:
  explicit WebContentsDestroyedObserver(WebContents* web_contents);
  ~WebContentsDestroyedObserver() override;
  bool IsDestroyed() { return destroyed_; }

 private:
  // Overridden WebContentsObserver methods.
  void WebContentsDestroyed() override;

  bool destroyed_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDestroyedObserver);
};

// Request a new frame be drawn, returns false if request fails.
bool RequestFrame(WebContents* web_contents);

// This class is intended to synchronize upon the submission of compositor
// frames from the renderer to the display compositor.
//
// This class enables observation of the provided
// RenderFrameMetadataProvider. Which notifies this of every
// subsequent frame submission. Observation ends upon the destruction of this
// class.
//
// Calling Wait will block the browser ui thread until the next time the
// renderer submits a frame.
//
// Tests interested in the associated RenderFrameMetadata will find it cached
// in the RenderFrameMetadataProvider.
class RenderFrameSubmissionObserver
    : public RenderFrameMetadataProvider::Observer {
 public:
  explicit RenderFrameSubmissionObserver(
      RenderFrameMetadataProvider* render_frame_metadata_provider);
  explicit RenderFrameSubmissionObserver(FrameTreeNode* node);
  explicit RenderFrameSubmissionObserver(WebContents* web_contents);
  ~RenderFrameSubmissionObserver() override;

  // Resets the current |render_frame_count|;
  void ResetCounter() { render_frame_count_ = 0; }

  // Blocks the browser ui thread until the next OnRenderFrameSubmission.
  void WaitForAnyFrameSubmission();

  // Blocks the browser ui thread until the next
  // OnRenderFrameMetadataChangedAfterActivation.
  void WaitForMetadataChange();

  // Blocks the browser ui thread until RenderFrameMetadata arrives where its
  // scroll offset matches |expected_offset|.
  void WaitForScrollOffset(const gfx::Vector2dF& expected_offset);

  // Blocks the browser ui thread until RenderFrameMetadata arrives where its
  // scroll offset at top matches |expected_scroll_offset_at_top|.
  void WaitForScrollOffsetAtTop(bool expected_scroll_offset_at_top);

  const cc::RenderFrameMetadata& LastRenderFrameMetadata() const;

  // Returns the number of frames submitted since the observer's creation.
  int render_frame_count() const { return render_frame_count_; }

 private:
  // Exits |run_loop_| unblocking the UI thread. Execution will resume in Wait.
  void Quit();

  // Blocks the browser ui thread.
  void Wait();

  // RenderFrameMetadataProvider::Observer
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override;
  void OnRenderFrameMetadataChangedAfterActivation() override;
  void OnRenderFrameSubmission() override;
  void OnLocalSurfaceIdChanged(
      const cc::RenderFrameMetadata& metadata) override;

  // If true then the next OnRenderFrameSubmission will cancel the blocking
  // |run_loop_| otherwise the blocking will continue until the next
  // OnRenderFrameMetadataChangedAfterActivation.
  bool break_on_any_frame_ = false;

  RenderFrameMetadataProvider* render_frame_metadata_provider_ = nullptr;
  base::OnceClosure quit_closure_;
  int render_frame_count_ = 0;
};

// This class is intended to synchronize the renderer main thread, renderer impl
// thread and the browser main thread.
//
// This is accomplished by sending an IPC to RenderWidget, then blocking until
// the ACK is received and processed.
//
// When the main thread receives the ACK it is enqueued. The queue is not
// processed until a new FrameToken is received.
//
// So while the ACK can arrive before a CompositorFrame submission occurs. The
// processing does not occur until after the FrameToken for that frame
// submission arrives to the main thread.
class MainThreadFrameObserver : public IPC::Listener {
 public:
  explicit MainThreadFrameObserver(RenderWidgetHost* render_widget_host);
  ~MainThreadFrameObserver() override;

  // Synchronizes the browser main thread with the renderer main thread and impl
  // thread.
  void Wait();

  // Overridden IPC::Listener methods.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  void Quit();

  RenderWidgetHost* render_widget_host_;
  base::OnceClosure quit_closure_;
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadFrameObserver);
};

// Watches for an input msg to be consumed.
class InputMsgWatcher : public RenderWidgetHost::InputEventObserver {
 public:
  InputMsgWatcher(RenderWidgetHost* render_widget_host,
                  blink::WebInputEvent::Type type);
  ~InputMsgWatcher() override;

  bool HasReceivedAck() const;

  // Wait until ack message occurs, returning the ack result from
  // the message.
  InputEventAckState WaitForAck();

  // Wait for the ack if it hasn't been received, if it has been
  // received return the result immediately.
  InputEventAckState GetAckStateWaitIfNecessary();

  InputEventAckSource last_event_ack_source() const { return ack_source_; }

 private:
  // Overridden InputEventObserver methods.
  void OnInputEventAck(InputEventAckSource source,
                       InputEventAckState state,
                       const blink::WebInputEvent&) override;

  RenderWidgetHost* render_widget_host_;
  blink::WebInputEvent::Type wait_for_type_;
  InputEventAckState ack_result_;
  InputEventAckSource ack_source_;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(InputMsgWatcher);
};

// Used to wait for a desired input event ack.
class InputEventAckWaiter : public RenderWidgetHost::InputEventObserver {
 public:
  // A function determining if a given |event| and its ack are what we're
  // waiting for.
  using InputEventAckPredicate =
      base::RepeatingCallback<bool(InputEventAckSource source,
                                   InputEventAckState state,
                                   const blink::WebInputEvent& event)>;

  // Wait for an event satisfying |predicate|.
  InputEventAckWaiter(RenderWidgetHost* render_widget_host,
                      InputEventAckPredicate predicate);
  // Wait for any event of the given |type|.
  InputEventAckWaiter(RenderWidgetHost* render_widget_host,
                      blink::WebInputEvent::Type type);
  ~InputEventAckWaiter() override;

  void Wait();
  void Reset();

  // RenderWidgetHost::InputEventObserver:
  void OnInputEventAck(InputEventAckSource source,
                       InputEventAckState state,
                       const blink::WebInputEvent& event) override;

 private:
  RenderWidgetHost* render_widget_host_;
  InputEventAckPredicate predicate_;
  bool event_received_;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(InputEventAckWaiter);
};

// Sets up a ui::TestClipboard for use in browser tests. On Windows,
// clipboard is handled on the IO thread, BrowserTestClipboardScope
// hops messages onto the right thread.
class BrowserTestClipboardScope {
 public:
  // Sets up a ui::TestClipboard.
  BrowserTestClipboardScope();

  // Tears down the clipboard.
  ~BrowserTestClipboardScope();

  // Puts text/rtf |rtf| on the clipboard.
  void SetRtf(const std::string& rtf);

  // Puts plain text |text| on the clipboard.
  void SetText(const std::string& text);

  // Gets plain text from the clipboard, if any.
  void GetText(std::string* text);

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserTestClipboardScope);
};

// This observer is used to wait for its owner Frame to become focused.
class FrameFocusedObserver {
 public:
  explicit FrameFocusedObserver(RenderFrameHost* owner_host);
  ~FrameFocusedObserver();

  void Wait();

 private:
  // Private impl struct which hides non public types including FrameTreeNode.
  class FrameTreeNodeObserverImpl;

  // FrameTreeNode::Observer
  std::unique_ptr<FrameTreeNodeObserverImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(FrameFocusedObserver);
};

// This observer is used to wait for its owner FrameTreeNode to become deleted.
class FrameDeletedObserver {
 public:
  explicit FrameDeletedObserver(RenderFrameHost* owner_host);
  ~FrameDeletedObserver();

  void Wait();

 private:
  // Private impl struct which hides non public types including FrameTreeNode.
  class FrameTreeNodeObserverImpl;

  // FrameTreeNode::Observer
  std::unique_ptr<FrameTreeNodeObserverImpl> impl_;

  DISALLOW_COPY_AND_ASSIGN(FrameDeletedObserver);
};

// This class can be used to pause and resume navigations, based on a URL
// match. Note that it only keeps track of one navigation at a time.
// Navigations are paused automatically before hitting the network, and are
// resumed automatically if a Wait method is called for a future event.
// Note: This class is one time use only! After it successfully tracks a
// navigation it will ignore all subsequent navigations. Explicitly create
// multiple instances of this class if you want to pause multiple navigations.
class TestNavigationManager : public WebContentsObserver {
 public:
  // Monitors any frame in WebContents.
  TestNavigationManager(WebContents* web_contents, const GURL& url);

  ~TestNavigationManager() override;

  // Waits until the navigation request is ready to be sent to the network
  // stack. Returns false if the request was aborted before starting.
  WARN_UNUSED_RESULT bool WaitForRequestStart();

  // Waits until the navigation response's headers have been received. Returns
  // false if the request was aborted before getting a response.
  WARN_UNUSED_RESULT bool WaitForResponse();

  // Waits until the navigation has been finished. Will automatically resume
  // navigations paused before this point.
  void WaitForNavigationFinished();

  // Resume the navigation.
  // * Called after |WaitForRequestStart|, it causes the request to be sent.
  // * Called after |WaitForResponse|, it causes the response to be committed.
  void ResumeNavigation();

  // Returns the NavigationHandle associated with the navigation. It is non-null
  // only in between DidStartNavigation(...) and DidFinishNavigation(...).
  NavigationHandle* GetNavigationHandle();

  // Whether the navigation successfully committed.
  bool was_successful() const { return was_successful_; }

 protected:
  // Derived classes can override if they want to filter out navigations. This
  // is called from DidStartNavigation.
  virtual bool ShouldMonitorNavigation(NavigationHandle* handle);

 private:
  enum class NavigationState {
    INITIAL = 0,
    STARTED = 1,
    RESPONSE = 2,
    FINISHED = 3,
  };

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* handle) override;
  void DidFinishNavigation(NavigationHandle* handle) override;

  // Called when the NavigationThrottle pauses the navigation in
  // WillStartRequest.
  void OnWillStartRequest();

  // Called when the NavigationThrottle pauses the navigation in
  // WillProcessResponse.
  void OnWillProcessResponse();

  // Waits for the desired state. Returns false if the desired state cannot be
  // reached (eg the navigation finishes before reaching this state).
  bool WaitForDesiredState();

  // Called when the state of the navigation has changed. This will either stop
  // the message loop if the state specified by the user has been reached, or
  // resume the navigation if it hasn't been reached yet.
  void OnNavigationStateChanged();

  const GURL url_;
  NavigationHandle* handle_;
  bool navigation_paused_;
  NavigationState current_state_;
  NavigationState desired_state_;
  bool was_successful_ = false;
  base::OnceClosure quit_closure_;

  base::WeakPtrFactory<TestNavigationManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestNavigationManager);
};

class NavigationHandleCommitObserver : public content::WebContentsObserver {
 public:
  NavigationHandleCommitObserver(content::WebContents* web_contents,
                                 const GURL& url);

  bool has_committed() const { return has_committed_; }
  bool was_same_document() const { return was_same_document_; }
  bool was_renderer_initiated() const { return was_renderer_initiated_; }

 private:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  const GURL url_;
  bool has_committed_;
  bool was_same_document_;
  bool was_renderer_initiated_;
};

// A WebContentsDelegate that catches messages sent to the console.
class ConsoleObserverDelegate : public WebContentsDelegate {
 public:
  ConsoleObserverDelegate(WebContents* web_contents, const std::string& filter);
  ~ConsoleObserverDelegate() override;

  // WebContentsDelegate method:
  bool DidAddMessageToConsole(WebContents* source,
                              int32_t level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;

  // Returns the most recent message sent to the console.
  std::string message() { return message_; }

  // Waits for the next message captured by the filter to be sent to the
  // console.
  void Wait();

 private:
  WebContents* web_contents_;
  std::string filter_;
  std::string message_;

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ConsoleObserverDelegate);
};

// Static methods that inject particular IPCs into the message pipe as if they
// came from |process|. Used to simulate a compromised renderer.
class PwnMessageHelper {
 public:
  // Sends BlobHostMsg_RegisterPublicURL
  static void RegisterBlobURL(RenderProcessHost* process,
                              GURL url,
                              std::string uuid);

  // Sends FileSystemHostMsg_Create
  static void FileSystemCreate(RenderProcessHost* process,
                               int request_id,
                               GURL path,
                               bool exclusive,
                               bool is_directory,
                               bool recursive);

  // Sends FileSystemHostMsg_Write
  static void FileSystemWrite(RenderProcessHost* process,
                              int request_id,
                              GURL file_path,
                              std::string blob_uuid,
                              int64_t position);

  // Sends WidgetHostMsg_LockMouse
  static void LockMouse(RenderProcessHost* process,
                        int routing_id,
                        bool user_gesture,
                        bool privileged);

 private:
  PwnMessageHelper();  // Not instantiable.

  DISALLOW_COPY_AND_ASSIGN(PwnMessageHelper);
};

#if defined(USE_AURA)
// Mock of an OverscrollController so we can inspect the scroll events that it
// receives. Note that this is only a partial mock as the methods of a real
// OverscrollController are being invoked.
// TODO(mcnee): Tests needing this are BrowserPlugin specific. Remove after
// removing BrowserPlugin (crbug.com/533069).
class MockOverscrollController {
 public:
  // Creates a mock and installs it on the given RenderWidgetHostViewAura.
  // The returned mock is owned by the RWHVA.
  static MockOverscrollController* Create(RenderWidgetHostView* rwhv);

  virtual ~MockOverscrollController() {}

  // Waits until the mock receives a consumed GestureScrollUpdate.
  virtual void WaitForConsumedScroll() = 0;
};
#endif  // defined(USE_AURA)

// This class filters for FrameHostMsg_ContextMenu messages coming in
// from a renderer process, and allows observing the ContextMenuParams
// as sent by the renderer.
class ContextMenuFilter : public content::BrowserMessageFilter {
 public:
  ContextMenuFilter();

  bool OnMessageReceived(const IPC::Message& message) override;
  void Wait();

  content::ContextMenuParams get_params() { return last_params_; }

 private:
  ~ContextMenuFilter() override;

  void OnContextMenu(const content::ContextMenuParams& params);

  std::unique_ptr<base::RunLoop> run_loop_;
  base::OnceClosure quit_closure_;
  content::ContextMenuParams last_params_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuFilter);
};

WebContents* GetEmbedderForGuest(content::WebContents* guest);

// Returns true if the network service is enabled and it's running in the
// browser process.
bool IsNetworkServiceRunningInProcess();

// Load the given |url| with |network_context| and return the |net::Error| code.
int LoadBasicRequest(network::mojom::NetworkContext* network_context,
                     const GURL& url,
                     int process_id = 0,
                     int render_frame_id = 0,
                     int load_flags = net::LOAD_NORMAL);

// Ensures that all StoragePartitions for the given BrowserContext have their
// cookies flushed to disk.
void EnsureCookiesFlushed(BrowserContext* browser_context);

// Returns true if there is a valid process for |process_group_name|. Must be
// called on the IO thread.
bool HasValidProcessForProcessGroup(const std::string& process_group_name);

// Performs a simple auto-resize flow and ensures that the embedder gets a
// single response messages back from the guest, with the expected values.
bool TestChildOrGuestAutoresize(bool is_guest,
                                RenderProcessHost* embedder_rph,
                                RenderWidgetHost* guest_rwh);

// Class to sniff incoming IPCs for either
// FrameHostMsg_SynchronizeVisualProperties or
// BrowserPluginHostMsg_SynchronizeVisualProperties messages. This allows the
// message to continue to the target child so that processing can be verified by
// tests.
class SynchronizeVisualPropertiesMessageFilter
    : public content::BrowserMessageFilter {
 public:
  SynchronizeVisualPropertiesMessageFilter();

  gfx::Rect last_rect() const { return last_rect_; }

  void WaitForRect();
  void ResetRectRunLoop();

  // Returns the new viz::FrameSinkId immediately if the IPC has been received.
  // Otherwise this will block the UI thread until it has been received, then it
  // will return the new viz::FrameSinkId.
  viz::FrameSinkId GetOrWaitForId();

  // Waits for the next viz::LocalSurfaceId be received and returns it.
  viz::LocalSurfaceId WaitForSurfaceId();

 protected:
  ~SynchronizeVisualPropertiesMessageFilter() override;

 private:
  void OnSynchronizeFrameHostVisualProperties(
      const viz::SurfaceId& surface_id,
      const FrameVisualProperties& visual_properties);
  void OnSynchronizeBrowserPluginVisualProperties(
      int browser_plugin_guest_instance_id,
      viz::LocalSurfaceId surface_id,
      FrameVisualProperties visual_properties);
  void OnSynchronizeVisualProperties(
      const viz::LocalSurfaceId& surface_id,
      const viz::FrameSinkId& frame_sink_id,
      const FrameVisualProperties& visual_properties);
  // |rect| is in DIPs.
  void OnUpdatedFrameRectOnUI(const gfx::Rect& rect);
  void OnUpdatedFrameSinkIdOnUI();
  void OnUpdatedSurfaceIdOnUI(viz::LocalSurfaceId surface_id);

  bool OnMessageReceived(const IPC::Message& message) override;

  static const uint32_t kMessageClassesToFilter[2];
  viz::FrameSinkId frame_sink_id_;
  base::RunLoop frame_sink_id_run_loop_;

  std::unique_ptr<base::RunLoop> screen_space_rect_run_loop_;
  bool screen_space_rect_received_;
  gfx::Rect last_rect_;

  viz::LocalSurfaceId last_surface_id_;
  std::unique_ptr<base::RunLoop> surface_id_run_loop_;

  DISALLOW_COPY_AND_ASSIGN(SynchronizeVisualPropertiesMessageFilter);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_
