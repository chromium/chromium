// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/queue.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/types/strong_alias.h"
#include "base/types/to_address.h"
#include "build/build_config.h"
#include "cc/test/pixel_test_utils.h"
#include "content/public/browser/commit_deferring_condition.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_metadata_provider.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_media_capture_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/common/page_type.h"
#include "content/public/test/test_utils.h"
#include "ipc/message_filter.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/load_flags.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"
#include "storage/common/file_system/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-test-utils.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom.h"
#include "third_party/blink/public/mojom/frame/user_activation_update_types.mojom.h"
#include "third_party/blink/public/mojom/keyboard_lock/keyboard_lock.mojom-shared.h"
#include "third_party/blink/public/mojom/navigation/navigation_params.mojom-shared.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/display/display_switches.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace base::test {
class ScopedFeatureList;
}

namespace gfx {
class Point;
}  // namespace gfx

namespace net {
class CanonicalCookie;
namespace test_server {
class EmbeddedTestServer;
}  // namespace test_server

// TODO(svaldez): Remove typedef once EmbeddedTestServer has been migrated
// out of net::test_server.
using test_server::EmbeddedTestServer;
}  // namespace net

namespace ui {
class AXPlatformNodeDelegate;
class AXTreeID;
}  // namespace ui

#if BUILDFLAG(IS_WIN)
namespace Microsoft {
namespace WRL {
template <typename>
class ComPtr;
}  // namespace WRL
}  // namespace Microsoft

typedef int PROPERTYID;
#endif

// A collections of functions designed for use with content_browsertests and
// browser_tests.
// TO BE CLEAR: any function here must work against both binaries. If it only
// works with browser_tests, it should be in chrome\test\base\ui_test_utils.h.
// If it only works with content_browsertests, it should be in
// content\test\content_browser_test_utils.h.

namespace blink {
class StorageKey;
struct TransferableMessage;

namespace mojom {
class FrameWidget;
}  // namespace mojom

}  // namespace blink

namespace storage {
class BlobUrlRegistry;
}

namespace content {

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
class MockCapturedSurfaceController;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if defined(USE_AURA)
class SelectionBoundsWaiter;
#endif  // defined(USE_AURA)

class BrowserContext;
class FileSystemAccessPermissionContext;
class FrameTreeNode;
class NavigationHandle;
class NavigationRequest;
class RenderFrameMetadataProviderImpl;
class RenderFrameProxyHost;
class RenderWidgetHost;
class RenderWidgetHostImpl;
class RenderWidgetHostView;
class ScopedAllowRendererCrashes;
class ToRenderFrameHost;
class WebContents;

// This encapsulates the pattern of waiting for an event and returning whether
// that event was received from `Wait`. This makes it easy to do the right thing
// in Wait, i.e. return with `[[nodiscard]]`.
class WaiterHelper {
 public:
  // Wait until OnEvent is called. Will return true if ended by OnEvent or false
  // if ended for some other reason (e.g. timeout).
  [[nodiscard]] bool Wait();
  // Steps the waiting.
  void OnEvent();

 private:
  [[nodiscard]] bool WaitInternal();
  base::RunLoop run_loop_;
  bool event_received_ = false;
};

// Navigates |web_contents| to |url|, blocking until the navigation finishes.
// Returns true if the page was loaded successfully and the last committed URL
// matches |url|.  This is a browser-initiated navigation that simulates a user
// typing |url| into the address bar.
[[nodiscard]] bool NavigateToURL(WebContents* web_contents, const GURL& url);

// Same as above, but takes in an additional URL, |expected_commit_url|, to
// which the navigation should eventually commit.  This is useful for cases
// like redirects, where navigation starts on one URL but ends up committing a
// different URL.  This function will return true if navigating to |url|
// results in a successful commit to |expected_commit_url|.
[[nodiscard]] bool NavigateToURL(WebContents* web_contents,
                                 const GURL& url,
                                 const GURL& expected_commit_url);

// Navigates |web_contents| to |url|, blocking until the given number of
// navigations finishes. If |ignore_uncommitted_navigations| is true, then an
// aborted navigation also counts toward |number_of_navigations| being complete.
void NavigateToURLBlockUntilNavigationsComplete(
    WebContents* web_contents,
    const GURL& url,
    int number_of_navigations,
    bool ignore_uncommitted_navigations = true);
// Same, but allows specifying the full LoadURLParams instead of just the URL.
void NavigateToURLBlockUntilNavigationsComplete(
    WebContents* web_contents,
    const NavigationController::LoadURLParams& params,
    int number_of_navigations,
    bool ignore_uncommitted_navigations = true);

// Helpers for performing renderer-initiated navigations. Note these helpers are
// only suitable when the navigation is expected to at least start successfully,
// i.e. result in a `DidStartNavigation()` notification.
//
// To write a test where the renderer itself blocks or cancels the navigation,
// use `ExecJs()` or `EvalJs()`, e.g.:
//
//   EXPECT_THAT(ExecJs("try { location = ''; } catch (e) { e.message; }")
//                   .ExtractString(),
//               HasSubStr("..."));
//
// To write a test where the navigation results in a bad message kill, use
// `ExecuteScriptAsync()` + `RenderProcessHostBadMojoMessageWaiter`, e.g.:
//
//   ExecuteScriptAsync(rfh, JsReplace("location = $1", "/title2.html"));
//   RenderProcessHostBadMojoMessageWaiter kill_waiter(rfh->GetProcess());
//   EXPECT_THAT(kill_waiter.Wait(), Optional(HasSubstr("...")));

// Perform a renderer-initiated navigation of the frame |adapter| to |url|,
// blocking until the navigation finishes. The navigation is done by assigning
// location.href in the frame |adapter|. Returns true if the page was loaded
// successfully and the last committed URL matches |url|.
[[nodiscard]] bool NavigateToURLFromRenderer(const ToRenderFrameHost& adapter,
                                             const GURL& url);
// Similar to above but takes in an additional URL, |expected_commit_url|, to
// which the navigation should eventually commit. (See the browser-initiated
// counterpart for more details).
[[nodiscard]] bool NavigateToURLFromRenderer(const ToRenderFrameHost& adapter,
                                             const GURL& url,
                                             const GURL& expected_commit_url);

// Similar to the two helpers above, but perform the renderer-initiated
// navigation without a user gesture.
[[nodiscard]] bool NavigateToURLFromRendererWithoutUserGesture(
    const ToRenderFrameHost& adapter,
    const GURL& url);

[[nodiscard]] bool NavigateToURLFromRendererWithoutUserGesture(
    const ToRenderFrameHost& adapter,
    const GURL& url,
    const GURL& expected_commit_url);

// Perform a renderer-initiated navigation of the frame `adapter` to `url`.
// Returns true if the navigation started successfully and false otherwise.
[[nodiscard]] bool BeginNavigateToURLFromRenderer(
    const ToRenderFrameHost& adapter,
    const GURL& url);

// Navigate a frame with ID |iframe_id| to |url|, blocking until the navigation
// finishes.  Uses a renderer-initiated navigation from script code in the
// main frame.
//
// This method does not trigger a user activation before the navigation.  If
// necessary, a user activation can be triggered right before calling this
// method, e.g. by calling |ExecJs(frame_tree_node, "")|.
bool NavigateIframeToURL(WebContents* web_contents,
                         std::string_view iframe_id,
                         const GURL& url);

// Similar to |NavigateIframeToURL()| but returns as soon as the navigation is
// initiated.
bool BeginNavigateIframeToURL(WebContents* web_contents,
                              std::string_view iframe_id,
                              const GURL& url);

// Generate a URL for a file path including a query string.
GURL GetFileUrlWithQuery(const base::FilePath& path,
                         std::string_view query_string);

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
// This function collects everything that needs to be done, except for user
// activation which is triggered only when |trigger_user_activation| is true.
// Note that beforeunload dialog attempts are ignored unless the frame has
// received a user activation.
void PrepContentsForBeforeUnloadTest(WebContents* web_contents,
                                     bool trigger_user_activation = true);

#if defined(USE_AURA) || BUILDFLAG(IS_ANDROID)
// If WebContent's view is currently being resized, this will wait for the ack
// from the renderer that the resize is complete and for the
// WindowEventDispatcher to release the pointer moves. If there's no resize in
// progress, the method will return right away.
void WaitForResizeComplete(WebContents* web_contents);
#endif  // defined(USE_AURA) || BUILDFLAG(IS_ANDROID)

void NotifyCopyableViewInWebContents(WebContents* web_contents,
                                     base::OnceClosure done_callback);
void NotifyCopyableViewInFrame(RenderFrameHost* render_frame_host,
                               base::OnceClosure done_callback);

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

// Test helper to check the content-internal CanCommitURL() method on
// ChildProcessSecurityPolicy, determining whether a particular process is
// allowed to commit a navigation to `url`.
bool CanCommitURLForTesting(int child_id, const GURL& url);

// Causes the specified web_contents to issue an OnUnresponsiveRenderer event
// to its observers.
void SimulateUnresponsiveRenderer(WebContents* web_contents,
                                  RenderWidgetHost* widget);

// Simulates clicking at the center of the given tab asynchronously; modifiers
// may contain bits from WebInputEvent::Modifiers. Sends the event through
// RenderWidgetHostInputEventRouter and thus can target OOPIFs. If an OOPIF is
// the intended target, ensure that its hit test data is available for routing,
// using `WaitForHitTestData`, first.
// Note: For simulating clicks inside a fenced frame tree, this function does
// not work. Use `SimulateClickInFencedFrameTree` in
// `content/public/test/fenced_frame_test_util.cc`
void SimulateMouseClick(WebContents* web_contents,
                        int modifiers,
                        blink::WebMouseEvent::Button button);

// Simulates clicking at the point |point| of the given tab asynchronously;
// modifiers may contain bits from WebInputEvent::Modifiers. Sends the event
// through RenderWidgetHostInputEventRouter and thus can target OOPIFs. If an
// OOPIF is the intended target, ensure that its hit test data is available for
// routing, using `WaitForHitTestData`, first.
// Note: For simulating clicks inside a fenced frame tree, this function does
// not work. Use `SimulateClickInFencedFrameTree` in
// `content/public/test/fenced_frame_test_util.cc`
void SimulateMouseClickAt(WebContents* web_contents,
                          int modifiers,
                          blink::WebMouseEvent::Button button,
                          const gfx::Point& point);

// Retrieves the center coordinates of the element with id |id|.
// ATTENTION: When using these coordinates to simulate a click or tap make sure
// that the viewport is not zoomed as the coordinates returned by this method
// are relative to the page not the viewport. In particular for Android make
// sure the page has the meta tag
// <meta name="viewport" content="width=device-width,minimum-scale=1">
// TODO(crbug.com/40177926): Make the Simulate* methods more user
// friendly by taking zooming into account.
gfx::PointF GetCenterCoordinatesOfElementWithId(
    const ToRenderFrameHost& adapter,
    std::string_view id);

// Retrieves the center coordinates of the element with id |id| and simulates a
// mouse click there using SimulateMouseClickAt().
void SimulateMouseClickOrTapElementWithId(content::WebContents* web_contents,
                                          std::string_view id);

// Simulates asynchronously a mouse enter/move/leave event. The mouse event is
// routed through RenderWidgetHostInputEventRouter and thus can target OOPIFs.
void SimulateMouseEvent(WebContents* web_contents,
                        blink::WebInputEvent::Type type,
                        const gfx::Point& point);
void SimulateMouseEvent(WebContents* web_contents,
                        blink::WebInputEvent::Type type,
                        blink::WebMouseEvent::Button button,
                        const gfx::Point& point);

// Simulate a mouse wheel event.
void SimulateMouseWheelEvent(WebContents* web_contents,
                             const gfx::Point& point,
                             const gfx::Vector2d& delta,
                             const blink::WebMouseWheelEvent::Phase phase);

#if !BUILDFLAG(IS_MAC)
// Simulate a mouse wheel event with the ctrl modifier set.
void SimulateMouseWheelCtrlZoomEvent(RenderWidgetHost* render_widget_host,
                                     const gfx::Point& point,
                                     bool zoom_in,
                                     blink::WebMouseWheelEvent::Phase phase);

void SimulateTouchscreenPinch(WebContents* web_contents,
                              const gfx::PointF& anchor,
                              float scale_change,
                              base::OnceClosure on_complete);

#endif  // !BUILDFLAG(IS_MAC)

// Sends a GesturePinch Begin/Update/End sequence.
void SimulateGesturePinchSequence(RenderWidgetHost* render_widget_host,
                                  const gfx::Point& point,
                                  float scale,
                                  blink::WebGestureDevice source_device);

void SimulateGesturePinchSequence(WebContents* web_contents,
                                  const gfx::Point& point,
                                  float scale,
                                  blink::WebGestureDevice source_device);

// Sends a simple, three-event (Begin/Update/End) gesture scroll.
void SimulateGestureScrollSequence(RenderWidgetHost* render_widget_host,
                                   const gfx::Point& point,
                                   const gfx::Vector2dF& delta);

void SimulateGestureScrollSequence(WebContents* web_contents,
                                   const gfx::Point& point,
                                   const gfx::Vector2dF& delta);

void SimulateGestureEvent(RenderWidgetHost* render_widget_host,
                          const blink::WebGestureEvent& gesture_event,
                          const ui::LatencyInfo& latency);

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
// Generates a TouchEvent of |event_type| at |point|.
void SimulateTouchEventAt(WebContents* web_contents,
                          ui::EventType event_type,
                          const gfx::Point& point);

void SimulateLongTapAt(WebContents* web_contents, const gfx::Point& point);

// Can be used to wait for the caret bounds associated with `web_contents` to
// have non-zero size.
class NonZeroCaretSizeWaiter {
 public:
  explicit NonZeroCaretSizeWaiter(WebContents* web_contents);
  NonZeroCaretSizeWaiter(const NonZeroCaretSizeWaiter&) = delete;
  NonZeroCaretSizeWaiter& operator=(const NonZeroCaretSizeWaiter&) = delete;
  ~NonZeroCaretSizeWaiter();

  void Wait();

 private:
  std::unique_ptr<SelectionBoundsWaiter> selection_bounds_waiter_;
};

// Can be used to wait for an update to the caret bounds associated with
// `web_contents`.
class CaretBoundsUpdateWaiter {
 public:
  explicit CaretBoundsUpdateWaiter(WebContents* web_contents);
  CaretBoundsUpdateWaiter(const CaretBoundsUpdateWaiter&) = delete;
  CaretBoundsUpdateWaiter& operator=(const CaretBoundsUpdateWaiter&) = delete;
  ~CaretBoundsUpdateWaiter();

  void Wait();

 private:
  std::unique_ptr<SelectionBoundsWaiter> selection_bounds_waiter_;
};

// Can be used to wait for updates to the bounding box (i.e. the rectangle
// enclosing the selection region) associated with `web_contents`.
class BoundingBoxUpdateWaiter {
 public:
  explicit BoundingBoxUpdateWaiter(WebContents* web_contents);
  BoundingBoxUpdateWaiter(const BoundingBoxUpdateWaiter&) = delete;
  BoundingBoxUpdateWaiter& operator=(const BoundingBoxUpdateWaiter&) = delete;
  ~BoundingBoxUpdateWaiter();

  void Wait();

 private:
  std::unique_ptr<SelectionBoundsWaiter> selection_bounds_waiter_;
};
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

// Simulates `source_render_frame_host` sending `message` to
// `target_render_frame_host` through a `RenderFrameProxyHost`. This allows
// testing corner cases where postMessage() should not be allowed, even from a
// compromised renderer. `target_render_frame_host`'s frame must have a
// `RenderFrameProxyHost` in `source_render_frame_host`'s `SiteInstanceGroup`.
void SimulateProxyHostPostMessage(RenderFrameHost* source_render_frame_host,
                                  RenderFrameHost* target_render_frame_host,
                                  blink::TransferableMessage message);

// Reset touch action for the embedder of a BrowserPluginGuest.
void ResetTouchAction(RenderWidgetHost* host);

// Spins a run loop until effects of previously forwarded input are fully
// realized.
void RunUntilInputProcessed(RenderWidgetHost* host);

// Returns a string representation of a given |referrer_policy|. This is used to
// setup <meta name=referrer> tags in documents used for referrer-policy-based
// tests. The value `no-meta` indicates no tag should be created.
std::string ReferrerPolicyToString(
    network::mojom::ReferrerPolicy referrer_policy);

// For testing, bind FakeFrameWidget to a RenderWidgetHost associated
// with a given RenderFrameHost
mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>
BindFakeFrameWidgetInterfaces(RenderFrameHost* frame);

// Set |active| state for a RenderWidgetHost associated with a given
// RenderFrameHost
void SimulateActiveStateForWidget(RenderFrameHost* frame, bool active);

// Return the value set for VisitedLinkSalt in the navigation's commit_params.
std::optional<uint64_t> GetVisitedLinkSaltForNavigation(
    NavigationHandle* navigation_handle);

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

  ScopedSimulateModifierKeyPress(const ScopedSimulateModifierKeyPress&) =
      delete;
  ScopedSimulateModifierKeyPress& operator=(
      const ScopedSimulateModifierKeyPress&) = delete;

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
  const raw_ptr<WebContents> web_contents_;
  int modifiers_;
  const bool control_;
  const bool shift_;
  const bool alt_;
  const bool command_;
};

// Method to check what devices we have on the system.
bool IsWebcamAvailableOnSystem(WebContents* web_contents);

// Allow ExecJs/EvalJs methods to target either a WebContents or a
// RenderFrameHost.  Targeting a WebContents means executing the script in the
// RenderFrameHost returned by WebContents::GetPrimaryMainFrame(), which is the
// main frame.  Pass a specific RenderFrameHost to target it. Embedders may
// declare additional ConvertToRenderFrameHost functions for convenience.
class ToRenderFrameHost {
 public:
  template <typename Ptr>
    requires requires(Ptr p) { base::to_address(p); }
  // NOLINTNEXTLINE(google-explicit-constructor)
  ToRenderFrameHost(Ptr frame_convertible_value)
      : render_frame_host_(ConvertToRenderFrameHost(
            base::to_address(frame_convertible_value))) {}

  // Extract the underlying frame.
  RenderFrameHost* render_frame_host() const { return render_frame_host_; }

 private:
  raw_ptr<RenderFrameHost, DanglingUntriaged> render_frame_host_;
};

RenderFrameHost* ConvertToRenderFrameHost(RenderFrameHost* render_view_host);
RenderFrameHost* ConvertToRenderFrameHost(WebContents* web_contents);

// Executes the passed |script| in the specified frame with a user gesture,
// without waiting for |script| completion.
//
// See also:
// - ExecJs (if you want to wait for script completion, or detect syntax errors)
// - EvalJs (if you want to retrieve a value)
// - DOMMessageQueue (to manually wait for domAutomationController.send(...))
void ExecuteScriptAsync(const ToRenderFrameHost& adapter,
                        std::string_view script);

// Same as `content::ExecuteScriptAsync()`, but doesn't send a user gesture to
// the renderer.
void ExecuteScriptAsyncWithoutUserGesture(const ToRenderFrameHost& adapter,
                                          std::string_view script);

// JsLiteralHelper is a helper class that determines what types are legal to
// pass to StringifyJsLiteral. Legal types include int, string,
// std::string_view, char*, bool, double, GURL, url::Origin, and base::Value&&.
template <typename T>
struct JsLiteralHelper {
  // This generic version enables passing any type from which base::Value can be
  // instantiated. This covers int, string, double, bool, base::Value&&, etc.
  template <typename U>
  static base::Value Convert(U&& arg) {
    return base::Value(std::forward<U>(arg));
  }

  static base::Value Convert(const base::Value& value) { return value.Clone(); }
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

// Construct a list-type base::Value from a mix of arguments.
//
// Each |arg| can be any type explicitly convertible to base::Value
// (including int/string/std::string_view/char*/double/bool), or any type that
// JsLiteralHelper is specialized for -- like URL and url::Origin, which emit
// string literals. |args| can be a mix of different types.
template <typename... Args>
base::Value ListValueOf(Args&&... args) {
  base::Value::List values;
  (values.Append(JsLiteralHelper<std::remove_cvref_t<Args>>::Convert(
       std::forward<Args>(args))),
   ...);
  return base::Value(std::move(values));
}

// Replaces $1, $2, $3, ..., $9 in |script_template| with JS literal values
// constructed from |args|, similar to base::ReplaceStringPlaceholders. Note
// that $10 and above aren't allowed.
//
// Unlike StringPrintf or manual concatenation, this version will properly
// escape string content, even if it contains slashes or quotation marks.
//
// Each |arg| can be any type explicitly convertible to base::Value
// (including int/string/std::string_view/char*/double/bool), or any type that
// JsLiteralHelper is specialized for -- like URL and url::Origin, which emit
// string literals. |args| can be a mix of different types.
//
// Example 1:
//
//   GURL page_url("http://example.com");
//   EXPECT_TRUE(ExecJs(
//       shell(), JsReplace("window.open($1, '_blank');", page_url)));
//
// $1 is replaced with a double-quoted JS string literal:
// "http://example.com". Note that quotes around $1 are not required.
//
// Example 2:
//
//   bool forced_reload = true;
//   EXPECT_TRUE(ExecJs(
//       shell(), JsReplace("window.location.reload($1);", forced_reload)));
//
// This becomes "window.location.reload(true);" -- because bool values are
// supported by base::Value. Numbers, lists, and dicts also work.
template <typename... Args>
std::string JsReplace(std::string_view script_template, Args&&... args) {
  base::Value::List values =
      ListValueOf(std::forward<Args>(args)...).TakeList();
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

  // Creates an EvalJs result. If |error| is non-empty, |value| will be
  // ignored.
  EvalJsResult(base::Value value, std::string_view error);

  // Copy ctor.
  EvalJsResult(const EvalJsResult& value);

  // Matchers for successful & unsuccessful runs.
  static auto IsOk() {
    return testing::Field(&EvalJsResult::error, testing::Eq(""));
  }
  static auto IsError() { return testing::Not(IsOk()); }

  // Extract a result value of the requested type, or die trying.
  //
  // If there was an error, or if returned value is of a different type, these
  // will fail with a CHECK. Use Extract methods only when accessing the
  // result value is necessary; prefer operator== and EXPECT_EQ() instead:
  // they don't CHECK, and give better error messages.
  [[nodiscard]] const std::string& ExtractString() const;
  [[nodiscard]] int ExtractInt() const;
  [[nodiscard]] bool ExtractBool() const;
  [[nodiscard]] double ExtractDouble() const;
  [[nodiscard]] base::Value ExtractList() const;
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
bool operator==(const EvalJsResult& a, const T& b) {
  return b == a;
}

template <typename T>
bool operator!=(const T& a, const EvalJsResult& b) {
  return b.error.empty() && (JsLiteralHelper<T>::Convert(a) != b.value);
}
template <typename T>
bool operator!=(const EvalJsResult& a, const T& b) {
  return b != a;
}

template <typename T>
bool operator>=(const T& a, const EvalJsResult& b) {
  return b.error.empty() && (JsLiteralHelper<T>::Convert(a) >= b.value);
}
template <typename T>
bool operator>=(const EvalJsResult& a, const T& b) {
  return b < a;
}

template <typename T>
bool operator<=(const T& a, const EvalJsResult& b) {
  return b.error.empty() && (JsLiteralHelper<T>::Convert(a) <= b.value);
}
template <typename T>
bool operator<=(const EvalJsResult& a, const T& b) {
  return b > a;
}

template <typename T>
bool operator<(const T& a, const EvalJsResult& b) {
  return b.error.empty() && (JsLiteralHelper<T>::Convert(a) < b.value);
}
template <typename T>
bool operator<(const EvalJsResult& a, const T& b) {
  return b >= a;
}

template <typename T>
bool operator>(const T& a, const EvalJsResult& b) {
  return b.error.empty() && (JsLiteralHelper<T>::Convert(a) > b.value);
}
template <typename T>
bool operator>(const EvalJsResult& a, const T& b) {
  return b <= a;
}

inline bool operator==(std::nullptr_t a, const EvalJsResult& b) {
  return b.error.empty() && (base::Value() == b.value);
}
template <typename T>
inline bool operator==(const EvalJsResult& a, std::nullptr_t b) {
  return nullptr == a;
}

// Provides informative failure messages when the result of EvalJs() is
// used in a failing ASSERT_EQ or EXPECT_EQ.
std::ostream& operator<<(std::ostream& os, const EvalJsResult& bar);

enum EvalJsOptions {
  EXECUTE_SCRIPT_DEFAULT_OPTIONS = 0,

  // By default, EvalJs() runs with a user gesture. This bit flag disables that.
  EXECUTE_SCRIPT_NO_USER_GESTURE = (1 << 0),

  // By default, when the script passed to EvalJs evaluates to a Promise, the
  // execution continues until the Promise resolves, and the resolved value is
  // returned. Setting this bit disables such Promise resolution.
  EXECUTE_SCRIPT_NO_RESOLVE_PROMISES = (1 << 1),

  // Content settings are a mechanism that allows users to disable various
  // functions of the browser. One content setting disables javascript. By
  // default, EvalJs() ignores this setting. This bit flag causes it to instead
  // honor JS content settings.
  EXECUTE_SCRIPT_HONOR_JS_CONTENT_SETTINGS = (1 << 2),
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
//  - If possible, pass the result of EvalJs() into the second argument of an
//    EXPECT_EQ macro. This will trigger failure (and a nice message) if an
//    error occurs.
//  - JS exceptions are reliably captured and will appear as C++ assertion
//    failures.
//  - JS stack traces arising from exceptions are annotated with the
//    corresponding source code; this also appears in C++ assertion failures.
//  - Delayed response is supported via Promises and JS async/await.
//  - |script| doesn't need to call domAutomationController.send directly.
//  - When a script doesn't produce a result, it's likely an assertion
//    failure rather than a hang.  Doesn't get confused by crosstalk with
//    callers of domAutomationController.send() -- EvalJs does not rely on
//    domAutomationController.
//  - Lists, dicts, null values, etc. can be returned as base::Values.
//  - |after_script_invoke| is an optional callback which will be invoked after
//    script execution has started in the renderer but before the RunLoop is
//    blocked on the result.
//
// It is guaranteed that EvalJs works even when the target frame is frozen.
[[nodiscard]] EvalJsResult EvalJs(
    const ToRenderFrameHost& execution_target,
    std::string_view script,
    int options = EXECUTE_SCRIPT_DEFAULT_OPTIONS,
    int32_t world_id = ISOLATED_WORLD_ID_GLOBAL,
    base::OnceClosure after_script_invoke = base::DoNothing());

// Like EvalJs(), but runs |raf_script| inside a requestAnimationFrame handler,
// and runs |script| after the rendering update has completed. By the time
// this method returns, any IPCs sent from the renderer process to the browser
// process during the lifecycle update should already have been received and
// processed by the browser.
[[nodiscard]] EvalJsResult EvalJsAfterLifecycleUpdate(
    const ToRenderFrameHost& execution_target,
    std::string_view raf_script,
    std::string_view script,
    int options = EXECUTE_SCRIPT_DEFAULT_OPTIONS,
    int32_t world_id = ISOLATED_WORLD_ID_GLOBAL);

// Run a script exactly the same as EvalJs(), but ignore the resulting value.
//
// Returns AssertionSuccess() if |script| ran successfully, and
// AssertionFailure() if |script| contained a syntax error or threw an
// exception.
//
// As with EvalJs(), if the script passed evaluates to a Promise, this waits
// until it resolves (by default).
[[nodiscard]] ::testing::AssertionResult ExecJs(
    const ToRenderFrameHost& execution_target,
    std::string_view script,
    int options = EXECUTE_SCRIPT_DEFAULT_OPTIONS,
    int32_t world_id = ISOLATED_WORLD_ID_GLOBAL);

// Walks the frame tree of the specified `page`, also descending into any inner
// frame-trees (e.g. GuestView), and returns the sole frame that matches the
// specified predicate function. Returns nullptr if no frame matches.
// EXPECTs that at most one frame matches.
RenderFrameHost* FrameMatchingPredicateOrNullptr(
    Page& page,
    base::RepeatingCallback<bool(RenderFrameHost*)> predicate);

// Same like FrameMatchingPredicateOrNullptr(), but EXPECTs that exactly one
// frame matches.
RenderFrameHost* FrameMatchingPredicate(
    Page& page,
    base::RepeatingCallback<bool(RenderFrameHost*)> predicate);

// Predicates for use with FrameMatchingPredicate[OrNullPtr]().
bool FrameMatchesName(std::string_view name, RenderFrameHost* frame);
bool FrameIsChildOfMainFrame(RenderFrameHost* frame);
bool FrameHasSourceUrl(const GURL& url, RenderFrameHost* frame);

// Finds the child frame at the specified |index| for |adapter| and returns its
// RenderFrameHost.  Returns nullptr if such child frame does not exist.
RenderFrameHost* ChildFrameAt(const ToRenderFrameHost& adapter, size_t index);

// Returns true if |frame| has origin-keyed process isolation due to the
// OriginAgentCluster header.
bool HasOriginKeyedProcess(RenderFrameHost* frame);

// Returns true if `frame` has a sandboxed SiteInstance.
bool HasSandboxedSiteInstance(RenderFrameHost* frame);

// Returns the frames visited by |RenderFrameHost::ForEachRenderFrameHost| in
// the same order.
std::vector<RenderFrameHost*> CollectAllRenderFrameHosts(
    RenderFrameHost* starting_rfh);
// Returns the frames visited by |RenderFrameHost::ForEachRenderFrameHost| on
// |page|'s main document in the same order.
std::vector<RenderFrameHost*> CollectAllRenderFrameHosts(Page& page);
// Returns the frames visited by |WebContents::ForEachRenderFrameHost| in
// the same order.
std::vector<RenderFrameHost*> CollectAllRenderFrameHosts(
    WebContents* web_contents);

// Returns all WebContents. Note that this includes WebContents from any
// BrowserContext.
std::vector<WebContents*> GetAllWebContents();

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Executes the WebUI resource tests. Injects the test runner script prior to
// executing the tests.
//
// Returns true if tests ran successfully, false otherwise.
bool ExecuteWebUIResourceTest(WebContents* web_contents);
#endif

// Returns the serialized cookie string for the given url. Uses an inclusive
// SameSiteCookieContext by default, which gets cookies regardless of their
// SameSite attribute.
std::string GetCookies(
    BrowserContext* browser_context,
    const GURL& url,
    net::CookieOptions::SameSiteCookieContext context =
        net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
    net::CookiePartitionKeyCollection key_collection =
        net::CookiePartitionKeyCollection::ContainsAll());

// Returns the canonical cookies for the given url.
std::vector<net::CanonicalCookie> GetCanonicalCookies(
    BrowserContext* browser_context,
    const GURL& url,
    net::CookiePartitionKeyCollection key_collection =
        net::CookiePartitionKeyCollection::ContainsAll());

// Sets a cookie for the given url. Uses inclusive SameSiteCookieContext by
// default, which gets cookies regardless of their SameSite attribute. The
// cookie is unpartitioned by default. Returns true on success.
bool SetCookie(BrowserContext* browser_context,
               const GURL& url,
               const std::string& value,
               net::CookieOptions::SameSiteCookieContext context =
                   net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
               net::CookiePartitionKey* cookie_partition_key = nullptr);

// Deletes cookies matching the provided filter. Returns the number of cookies
// that were deleted.
uint32_t DeleteCookies(BrowserContext* browser_context,
                       network::mojom::CookieDeletionFilter filter);

// Fetches the histograms data from other processes.
//
// This function should be called after a child process has logged the
// histogram/metric being tested, to ensure that base::HistogramTester sees all
// the data from the child process.
//
// The caller should ensure that there is no race between 1) the call to
// FetchHistogramsFromChildProcesses() and 2) the child process shutting down.
// See also https://crbug.com/1246137 which describes how this is a test-only
// problem.
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

// Sets the access permission context in FileSystemAccessManagerImpl.
void SetFileSystemAccessPermissionContext(
    BrowserContext* browser_context,
    FileSystemAccessPermissionContext* permission_context);

// Waits until all resources have loaded in the given RenderFrameHost.
[[nodiscard]] bool WaitForRenderFrameReady(RenderFrameHost* rfh);

// Wait until the focused accessible node changes in any WebContents.
void WaitForAccessibilityFocusChange();

// Retrieve information about the node that's focused in the accessibility tree.
ui::AXNodeData GetFocusedAccessibilityNodeInfo(WebContents* web_contents);

// This is intended to be a robust way to assert that the accessibility
// tree eventually gets into the correct state, without worrying about
// the exact ordering of events received while getting there. Blocks
// until any change happens to the accessibility tree.
void WaitForAccessibilityTreeToChange(WebContents* web_contents);

// Searches the accessibility tree to see if any node's accessible name
// is equal to the given name. If not, repeatedly calls
// WaitForAccessibilityTreeToChange, above, and then checks again.
// Keeps looping until the text is found (or the test times out).
void WaitForAccessibilityTreeToContainNodeWithName(WebContents* web_contents,
                                                   std::string_view name);

// Get a snapshot of a web page's accessibility tree.
ui::AXTreeUpdate GetAccessibilityTreeSnapshot(WebContents* web_contents);

// Get a snapshot of an accessibility tree given a `tree_id`.
ui::AXTreeUpdate GetAccessibilityTreeSnapshotFromId(
    const ui::AXTreeID& tree_id);

// Returns the root accessibility node for the given WebContents.
ui::AXPlatformNodeDelegate* GetRootAccessibilityNode(WebContents* web_contents);

// Finds an accessibility node matching the given criteria.
struct FindAccessibilityNodeCriteria {
  FindAccessibilityNodeCriteria();
  ~FindAccessibilityNodeCriteria();
  std::optional<ax::mojom::Role> role;
  std::optional<std::string> name;
};
ui::AXPlatformNodeDelegate* FindAccessibilityNode(
    WebContents* web_contents,
    const FindAccessibilityNodeCriteria& criteria);
ui::AXPlatformNodeDelegate* FindAccessibilityNodeInSubtree(
    ui::AXPlatformNodeDelegate* node,
    const FindAccessibilityNodeCriteria& criteria);

#if BUILDFLAG(IS_WIN)
// Retrieve the specified interface from an accessibility node.
template <typename T>
Microsoft::WRL::ComPtr<T> QueryInterfaceFromNode(
    ui::AXPlatformNodeDelegate* node);

// Call GetPropertyValue with the given UIA property id with variant type
// VT_ARRAY | VT_UNKNOWN  on the target browser accessibility node to retrieve
// an array of automation elements, then validate the name property of the
// automation elements with the expected names.
void UiaGetPropertyValueVtArrayVtUnknownValidate(
    PROPERTYID property_id,
    ui::AXPlatformNodeDelegate* target_node,
    const std::vector<std::string>& expected_names);
#endif

// Returns the RenderWidgetHost that holds the keyboard lock.
RenderWidgetHost* GetKeyboardLockWidget(WebContents* web_contents);

// Returns the RenderWidgetHost that holds the mouse lock.
RenderWidgetHost* GetMouseLockWidget(WebContents* web_contents);

// Allows tests to drive keyboard lock functionality without requiring access
// to the RenderWidgetHostImpl header or setting up an HTTP test server.
// |codes| represents the set of keys to lock.  If |codes| has no value, then
// all keys will be considered locked.  If |codes| has a value, then at least
// one key must be specified.
void RequestKeyboardLock(
    WebContents* web_contents,
    std::optional<base::flat_set<ui::DomCode>> codes,
    base::OnceCallback<void(blink::mojom::KeyboardLockRequestResult)> callback);
void CancelKeyboardLock(WebContents* web_contents);

// Returns the screen orientation provider that's been set via
// WebContents::SetScreenOrientationDelegate(). May return null.
ScreenOrientationDelegate* GetScreenOrientationDelegate();

// Returns all the RenderWidgetHostViews inside the |web_contents| that are
// registered in the RenderWidgetHostInputEventRouter.
std::vector<RenderWidgetHostView*> GetInputEventRouterRenderWidgetHostViews(
    WebContents* web_contents);

// Returns the focused RenderWidgetHost.
RenderWidgetHost* GetFocusedRenderWidgetHost(WebContents* web_contents);

// Returns whether or not the RenderWidgetHost thinks it is focused.
bool IsRenderWidgetHostFocused(const RenderWidgetHost*);

// Returns the focused WebContents.
WebContents* GetFocusedWebContents(WebContents* web_contents);

// Watches title changes on a WebContents, blocking until an expected title is
// set.
class TitleWatcher : public WebContentsObserver {
 public:
  // |web_contents| must be non-NULL and needs to stay alive for the
  // entire lifetime of |this|. |expected_title| is the title that |this|
  // will wait for.
  TitleWatcher(WebContents* web_contents, std::u16string_view expected_title);

  TitleWatcher(const TitleWatcher&) = delete;
  TitleWatcher& operator=(const TitleWatcher&) = delete;

  ~TitleWatcher() override;

  // Adds another title to watch for.
  void AlsoWaitForTitle(std::u16string_view expected_title);

  // Waits until the title matches either expected_title or one of the titles
  // added with AlsoWaitForTitle. Returns the value of the most recently
  // observed matching title.
  [[nodiscard]] const std::u16string& WaitAndGetTitle();

 private:
  // Overridden WebContentsObserver methods.
  void DidStopLoading() override;
  void TitleWasSet(NavigationEntry* entry) override;

  void TestTitle();

  std::vector<std::u16string> expected_titles_;
  base::RunLoop run_loop_;

  // The most recently observed expected title, if any.
  std::u16string observed_title_;
};

// Watches a RenderProcessHost and waits for a specified lifecycle event.
class RenderProcessHostWatcher : public RenderProcessHostObserver {
 public:
  enum WatchType {
    WATCH_FOR_PROCESS_READY,
    WATCH_FOR_PROCESS_EXIT,
    WATCH_FOR_HOST_DESTRUCTION
  };

  RenderProcessHostWatcher(RenderProcessHost* render_process_host,
                           WatchType type);
  // Waits for the renderer process that contains the specified web contents.
  RenderProcessHostWatcher(WebContents* web_contents, WatchType type);

  RenderProcessHostWatcher(const RenderProcessHostWatcher&) = delete;
  RenderProcessHostWatcher& operator=(const RenderProcessHostWatcher&) = delete;

  ~RenderProcessHostWatcher() override;

  // Waits until the expected event is triggered. This may only be called once.
  void Wait();

  // Returns true if a renderer process exited cleanly (without hitting
  // RenderProcessExited with an abnormal TerminationStatus). This should be
  // called after Wait().
  bool did_exit_normally() { return did_exit_normally_; }

 private:
  // Quit the run loop and clean up.
  void QuitRunLoop();

  // Overridden RenderProcessHost::LifecycleObserver methods.
  void RenderProcessReady(RenderProcessHost* host) override;
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  base::ScopedObservation<RenderProcessHost, RenderProcessHostObserver>
      observation_{this};
  WatchType type_;
  bool did_exit_normally_;

  std::unique_ptr<ScopedAllowRendererCrashes> allow_renderer_crashes_;

  base::RunLoop run_loop_;
  base::OnceClosure quit_closure_;
};

// Implementation helper for:
// *) content-internal content::RenderProcessHostBadIpcMessageWaiter
//    (declared in //content/test/content_browser_test_utils_internal.h)
// *) content-public content::RenderProcessHostBadMojoMessageWaiter
//    (declared below)
// *) maybe in the future: similar helpers for chrome-layer BadMessageReason
class RenderProcessHostKillWaiter {
 public:
  // |uma_name| is the name of the histogram from which the |bad_message_reason|
  // can be extracted.
  RenderProcessHostKillWaiter(RenderProcessHost* render_process_host,
                              std::string_view uma_name);

  RenderProcessHostKillWaiter(const RenderProcessHostKillWaiter&) = delete;
  RenderProcessHostKillWaiter& operator=(const RenderProcessHostKillWaiter&) =
      delete;

  // Waits until the renderer process exits.  Extracts and returns the bad
  // message reason that should be logged in the |uma_name_| histogram.
  // Returns |std::nullopt| if the renderer exited normally or didn't log
  // the |uma_name_| histogram.
  [[nodiscard]] std::optional<int> Wait();

 private:
  RenderProcessHostWatcher exit_watcher_;
  base::HistogramTester histogram_tester_;
  std::string uma_name_;
};

// Helps tests to wait until the given renderer process is terminated because of
// a bad/invalid mojo message.
//
// Example usage:
//   RenderProcessHostBadMojoMessageWaiter kill_waiter(render_process_host);
//   ... test code that triggers a renderer kill ...
//   EXPECT_EQ("expected error message", kill_waiter.Wait());
class RenderProcessHostBadMojoMessageWaiter {
 public:
  explicit RenderProcessHostBadMojoMessageWaiter(
      RenderProcessHost* render_process_host);
  ~RenderProcessHostBadMojoMessageWaiter();

  RenderProcessHostBadMojoMessageWaiter(
      const RenderProcessHostBadMojoMessageWaiter&) = delete;
  RenderProcessHostBadMojoMessageWaiter& operator=(
      const RenderProcessHostBadMojoMessageWaiter&) = delete;

  // Waits until |render_process_host| from the constructor is terminated
  // because of a bad/invalid mojo message and returns the associated error
  // string.  Returns std::nullopt if the process was terminated for an
  // unrelated reason.
  [[nodiscard]] std::optional<std::string> Wait();

 private:
  void OnBadMojoMessage(int render_process_id, const std::string& error);

  int monitored_render_process_id_;
  std::optional<std::string> observed_mojo_error_;
  RenderProcessHostKillWaiter kill_waiter_;
};

// Watches for responses from the DOMAutomationController and keeps them in a
// queue. Useful for waiting for a message to be received.
class DOMMessageQueue {
 public:
  // Constructs a DOMMessageQueue and begins listening for messages from the
  // DOMAutomationController. Do not construct this until the browser has
  // started.
  // NOTE: Use one of the below constructors if observing messages for a single
  // WebContents instance.
  DOMMessageQueue();

  // Same as the default constructor, but only listens for messages
  // sent from a particular |web_contents|.
  explicit DOMMessageQueue(WebContents* web_contents);

  // Same as the constructor with a WebContents, but observes the
  // RenderFrameHost deletion.
  explicit DOMMessageQueue(RenderFrameHost* render_frame_host);

  DOMMessageQueue(const DOMMessageQueue&) = delete;
  DOMMessageQueue& operator=(const DOMMessageQueue&) = delete;

  ~DOMMessageQueue();

  // Removes all messages in the message queue.
  void ClearQueue();

  // Wait for the next message to arrive. |message| will be set to the next
  // message. Returns true on success.
  [[nodiscard]] bool WaitForMessage(std::string* message);

  // Facilitates asynchronous use of the DOMMessageQueue. The given callback
  // will be called once a message was put into the queue (or when there is a
  // renderer crash).
  // NOTE: This is incompatible with the DOMMessageQueue(RenderFrameHost*) ctor.
  void SetOnMessageAvailableCallback(base::OnceClosure callback);

  // If there is a message in the queue, then copies it to |message| and returns
  // true.  Otherwise (if the queue is empty), returns false.
  [[nodiscard]] bool PopMessage(std::string* message);

  // Returns true if there are currently any messages in the queue.
  bool HasMessages();

 private:
  class MessageObserver;
  friend class MessageObserver;

  void OnDomMessageReceived(const std::string& message);
  void PrimaryMainFrameRenderProcessGone(base::TerminationStatus status);
  void RenderFrameDeleted();

  void OnWebContentsCreated(WebContents* contents);
  void OnBackingWebContentsDestroyed(MessageObserver* observer);

  std::set<std::unique_ptr<MessageObserver>> observers_;
  base::CallbackListSubscription web_contents_creation_subscription_;
  base::queue<std::string> message_queue_;
  base::OnceClosure callback_;
  bool renderer_crashed_ = false;
};

// Used to wait for a new WebContents to be created. Instantiate this object
// before the operation that will create the window.
class WebContentsAddedObserver {
 public:
  WebContentsAddedObserver();

  WebContentsAddedObserver(const WebContentsAddedObserver&) = delete;
  WebContentsAddedObserver& operator=(const WebContentsAddedObserver&) = delete;

  ~WebContentsAddedObserver();

  // Will run a message loop to wait for the new window if it hasn't been
  // created since the constructor
  WebContents* GetWebContents();

 private:
  void WebContentsCreated(WebContents* web_contents);

  base::CallbackListSubscription creation_subscription_;

  raw_ptr<WebContents, AcrossTasksDanglingUntriaged> web_contents_ = nullptr;
  base::OnceClosure quit_closure_;
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
      RenderFrameMetadataProviderImpl* render_frame_metadata_provider);
  explicit RenderFrameSubmissionObserver(FrameTreeNode* node);
  explicit RenderFrameSubmissionObserver(WebContents* web_contents);
  explicit RenderFrameSubmissionObserver(RenderFrameHost* rfh);
  ~RenderFrameSubmissionObserver() override;

  // Resets the current |render_frame_count|;
  void ResetCounter() { render_frame_count_ = 0; }

  // Blocks the browser ui thread until the next OnRenderFrameSubmission.
  void WaitForAnyFrameSubmission();

  // Blocks the browser ui thread until the next
  // OnRenderFrameMetadataChangedAfterActivation.
  void WaitForMetadataChange();

  // Blocks the browser ui thread until RenderFrameMetadata arrives with
  // page scale factor matching |expected_page_scale_factor|.
  void WaitForPageScaleFactor(float expected_page_scale_factor,
                              const float tolerance);

  // Blocks the browser ui thread until RenderFrameMetadata arrives with
  // external page scale factor matching |expected_external_page_scale_factor|.
  void WaitForExternalPageScaleFactor(float expected_external_page_scale_factor,
                                      const float tolerance);

  // Blocks the browser ui thread until RenderFrameMetadata arrives where its
  // scroll offset matches |expected_offset|.
  void WaitForScrollOffset(const gfx::PointF& expected_offset);

  // Blocks the browser ui thread until RenderFrameMetadata arrives where its
  // scroll offset at top matches |expected_scroll_offset_at_top|.
  void WaitForScrollOffsetAtTop(bool expected_scroll_offset_at_top);

  const cc::RenderFrameMetadata& LastRenderFrameMetadata() const;

  // Returns the number of frames submitted since the observer's creation.
  int render_frame_count() const { return render_frame_count_; }

  // Runs |closure| the next time metadata changes.
  void NotifyOnNextMetadataChange(base::OnceClosure closure);

 private:
  // Exits |run_loop_| unblocking the UI thread. Execution will resume in Wait.
  void Quit();

  // Blocks the browser ui thread.
  void Wait();

  // RenderFrameMetadataProvider::Observer
  void OnRenderFrameMetadataChangedBeforeActivation(
      const cc::RenderFrameMetadata& metadata) override;
  void OnRenderFrameMetadataChangedAfterActivation(
      base::TimeTicks activation_time) override;
  void OnRenderFrameSubmission() override;
  void OnLocalSurfaceIdChanged(
      const cc::RenderFrameMetadata& metadata) override;

  // If true then the next OnRenderFrameSubmission will cancel the blocking
  // |run_loop_| otherwise the blocking will continue until the next
  // OnRenderFrameMetadataChangedAfterActivation.
  bool break_on_any_frame_ = false;

  raw_ptr<RenderFrameMetadataProviderImpl> render_frame_metadata_provider_ =
      nullptr;
  base::OnceClosure quit_closure_;
  // If non-null, run when metadata changes.
  base::OnceClosure metadata_change_closure_;
  int render_frame_count_ = 0;
};

// This class is intended to synchronize the renderer main thread, renderer impl
// thread and the browser main thread.
//
// This is accomplished by sending an IPC to RenderWidget, then blocking until
// the ACK is received and processed.
//
// The ACK is sent from compositor thread, when the CompositorFrame is submited
// to the display compositor
// TODO(danakj): This class seems to provide the same as
// RenderFrameSubmissionObserver, consider using that instead.
class MainThreadFrameObserver {
 public:
  explicit MainThreadFrameObserver(RenderWidgetHost* render_widget_host);

  MainThreadFrameObserver(const MainThreadFrameObserver&) = delete;
  MainThreadFrameObserver& operator=(const MainThreadFrameObserver&) = delete;

  ~MainThreadFrameObserver();

  // Synchronizes the browser main thread with the renderer main thread and impl
  // thread.
  void Wait();

 private:
  void Quit(bool);

  // This dangling raw_ptr occurred in:
  // browser_tests: All/NavigationPageLoadMetricsBrowserTest.FirstInputDelay/1
  // https://ci.chromium.org/ui/p/chromium/builders/try/win-rel/180209/test-results?q=ExactID%3Aninja%3A%2F%2Fchrome%2Ftest%3Abrowser_tests%2FNavigationPageLoadMetricsBrowserTest.FirstInputDelay%2FAll.1+VHash%3Abdbee181b3e0309b
  raw_ptr<RenderWidgetHost, FlakyDanglingUntriaged> render_widget_host_;
  base::OnceClosure quit_closure_;
  int routing_id_;
};

// Watches for an input msg to be consumed.
class InputMsgWatcher : public RenderWidgetHost::InputEventObserver {
 public:
  InputMsgWatcher(RenderWidgetHost* render_widget_host,
                  blink::WebInputEvent::Type type);

  InputMsgWatcher(const InputMsgWatcher&) = delete;
  InputMsgWatcher& operator=(const InputMsgWatcher&) = delete;

  ~InputMsgWatcher() override;

  bool HasReceivedAck() const;

  // Wait until ack message occurs, returning the ack result from
  // the message.
  blink::mojom::InputEventResultState WaitForAck();

  // Wait for the ack if it hasn't been received, if it has been
  // received return the result immediately.
  blink::mojom::InputEventResultState GetAckStateWaitIfNecessary();

  blink::mojom::InputEventResultSource last_event_ack_source() const {
    return ack_source_;
  }

  blink::WebInputEvent::Type last_sent_event_type() const {
    return last_sent_event_type_;
  }

 private:
  // Overridden InputEventObserver methods.
  void OnInputEventAck(blink::mojom::InputEventResultSource source,
                       blink::mojom::InputEventResultState state,
                       const blink::WebInputEvent&) override;

  void OnInputEvent(const blink::WebInputEvent&) override;

  raw_ptr<RenderWidgetHost> render_widget_host_;
  blink::WebInputEvent::Type last_sent_event_type_ =
      blink::WebInputEvent::Type::kUndefined;
  blink::WebInputEvent::Type wait_for_type_;
  blink::mojom::InputEventResultState ack_result_;
  blink::mojom::InputEventResultSource ack_source_;
  base::OnceClosure quit_closure_;
};

// Used to wait for a desired input event ack.
class InputEventAckWaiter : public RenderWidgetHost::InputEventObserver {
 public:
  // A function determining if a given |event| and its ack are what we're
  // waiting for.
  using InputEventAckPredicate =
      base::RepeatingCallback<bool(blink::mojom::InputEventResultSource source,
                                   blink::mojom::InputEventResultState state,
                                   const blink::WebInputEvent& event)>;

  // Wait for an event satisfying |predicate|.
  InputEventAckWaiter(RenderWidgetHost* render_widget_host,
                      InputEventAckPredicate predicate);
  // Wait for any event of the given |type|.
  InputEventAckWaiter(RenderWidgetHost* render_widget_host,
                      blink::WebInputEvent::Type type);

  InputEventAckWaiter(const InputEventAckWaiter&) = delete;
  InputEventAckWaiter& operator=(const InputEventAckWaiter&) = delete;

  ~InputEventAckWaiter() override;

  void Wait();
  void Reset();

  // RenderWidgetHost::InputEventObserver:
  void OnInputEventAck(blink::mojom::InputEventResultSource source,
                       blink::mojom::InputEventResultState state,
                       const blink::WebInputEvent& event) override;

 private:
  base::WeakPtr<RenderWidgetHostImpl> render_widget_host_;
  InputEventAckPredicate predicate_;
  bool event_received_ = false;
  base::OnceClosure quit_closure_;
};

// Sets up a ui::TestClipboard for use in browser tests. On Windows,
// clipboard is handled on the IO thread, BrowserTestClipboardScope
// hops messages onto the right thread.
class BrowserTestClipboardScope {
 public:
  // Sets up a ui::TestClipboard.
  BrowserTestClipboardScope();

  BrowserTestClipboardScope(const BrowserTestClipboardScope&) = delete;
  BrowserTestClipboardScope& operator=(const BrowserTestClipboardScope&) =
      delete;

  // Tears down the clipboard.
  ~BrowserTestClipboardScope();

  // Puts text/rtf |rtf| on the clipboard.
  void SetRtf(const std::string& rtf);

  // Puts plain text |text| on the clipboard.
  void SetText(const std::string& text);

  // Gets plain text from the clipboard, if any.
  void GetText(std::string* text);
};

// This observer is used to wait for its owner Frame to become focused.
class FrameFocusedObserver {
 public:
  explicit FrameFocusedObserver(RenderFrameHost* owner_host);

  FrameFocusedObserver(const FrameFocusedObserver&) = delete;
  FrameFocusedObserver& operator=(const FrameFocusedObserver&) = delete;

  ~FrameFocusedObserver();

  void Wait();

 private:
  // Private impl struct which hides non public types including FrameTreeNode.
  class FrameTreeNodeObserverImpl;

  // FrameTreeNode::Observer
  std::unique_ptr<FrameTreeNodeObserverImpl> impl_;
};

// This observer is used to wait for its owner FrameTreeNode to become deleted.
class FrameDeletedObserver {
 public:
  explicit FrameDeletedObserver(RenderFrameHost* owner_host);

  FrameDeletedObserver(const FrameDeletedObserver&) = delete;
  FrameDeletedObserver& operator=(const FrameDeletedObserver&) = delete;

  ~FrameDeletedObserver();

  void Wait();

  bool IsDeleted() const;

  FrameTreeNodeId GetFrameTreeNodeId() const;

 private:
  // Private impl struct which hides non public types including FrameTreeNode.
  class FrameTreeNodeObserverImpl;

  // FrameTreeNode::Observer
  std::unique_ptr<FrameTreeNodeObserverImpl> impl_;
};

// This class can be used to pause and resume navigations, based on a URL
// match. Note that it only keeps track of one navigation at a time.
// Navigations are paused automatically before hitting the network, and are
// resumed automatically if a Wait method is called for a future event.
//
// Note: This class is one time use only! After it successfully tracks a
// navigation it will ignore all subsequent navigations. Explicitly create
// multiple instances of this class if you want to pause multiple navigations.
//
// Note2: This class cannot be used with page activating navigations (e.g.
// BFCache, prerendering, etc.) as the activation doesn't run
// NavigationThrottles. Use TestActivationManager in these cases instead.
class TestNavigationManager : public WebContentsObserver {
 public:
  // Monitors any frame in WebContents.
  TestNavigationManager(WebContents* web_contents, const GURL& url);

  TestNavigationManager(const TestNavigationManager&) = delete;
  TestNavigationManager& operator=(const TestNavigationManager&) = delete;

  ~TestNavigationManager() override;

  // Waits until the first yield point after DidStartNavigation. Unlike
  // WaitForRequestStart, this can be used to wait for a pause in cases where a
  // test expects a NavigationThrottle to defer in WillStartRequest. In cases
  // where throttles run and none defer, this will break at the same time as
  // WaitForRequestStart. Note: since we won't know which throttle deferred,
  // don't use ResumeNavigation() after this call since it assumes we paused
  // from the TestNavigationManagerThrottle. Returns false if the waiting was
  // terminated before reaching DidStartNavigation (e.g. timeout).
  bool WaitForFirstYieldAfterDidStartNavigation();

  // Waits until the navigation request is ready to be sent to the network
  // stack. This will wait until all NavigationThrottles have proceeded through
  // WillStartRequest. Returns false if the request was aborted before starting.
  [[nodiscard]] bool WaitForRequestStart();

  // Waits for the URLLoader of the navigation has been started. Returns false
  // if the navigation never started the URLLoader.
  [[nodiscard]] bool WaitForLoaderStart();

  // Waits until the navigation request has received a redirect response. This
  // will wait until all NavigationThrottles have proceeded through
  // WillRedirectRequest. Returns false if the request was aborted before
  // getting redirected, or it never got redirected. Note that this will only
  // wait for only one redirection, so if the navigation gets redirected
  // multiple times, this needs to be called multiple times.
  [[nodiscard]] bool WaitForRequestRedirected();

  // Waits until the navigation response's headers have been received. This
  // will wait until all NavigationThrottles have proceeded through
  // WillProcessResponse. Returns false if the request was aborted before
  // getting a response.
  [[nodiscard]] bool WaitForResponse();

  // Waits until the navigation has been finished. Will automatically resume
  // navigations paused before this point. Returns false if the waiting was
  // terminated before reaching DidStartNavigation (e.g. timeout).
  [[nodiscard]] bool WaitForNavigationFinished();

  // Waits until a speculative render frame host is created.
  // Note that the network may or may not be accessed.
  void WaitForSpeculativeRenderFrameHostCreation();

  RenderFrameHost* GetCreatedSpeculativeRFH();

  // Resume the navigation.
  // * Called after |WaitForRequestStart|, it causes the request to be sent.
  // * Called after |WaitForRequestRedirected|, it causes the redirect to
  // proceed.
  // * Called after |WaitForResponse|, it causes the response to be committed.
  void ResumeNavigation();

  // Returns the NavigationHandle associated with the navigation. It is non-null
  // only in between DidStartNavigation(...) and DidFinishNavigation(...).
  NavigationHandle* GetNavigationHandle();

  // Whether the navigation successfully committed.
  bool was_committed() const { return was_committed_; }

  // Whether the navigation successfully committed and was not an error page.
  bool was_successful() const { return was_successful_; }

  // Allows nestable tasks when running a message loop in the Wait* functions.
  // This is useful for utilizing this class from within another message loop.
  void AllowNestableTasks();

  // Write a representation of this class into trace.
  void WriteIntoTrace(perfetto::TracedValue ctx) const;

 protected:
  // Derived classes can override if they want to filter out navigations. This
  // is called from DidStartNavigation.
  virtual bool ShouldMonitorNavigation(NavigationHandle* handle);

 private:
  enum class NavigationState {
    INITIAL = 0,
    WILL_START = 1,
    REQUEST_STARTED = 2,
    LOADER_STARTED = 3,
    REDIRECTED = 4,
    RESPONSE = 5,
    FINISHED = 6,
  };

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* handle) override;
  void DidRedirectNavigation(NavigationHandle* handle) override;
  void DidFinishNavigation(NavigationHandle* handle) override;
  void DidUpdateNavigationHandleTiming(NavigationHandle* handle) override;
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;

  // Called when the NavigationThrottle pauses the navigation in
  // WillStartRequest.
  void OnWillStartRequest();

  // Called when the NavigationThrottle pauses the navigation in
  // WillRedirectRequest.
  void OnWillRedirectRequest();

  // Called when the NavigationThrottle pauses the navigation in
  // WillProcessResponse.
  void OnWillProcessResponse();

  // Waits for the desired state. Returns false if the desired state cannot be
  // reached (eg the navigation finishes before reaching this state).
  [[nodiscard]] bool WaitForDesiredState();

  // Called when the state of the navigation has changed. This will either stop
  // the message loop if the state specified by the user has been reached, or
  // resume the navigation if it hasn't been reached yet.
  void OnNavigationStateChanged();

  void ResumeIfPaused();

  const GURL url_;
  // RAW_PTR_EXCLUSION: Incompatible with tracing (TRACE_EVENT*),
  // perfetto::TracedDictionary::Add and gmock/EXPECT_THAT.
  RAW_PTR_EXCLUSION NavigationRequest* request_ = nullptr;
  bool navigation_paused_ = false;
  NavigationState current_state_ = NavigationState::INITIAL;
  NavigationState desired_state_ = NavigationState::WILL_START;
  bool was_committed_ = false;
  bool was_successful_ = false;
  bool speculative_rfh_created_ = false;
  std::unique_ptr<RenderFrameHostWrapper> created_speculative_rfh_;
  base::OnceClosure state_quit_closure_;
  base::OnceClosure wait_rfh_closure_;
  base::RunLoop::Type message_loop_type_ = base::RunLoop::Type::kDefault;

  base::WeakPtrFactory<TestNavigationManager> weak_factory_{this};
};

// Like TestNavigationManager but for page activating navigations like
// Back-Forward Cache restores and prerendering activations. This can be used
// to pause and resume the navigation at certain points during an activation.
// Note that only a single navigation will be tracked by this manager. When
// this object is live, a matching navigation will be automatically paused at
// kBeforeChecks and resumed automatically when a Wait method is called.
class TestActivationManager : public WebContentsObserver {
 public:
  TestActivationManager(WebContents* web_contents, const GURL& url);
  TestActivationManager(const TestActivationManager&) = delete;
  TestActivationManager& operator=(const TestActivationManager&) = delete;

  ~TestActivationManager() override;

  // Waits until the navigation is about to start running
  // CommitDeferringConditions. i.e. before any (non-test)
  // CommitDeferringConditions have run.
  [[nodiscard]] bool WaitForBeforeChecks();

  // Waits until the navigation has finished running CommitDeferringConditions.
  // i.e. all (non-test) CommitDeferringConditions have completed.  Resuming
  // from here will post a task that will synchronously commit the navigation.
  [[nodiscard]] bool WaitForAfterChecks();

  // Waits until the navigation has completed or been deleted.
  void WaitForNavigationFinished();

  // If the manager paused the navigation, this method can be used to proceed
  // to the next state.
  void ResumeActivation();

  // Returns the navigation handle being observed by this
  // TestActivationManager. This will be null until a navigation matching the
  // given URL starts running its CommitDeferringConditions. It'll also be
  // cleared when the navigation finishes.
  NavigationHandle* GetNavigationHandle();

  // Sets the callback that is called after all commit deferring conditions run.
  void SetCallbackCalledAfterActivationIsReady(base::OnceClosure callback);

  // Whether the navigation successfully committed.
  bool was_committed() const { return was_committed_; }

  // Whether the navigation successfully committed and was not an error page.
  bool was_successful() const { return was_successful_; }

  // Whether the navigation finished successfully as a page activation. This
  // can be false in the case that the activation failed and the navigation
  // continues as a regular, non-activating, navigation.
  bool was_activated() const { return was_activated_; }

  // Returns true if the navigation is paused by the TestActivationManager.
  bool is_paused() const { return !resume_callback_.is_null(); }

  // Returns ukm::SourceId for the navigated page. This must be called after the
  // navigation finished.
  ukm::SourceId next_page_ukm_source_id() const;

 private:
  enum class ActivationState {
    kInitial,
    kBeforeChecks,
    kAfterChecks,
    kFinished
  };

  CommitDeferringCondition::Result FirstConditionCallback(
      CommitDeferringCondition& condition,
      base::OnceClosure resume_callback);
  CommitDeferringCondition::Result LastConditionCallback(
      CommitDeferringCondition& condition,
      base::OnceClosure resume_callback);

  // WebContentsObserver:
  void DidFinishNavigation(NavigationHandle* handle) override;

  bool WaitForDesiredState();
  void StopWaitingIfNeeded();

  // The URL this manager is watching for. Navigations to a URL that do not
  // match this will be ignored.
  const GURL url_;

  // Set when a matching navigation reaches kBeforeChecks and cleared when the
  // navigation is deleted/finished.
  raw_ptr<NavigationRequest> request_ = nullptr;

  // If the navigation is paused in the first or last CommitDeferringCondition
  // (i.e. the one installed by this manager for testing), this will be the
  // closure used to resume the navigation. Note: invoking resume_callback_
  // will start running further conditions, it doesn't just quit a RunLoop like
  // `quit_closure_`.
  base::OnceClosure resume_callback_;

  // When the manager is blocked waiting on a state (e.g.
  // WaitForNavigationFinished), this closure will quit the waiting runloop so
  // that the manager client can proceed.
  base::OnceClosure quit_closure_;

  // These are RAII helpers that will install a testing
  // CommitDeferringCondition before (first_condition_inserter_) and after
  // (last_condition_inserter_) all other CommitDeferringConditions that
  // TestActivationManager will use to pause the navigation.
  class ConditionInserter;
  std::unique_ptr<ConditionInserter> first_condition_inserter_;
  std::unique_ptr<ConditionInserter> last_condition_inserter_;

  ActivationState current_state_ = ActivationState::kInitial;
  ActivationState desired_state_ = ActivationState::kBeforeChecks;

  // Prevents the TestActivationManager from attaching to more than one
  // matching activation.
  bool is_tracking_activation_ = false;

  bool was_committed_ = false;
  bool was_successful_ = false;
  bool was_activated_ = false;

  ukm::SourceId next_page_ukm_source_id_ = ukm::kInvalidSourceId;

  // Callback to be called in the last condition callback after all commit
  // deferring conditions run.
  base::OnceClosure callback_in_last_condition;

  base::WeakPtrFactory<TestActivationManager> weak_factory_{this};
};

class NavigationHandleCommitObserver : public content::WebContentsObserver {
 public:
  NavigationHandleCommitObserver(content::WebContents* web_contents,
                                 const GURL& url);

  bool has_committed() const { return has_committed_; }
  bool was_same_document() const { return was_same_document_; }
  bool was_renderer_initiated() const { return was_renderer_initiated_; }
  blink::mojom::NavigationType navigation_type() const {
    return navigation_type_;
  }

 private:
  void DidFinishNavigation(content::NavigationHandle* handle) override;

  const GURL url_;
  bool has_committed_ = false;
  bool was_same_document_ = false;
  bool was_renderer_initiated_ = false;
  blink::mojom::NavigationType navigation_type_ =
      blink::mojom::NavigationType::RELOAD;
};

// A test utility that monitors console messages sent to a WebContents. This
// can be used to wait for a message that matches a specific filter, an
// arbitrary message, or monitor all messages sent to the WebContents' console.
class WebContentsConsoleObserver : public WebContentsObserver {
 public:
  struct Message {
    raw_ptr<RenderFrameHost, AcrossTasksDanglingUntriaged> source_frame;
    blink::mojom::ConsoleMessageLevel log_level;
    std::u16string message;
    int32_t line_no;
    std::u16string source_id;
  };

  // A filter to apply to incoming console messages to determine whether to
  // record them. The filter should return `true` if the observer should record
  // the message, and stop waiting (if it was waiting).
  using Filter = base::RepeatingCallback<bool(const Message& message)>;

  explicit WebContentsConsoleObserver(content::WebContents* web_contents);
  ~WebContentsConsoleObserver() override;

  // Waits for a message to come in that matches the set filter, if any. If no
  // filter is set, waits for the first message that comes in.
  [[nodiscard]] bool Wait();

  // Sets a custom filter to be used while waiting for a message, allowing
  // more custom filtering (e.g. based on source).
  void SetFilter(Filter filter);

  // A convenience method to just match the message against a string pattern.
  void SetPattern(std::string pattern);

  // A helper method to return the string content (in UTF8) of the message at
  // the given |index|. This will cause a test failure if there is no such
  // message.
  std::string GetMessageAt(size_t index) const;

  const std::vector<Message>& messages() const { return messages_; }

 private:
  // WebContentsObserver:
  void OnDidAddMessageToConsole(
      RenderFrameHost* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) override;

  Filter filter_;
  std::string pattern_;
  WaiterHelper waiter_helper_;
  std::vector<Message> messages_;
};

// A helper class to get DevTools inspector log messages (e.g. network errors).
class DevToolsInspectorLogWatcher : public DevToolsAgentHostClient {
 public:
  explicit DevToolsInspectorLogWatcher(WebContents* web_contents);
  ~DevToolsInspectorLogWatcher() override;

  void FlushAndStopWatching();
  std::string last_message() { return last_message_; }
  GURL last_url() { return last_url_; }

  // DevToolsAgentHostClient:
  void DispatchProtocolMessage(DevToolsAgentHost* host,
                               base::span<const uint8_t> message) override;
  void AgentHostClosed(DevToolsAgentHost* host) override;

 private:
  scoped_refptr<DevToolsAgentHost> host_;
  base::RunLoop run_loop_enable_log_;
  base::RunLoop run_loop_disable_log_;
  std::string last_message_;
  GURL last_url_;
};

// Static methods that simulates Mojo methods as if they were called by a
// renderer. Used to simulate a compromised renderer.
class PwnMessageHelper {
 public:
  PwnMessageHelper(const PwnMessageHelper&) = delete;
  PwnMessageHelper& operator=(const PwnMessageHelper&) = delete;

  // Calls Create method in FileSystemHost Mojo interface.
  static void FileSystemCreate(RenderProcessHost* process,
                               int request_id,
                               GURL path,
                               bool exclusive,
                               bool is_directory,
                               bool recursive,
                               const blink::StorageKey& storage_key);

  // Calls Write method in FileSystemHost Mojo interface.
  static void FileSystemWrite(RenderProcessHost* process,
                              int request_id,
                              GURL file_path,
                              std::string blob_uuid,
                              int64_t position,
                              const blink::StorageKey& storage_key);

  // Calls OpenURL method in FrameHost Mojo interface.
  static void OpenURL(RenderFrameHost* render_frame_host, const GURL& url);

 private:
  PwnMessageHelper();  // Not instantiable.
};

#if defined(USE_AURA)

// Tests that a |render_widget_host_view| stores a stale content when its frame
// gets evicted. |render_widget_host_view| has to be a RenderWidgetHostViewAura.
void VerifyStaleContentOnFrameEviction(
    RenderWidgetHostView* render_widget_host_view);

#endif  // defined(USE_AURA)

// Helper class to interpose on Blob URL registrations, replacing the URL
// contained in incoming registration requests with the specified URL.
class BlobURLStoreInterceptor
    : public blink::mojom::BlobURLStoreInterceptorForTesting {
 public:
  static void InterceptDeprecated(
      GURL target_url,
      mojo::SelfOwnedAssociatedReceiverRef<blink::mojom::BlobURLStore>
          receiver);

  static void Intercept(GURL target_url,
                        storage::BlobUrlRegistry* registry,
                        mojo::ReceiverId receiver_id);

  ~BlobURLStoreInterceptor() override;

  blink::mojom::BlobURLStore* GetForwardingInterface() override;

  void Register(
      mojo::PendingRemote<blink::mojom::Blob> blob,
      const GURL& url,
      // TODO(crbug.com/40775506): Remove these once experiment is over.
      const base::UnguessableToken& unsafe_agent_cluster_id,
      const std::optional<net::SchemefulSite>& unsafe_top_level_site,
      RegisterCallback callback) override;

 private:
  explicit BlobURLStoreInterceptor(GURL target_url);

  std::unique_ptr<blink::mojom::BlobURLStore> url_store_;
  GURL target_url_;
};

// Load the given |url| with |network_context| and return the |net::Error| code.
//
// This overload simulates loading through a URLLoaderFactory created for a
// Browser process.
int LoadBasicRequest(network::mojom::NetworkContext* network_context,
                     const GURL& url,
                     int load_flags = net::LOAD_NORMAL);

// Load the given |url| via URLLoaderFactory created by |frame|.  Return the
// |net::Error| code.
//
// This overload simulates loading through a URLLoaderFactory created for a
// Renderer process (the factory is driven from the Test/Browser process, but
// has the same properties as factories vended to the Renderer process that
// hosts the |frame|).
int LoadBasicRequest(RenderFrameHost* frame, const GURL& url);

// Ensures that all StoragePartitions for the given BrowserContext have their
// cookies flushed to disk.
void EnsureCookiesFlushed(BrowserContext* browser_context);

// Performs a simple auto-resize flow and ensures that the embedder gets a
// single response messages back from the guest, with the expected values.
bool TestGuestAutoresize(WebContents* embedder_web_contents,
                         RenderFrameHost* guest_main_frame);

// This class allows monitoring of mouse events received by a specific
// RenderWidgetHost.
class RenderWidgetHostMouseEventMonitor {
 public:
  explicit RenderWidgetHostMouseEventMonitor(RenderWidgetHost* host);

  RenderWidgetHostMouseEventMonitor(const RenderWidgetHostMouseEventMonitor&) =
      delete;
  RenderWidgetHostMouseEventMonitor& operator=(
      const RenderWidgetHostMouseEventMonitor&) = delete;

  ~RenderWidgetHostMouseEventMonitor();
  bool EventWasReceived() const { return event_received_; }
  void ResetEventReceived() { event_received_ = false; }
  const blink::WebMouseEvent& event() const { return event_; }

 private:
  bool MouseEventCallback(const blink::WebMouseEvent& event) {
    event_received_ = true;
    event_ = event;
    return false;
  }
  RenderWidgetHost::MouseEventCallback mouse_callback_;
  raw_ptr<RenderWidgetHost> host_;
  bool event_received_;
  blink::WebMouseEvent event_;
};

// Helper class to track and allow waiting for navigation start events.
class DidStartNavigationObserver : public WebContentsObserver {
 public:
  explicit DidStartNavigationObserver(WebContents* web_contents);

  DidStartNavigationObserver(const DidStartNavigationObserver&) = delete;
  DidStartNavigationObserver& operator=(const DidStartNavigationObserver&) =
      delete;

  ~DidStartNavigationObserver() override;

  void Wait() { run_loop_.Run(); }
  bool observed() { return observed_; }

  // If the navigation was observed and is still not finished yet, this returns
  // its handle, otherwise it returns nullptr.
  NavigationHandle* navigation_handle() { return navigation_handle_; }

  // WebContentsObserver override:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

 private:
  bool observed_ = false;
  base::RunLoop run_loop_;
  raw_ptr<NavigationHandle> navigation_handle_ = nullptr;
};

// Tracks the creation of RenderFrameProxyHosts that have
// CrossProcessFrameConnectors, and records the initial (post-construction)
// device scale factor in the CrossProcessFrameConnector.
class ProxyDSFObserver {
 public:
  ProxyDSFObserver();
  ~ProxyDSFObserver();

  // Waits until a single RenderFrameProxyHost with a CrossProcessFrameConnector
  // has been created. If a creation occurs before this function is called, it
  // returns immediately.
  void WaitForOneProxyHostCreation();

  size_t num_creations() { return proxy_host_created_dsf_.size(); }

  float get_proxy_host_dsf(unsigned index) {
    return proxy_host_created_dsf_[index];
  }

 private:
  void OnCreation(RenderFrameProxyHost* rfph);

  // Make this a vector, just in case we encounter multiple creations prior to
  // calling WaitForOneProxyHostCreation(). That way we can confirm we're
  // getting the device scale factor we expect.
  // Note: We can modify the vector to collect a void* id for each
  // RenderFrameProxyHost if we want to expand to cases where multiple creations
  // must be observed.
  std::vector<float> proxy_host_created_dsf_;
  std::unique_ptr<base::RunLoop> runner_;
};

// Helper used by the wrapper class below. Compares the output of the given
// |web_contents| to the PNG file at |expected_path| across the region defined
// by |snapshot_size| and returns true if the images are equivalent. Uses a
// ManhattanDistancePixelComparator which allows some small differences.  If
// the flag switches::kRebaselinePixelTests (--rebaseline-pixel-tests) is set,
// this function will (over)write the reference file with the produced output.
bool CompareWebContentsOutputToReference(
    WebContents* web_contents,
    const base::FilePath& expected_path,
    const gfx::Size& snapshot_size,
    const cc::PixelComparator& comparator =
        cc::ManhattanDistancePixelComparator());

typedef base::OnceCallback<void(RenderFrameHost*, RenderFrameHost*)>
    RenderFrameHostChangedCallback;

// Runs callback at RenderFrameHostChanged time. On cross-RFH navigations, this
// will run the callback after the new RenderFrameHost committed and is set as
// the current RenderFrameHost, etc. but before the old RenderFrameHost gets
// unloaded.
class RenderFrameHostChangedCallbackRunner : public WebContentsObserver {
 public:
  explicit RenderFrameHostChangedCallbackRunner(
      WebContents* content,
      RenderFrameHostChangedCallback callback);

  RenderFrameHostChangedCallbackRunner(
      const RenderFrameHostChangedCallbackRunner&) = delete;
  RenderFrameHostChangedCallbackRunner& operator=(
      const RenderFrameHostChangedCallbackRunner&) = delete;

  ~RenderFrameHostChangedCallbackRunner() override;

 private:
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;

  RenderFrameHostChangedCallback callback_;
};

// Calls |callback| whenever a navigation finishes in |render_frame_host|.
class DidFinishNavigationObserver : public WebContentsObserver {
 public:
  DidFinishNavigationObserver(
      WebContents* web_contents,
      base::RepeatingCallback<void(NavigationHandle*)> callback);

  DidFinishNavigationObserver(
      RenderFrameHost* render_frame_host,
      base::RepeatingCallback<void(NavigationHandle*)> callback);

  ~DidFinishNavigationObserver() override;

  DidFinishNavigationObserver(const DidFinishNavigationObserver&) = delete;
  DidFinishNavigationObserver& operator=(const DidFinishNavigationObserver&) =
      delete;

 protected:
  // WebContentsObserver:
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

 private:
  base::RepeatingCallback<void(NavigationHandle*)> callback_;
};

// Wait for a new WebContents to be created, and for it to finish navigation.
// It will detect WebContents creation after construction, even if it's before
// Wait() is called.  The intended pattern is:
//
// CreateAndLoadWebContentsObserver observer;
// ...Do something that creates one WebContents and causes it to navigate...
// observer.Wait();
class CreateAndLoadWebContentsObserver {
 public:
  // Used to wait for the given number of WebContents to be created. The load of
  // the last expected WebContents is awaited.
  explicit CreateAndLoadWebContentsObserver(int num_expected_contents = 1);
  ~CreateAndLoadWebContentsObserver();

  // Wait for the last expected WebContents to finish loading. The test will
  // fail if an additional WebContents creation is observed before `Wait()`
  // completes.
  WebContents* Wait();

 private:
  void OnWebContentsCreated(WebContents* web_contents);

  // Unregister for WebContents creation callbacks if we are registered.  May
  // be called multiple times.
  void UnregisterIfNeeded();

  std::optional<LoadStopObserver> load_stop_observer_;
  base::CallbackListSubscription creation_subscription_;

  raw_ptr<WebContents, DanglingUntriaged> web_contents_ = nullptr;
  base::OnceClosure contents_creation_quit_closure_;

  const int num_expected_contents_;
  int num_new_contents_seen_ = 0;
};

// Waits for the given number of calls to
// WebContentsObserver::OnCookiesAccessed.
class CookieChangeObserver : public content::WebContentsObserver {
 public:
  explicit CookieChangeObserver(content::WebContents* web_contents,
                                int num_expected_calls = 1);
  ~CookieChangeObserver() override;

  void Wait();

  int num_read_seen() const { return num_read_seen_; }
  int num_write_seen() const { return num_write_seen_; }

 private:
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation,
                         const content::CookieAccessDetails& details) override;

  void OnCookieAccessed(const content::CookieAccessDetails& details);

  base::RunLoop run_loop_;
  int num_seen_ = 0;
  int num_expected_calls_;
  int num_read_seen_ = 0;
  int num_write_seen_ = 0;
};

// Wait for the creation of Speculative RFH without throttling the navigation.
// Since the TestNavigationManager will throttle the navigation, using with
// class with TestNavigationManager is not recommended. Manually driving the
// run loop will be required to receive the events in both objects. We recommend
// to use TestNavigationManager for simply driving the navigation. However if it
// is required to intercept the navigation with other observers such as
// CommitPauser, it would be better to use SpeculativeRenderFrameHostObserver to
// ensure the speculative RFH to avoid interference caused by the
// TestNavigationManager.
class SpeculativeRenderFrameHostObserver : public content::WebContentsObserver {
 public:
  explicit SpeculativeRenderFrameHostObserver(
      content::WebContents* web_contents,
      const GURL& url);
  ~SpeculativeRenderFrameHostObserver() override;

  void Wait();

 private:
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;

  base::RunLoop run_loop_;
  GURL url_;
};

class SpareRenderProcessHostStartedObserver
    : public SpareRenderProcessHostManager::Observer {
 public:
  SpareRenderProcessHostStartedObserver();
  ~SpareRenderProcessHostStartedObserver() override;

  // SpareRenderProcessHostManager::Observer:
  void OnSpareRenderProcessHostReady(RenderProcessHost* host) override;

  RenderProcessHost* WaitForSpareRenderProcessStarted();

 private:
  base::ScopedObservation<SpareRenderProcessHostManager,
                          SpareRenderProcessHostManager::Observer>
      scoped_observation_{this};
  raw_ptr<RenderProcessHost> spare_render_process_host_ = nullptr;
  base::OnceClosure quit_closure_;
};

[[nodiscard]] base::CallbackListSubscription
RegisterWebContentsCreationCallback(
    base::RepeatingCallback<void(WebContents*)> callback);

// Functions to traverse history and wait until the traversal completes, even if
// the loading is stopped halfway (e.g. if a BackForwardCache entry is evicted
// during the restoration, causing the old NavigationRequest to be reset and a
// new NavigationRequest to be restarted). These are wrappers around the
// same-named methods of the `NavigationController`.
[[nodiscard]] bool HistoryGoToIndex(WebContents* wc, int index);
[[nodiscard]] bool HistoryGoToOffset(WebContents* wc, int offset);
[[nodiscard]] bool HistoryGoBack(WebContents* wc);
[[nodiscard]] bool HistoryGoForward(WebContents* wc);

#if BUILDFLAG(IS_MAC)
// Grant native windows the ability to activate, allowing them to become key
// and/or main. This can be useful to enable when the process hosting the window
// is a standalone executable without an Info.plist.
bool EnableNativeWindowActivation();
#endif  // BUILDFLAG(IS_MAC)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Set the global factory for CapturedSurfaceController objects.
void SetCapturedSurfaceControllerFactoryForTesting(
    base::RepeatingCallback<std::unique_ptr<MockCapturedSurfaceController>(
        GlobalRenderFrameHostId,
        WebContentsMediaCaptureId)> factory);
#endif  // !BUILDFLAG(IS_ANDROID)

void InitAndEnableRenderDocumentForAllFrames(
    base::test::ScopedFeatureList* feature_list);
}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_
