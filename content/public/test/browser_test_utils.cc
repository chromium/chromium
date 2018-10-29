// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_utils.h"

#include <stddef.h>

#include <set>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/process/kill.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/browser_plugin_message_filter.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/fileapi/file_system_manager_impl.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/screen_orientation/screen_orientation_provider.h"
#include "content/browser/service_manager/service_manager_context.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/fileapi/webblob_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_visual_properties.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_fileapi_operation_waiter.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "content/test/did_commit_provisional_load_interceptor.h"
#include "ipc/ipc_security_test_util.h"
#include "net/base/filename_util.h"
#include "net/base/io_buffer.h"
#include "net/cookies/canonical_cookie.h"
#include "net/filter/gzip_header.h"
#include "net/filter/gzip_source_stream.h"
#include "net/filter/mock_source_stream.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/python_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/test/test_clipboard.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/latency/latency_info.h"
#include "ui/resources/grit/webui_resources.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "ui/aura/test/window_event_dispatcher_test_api.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#endif  // USE_AURA

namespace content {
namespace {

class InterstitialObserver : public content::WebContentsObserver {
 public:
  InterstitialObserver(content::WebContents* web_contents,
                       base::OnceClosure attach_callback,
                       base::OnceClosure detach_callback)
      : WebContentsObserver(web_contents),
        attach_callback_(std::move(attach_callback)),
        detach_callback_(std::move(detach_callback)) {}
  ~InterstitialObserver() override {}

  // WebContentsObserver methods:
  void DidAttachInterstitialPage() override {
    std::move(attach_callback_).Run();
  }
  void DidDetachInterstitialPage() override {
    std::move(detach_callback_).Run();
  }

 private:
  base::OnceClosure attach_callback_;
  base::OnceClosure detach_callback_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialObserver);
};

// Specifying a prototype so that we can add the WARN_UNUSED_RESULT attribute.
bool ExecuteScriptHelper(RenderFrameHost* render_frame_host,
                         const std::string& script,
                         bool user_gesture,
                         int world_id,
                         std::unique_ptr<base::Value>* result)
    WARN_UNUSED_RESULT;

// Executes the passed |script| in the frame specified by |render_frame_host|.
// If |result| is not NULL, stores the value that the evaluation of the script
// in |result|.  Returns true on success.
bool ExecuteScriptHelper(RenderFrameHost* render_frame_host,
                         const std::string& script,
                         bool user_gesture,
                         int world_id,
                         std::unique_ptr<base::Value>* result) {
  // TODO(lukasza): Only get messages from the specific |render_frame_host|.
  DOMMessageQueue dom_message_queue(
      WebContents::FromRenderFrameHost(render_frame_host));
  base::string16 script16 = base::UTF8ToUTF16(script);
  if (world_id == ISOLATED_WORLD_ID_GLOBAL) {
    if (user_gesture)
      render_frame_host->ExecuteJavaScriptWithUserGestureForTests(script16);
    else
      render_frame_host->ExecuteJavaScriptForTests(script16);
  } else {
    // Note that |user_gesture| here is ignored. We allow a value of |true|
    // because it's the default, but in blink, the execution will occur with
    // no user gesture.
    render_frame_host->ExecuteJavaScriptInIsolatedWorld(
        script16, RenderFrameHost::JavaScriptResultCallback(), world_id);
  }
  std::string json;
  if (!dom_message_queue.WaitForMessage(&json)) {
    DLOG(ERROR) << "Cannot communicate with DOMMessageQueue.";
    return false;
  }

  // Nothing more to do for callers that ignore the returned JS value.
  if (!result)
    return true;

  base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
  *result = reader.ReadToValue(json);
  if (!*result) {
    DLOG(ERROR) << reader.GetErrorMessage();
    return false;
  }

  return true;
}

bool ExecuteScriptWithUserGestureControl(RenderFrameHost* frame,
                                         const std::string& script,
                                         bool user_gesture) {
  // TODO(lukasza): ExecuteScript should just call
  // ExecuteJavaScriptWithUserGestureForTests and avoid modifying the original
  // script (and at that point we should merge it with and remove
  // ExecuteScriptAsync).  This is difficult to change, because many tests
  // depend on the message loop pumping done by ExecuteScriptHelper below (this
  // is fragile - these tests should wait on a more specific thing instead).

  // TODO(nick): This function can't be replaced with a call to ExecJs(), since
  // ExecJs calls eval() which might be blocked by the page's CSP.
  std::string expected_response = "ExecuteScript-" + base::GenerateGUID();
  std::string new_script = base::StringPrintf(
      R"( %s;  // Original script.
          window.domAutomationController.send('%s'); )",
      script.c_str(), expected_response.c_str());

  std::unique_ptr<base::Value> value;
  if (!ExecuteScriptHelper(frame, new_script, user_gesture,
                           ISOLATED_WORLD_ID_GLOBAL, &value) ||
      !value.get()) {
    return false;
  }

  DCHECK_EQ(base::Value::Type::STRING, value->type());
  std::string actual_response;
  if (value->GetAsString(&actual_response))
    DCHECK_EQ(expected_response, actual_response);

  return true;
}

void BuildSimpleWebKeyEvent(blink::WebInputEvent::Type type,
                            ui::DomKey key,
                            ui::DomCode code,
                            ui::KeyboardCode key_code,
                            NativeWebKeyboardEvent* event) {
  event->dom_key = key;
  event->dom_code = static_cast<int>(code);
  event->native_key_code = ui::KeycodeConverter::DomCodeToNativeKeycode(code);
  event->windows_key_code = key_code;
  event->is_system_key = false;
  event->skip_in_browser = true;

  if (type == blink::WebInputEvent::kChar ||
      type == blink::WebInputEvent::kRawKeyDown) {
    // |key| is the only parameter that contains information about the case of
    // the character. Use it to be able to generate lower case input.
    if (key.IsCharacter()) {
      event->text[0] = key.ToCharacter();
      event->unmodified_text[0] = key.ToCharacter();
    } else {
      event->text[0] = key_code;
      event->unmodified_text[0] = key_code;
    }
  }
}

void InjectRawKeyEvent(WebContents* web_contents,
                       blink::WebInputEvent::Type type,
                       ui::DomKey key,
                       ui::DomCode code,
                       ui::KeyboardCode key_code,
                       int modifiers) {
  NativeWebKeyboardEvent event(type, modifiers, base::TimeTicks::Now());
  BuildSimpleWebKeyEvent(type, key, code, key_code, &event);
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  RenderWidgetHostImpl* main_frame_rwh =
      web_contents_impl->GetMainFrame()->GetRenderWidgetHost();
  web_contents_impl->GetFocusedRenderWidgetHost(main_frame_rwh)
      ->ForwardKeyboardEvent(event);
}

int SimulateModifierKeysDown(WebContents* web_contents,
                             bool control,
                             bool shift,
                             bool alt,
                             bool command) {
  int modifiers = 0;

  // The order of these key down events shouldn't matter for our simulation.
  // For our simulation we can use either the left keys or the right keys.
  if (control) {
    modifiers |= blink::WebInputEvent::kControlKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kRawKeyDown,
                      ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT,
                      ui::VKEY_CONTROL, modifiers);
  }
  if (shift) {
    modifiers |= blink::WebInputEvent::kShiftKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kRawKeyDown,
                      ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT,
                      ui::VKEY_SHIFT, modifiers);
  }
  if (alt) {
    modifiers |= blink::WebInputEvent::kAltKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kRawKeyDown,
                      ui::DomKey::ALT, ui::DomCode::ALT_LEFT, ui::VKEY_MENU,
                      modifiers);
  }
  if (command) {
    modifiers |= blink::WebInputEvent::kMetaKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kRawKeyDown,
                      ui::DomKey::META, ui::DomCode::META_LEFT,
                      ui::VKEY_COMMAND, modifiers);
  }
  return modifiers;
}

int SimulateModifierKeysUp(WebContents* web_contents,
                           bool control,
                           bool shift,
                           bool alt,
                           bool command,
                           int modifiers) {
  // The order of these key releases shouldn't matter for our simulation.
  if (control) {
    modifiers &= ~blink::WebInputEvent::kControlKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kKeyUp,
                      ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT,
                      ui::VKEY_CONTROL, modifiers);
  }

  if (shift) {
    modifiers &= ~blink::WebInputEvent::kShiftKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kKeyUp,
                      ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT,
                      ui::VKEY_SHIFT, modifiers);
  }

  if (alt) {
    modifiers &= ~blink::WebInputEvent::kAltKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kKeyUp,
                      ui::DomKey::ALT, ui::DomCode::ALT_LEFT, ui::VKEY_MENU,
                      modifiers);
  }

  if (command) {
    modifiers &= ~blink::WebInputEvent::kMetaKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kKeyUp,
                      ui::DomKey::META, ui::DomCode::META_LEFT,
                      ui::VKEY_COMMAND, modifiers);
  }
  return modifiers;
}

void SimulateKeyEvent(WebContents* web_contents,
                      ui::DomKey key,
                      ui::DomCode code,
                      ui::KeyboardCode key_code,
                      bool send_char,
                      int modifiers) {
  InjectRawKeyEvent(web_contents, blink::WebInputEvent::kRawKeyDown, key, code,
                    key_code, modifiers);
  if (send_char) {
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kChar, key, code,
                      key_code, modifiers);
  }
  InjectRawKeyEvent(web_contents, blink::WebInputEvent::kKeyUp, key, code,
                    key_code, modifiers);
}

void SimulateKeyPressImpl(WebContents* web_contents,
                          ui::DomKey key,
                          ui::DomCode code,
                          ui::KeyboardCode key_code,
                          bool control,
                          bool shift,
                          bool alt,
                          bool command,
                          bool send_char) {
  int modifiers =
      SimulateModifierKeysDown(web_contents, control, shift, alt, command);
  SimulateKeyEvent(web_contents, key, code, key_code, send_char, modifiers);
  modifiers = SimulateModifierKeysUp(web_contents, control, shift, alt, command,
                                     modifiers);
  ASSERT_EQ(modifiers, 0);
}

std::unique_ptr<net::test_server::HttpResponse>
CrossSiteRedirectResponseHandler(const net::EmbeddedTestServer* test_server,
                                 const net::test_server::HttpRequest& request) {
  net::HttpStatusCode http_status_code;

  // Inspect the prefix and extract the remainder of the url into |params|.
  size_t length_of_chosen_prefix;
  std::string prefix_302("/cross-site/");
  std::string prefix_307("/cross-site-307/");
  if (base::StartsWith(request.relative_url, prefix_302,
                       base::CompareCase::SENSITIVE)) {
    http_status_code = net::HTTP_MOVED_PERMANENTLY;
    length_of_chosen_prefix = prefix_302.length();
  } else if (base::StartsWith(request.relative_url, prefix_307,
                              base::CompareCase::SENSITIVE)) {
    http_status_code = net::HTTP_TEMPORARY_REDIRECT;
    length_of_chosen_prefix = prefix_307.length();
  } else {
    // Unrecognized prefix - let somebody else handle this request.
    return std::unique_ptr<net::test_server::HttpResponse>();
  }
  std::string params = request.relative_url.substr(length_of_chosen_prefix);

  // A hostname to redirect to must be included in the URL, therefore at least
  // one '/' character is expected.
  size_t slash = params.find('/');
  if (slash == std::string::npos)
    return std::unique_ptr<net::test_server::HttpResponse>();

  // Replace the host of the URL with the one passed in the URL.
  GURL::Replacements replace_host;
  replace_host.SetHostStr(base::StringPiece(params).substr(0, slash));
  GURL redirect_server =
      test_server->base_url().ReplaceComponents(replace_host);

  // Append the real part of the path to the new URL.
  std::string path = params.substr(slash + 1);
  GURL redirect_target(redirect_server.Resolve(path));
  DCHECK(redirect_target.is_valid());

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(http_status_code);
  http_response->AddCustomHeader("Location", redirect_target.spec());
  return std::move(http_response);
}

// Helper class used by the TestNavigationManager to pause navigations.
// Note: the throttle should be added to the *end* of the list of throttles,
// so all NavigationThrottles that should be attached observe the
// WillStartRequest callback. RegisterThrottleForTesting has this behavior.
class TestNavigationManagerThrottle : public NavigationThrottle {
 public:
  TestNavigationManagerThrottle(NavigationHandle* handle,
                                base::Closure on_will_start_request_closure,
                                base::Closure on_will_process_response_closure)
      : NavigationThrottle(handle),
        on_will_start_request_closure_(on_will_start_request_closure),
        on_will_process_response_closure_(on_will_process_response_closure) {}
  ~TestNavigationManagerThrottle() override {}

  const char* GetNameForLogging() override {
    return "TestNavigationManagerThrottle";
  }

 private:
  // NavigationThrottle:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             on_will_start_request_closure_);
    return NavigationThrottle::DEFER;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                             on_will_process_response_closure_);
    return NavigationThrottle::DEFER;
  }

  base::Closure on_will_start_request_closure_;
  base::Closure on_will_process_response_closure_;
};

bool HasGzipHeader(const base::RefCountedMemory& maybe_gzipped) {
  net::GZipHeader header;
  net::GZipHeader::Status header_status = net::GZipHeader::INCOMPLETE_HEADER;
  const char* header_end = nullptr;
  while (header_status == net::GZipHeader::INCOMPLETE_HEADER) {
    header_status = header.ReadMore(maybe_gzipped.front_as<char>(),
                                    maybe_gzipped.size(),
                                    &header_end);
  }
  return header_status == net::GZipHeader::COMPLETE_HEADER;
}

void AppendGzippedResource(const base::RefCountedMemory& encoded,
                           std::string* to_append) {
  std::unique_ptr<net::MockSourceStream> source_stream(
      new net::MockSourceStream());
  source_stream->AddReadResult(encoded.front_as<char>(), encoded.size(),
                               net::OK, net::MockSourceStream::SYNC);
  // Add an EOF.
  source_stream->AddReadResult(encoded.front_as<char>() + encoded.size(), 0,
                               net::OK, net::MockSourceStream::SYNC);
  std::unique_ptr<net::GzipSourceStream> filter = net::GzipSourceStream::Create(
      std::move(source_stream), net::SourceStream::TYPE_GZIP);
  scoped_refptr<net::IOBufferWithSize> dest_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(4096);
  net::CompletionCallback callback;
  while (true) {
    int rv = filter->Read(dest_buffer.get(), dest_buffer->size(), callback);
    ASSERT_LE(0, rv);
    if (rv <= 0)
      break;
    to_append->append(dest_buffer->data(), rv);
  }
}

// Queries for video input devices on the current system using the getSources
// API.
//
// This does not guarantee that a getUserMedia with video will succeed, as the
// camera could be busy for instance.
//
// Returns has-video-input-device to the test if there is a webcam available,
// no-video-input-devices otherwise.
const char kHasVideoInputDeviceOnSystem[] = R"(
    (function() {
      navigator.mediaDevices.enumerateDevices()
      .then(function(devices) {
        if (devices.some((device) => device.kind == 'videoinput')) {
          window.domAutomationController.send('has-video-input-device');
        } else {
          window.domAutomationController.send('no-video-input-devices');
        }
      });
    })()
)";

const char kHasVideoInputDevice[] = "has-video-input-device";

// Interceptor that replaces params.url with |new_url| and params.origin with
// |new_origin| for any commits to |target_url|.
class CommitOriginInterceptor : public DidCommitProvisionalLoadInterceptor {
 public:
  CommitOriginInterceptor(WebContents* web_contents,
                          const GURL& target_url,
                          const GURL& new_url,
                          const url::Origin& new_origin)
      : DidCommitProvisionalLoadInterceptor(web_contents),
        target_url_(target_url),
        new_url_(new_url),
        new_origin_(new_origin) {}
  ~CommitOriginInterceptor() override = default;

  // WebContentsObserver:
  void WebContentsDestroyed() override { delete this; }

 protected:
  bool WillDispatchDidCommitProvisionalLoad(
      RenderFrameHost* render_frame_host,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      service_manager::mojom::InterfaceProviderRequest*
          interface_provider_request) override {
    if (params->url == target_url_) {
      params->url = new_url_;
      params->origin = new_origin_;
    }
    return true;
  }

 private:
  GURL target_url_;
  GURL new_url_;
  url::Origin new_origin_;

  DISALLOW_COPY_AND_ASSIGN(CommitOriginInterceptor);
};

}  // namespace

bool NavigateToURL(WebContents* web_contents, const GURL& url) {
  NavigateToURLBlockUntilNavigationsComplete(web_contents, url, 1);
  if (!IsLastCommittedEntryOfPageType(web_contents, PAGE_TYPE_NORMAL)) {
    // TODO(crbug.com/882545) remove the following debug information:
    {
      NavigationEntry* last_entry =
          web_contents->GetController().GetLastCommittedEntry();
      if (!last_entry) {
        DLOG(WARNING) << "No last committed entry";
      } else {
        DLOG(WARNING) << "Last committed entry is of type "
                      << last_entry->GetPageType();
      }
    }
    return false;
  }

  // TODO(crbug.com/882545) revert this to the return statement below.
  bool same_url = web_contents->GetLastCommittedURL() == url;
  if (!same_url) {
    DLOG(WARNING) << "Expected URL " << url << " but observed "
                  << web_contents->GetLastCommittedURL();
  }
  return same_url;
  // return web_contents->GetLastCommittedURL() == url;
}

bool NavigateIframeToURL(WebContents* web_contents,
                         std::string iframe_id,
                         const GURL& url) {
  std::string script = base::StringPrintf(
      "setTimeout(\""
      "var iframes = document.getElementById('%s');iframes.src='%s';"
      "\",0)",
      iframe_id.c_str(), url.spec().c_str());
  TestNavigationObserver load_observer(web_contents);
  bool result = ExecuteScript(web_contents, script);
  load_observer.Wait();
  return result;
}

void NavigateToURLBlockUntilNavigationsComplete(WebContents* web_contents,
                                                const GURL& url,
                                                int number_of_navigations) {
  // Prepare for the navigation.
  WaitForLoadStop(web_contents);
  TestNavigationObserver same_tab_observer(web_contents, number_of_navigations);

  // This mimics behavior of Shell::LoadURL...
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web_contents->GetController().LoadURLWithParams(params);
  web_contents->Focus();

  // Wait until the expected number of navigations finish.
  same_tab_observer.Wait();
  // TODO(crbug.com/882545) Delete this if statement once the problem has been
  // identified.
  if (!same_tab_observer.last_navigation_succeeded()) {
    DLOG(WARNING) << "Last navigation to " << url << " failed with net error "
                  << same_tab_observer.last_net_error_code();
  }
}

GURL GetFileUrlWithQuery(const base::FilePath& path,
                         const std::string& query_string) {
  GURL url = net::FilePathToFileURL(path);
  if (!query_string.empty()) {
    GURL::Replacements replacements;
    replacements.SetQueryStr(query_string);
    return url.ReplaceComponents(replacements);
  }
  return url;
}

void ResetTouchAction(RenderWidgetHost* host) {
  static_cast<InputRouterImpl*>(
      static_cast<RenderWidgetHostImpl*>(host)->input_router())
      ->ForceResetTouchActionForTest();
}

void ResendGestureScrollUpdateToEmbedder(WebContents* guest_web_contents,
                                         const blink::WebInputEvent& event) {
  DCHECK(guest_web_contents->GetBrowserPluginGuest());
  guest_web_contents->GetBrowserPluginGuest()->ResendEventToEmbedder(event);
}

void MaybeSendSyntheticTapGesture(WebContents* guest_web_contents) {
  content::RenderWidgetHostViewGuest* rwhv =
      static_cast<content::RenderWidgetHostViewGuest*>(
          guest_web_contents->GetRenderWidgetHostView());
  DCHECK(rwhv);
  rwhv->MaybeSendSyntheticTapGestureForTest(blink::WebFloatPoint(1, 1),
                                            blink::WebFloatPoint(1, 1));
}

void WaitForLoadStopWithoutSuccessCheck(WebContents* web_contents) {
  // In many cases, the load may have finished before we get here.  Only wait if
  // the tab still has a pending navigation.
  if (web_contents->IsLoading()) {
    WindowedNotificationObserver load_stop_observer(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(&web_contents->GetController()));
    load_stop_observer.Wait();
  }
}

bool WaitForLoadStop(WebContents* web_contents) {
  WebContentsDestroyedObserver observer(web_contents);
  WaitForLoadStopWithoutSuccessCheck(web_contents);
  if (observer.IsDestroyed()) {
    LOG(ERROR) << "WebContents was destroyed during waiting for load stop.";
    return false;
  }
  return IsLastCommittedEntryOfPageType(web_contents, PAGE_TYPE_NORMAL);
}

void PrepContentsForBeforeUnloadTest(WebContents* web_contents) {
  for (auto* frame : web_contents->GetAllFrames()) {
    // JavaScript onbeforeunload dialogs are ignored unless the frame received a
    // user gesture. Make sure the frames have user gestures.
    frame->ExecuteJavaScriptWithUserGestureForTests(base::string16());

    // Disable the hang monitor, otherwise there will be a race between the
    // beforeunload dialog and the beforeunload hang timer.
    frame->DisableBeforeUnloadHangMonitorForTesting();
  }
}

bool IsLastCommittedEntryOfPageType(WebContents* web_contents,
                                    content::PageType page_type) {
  NavigationEntry* last_entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!last_entry)
    return false;
  return last_entry->GetPageType() == page_type;
}

void OverrideLastCommittedOrigin(RenderFrameHost* render_frame_host,
                                 const url::Origin& origin) {
  static_cast<RenderFrameHostImpl*>(render_frame_host)
      ->SetLastCommittedOriginForTesting(origin);
}

void CrashTab(WebContents* web_contents) {
  RenderProcessHost* rph = web_contents->GetMainFrame()->GetProcess();
  RenderProcessHostWatcher watcher(
      rph, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  rph->Shutdown(0);
  watcher.Wait();
}

void PwnCommitIPC(WebContents* web_contents,
                  const GURL& target_url,
                  const GURL& new_url,
                  const url::Origin& new_origin) {
  // This will be cleaned up when |web_contents| is destroyed.
  new CommitOriginInterceptor(web_contents, target_url, new_url, new_origin);
}

void SimulateUnresponsiveRenderer(WebContents* web_contents,
                                  RenderWidgetHost* widget) {
  static_cast<WebContentsImpl*>(web_contents)
      ->RendererUnresponsive(RenderWidgetHostImpl::From(widget),
                             base::DoNothing::Repeatedly());
}

#if defined(USE_AURA)
bool IsResizeComplete(aura::test::WindowEventDispatcherTestApi* dispatcher_test,
                      RenderWidgetHostImpl* widget_host) {
  dispatcher_test->WaitUntilPointerMovesDispatched();
  widget_host->SynchronizeVisualProperties();
  return !widget_host->visual_properties_ack_pending_for_testing();
}

void WaitForResizeComplete(WebContents* web_contents) {
  aura::Window* content = web_contents->GetContentNativeView();
  if (!content)
    return;

  aura::WindowTreeHost* window_host = content->GetHost();
  aura::WindowEventDispatcher* dispatcher = window_host->dispatcher();
  aura::test::WindowEventDispatcherTestApi dispatcher_test(dispatcher);
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  if (!IsResizeComplete(&dispatcher_test, widget_host)) {
    WindowedNotificationObserver resize_observer(
        NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES,
        base::Bind(IsResizeComplete, &dispatcher_test, widget_host));
    resize_observer.Wait();
  }
}
#elif defined(OS_ANDROID)
bool IsResizeComplete(RenderWidgetHostImpl* widget_host) {
  return !widget_host->visual_properties_ack_pending_for_testing();
}

void WaitForResizeComplete(WebContents* web_contents) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  if (!IsResizeComplete(widget_host)) {
    WindowedNotificationObserver resize_observer(
        NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES,
        base::Bind(IsResizeComplete, widget_host));
    resize_observer.Wait();
  }
}
#endif

void SimulateMouseClick(WebContents* web_contents,
                        int modifiers,
                        blink::WebMouseEvent::Button button) {
  int x = web_contents->GetContainerBounds().width() / 2;
  int y = web_contents->GetContainerBounds().height() / 2;
  SimulateMouseClickAt(web_contents, modifiers, button, gfx::Point(x, y));
}

void SimulateMouseClickAt(WebContents* web_contents,
                          int modifiers,
                          blink::WebMouseEvent::Button button,
                          const gfx::Point& point) {
  blink::WebMouseEvent mouse_event(blink::WebInputEvent::kMouseDown, modifiers,
                                   ui::EventTimeForNow());
  mouse_event.button = button;
  mouse_event.SetPositionInWidget(point.x(), point.y());
  // Mac needs positionInScreen for events to plugins.
  gfx::Rect offset = web_contents->GetContainerBounds();
  mouse_event.SetPositionInScreen(point.x() + offset.x(),
                                  point.y() + offset.y());
  mouse_event.click_count = 1;
  web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
}

void SimulateRoutedMouseClickAt(WebContents* web_contents,
                                int modifiers,
                                blink::WebMouseEvent::Button button,
                                const gfx::Point& point) {
  content::WebContentsImpl* web_contents_impl =
      static_cast<content::WebContentsImpl*>(web_contents);
  content::RenderWidgetHostViewBase* rwhvb =
      static_cast<content::RenderWidgetHostViewBase*>(
          web_contents->GetRenderWidgetHostView());
  blink::WebMouseEvent mouse_event(blink::WebInputEvent::kMouseDown, modifiers,
                                   ui::EventTimeForNow());
  mouse_event.button = button;
  mouse_event.SetPositionInWidget(point.x(), point.y());
  // Mac needs positionInScreen for events to plugins.
  gfx::Rect offset = web_contents->GetContainerBounds();
  mouse_event.SetPositionInScreen(point.x() + offset.x(),
                                  point.y() + offset.y());
  mouse_event.click_count = 1;
  web_contents_impl->GetInputEventRouter()->RouteMouseEvent(rwhvb, &mouse_event,
                                                            ui::LatencyInfo());
  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  web_contents_impl->GetInputEventRouter()->RouteMouseEvent(rwhvb, &mouse_event,
                                                            ui::LatencyInfo());
}

void SendMouseDownToWidget(RenderWidgetHost* target,
                           int modifiers,
                           blink::WebMouseEvent::Button button) {
  auto* view = static_cast<content::RenderWidgetHostImpl*>(target)->GetView();

  blink::WebMouseEvent mouse_event(blink::WebInputEvent::kMouseDown, modifiers,
                                   ui::EventTimeForNow());
  mouse_event.button = button;
  int x = view->GetViewBounds().width() / 2;
  int y = view->GetViewBounds().height() / 2;
  mouse_event.SetPositionInWidget(x, y);
  mouse_event.click_count = 1;
  target->ForwardMouseEvent(mouse_event);
}

void SimulateMouseEvent(WebContents* web_contents,
                        blink::WebInputEvent::Type type,
                        const gfx::Point& point) {
  blink::WebMouseEvent mouse_event(type, blink::WebInputEvent::kNoModifiers,
                                   ui::EventTimeForNow());
  mouse_event.SetPositionInWidget(point.x(), point.y());
  web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
}

void SimulateRoutedMouseEvent(WebContents* web_contents,
                              blink::WebInputEvent::Type type,
                              const gfx::Point& point) {
  content::WebContentsImpl* web_contents_impl =
      static_cast<content::WebContentsImpl*>(web_contents);
  content::RenderWidgetHostViewBase* rwhvb =
      static_cast<content::RenderWidgetHostViewBase*>(
          web_contents->GetRenderWidgetHostView());
  blink::WebMouseEvent mouse_event(type, 0, ui::EventTimeForNow());
  mouse_event.SetPositionInWidget(point.x(), point.y());
  // Mac needs positionInScreen for events to plugins.
  gfx::Rect offset = web_contents->GetContainerBounds();
  mouse_event.SetPositionInScreen(point.x() + offset.x(),
                                  point.y() + offset.y());

  web_contents_impl->GetInputEventRouter()->RouteMouseEvent(rwhvb, &mouse_event,
                                                            ui::LatencyInfo());
}

void SimulateMouseWheelEvent(WebContents* web_contents,
                             const gfx::Point& point,
                             const gfx::Vector2d& delta,
                             const blink::WebMouseWheelEvent::Phase phase) {
  blink::WebMouseWheelEvent wheel_event(blink::WebInputEvent::kMouseWheel,
                                        blink::WebInputEvent::kNoModifiers,
                                        ui::EventTimeForNow());

  wheel_event.SetPositionInWidget(point.x(), point.y());
  wheel_event.delta_x = delta.x();
  wheel_event.delta_y = delta.y();
  wheel_event.phase = phase;
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->ForwardWheelEvent(wheel_event);
}

#if !defined(OS_MACOSX)
void SimulateMouseWheelCtrlZoomEvent(WebContents* web_contents,
                                     const gfx::Point& point,
                                     bool zoom_in,
                                     blink::WebMouseWheelEvent::Phase phase) {
  blink::WebMouseWheelEvent wheel_event(blink::WebInputEvent::kMouseWheel,
                                        blink::WebInputEvent::kControlKey,
                                        ui::EventTimeForNow());

  wheel_event.SetPositionInWidget(point.x(), point.y());
  wheel_event.delta_y =
      (zoom_in ? 1.0 : -1.0) * ui::MouseWheelEvent::kWheelDelta;
  wheel_event.wheel_ticks_y = (zoom_in ? 1.0 : -1.0);
  wheel_event.has_precise_scrolling_deltas = false;
  wheel_event.phase = phase;
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->ForwardWheelEvent(wheel_event);
}
#endif  // !defined(OS_MACOSX)

void SimulateGesturePinchSequence(WebContents* web_contents,
                                  const gfx::Point& point,
                                  float scale,
                                  blink::WebGestureDevice source_device) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());

  blink::WebGestureEvent pinch_begin(blink::WebInputEvent::kGesturePinchBegin,
                                     blink::WebInputEvent::kNoModifiers,
                                     ui::EventTimeForNow(), source_device);
  pinch_begin.SetPositionInWidget(gfx::PointF(point));
  pinch_begin.SetPositionInScreen(gfx::PointF(point));
  pinch_begin.SetNeedsWheelEvent(source_device ==
                                 blink::kWebGestureDeviceTouchpad);
  widget_host->ForwardGestureEvent(pinch_begin);

  blink::WebGestureEvent pinch_update(pinch_begin);
  pinch_update.SetType(blink::WebInputEvent::kGesturePinchUpdate);
  pinch_update.data.pinch_update.scale = scale;
  pinch_update.SetNeedsWheelEvent(source_device ==
                                  blink::kWebGestureDeviceTouchpad);
  widget_host->ForwardGestureEvent(pinch_update);

  blink::WebGestureEvent pinch_end(pinch_begin);
  pinch_end.SetType(blink::WebInputEvent::kGesturePinchEnd);
  pinch_end.SetNeedsWheelEvent(source_device ==
                               blink::kWebGestureDeviceTouchpad);
  widget_host->ForwardGestureEvent(pinch_end);
}

void SimulateGestureScrollSequence(WebContents* web_contents,
                                   const gfx::Point& point,
                                   const gfx::Vector2dF& delta) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());

  blink::WebGestureEvent scroll_begin(
      blink::WebGestureEvent::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::kWebGestureDeviceTouchpad);
  scroll_begin.SetPositionInWidget(gfx::PointF(point));
  scroll_begin.data.scroll_begin.delta_x_hint = delta.x();
  scroll_begin.data.scroll_begin.delta_y_hint = delta.y();
  widget_host->ForwardGestureEvent(scroll_begin);

  blink::WebGestureEvent scroll_update(
      blink::WebGestureEvent::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::kWebGestureDeviceTouchpad);
  scroll_update.SetPositionInWidget(gfx::PointF(point));
  scroll_update.data.scroll_update.delta_x = delta.x();
  scroll_update.data.scroll_update.delta_y = delta.y();
  scroll_update.data.scroll_update.velocity_x = 0;
  scroll_update.data.scroll_update.velocity_y = 0;
  widget_host->ForwardGestureEvent(scroll_update);

  blink::WebGestureEvent scroll_end(blink::WebGestureEvent::kGestureScrollEnd,
                                    blink::WebInputEvent::kNoModifiers,
                                    ui::EventTimeForNow(),
                                    blink::kWebGestureDeviceTouchpad);
  scroll_end.SetPositionInWidget(gfx::PointF(point));
  widget_host->ForwardGestureEvent(scroll_end);
}

void SimulateGestureFlingSequence(WebContents* web_contents,
                                  const gfx::Point& point,
                                  const gfx::Vector2dF& velocity) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());

  blink::WebGestureEvent scroll_begin(
      blink::WebGestureEvent::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::kWebGestureDeviceTouchpad);
  scroll_begin.SetPositionInWidget(gfx::PointF(point));
  widget_host->ForwardGestureEvent(scroll_begin);

  blink::WebGestureEvent scroll_end(blink::WebGestureEvent::kGestureScrollEnd,
                                    blink::WebInputEvent::kNoModifiers,
                                    ui::EventTimeForNow(),
                                    blink::kWebGestureDeviceTouchpad);
  scroll_end.SetPositionInWidget(gfx::PointF(point));
  widget_host->ForwardGestureEvent(scroll_end);

  blink::WebGestureEvent fling_start(blink::WebGestureEvent::kGestureFlingStart,
                                     blink::WebInputEvent::kNoModifiers,
                                     ui::EventTimeForNow(),
                                     blink::kWebGestureDeviceTouchpad);
  fling_start.SetPositionInWidget(gfx::PointF(point));
  fling_start.data.fling_start.target_viewport = false;
  fling_start.data.fling_start.velocity_x = velocity.x();
  fling_start.data.fling_start.velocity_y = velocity.y();
  widget_host->ForwardGestureEvent(fling_start);
}

void SimulateGestureEvent(WebContents* web_contents,
                          const blink::WebGestureEvent& gesture_event,
                          const ui::LatencyInfo& latency) {
  RenderWidgetHostViewBase* view = static_cast<RenderWidgetHostViewBase*>(
      web_contents->GetRenderWidgetHostView());
  view->ProcessGestureEvent(gesture_event, latency);
}

void SimulateTouchGestureAt(WebContents* web_contents,
                            const gfx::Point& point,
                            blink::WebInputEvent::Type type) {
  blink::WebGestureEvent gesture(type, 0, ui::EventTimeForNow(),
                                 blink::kWebGestureDeviceTouchscreen);
  gesture.SetPositionInWidget(gfx::PointF(point));
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->ForwardGestureEvent(gesture);
}

void SimulateTapDownAt(WebContents* web_contents, const gfx::Point& point) {
  SimulateTouchGestureAt(web_contents, point,
                         blink::WebGestureEvent::kGestureTapDown);
}

void SimulateTapAt(WebContents* web_contents, const gfx::Point& point) {
  SimulateTouchGestureAt(web_contents, point,
                         blink::WebGestureEvent::kGestureTap);
}

void SimulateTapWithModifiersAt(WebContents* web_contents,
                                unsigned modifiers,
                                const gfx::Point& point) {
  blink::WebGestureEvent tap(blink::WebGestureEvent::kGestureTap, modifiers,
                             ui::EventTimeForNow(),
                             blink::kWebGestureDeviceTouchpad);
  tap.SetPositionInWidget(gfx::PointF(point));
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->ForwardGestureEvent(tap);
}

#if defined(USE_AURA)
void SimulateTouchPressAt(WebContents* web_contents, const gfx::Point& point) {
  ui::TouchEvent touch(
      ui::ET_TOUCH_PRESSED, point, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView())
      ->OnTouchEvent(&touch);
}

void SimulateLongTapAt(WebContents* web_contents, const gfx::Point& point) {
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());

  ui::TouchEvent touch_start(
      ui::ET_TOUCH_PRESSED, point, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_start);

  ui::GestureEventDetails tap_down_details(ui::ET_GESTURE_TAP_DOWN);
  tap_down_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent tap_down(point.x(), point.y(), 0, ui::EventTimeForNow(),
                            tap_down_details, touch_start.unique_event_id());
  rwhva->OnGestureEvent(&tap_down);

  ui::GestureEventDetails long_press_details(ui::ET_GESTURE_LONG_PRESS);
  long_press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent long_press(point.x(), point.y(), 0, ui::EventTimeForNow(),
                              long_press_details,
                              touch_start.unique_event_id());
  rwhva->OnGestureEvent(&long_press);

  ui::TouchEvent touch_end(
      ui::ET_TOUCH_RELEASED, point, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_end);

  ui::GestureEventDetails long_tap_details(ui::ET_GESTURE_LONG_TAP);
  long_tap_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent long_tap(point.x(), point.y(), 0, ui::EventTimeForNow(),
                            long_tap_details, touch_end.unique_event_id());
  rwhva->OnGestureEvent(&long_tap);
}
#endif

void SimulateKeyPress(WebContents* web_contents,
                      ui::DomKey key,
                      ui::DomCode code,
                      ui::KeyboardCode key_code,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command) {
  SimulateKeyPressImpl(web_contents, key, code, key_code, control, shift, alt,
                       command, /*send_char=*/true);
}

void SimulateKeyPressWithoutChar(WebContents* web_contents,
                                 ui::DomKey key,
                                 ui::DomCode code,
                                 ui::KeyboardCode key_code,
                                 bool control,
                                 bool shift,
                                 bool alt,
                                 bool command) {
  SimulateKeyPressImpl(web_contents, key, code, key_code, control, shift, alt,
                       command, /*send_char=*/false);
}

ScopedSimulateModifierKeyPress::ScopedSimulateModifierKeyPress(
    WebContents* web_contents,
    bool control,
    bool shift,
    bool alt,
    bool command)
    : web_contents_(web_contents),
      modifiers_(0),
      control_(control),
      shift_(shift),
      alt_(alt),
      command_(command) {
  modifiers_ =
      SimulateModifierKeysDown(web_contents_, control_, shift_, alt_, command_);
}

ScopedSimulateModifierKeyPress::~ScopedSimulateModifierKeyPress() {
  modifiers_ = SimulateModifierKeysUp(web_contents_, control_, shift_, alt_,
                                      command_, modifiers_);
  DCHECK_EQ(0, modifiers_);
}

void ScopedSimulateModifierKeyPress::MouseClickAt(
    int additional_modifiers,
    blink::WebMouseEvent::Button button,
    const gfx::Point& point) {
  SimulateMouseClickAt(web_contents_, modifiers_ | additional_modifiers, button,
                       point);
}

void ScopedSimulateModifierKeyPress::KeyPress(ui::DomKey key,
                                              ui::DomCode code,
                                              ui::KeyboardCode key_code) {
  SimulateKeyEvent(web_contents_, key, code, key_code, /*send_char=*/true,
                   modifiers_);
}

void ScopedSimulateModifierKeyPress::KeyPressWithoutChar(
    ui::DomKey key,
    ui::DomCode code,
    ui::KeyboardCode key_code) {
  SimulateKeyEvent(web_contents_, key, code, key_code, /*send_char=*/false,
                   modifiers_);
}

bool IsWebcamAvailableOnSystem(WebContents* web_contents) {
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, kHasVideoInputDeviceOnSystem, &result));
  return result == kHasVideoInputDevice;
}

RenderFrameHost* ConvertToRenderFrameHost(WebContents* web_contents) {
  return web_contents->GetMainFrame();
}

RenderFrameHost* ConvertToRenderFrameHost(RenderFrameHost* render_frame_host) {
  return render_frame_host;
}

bool ExecuteScript(const ToRenderFrameHost& adapter,
                   const std::string& script) {
  return ExecuteScriptWithUserGestureControl(adapter.render_frame_host(),
                                             script, true);
}

bool ExecuteScriptWithoutUserGesture(const ToRenderFrameHost& adapter,
                                     const std::string& script) {
  return ExecuteScriptWithUserGestureControl(adapter.render_frame_host(),
                                             script, false);
}

void ExecuteScriptAsync(const ToRenderFrameHost& adapter,
                        const std::string& script) {
  adapter.render_frame_host()->ExecuteJavaScriptWithUserGestureForTests(
      base::UTF8ToUTF16(script));
}

bool ExecuteScriptAndExtractDouble(const ToRenderFrameHost& adapter,
                                   const std::string& script, double* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  return ExecuteScriptHelper(adapter.render_frame_host(), script, true,
                             ISOLATED_WORLD_ID_GLOBAL, &value) &&
         value && value->GetAsDouble(result);
}

bool ExecuteScriptAndExtractInt(const ToRenderFrameHost& adapter,
                                const std::string& script, int* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  return ExecuteScriptHelper(adapter.render_frame_host(), script, true,
                             ISOLATED_WORLD_ID_GLOBAL, &value) &&
         value && value->GetAsInteger(result);
}

bool ExecuteScriptAndExtractBool(const ToRenderFrameHost& adapter,
                                 const std::string& script, bool* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  return ExecuteScriptHelper(adapter.render_frame_host(), script, true,
                             ISOLATED_WORLD_ID_GLOBAL, &value) &&
         value && value->GetAsBoolean(result);
}

bool ExecuteScriptAndExtractString(const ToRenderFrameHost& adapter,
                                   const std::string& script,
                                   std::string* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  return ExecuteScriptHelper(adapter.render_frame_host(), script, true,
                             ISOLATED_WORLD_ID_GLOBAL, &value) &&
         value && value->GetAsString(result);
}

bool ExecuteScriptWithoutUserGestureAndExtractDouble(
    const ToRenderFrameHost& adapter,
    const std::string& script,
    double* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  return ExecuteScriptHelper(adapter.render_frame_host(), script, false,
                             ISOLATED_WORLD_ID_GLOBAL, &value) &&
         value && value->GetAsDouble(result);
}

bool ExecuteScriptWithoutUserGestureAndExtractInt(
    const ToRenderFrameHost& adapter,
    const std::string& script,
    int* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  return ExecuteScriptHelper(adapter.render_frame_host(), script, false,
                             ISOLATED_WORLD_ID_GLOBAL, &value) &&
         value && value->GetAsInteger(result);
}

bool ExecuteScriptWithoutUserGestureAndExtractBool(
    const ToRenderFrameHost& adapter,
    const std::string& script,
    bool* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  return ExecuteScriptHelper(adapter.render_frame_host(), script, false,
                             ISOLATED_WORLD_ID_GLOBAL, &value) &&
         value && value->GetAsBoolean(result);
}

bool ExecuteScriptWithoutUserGestureAndExtractString(
    const ToRenderFrameHost& adapter,
    const std::string& script,
    std::string* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  return ExecuteScriptHelper(adapter.render_frame_host(), script, false,
                             ISOLATED_WORLD_ID_GLOBAL, &value) &&
         value && value->GetAsString(result);
}

// EvalJsResult methods.
EvalJsResult::EvalJsResult(base::Value value, const std::string& error)
    : value(error.empty() ? std::move(value) : base::Value()), error(error) {}

EvalJsResult::EvalJsResult(const EvalJsResult& other)
    : value(other.value.Clone()), error(other.error) {}

const std::string& EvalJsResult::ExtractString() const {
  CHECK(error.empty())
      << "Can't ExtractString() because the script encountered a problem: "
      << error;
  CHECK(value.is_string()) << "Can't ExtractString() because script result: "
                           << value << "is not a string.";
  return value.GetString();
}

int EvalJsResult::ExtractInt() const {
  CHECK(error.empty())
      << "Can't ExtractInt() because the script encountered a problem: "
      << error;
  CHECK(value.is_int()) << "Can't ExtractInt() because script result: " << value
                        << "is not an int.";
  return value.GetInt();
}

bool EvalJsResult::ExtractBool() const {
  CHECK(error.empty())
      << "Can't ExtractBool() because the script encountered a problem: "
      << error;
  CHECK(value.is_bool()) << "Can't ExtractBool() because script result: "
                         << value << "is not a bool.";
  return value.GetBool();
}

double EvalJsResult::ExtractDouble() const {
  CHECK(error.empty())
      << "Can't ExtractDouble() because the script encountered a problem: "
      << error;
  CHECK(value.is_double() || value.is_int())
      << "Can't ExtractDouble() because script result: " << value
      << "is not a double or int.";
  return value.GetDouble();
}

base::ListValue EvalJsResult::ExtractList() const {
  CHECK(error.empty())
      << "Can't ExtractList() because the script encountered a problem: "
      << error;
  CHECK(value.is_list()) << "Can't ExtractList() because script result: "
                         << value << "is not a list.";
  return base::ListValue(value.GetList());
}

void PrintTo(const EvalJsResult& bar, ::std::ostream* os) {
  if (!bar.error.empty()) {
    *os << bar.error;
  } else {
    *os << bar.value;
  }
}

namespace {

// Parse a JS stack trace out of |js_error|, detect frames that match
// |source_name|, and interleave the appropriate lines of source code from
// |source| into the error report. This is meant to be useful for scripts that
// are passed to ExecuteScript functions, and hence dynamically generated.
//
// An adjustment of |column_adjustment_for_line_one| characters is subtracted
// when mapping positions from line 1 of |source|. This is to offset the effect
// of boilerplate added by the script runner.
//
// TODO(nick): Elide snippets to 80 chars, since it is common for sources to not
// include newlines.
std::string AnnotateAndAdjustJsStackTraces(const std::string& js_error,
                                           std::string source_name,
                                           const std::string& source,
                                           int column_adjustment_for_line_one) {
  // Escape wildcards in |source_name| for use in MatchPattern.
  base::ReplaceChars(source_name, "\\", "\\\\", &source_name);
  base::ReplaceChars(source_name, "*", "\\*", &source_name);
  base::ReplaceChars(source_name, "?", "\\?", &source_name);

  // This vector maps line numbers to the corresponding text in |source|.
  const std::vector<base::StringPiece> source_lines = base::SplitStringPiece(
      source, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // |source_frame_pattern| should match any line that looks like a stack frame
  // from a source file named |source_name|.
  const std::string source_frame_pattern =
      base::StringPrintf("    at * (%s:*:*)", source_name.c_str());

  // This is the amount of indentation that is applied to the lines of inserted
  // annotations.
  const std::string indent(8, ' ');
  const base::StringPiece elision_mark = "";

  // Loop over each line of |js_error|, and append each to |annotated_error| --
  // possibly rewriting to include extra context.
  std::ostringstream annotated_error;
  for (const base::StringPiece& error_line : base::SplitStringPiece(
           js_error, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    // Does this look like a stack frame whose URL source matches |source_name|?
    if (base::MatchPattern(error_line, source_frame_pattern)) {
      // When a match occurs, annotate the stack trace with the corresponding
      // line from |source|, along with a ^^^ underneath, indicating the column
      // position.
      std::vector<base::StringPiece> error_line_parts = base::SplitStringPiece(
          error_line, ":", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
      CHECK_GE(error_line_parts.size(), 2u);

      int column_number = 0;
      base::StringToInt(error_line_parts.back(), &column_number);
      error_line_parts.pop_back();
      int line_number = 0;
      base::StringToInt(error_line_parts.back(), &line_number);
      error_line_parts.pop_back();

      // Protect against out-of-range matches.
      if ((line_number > 0) && (column_number > 0) &&
          static_cast<size_t>(line_number) <= source_lines.size()) {
        // Apply adjustment requested by caller to columns on the first line.
        // This allows us to add preamble boilerplate to the script, but still
        // locate errors correctly.
        if (line_number == 1 && column_number > column_adjustment_for_line_one)
          column_number -= column_adjustment_for_line_one;

        // Some source lines are huge. Elide |source_line| so that it doesn't
        // occupy more than one actual line.
        std::string source_line = source_lines[line_number - 1].as_string();

        int max_column_number = 60 - indent.length();
        if (column_number > max_column_number) {
          source_line = source_line.substr(column_number - max_column_number);
          column_number = max_column_number;
          source_line.replace(0, elision_mark.length(), elision_mark.data(),
                              elision_mark.length());
        }

        size_t max_length = 80 - indent.length();
        if (source_line.length() > max_length) {
          source_line =
              source_line.substr(0, max_length - elision_mark.length());
          elision_mark.AppendToString(&source_line);
        }

        annotated_error << base::JoinString(error_line_parts, ":") << ":"
                        << line_number << ":" << column_number << "):\n"
                        << indent << source_line << '\n'
                        << indent << std::string(column_number - 1, ' ')
                        << "^^^^^\n";
        continue;
      }
    }
    // This line was not rewritten -- just append it as-is.
    annotated_error << error_line << "\n";
  }
  return annotated_error.str();
}

}  // namespace

testing::AssertionResult ExecJs(const ToRenderFrameHost& execution_target,
                                const std::string& script,
                                int options,
                                int world_id) {
  CHECK(!(options & EXECUTE_SCRIPT_USE_MANUAL_REPLY))
      << "USE_MANUAL_REPLY does not make sense with ExecJs.";

  // ExecJs() doesn't care about the result, so disable promise resolution.
  // Instead of using ExecJs() to wait for an async event, callers may use
  // EvalJs() with a sentinel result value like "success".
  options |= EXECUTE_SCRIPT_NO_RESOLVE_PROMISES;

  // TODO(nick): Do we care enough about folks shooting themselves in the foot
  // here with e.g. ASSERT_TRUE(ExecJs("window == window.top")) -- when they
  // mean EvalJs -- to fail a CHECK() when eval_result.value.is_bool()?
  EvalJsResult eval_result =
      EvalJs(execution_target, script, options, world_id);

  // NOTE: |eval_result.value| is intentionally ignored by ExecJs().
  if (!eval_result.error.empty())
    return testing::AssertionFailure() << eval_result.error;
  return testing::AssertionSuccess();
}

EvalJsResult EvalJs(const ToRenderFrameHost& execution_target,
                    const std::string& script,
                    int options,
                    int world_id) {
  // The sourceURL= parameter provides a string that replaces <anonymous> in
  // stack traces, if an Error is thrown. 'std::string' is meant to communicate
  // that this is a dynamic argument originating from C++ code.
  const char* kSourceURL = "__const_std::string&_script__";
  std::string modified_script =
      base::StringPrintf("%s;\n//# sourceURL=%s", script.c_str(), kSourceURL);

  // An extra eval() indirection is used here to catch syntax errors and return
  // them as assertion failures. This eval() operation deliberately occurs in
  // the global scope, so 'var' declarations in |script| will persist for later
  // script executions. (As an aside: global/local scope for eval depends on
  // whether 'eval' is called directly or indirectly; 'window.eval()' is
  // indirect).
  //
  // The call to eval() itself is inside a .then() handler so that syntax errors
  // result in Promise rejection. Calling eval() either throws (in the event of
  // a SyntaxError) or returns the script's completion value.
  //
  // The result of eval() (i.e., the statement completion value of |script|) is
  // wrapped in an array and passed to a second .then() handler. If eval()
  // returned a Promise and the |resolve_promises| option is set, this handler
  // calls Promise.all to reply after the returned Promise resolves.
  //
  // If |script| evaluated successfully, the third.then() handler maps the
  // resolved |result| of eval() to a |reply| that is a one-element list
  // containing the value (this element can be any JSON-serializable type). If
  // the manual reply option is being used, no reply is emitted after successful
  // execution -- the script is expected to call send() itself. The call to
  // Promise.reject() squelches this reply, and the final .then() handler is not
  // called.
  //
  // If an uncaught error was thrown, or eval() returns a Promise that is
  // rejected, the third .then() handler maps the |error| to a |reply| that is
  // a string value.
  //
  // The fourth and final .then() handler passes the |reply| (whether
  // successful or unsuccessful) to domAutomationController.send(), so that it's
  // transmitted back here in browser process C++ land. A GUID token is also
  // included, that protects against |script| directly calling
  // domAutomationController.send() itself, which is disallowed in EvalJs.
  bool use_automatic_reply = !(options & EXECUTE_SCRIPT_USE_MANUAL_REPLY);
  bool resolve_promises = !(options & EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);

  std::string token = "EvalJs-" + base::GenerateGUID();
  std::string runner_script = JsReplace(
      R"(Promise.resolve($1)
         .then(script => [window.eval(script)])
         .then((result) => $2 ? Promise.all(result) : result )
         .then((result) => $3 ? result : Promise.reject(),
               (error) => 'a JavaScript error:' +
                          (error && error.stack ? '\n' + error.stack
                                                : ' "' + error + '"'))
         .then((reply) => window.domAutomationController.send([$4, reply]));
      //# sourceURL=EvalJs-runner.js)",
      modified_script, resolve_promises, use_automatic_reply, token);

  bool user_gesture = !(options & EXECUTE_SCRIPT_NO_USER_GESTURE);
  std::ostringstream error_stream;
  std::unique_ptr<base::Value> response;
  if (!execution_target.render_frame_host()->IsRenderFrameLive()) {
    error_stream << "Error: EvalJs won't work on an already-crashed frame.";
  } else if (!ExecuteScriptHelper(execution_target.render_frame_host(),
                                  runner_script, user_gesture, world_id,
                                  &response)) {
    error_stream << "Internal Error: ExecuteScriptHelper failed";
  } else if (!response) {
    error_stream << "Internal Error: no value";
  } else {
    bool is_reply_from_runner_script =
        response->is_list() && response->GetList().size() == 2 &&
        response->GetList()[0].is_string() &&
        response->GetList()[0].GetString() == token;

    bool is_error =
        is_reply_from_runner_script && response->GetList()[1].is_string();
    bool is_automatic_success_reply =
        is_reply_from_runner_script && response->GetList()[1].is_list() &&
        response->GetList()[1].GetList().size() == 1;

    if (is_error) {
      // This is a response generated by the error handler in our runner
      // script. This occurs when the script throws an exception, or when
      // eval throws a SyntaxError.
      //
      // Parse the stack trace here, and interleave lines of source code from
      // |script| to aid debugging.
      std::string error_text = response->GetList()[1].GetString();

      if (base::StartsWith(error_text,
                           "a JavaScript error:\nEvalError: Refused",
                           base::CompareCase::SENSITIVE)) {
        error_text =
            "EvalJs encountered an EvalError, because eval() is blocked by the "
            "document's CSP on this page. To test content that is protected by "
            "CSP, consider using EvalJs with an isolated world. Details: " +
            error_text;
      }

      CHECK(!error_text.empty());
      error_stream << AnnotateAndAdjustJsStackTraces(error_text, kSourceURL,
                                                     script, 0);
    } else if (!use_automatic_reply) {
      // When |script| itself calls domAutomationController.send() on success,
      // |response| could be anything; so there's no more checking we can do:
      // return |response| as success, with an empty error.
      return EvalJsResult(std::move(*response), std::string());
    } else if (is_automatic_success_reply) {
      // Got a response from the runner script that indicates success (of the
      // form [token, [completion_value]]. Return the completion value, with an
      // empty error.
      return EvalJsResult(std::move(response->GetList()[1].GetList()[0]),
                          std::string());
    } else {
      // The response was not well-formed (it failed the token match), so it's
      // not from our runner script. Fail with an explanation of the raw
      // message. This allows us to reject other calls
      // domAutomationController.send().
      error_stream
          << "Internal Error: expected a 2-element list of the form "
          << "['" << token << "', [result]]; but got instead: " << *response
          << " ... This is potentially because a script tried to call "
             "domAutomationController.send itself -- that is only allowed "
             "when using EvalJsWithManualReply().  When using EvalJs(), result "
             "values are just the result of calling eval() on the script -- "
             "the completion value is the value of the last executed "
             "statement.  When using ExecJs(), there is no result value.";
    }
  }

  // Something went wrong. Return an empty value and a non-empty error.
  return EvalJsResult(base::Value(), error_stream.str());
}

EvalJsResult EvalJsWithManualReply(const ToRenderFrameHost& execution_target,
                                   const std::string& script,
                                   int options,
                                   int world_id) {
  return EvalJs(execution_target, script,
                options | EXECUTE_SCRIPT_USE_MANUAL_REPLY, world_id);
}

namespace {
void AddToSetIfFrameMatchesPredicate(
    std::set<RenderFrameHost*>* frame_set,
    const base::Callback<bool(RenderFrameHost*)>& predicate,
    RenderFrameHost* host) {
  if (predicate.Run(host))
    frame_set->insert(host);
}
}

RenderFrameHost* FrameMatchingPredicate(
    WebContents* web_contents,
    const base::Callback<bool(RenderFrameHost*)>& predicate) {
  std::set<RenderFrameHost*> frame_set;
  web_contents->ForEachFrame(base::BindRepeating(
      &AddToSetIfFrameMatchesPredicate, &frame_set, predicate));
  EXPECT_EQ(1U, frame_set.size());
  return frame_set.size() == 1 ? *frame_set.begin() : nullptr;
}

bool FrameMatchesName(const std::string& name, RenderFrameHost* frame) {
  return frame->GetFrameName() == name;
}

bool FrameIsChildOfMainFrame(RenderFrameHost* frame) {
  return frame->GetParent() && !frame->GetParent()->GetParent();
}

bool FrameHasSourceUrl(const GURL& url, RenderFrameHost* frame) {
  return frame->GetLastCommittedURL() == url;
}

RenderFrameHost* ChildFrameAt(RenderFrameHost* frame, size_t index) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(frame);
  if (index >= rfh->frame_tree_node()->child_count())
    return nullptr;
  return rfh->frame_tree_node()->child_at(index)->current_frame_host();
}

bool ExecuteWebUIResourceTest(WebContents* web_contents,
                              const std::vector<int>& js_resource_ids) {
  // Inject WebUI test runner script first prior to other scripts required to
  // run the test as scripts may depend on it being declared.
  std::vector<int> ids;
  ids.push_back(IDR_WEBUI_JS_WEBUI_RESOURCE_TEST);
  ids.insert(ids.end(), js_resource_ids.begin(), js_resource_ids.end());

  std::string script;
  for (int id : ids) {
    scoped_refptr<base::RefCountedMemory> bytes =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(id);

    if (HasGzipHeader(*bytes))
      AppendGzippedResource(*bytes, &script);
    else
      script.append(bytes->front_as<char>(), bytes->size());

    script.append("\n");
  }
  ExecuteScriptAsync(web_contents, script);

  DOMMessageQueue message_queue;

  bool should_wait_flag =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kWaitForDebuggerWebUI);

  const std::string debugger_port =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          ::switches::kRemoteDebuggingPort);

  // Only wait if there is a debugger port, so user can issue go() command.
  if (should_wait_flag && !debugger_port.empty()) {
    ExecuteScriptAsync(
        web_contents,
        "window.waitUser = true; "
        "window.go = function() { window.waitUser = false }; "
        "console.log('Waiting for debugger...'); "
        "console.log('Run: go() in the JS console when you are ready.');");
  }

  ExecuteScriptAsync(web_contents, "runTests()");

  std::string message;
  do {
    if (!message_queue.WaitForMessage(&message))
      return false;
  } while (message.compare("\"PENDING\"") == 0);

  return message.compare("\"SUCCESS\"") == 0;
}

std::string GetCookies(BrowserContext* browser_context, const GURL& url) {
  std::string cookies;
  base::RunLoop run_loop;
  network::mojom::CookieManagerPtr cookie_manager;
  BrowserContext::GetDefaultStoragePartition(browser_context)
      ->GetNetworkContext()
      ->GetCookieManager(mojo::MakeRequest(&cookie_manager));
  cookie_manager->GetCookieList(
      url, net::CookieOptions(),
      base::BindOnce(
          [](std::string* cookies_out, base::RunLoop* run_loop,
             const std::vector<net::CanonicalCookie>& cookies) {
            *cookies_out = net::CanonicalCookie::BuildCookieLine(cookies);
            run_loop->Quit();
          },
          &cookies, &run_loop));
  run_loop.Run();
  return cookies;
}

std::vector<net::CanonicalCookie> GetCanonicalCookies(
    BrowserContext* browser_context,
    const GURL& url) {
  std::vector<net::CanonicalCookie> cookies;
  base::RunLoop run_loop;
  network::mojom::CookieManagerPtr cookie_manager;
  BrowserContext::GetDefaultStoragePartition(browser_context)
      ->GetNetworkContext()
      ->GetCookieManager(mojo::MakeRequest(&cookie_manager));
  cookie_manager->GetCookieList(
      url, net::CookieOptions(),
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::vector<net::CanonicalCookie>* cookies_out,
             const std::vector<net::CanonicalCookie>& cookies) {
            *cookies_out = cookies;
            run_loop->Quit();
          },
          &run_loop, &cookies));
  run_loop.Run();
  return cookies;
}

bool SetCookie(BrowserContext* browser_context,
               const GURL& url,
               const std::string& value) {
  bool result = false;
  base::RunLoop run_loop;
  network::mojom::CookieManagerPtr cookie_manager;
  BrowserContext::GetDefaultStoragePartition(browser_context)
      ->GetNetworkContext()
      ->GetCookieManager(mojo::MakeRequest(&cookie_manager));
  std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
      url, value, base::Time::Now(), net::CookieOptions()));
  DCHECK(cc.get());

  cookie_manager->SetCanonicalCookie(
      *cc.get(), true /* secure_source */, true /* modify_http_only */,
      base::BindOnce(
          [](bool* result, base::RunLoop* run_loop, bool success) {
            *result = success;
            run_loop->Quit();
          },
          &result, &run_loop));
  run_loop.Run();
  return result;
}

void FetchHistogramsFromChildProcesses() {
  base::RunLoop run_loop;

  FetchHistogramsAsynchronously(
      base::ThreadTaskRunnerHandle::Get(), run_loop.QuitClosure(),
      // If this call times out, it means that a child process is not
      // responding, which is something we should not ignore.  The timeout is
      // set to be longer than the normal browser test timeout so that it will
      // be prempted by the normal timeout.
      TestTimeouts::action_max_timeout());
  run_loop.Run();
}

void SetupCrossSiteRedirector(net::EmbeddedTestServer* embedded_test_server) {
  embedded_test_server->RegisterRequestHandler(
      base::Bind(&CrossSiteRedirectResponseHandler, embedded_test_server));
}

void WaitForInterstitialAttach(content::WebContents* web_contents) {
  if (web_contents->ShowingInterstitialPage())
    return;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  InterstitialObserver observer(web_contents, run_loop.QuitClosure(),
                                base::OnceClosure());
  run_loop.Run();
}

void WaitForInterstitialDetach(content::WebContents* web_contents) {
  RunTaskAndWaitForInterstitialDetach(web_contents, base::Closure());
}

void RunTaskAndWaitForInterstitialDetach(content::WebContents* web_contents,
                                         const base::Closure& task) {
  if (!web_contents || !web_contents->ShowingInterstitialPage())
    return;
  base::RunLoop run_loop;
  InterstitialObserver observer(web_contents, base::OnceClosure(),
                                run_loop.QuitClosure());
  if (!task.is_null())
    task.Run();
  // At this point, web_contents may have been deleted.
  run_loop.Run();
}

bool WaitForRenderFrameReady(RenderFrameHost* rfh) {
  if (!rfh)
    return false;
  // TODO(nick): This can't switch to EvalJs yet, because of hardcoded
  // dependencies on 'pageLoadComplete' in some interstitial implementations.
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      rfh,
      "(async function() {"
      "  if (document.readyState != 'complete') {"
      "    await new Promise((resolve) =>"
      "      document.addEventListener('readystatechange', event => {"
      "        if (document.readyState == 'complete') {"
      "          resolve();"
      "        }"
      "      }));"
      "  }"
      "})().then(() => {"
      "  window.domAutomationController.send('pageLoadComplete');"
      "});",
      &result));
  EXPECT_EQ("pageLoadComplete", result);
  return "pageLoadComplete" == result;
}

void EnableAccessibilityForWebContents(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  web_contents_impl->SetAccessibilityMode(ui::kAXModeComplete);
}

void WaitForAccessibilityFocusChange() {
  base::RunLoop run_loop;
  BrowserAccessibilityManager::SetFocusChangeCallbackForTesting(
      run_loop.QuitClosure());
  run_loop.Run();
}

ui::AXNodeData GetFocusedAccessibilityNodeInfo(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  BrowserAccessibilityManager* manager =
      web_contents_impl->GetRootBrowserAccessibilityManager();
  if (!manager)
    return ui::AXNodeData();
  BrowserAccessibility* focused_node = manager->GetFocus();
  return focused_node->GetData();
}

bool AccessibilityTreeContainsNodeWithName(BrowserAccessibility* node,
                                           const std::string& name) {
  if (node->GetStringAttribute(ax::mojom::StringAttribute::kName) == name)
    return true;
  for (unsigned i = 0; i < node->PlatformChildCount(); i++) {
    if (AccessibilityTreeContainsNodeWithName(node->PlatformGetChild(i), name))
      return true;
  }
  return false;
}

bool ListenToGuestWebContents(
    AccessibilityNotificationWaiter* accessibility_waiter,
    WebContents* web_contents) {
  accessibility_waiter->ListenToAdditionalFrame(
      static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame()));
  return true;
}

void WaitForAccessibilityTreeToContainNodeWithName(WebContents* web_contents,
                                                   const std::string& name) {
  WebContentsImpl* web_contents_impl = static_cast<WebContentsImpl*>(
      web_contents);
  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      web_contents_impl->GetMainFrame());
  BrowserAccessibilityManager* main_frame_manager =
      main_frame->browser_accessibility_manager();
  FrameTree* frame_tree = web_contents_impl->GetFrameTree();
  while (!main_frame_manager || !AccessibilityTreeContainsNodeWithName(
             main_frame_manager->GetRoot(), name)) {
    AccessibilityNotificationWaiter accessibility_waiter(
        main_frame, ax::mojom::Event::kNone);
    for (FrameTreeNode* node : frame_tree->Nodes()) {
      accessibility_waiter.ListenToAdditionalFrame(
          node->current_frame_host());
    }

    content::BrowserPluginGuestManager* guest_manager =
        web_contents_impl->GetBrowserContext()->GetGuestManager();
    if (guest_manager) {
      guest_manager->ForEachGuest(web_contents_impl,
                                  base::BindRepeating(&ListenToGuestWebContents,
                                                      &accessibility_waiter));
    }

    accessibility_waiter.WaitForNotification();
    main_frame_manager = main_frame->browser_accessibility_manager();
  }
}

ui::AXTreeUpdate GetAccessibilityTreeSnapshot(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  BrowserAccessibilityManager* manager =
      web_contents_impl->GetRootBrowserAccessibilityManager();
  if (!manager)
    return ui::AXTreeUpdate();
  return manager->SnapshotAXTreeForTesting();
}

bool IsWebContentsBrowserPluginFocused(content::WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  BrowserPluginGuest* browser_plugin_guest =
      web_contents_impl->GetBrowserPluginGuest();
  return browser_plugin_guest ? browser_plugin_guest->focused() : false;
}

RenderWidgetHost* GetMouseLockWidget(WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)->GetMouseLockWidget();
}

RenderWidgetHost* GetKeyboardLockWidget(WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)->GetKeyboardLockWidget();
}

RenderWidgetHost* GetMouseCaptureWidget(WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)
      ->GetInputEventRouter()
      ->GetMouseCaptureWidgetForTests();
}

bool RequestKeyboardLock(WebContents* web_contents,
                         base::Optional<base::flat_set<ui::DomCode>> codes) {
  DCHECK(!codes.has_value() || !codes.value().empty());
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  RenderWidgetHostImpl* render_widget_host_impl =
      web_contents_impl->GetMainFrame()->GetRenderWidgetHost();
  return render_widget_host_impl->RequestKeyboardLock(std::move(codes));
}

void CancelKeyboardLock(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  RenderWidgetHostImpl* render_widget_host_impl =
      web_contents_impl->GetMainFrame()->GetRenderWidgetHost();
  render_widget_host_impl->CancelKeyboardLock();
}

bool IsInnerInterstitialPageConnected(InterstitialPage* interstitial_page) {
  InterstitialPageImpl* impl =
      static_cast<InterstitialPageImpl*>(interstitial_page);

  RenderWidgetHostViewBase* rwhvb =
      static_cast<RenderWidgetHostViewBase*>(impl->GetView());
  EXPECT_TRUE(rwhvb->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewChildFrame* rwhvcf =
      static_cast<RenderWidgetHostViewChildFrame*>(rwhvb);

  CrossProcessFrameConnector* frame_connector =
      static_cast<CrossProcessFrameConnector*>(
          rwhvcf->FrameConnectorForTesting());

  WebContentsImpl* inner_web_contents =
      static_cast<WebContentsImpl*>(impl->GetWebContents());
  FrameTreeNode* outer_node = FrameTreeNode::GloballyFindByID(
      inner_web_contents->GetOuterDelegateFrameTreeNodeId());

  return outer_node->current_frame_host()->GetView() ==
         frame_connector->GetParentRenderWidgetHostView();
}

ScreenOrientationDelegate* GetScreenOrientationDelegate() {
  return ScreenOrientationProvider::GetDelegateForTesting();
}

std::vector<RenderWidgetHostView*> GetInputEventRouterRenderWidgetHostViews(
    WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)
      ->GetInputEventRouter()
      ->GetRenderWidgetHostViewsForTests();
}

RenderWidgetHost* GetFocusedRenderWidgetHost(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  return web_contents_impl->GetFocusedRenderWidgetHost(
      web_contents_impl->GetMainFrame()->GetRenderWidgetHost());
}

WebContents* GetFocusedWebContents(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  return web_contents_impl->GetFocusedWebContents();
}

void RouteMouseEvent(WebContents* web_contents, blink::WebMouseEvent* event) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  web_contents_impl->GetInputEventRouter()->RouteMouseEvent(
      static_cast<RenderWidgetHostViewBase*>(
          web_contents_impl->GetMainFrame()->GetView()),
      event, ui::LatencyInfo());
}

#if defined(USE_AURA)
void SendRoutedTouchTapSequence(content::WebContents* web_contents,
                                gfx::Point point) {
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  ui::TouchEvent touch_start(
      ui::ET_TOUCH_PRESSED, point, base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_start);
  ui::TouchEvent touch_end(
      ui::ET_TOUCH_RELEASED, point, base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_end);
}

void SendRoutedGestureTapSequence(content::WebContents* web_contents,
                                  gfx::Point point) {
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  ui::GestureEventDetails gesture_tap_down_details(ui::ET_GESTURE_TAP_DOWN);
  gesture_tap_down_details.set_is_source_touch_event_set_non_blocking(true);
  gesture_tap_down_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_tap_down(point.x(), point.y(), 0,
                                    base::TimeTicks::Now(),
                                    gesture_tap_down_details);
  rwhva->OnGestureEvent(&gesture_tap_down);
  ui::GestureEventDetails gesture_tap_details(ui::ET_GESTURE_TAP);
  gesture_tap_details.set_is_source_touch_event_set_non_blocking(true);
  gesture_tap_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  gesture_tap_details.set_tap_count(1);
  ui::GestureEvent gesture_tap(point.x(), point.y(), 0, base::TimeTicks::Now(),
                               gesture_tap_details);
  rwhva->OnGestureEvent(&gesture_tap);
}

#endif

namespace {

RenderFrameMetadataProvider* RenderFrameMetadataProviderFromFrameTreeNode(
    FrameTreeNode* node) {
  DCHECK(node);
  DCHECK(node->current_frame_host());
  DCHECK(node->current_frame_host()->GetRenderWidgetHost());
  return node->current_frame_host()
      ->GetRenderWidgetHost()
      ->render_frame_metadata_provider();
}

RenderFrameMetadataProvider* RenderFrameMetadataProviderFromWebContents(
    WebContents* web_contents) {
  DCHECK(web_contents);
  DCHECK(web_contents->GetRenderViewHost());
  DCHECK(
      RenderWidgetHostImpl::From(web_contents->GetRenderViewHost()->GetWidget())
          ->render_frame_metadata_provider());
  return RenderWidgetHostImpl::From(
             web_contents->GetRenderViewHost()->GetWidget())
      ->render_frame_metadata_provider();
}

}  // namespace

TitleWatcher::TitleWatcher(WebContents* web_contents,
                           const base::string16& expected_title)
    : WebContentsObserver(web_contents) {
  expected_titles_.push_back(expected_title);
}

void TitleWatcher::AlsoWaitForTitle(const base::string16& expected_title) {
  expected_titles_.push_back(expected_title);
}

TitleWatcher::~TitleWatcher() {
}

const base::string16& TitleWatcher::WaitAndGetTitle() {
  TestTitle();
  run_loop_.Run();
  return observed_title_;
}

void TitleWatcher::DidStopLoading() {
  // When navigating through the history, the restored NavigationEntry's title
  // will be used. If the entry ends up having the same title after we return
  // to it, as will usually be the case, then WebContentsObserver::TitleSet
  // will then be suppressed, since the NavigationEntry's title hasn't changed.
  TestTitle();
}

void TitleWatcher::TitleWasSet(NavigationEntry* entry) {
  TestTitle();
}

void TitleWatcher::TestTitle() {
  const base::string16& current_title = web_contents()->GetTitle();
  if (base::ContainsValue(expected_titles_, current_title)) {
    observed_title_ = current_title;
    run_loop_.Quit();
  }
}

RenderProcessHostWatcher::RenderProcessHostWatcher(
    RenderProcessHost* render_process_host,
    WatchType type)
    : render_process_host_(render_process_host),
      type_(type),
      did_exit_normally_(true),
      quit_closure_(run_loop_.QuitClosure()) {
  render_process_host_->AddObserver(this);
}

RenderProcessHostWatcher::RenderProcessHostWatcher(WebContents* web_contents,
                                                   WatchType type)
    : RenderProcessHostWatcher(web_contents->GetMainFrame()->GetProcess(),
                               type) {}

RenderProcessHostWatcher::~RenderProcessHostWatcher() {
  if (render_process_host_)
    render_process_host_->RemoveObserver(this);
}

void RenderProcessHostWatcher::Wait() {
  run_loop_.Run();
}

void RenderProcessHostWatcher::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  did_exit_normally_ =
      info.status == base::TERMINATION_STATUS_NORMAL_TERMINATION;
  if (type_ == WATCH_FOR_PROCESS_EXIT)
    std::move(quit_closure_).Run();
}

void RenderProcessHostWatcher::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  render_process_host_ = nullptr;
  if (type_ == WATCH_FOR_HOST_DESTRUCTION)
    std::move(quit_closure_).Run();
}

DOMMessageQueue::DOMMessageQueue() {
  registrar_.Add(this, NOTIFICATION_DOM_OPERATION_RESPONSE,
                 NotificationService::AllSources());
}

DOMMessageQueue::DOMMessageQueue(WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  registrar_.Add(this, NOTIFICATION_DOM_OPERATION_RESPONSE,
                 Source<WebContents>(web_contents));
}

DOMMessageQueue::~DOMMessageQueue() {}

void DOMMessageQueue::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  Details<std::string> dom_op_result(details);
  message_queue_.push(*dom_op_result.ptr());
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

void DOMMessageQueue::RenderProcessGone(base::TerminationStatus status) {
  VLOG(0) << "DOMMessageQueue::RenderProcessGone " << status;
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
      break;
    default:
      renderer_crashed_ = true;
      if (quit_closure_)
        std::move(quit_closure_).Run();
      break;
  }
}

void DOMMessageQueue::ClearQueue() {
  message_queue_ = base::queue<std::string>();
}

bool DOMMessageQueue::WaitForMessage(std::string* message) {
  DCHECK(message);
  if (!renderer_crashed_ && message_queue_.empty()) {
    // This will be quit when a new message comes in.
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  return PopMessage(message);
}

bool DOMMessageQueue::PopMessage(std::string* message) {
  DCHECK(message);
  if (renderer_crashed_ || message_queue_.empty())
    return false;
  *message = message_queue_.front();
  message_queue_.pop();
  return true;
}

class WebContentsAddedObserver::RenderViewCreatedObserver
    : public WebContentsObserver {
 public:
  explicit RenderViewCreatedObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        render_view_created_called_(false),
        main_frame_created_called_(false) {}

  // WebContentsObserver:
  void RenderViewCreated(RenderViewHost* rvh) override {
    render_view_created_called_ = true;
  }

  void RenderFrameCreated(RenderFrameHost* rfh) override {
    if (rfh == web_contents()->GetMainFrame())
      main_frame_created_called_ = true;
  }

  bool render_view_created_called_;
  bool main_frame_created_called_;
};

WebContentsAddedObserver::WebContentsAddedObserver()
    : web_contents_created_callback_(
          base::Bind(&WebContentsAddedObserver::WebContentsCreated,
                     base::Unretained(this))),
      web_contents_(nullptr) {
  WebContentsImpl::FriendWrapper::AddCreatedCallbackForTesting(
      web_contents_created_callback_);
}

WebContentsAddedObserver::~WebContentsAddedObserver() {
  WebContentsImpl::FriendWrapper::RemoveCreatedCallbackForTesting(
      web_contents_created_callback_);
}

void WebContentsAddedObserver::WebContentsCreated(WebContents* web_contents) {
  DCHECK(!web_contents_);
  web_contents_ = web_contents;
  child_observer_.reset(new RenderViewCreatedObserver(web_contents));

  if (quit_closure_)
    std::move(quit_closure_).Run();
}

WebContents* WebContentsAddedObserver::GetWebContents() {
  if (web_contents_)
    return web_contents_;

  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  return web_contents_;
}

bool WebContentsAddedObserver::RenderViewCreatedCalled() {
  if (child_observer_) {
    return child_observer_->render_view_created_called_ &&
           child_observer_->main_frame_created_called_;
  }
  return false;
}

WebContentsDestroyedObserver::WebContentsDestroyedObserver(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  DCHECK(web_contents);
}

WebContentsDestroyedObserver::~WebContentsDestroyedObserver() {}

void WebContentsDestroyedObserver::WebContentsDestroyed() {
  destroyed_ = true;
}

bool RequestFrame(WebContents* web_contents) {
  DCHECK(web_contents);
  return RenderWidgetHostImpl::From(
             web_contents->GetRenderViewHost()->GetWidget())
      ->RequestRepaintForTesting();
}

RenderFrameSubmissionObserver::RenderFrameSubmissionObserver(
    RenderFrameMetadataProvider* render_frame_metadata_provider)
    : render_frame_metadata_provider_(render_frame_metadata_provider) {
  render_frame_metadata_provider_->AddObserver(this);
  render_frame_metadata_provider_->ReportAllFrameSubmissionsForTesting(true);
}

RenderFrameSubmissionObserver::RenderFrameSubmissionObserver(
    FrameTreeNode* node)
    : RenderFrameSubmissionObserver(
          RenderFrameMetadataProviderFromFrameTreeNode(node)) {}

RenderFrameSubmissionObserver::RenderFrameSubmissionObserver(
    WebContents* web_contents)
    : RenderFrameSubmissionObserver(
          RenderFrameMetadataProviderFromWebContents(web_contents)) {}

RenderFrameSubmissionObserver::~RenderFrameSubmissionObserver() {
  render_frame_metadata_provider_->RemoveObserver(this);
  render_frame_metadata_provider_->ReportAllFrameSubmissionsForTesting(false);
}

void RenderFrameSubmissionObserver::WaitForAnyFrameSubmission() {
  break_on_any_frame_ = true;
  Wait();
  break_on_any_frame_ = false;
}

void RenderFrameSubmissionObserver::WaitForMetadataChange() {
  Wait();
}

void RenderFrameSubmissionObserver::WaitForScrollOffset(
    const gfx::Vector2dF& expected_offset) {
  while (render_frame_metadata_provider_->LastRenderFrameMetadata()
             .root_scroll_offset != expected_offset) {
    WaitForMetadataChange();
  }
}

void RenderFrameSubmissionObserver::WaitForScrollOffsetAtTop(
    bool expected_scroll_offset_at_top) {
  while (render_frame_metadata_provider_->LastRenderFrameMetadata()
             .is_scroll_offset_at_top != expected_scroll_offset_at_top) {
    WaitForMetadataChange();
  }
}

const cc::RenderFrameMetadata&
RenderFrameSubmissionObserver::LastRenderFrameMetadata() const {
  return render_frame_metadata_provider_->LastRenderFrameMetadata();
}

void RenderFrameSubmissionObserver::Quit() {
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

void RenderFrameSubmissionObserver::Wait() {
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void RenderFrameSubmissionObserver::
    OnRenderFrameMetadataChangedBeforeActivation(
        const cc::RenderFrameMetadata& metadata) {}

void RenderFrameSubmissionObserver::
    OnRenderFrameMetadataChangedAfterActivation() {
  Quit();
}

void RenderFrameSubmissionObserver::OnRenderFrameSubmission() {
  render_frame_count_++;
  if (break_on_any_frame_)
    Quit();
}

void RenderFrameSubmissionObserver::OnLocalSurfaceIdChanged(
    const cc::RenderFrameMetadata& metadata) {}

MainThreadFrameObserver::MainThreadFrameObserver(
    RenderWidgetHost* render_widget_host)
    : render_widget_host_(render_widget_host),
      routing_id_(render_widget_host_->GetProcess()->GetNextRoutingID()) {
  // TODO(lfg): We should look into adding a way to observe RenderWidgetHost
  // messages similarly to what WebContentsObserver can do with RFH and RVW.
  render_widget_host_->GetProcess()->AddRoute(routing_id_, this);
}

MainThreadFrameObserver::~MainThreadFrameObserver() {
  render_widget_host_->GetProcess()->RemoveRoute(routing_id_);
}

void MainThreadFrameObserver::Wait() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  render_widget_host_->Send(new WidgetMsg_WaitForNextFrameForTests(
      render_widget_host_->GetRoutingID(), routing_id_));
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MainThreadFrameObserver::Quit() {
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

bool MainThreadFrameObserver::OnMessageReceived(const IPC::Message& msg) {
  if (msg.type() == WidgetHostMsg_WaitForNextFrameForTests_ACK::ID &&
      msg.routing_id() == routing_id_) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&MainThreadFrameObserver::Quit, base::Unretained(this)));
  }
  return true;
}

InputMsgWatcher::InputMsgWatcher(RenderWidgetHost* render_widget_host,
                                 blink::WebInputEvent::Type type)
    : render_widget_host_(render_widget_host),
      wait_for_type_(type),
      ack_result_(INPUT_EVENT_ACK_STATE_UNKNOWN),
      ack_source_(InputEventAckSource::UNKNOWN) {
  render_widget_host->AddInputEventObserver(this);
}

InputMsgWatcher::~InputMsgWatcher() {
  render_widget_host_->RemoveInputEventObserver(this);
}

void InputMsgWatcher::OnInputEventAck(InputEventAckSource ack_source,
                                      InputEventAckState ack_state,
                                      const blink::WebInputEvent& event) {
  if (event.GetType() == wait_for_type_) {
    ack_result_ = ack_state;
    ack_source_ = ack_source;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }
}

bool InputMsgWatcher::HasReceivedAck() const {
  return ack_result_ != INPUT_EVENT_ACK_STATE_UNKNOWN;
}

InputEventAckState InputMsgWatcher::WaitForAck() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  return ack_result_;
}

InputEventAckState InputMsgWatcher::GetAckStateWaitIfNecessary() {
  if (HasReceivedAck())
    return ack_result_;
  return WaitForAck();
}

InputEventAckWaiter::InputEventAckWaiter(RenderWidgetHost* render_widget_host,
                                         InputEventAckPredicate predicate)
    : render_widget_host_(render_widget_host),
      predicate_(predicate),
      event_received_(false) {
  render_widget_host_->AddInputEventObserver(this);
}

namespace {
InputEventAckWaiter::InputEventAckPredicate EventAckHasType(
    blink::WebInputEvent::Type type) {
  return base::BindRepeating(
      [](blink::WebInputEvent::Type expected_type, InputEventAckSource source,
         InputEventAckState state, const blink::WebInputEvent& event) {
        return event.GetType() == expected_type;
      },
      type);
}
}  // namespace

InputEventAckWaiter::InputEventAckWaiter(RenderWidgetHost* render_widget_host,
                                         blink::WebInputEvent::Type type)
    : InputEventAckWaiter(render_widget_host, EventAckHasType(type)) {}

InputEventAckWaiter::~InputEventAckWaiter() {
  render_widget_host_->RemoveInputEventObserver(this);
}

void InputEventAckWaiter::Wait() {
  if (!event_received_) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

void InputEventAckWaiter::Reset() {
  event_received_ = false;
  quit_closure_ = base::OnceClosure();
}

void InputEventAckWaiter::OnInputEventAck(InputEventAckSource source,
                                          InputEventAckState state,
                                          const blink::WebInputEvent& event) {
  if (predicate_.Run(source, state, event)) {
    event_received_ = true;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }
}

// TODO(dcheng): Make the test clipboard on different threads share the
// same backing store. crbug.com/629765
// TODO(slangley): crbug.com/775830 - Cleanup BrowserTestClipboardScope now that
// there is no need to thread hop for Windows.
BrowserTestClipboardScope::BrowserTestClipboardScope() {
  ui::TestClipboard::CreateForCurrentThread();
}

BrowserTestClipboardScope::~BrowserTestClipboardScope() {
  ui::Clipboard::DestroyClipboardForCurrentThread();
}

void BrowserTestClipboardScope::SetRtf(const std::string& rtf) {
  ui::ScopedClipboardWriter clipboard_writer(ui::CLIPBOARD_TYPE_COPY_PASTE);
  clipboard_writer.WriteRTF(rtf);
}

void BrowserTestClipboardScope::SetText(const std::string& text) {
  ui::ScopedClipboardWriter clipboard_writer(ui::CLIPBOARD_TYPE_COPY_PASTE);
  clipboard_writer.WriteText(base::ASCIIToUTF16(text));
}

void BrowserTestClipboardScope::GetText(std::string* result) {
  ui::Clipboard::GetForCurrentThread()->ReadAsciiText(
      ui::CLIPBOARD_TYPE_COPY_PASTE, result);
}

class FrameFocusedObserver::FrameTreeNodeObserverImpl
    : public FrameTreeNode::Observer {
 public:
  explicit FrameTreeNodeObserverImpl(FrameTreeNode* owner) : owner_(owner) {
    owner->AddObserver(this);
  }
  ~FrameTreeNodeObserverImpl() override { owner_->RemoveObserver(this); }

  void Run() { run_loop_.Run(); }

  void OnFrameTreeNodeFocused(FrameTreeNode* node) override {
    if (node == owner_)
      run_loop_.Quit();
  }

 private:
  FrameTreeNode* owner_;
  base::RunLoop run_loop_;
};

FrameFocusedObserver::FrameFocusedObserver(RenderFrameHost* owner_host)
    : impl_(new FrameTreeNodeObserverImpl(
          static_cast<RenderFrameHostImpl*>(owner_host)->frame_tree_node())) {}

FrameFocusedObserver::~FrameFocusedObserver() {}

void FrameFocusedObserver::Wait() {
  impl_->Run();
}

class FrameDeletedObserver::FrameTreeNodeObserverImpl
    : public FrameTreeNode::Observer {
 public:
  explicit FrameTreeNodeObserverImpl(FrameTreeNode* owner) : owner_(owner) {
    owner->AddObserver(this);
  }
  ~FrameTreeNodeObserverImpl() override = default;

  void Run() { run_loop_.Run(); }

 private:
  // FrameTreeNode::Observer
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override {
    if (node == owner_)
      run_loop_.Quit();
  }

  FrameTreeNode* owner_;
  base::RunLoop run_loop_;
};

FrameDeletedObserver::FrameDeletedObserver(RenderFrameHost* owner_host)
    : impl_(new FrameTreeNodeObserverImpl(
          static_cast<RenderFrameHostImpl*>(owner_host)->frame_tree_node())) {}

FrameDeletedObserver::~FrameDeletedObserver() = default;

void FrameDeletedObserver::Wait() {
  impl_->Run();
}

TestNavigationManager::TestNavigationManager(WebContents* web_contents,
                                             const GURL& url)
    : WebContentsObserver(web_contents),
      url_(url),
      handle_(nullptr),
      navigation_paused_(false),
      current_state_(NavigationState::INITIAL),
      desired_state_(NavigationState::STARTED),
      weak_factory_(this) {}

TestNavigationManager::~TestNavigationManager() {
  if (navigation_paused_)
    handle_->CallResumeForTesting();
}

bool TestNavigationManager::WaitForRequestStart() {
  // This is the default desired state. In PlzNavigate, a browser-initiated
  // navigation can reach this state synchronously, so the TestNavigationManager
  // is set to always pause navigations at WillStartRequest. This ensures the
  // user can always call WaitForWillStartRequest.
  DCHECK(desired_state_ == NavigationState::STARTED);
  return WaitForDesiredState();
}

void TestNavigationManager::ResumeNavigation() {
  DCHECK(current_state_ == NavigationState::STARTED ||
         current_state_ == NavigationState::RESPONSE);
  DCHECK_EQ(current_state_, desired_state_);
  DCHECK(navigation_paused_);
  navigation_paused_ = false;
  handle_->CallResumeForTesting();
}

NavigationHandle* TestNavigationManager::GetNavigationHandle() {
  return handle_;
}

bool TestNavigationManager::WaitForResponse() {
  desired_state_ = NavigationState::RESPONSE;
  return WaitForDesiredState();
}

void TestNavigationManager::WaitForNavigationFinished() {
  desired_state_ = NavigationState::FINISHED;
  WaitForDesiredState();
}

void TestNavigationManager::DidStartNavigation(NavigationHandle* handle) {
  if (!ShouldMonitorNavigation(handle))
    return;

  handle_ = handle;
  std::unique_ptr<NavigationThrottle> throttle(
      new TestNavigationManagerThrottle(
          handle_, base::Bind(&TestNavigationManager::OnWillStartRequest,
                              weak_factory_.GetWeakPtr()),
          base::Bind(&TestNavigationManager::OnWillProcessResponse,
                     weak_factory_.GetWeakPtr())));
  handle_->RegisterThrottleForTesting(std::move(throttle));
}

void TestNavigationManager::DidFinishNavigation(NavigationHandle* handle) {
  if (handle != handle_)
    return;
  was_successful_ = handle->HasCommitted() && !handle->IsErrorPage();
  current_state_ = NavigationState::FINISHED;
  navigation_paused_ = false;
  handle_ = nullptr;
  OnNavigationStateChanged();
}

void TestNavigationManager::OnWillStartRequest() {
  current_state_ = NavigationState::STARTED;
  navigation_paused_ = true;
  OnNavigationStateChanged();
}

void TestNavigationManager::OnWillProcessResponse() {
  current_state_ = NavigationState::RESPONSE;
  navigation_paused_ = true;
  OnNavigationStateChanged();
}

// TODO(csharrison): Remove CallResumeForTesting method calls in favor of doing
// it through the throttle.
bool TestNavigationManager::WaitForDesiredState() {
  // If the desired state has laready been reached, just return.
  if (current_state_ == desired_state_)
    return true;

  // Resume the navigation if it was paused.
  if (navigation_paused_)
    handle_->CallResumeForTesting();

  // Wait for the desired state if needed.
  if (current_state_ < desired_state_) {
    DCHECK(!quit_closure_);
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Return false if the navigation did not reach the state specified by the
  // user.
  return current_state_ == desired_state_;
}

void TestNavigationManager::OnNavigationStateChanged() {
  // If the state the user was waiting for has been reached, exit the message
  // loop.
  if (current_state_ >= desired_state_) {
    if (quit_closure_)
      std::move(quit_closure_).Run();
    return;
  }

  // Otherwise, the navigation should be resumed if it was previously paused.
  if (navigation_paused_)
    handle_->CallResumeForTesting();
}

bool TestNavigationManager::ShouldMonitorNavigation(NavigationHandle* handle) {
  if (handle_ || handle->GetURL() != url_)
    return false;
  if (current_state_ != NavigationState::INITIAL)
    return false;
  return true;
}

NavigationHandleCommitObserver::NavigationHandleCommitObserver(
    content::WebContents* web_contents,
    const GURL& url)
    : content::WebContentsObserver(web_contents),
      url_(url),
      has_committed_(false),
      was_same_document_(false),
      was_renderer_initiated_(false) {}

void NavigationHandleCommitObserver::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (handle->GetURL() != url_)
    return;
  has_committed_ = true;
  was_same_document_ = handle->IsSameDocument();
  was_renderer_initiated_ = handle->IsRendererInitiated();
}

ConsoleObserverDelegate::ConsoleObserverDelegate(WebContents* web_contents,
                                                 const std::string& filter)
    : web_contents_(web_contents), filter_(filter) {}

ConsoleObserverDelegate::~ConsoleObserverDelegate() {}

void ConsoleObserverDelegate::Wait() {
  run_loop_.Run();
}

bool ConsoleObserverDelegate::DidAddMessageToConsole(
    WebContents* source,
    int32_t level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  DCHECK(source == web_contents_);

  std::string ascii_message = base::UTF16ToASCII(message);
  if (base::MatchPattern(ascii_message, filter_)) {
    message_ = ascii_message;
    run_loop_.Quit();
  }
  return false;
}

// static
void PwnMessageHelper::RegisterBlobURL(RenderProcessHost* process,
                                       GURL url,
                                       std::string uuid) {
  IPC::IpcSecurityTestUtil::PwnMessageReceived(
      process->GetChannel(), BlobHostMsg_RegisterPublicURL(url, uuid));
}

namespace {
blink::mojom::FileSystemManagerPtr GetFileSystemManager(
    RenderProcessHost* rph) {
  FileSystemManagerImpl* file_system = static_cast<RenderProcessHostImpl*>(rph)
                                           ->GetFileSystemManagerForTesting();
  blink::mojom::FileSystemManagerPtr file_system_manager_ptr;
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&FileSystemManagerImpl::BindRequest,
                     base::Unretained(file_system),
                     mojo::MakeRequest(&file_system_manager_ptr)));
  return file_system_manager_ptr;
}
}  // namespace

// static
void PwnMessageHelper::FileSystemCreate(RenderProcessHost* process,
                                        int request_id,
                                        GURL path,
                                        bool exclusive,
                                        bool is_directory,
                                        bool recursive) {
  TestFileapiOperationWaiter waiter;
  blink::mojom::FileSystemManagerPtr file_system_manager_ptr =
      GetFileSystemManager(process);
  file_system_manager_ptr->Create(
      path, exclusive, is_directory, recursive,
      base::BindOnce(&TestFileapiOperationWaiter::DidCreate,
                     base::Unretained(&waiter)));
  waiter.WaitForOperationToFinish();
}

// static
void PwnMessageHelper::FileSystemWrite(RenderProcessHost* process,
                                       int request_id,
                                       GURL file_path,
                                       std::string blob_uuid,
                                       int64_t position) {
  TestFileapiOperationWaiter waiter;
  blink::mojom::FileSystemManagerPtr file_system_manager_ptr =
      GetFileSystemManager(process);
  blink::mojom::FileSystemOperationListenerPtr listener_ptr;
  mojo::Binding<blink::mojom::FileSystemOperationListener> binding(
      &waiter, mojo::MakeRequest(&listener_ptr));
  blink::mojom::FileSystemCancellableOperationPtr op_ptr;

  file_system_manager_ptr->Write(file_path, blob_uuid, position,
                                 mojo::MakeRequest(&op_ptr),
                                 std::move(listener_ptr));
  waiter.WaitForOperationToFinish();
}

void PwnMessageHelper::LockMouse(RenderProcessHost* process,
                                 int routing_id,
                                 bool user_gesture,
                                 bool privileged) {
  IPC::IpcSecurityTestUtil::PwnMessageReceived(
      process->GetChannel(),
      WidgetHostMsg_LockMouse(routing_id, user_gesture, privileged));
}

#if defined(USE_AURA)
namespace {
class MockOverscrollControllerImpl : public OverscrollController,
                                     public MockOverscrollController {
 public:
  MockOverscrollControllerImpl() : content_scrolling_(false) {}
  ~MockOverscrollControllerImpl() override {}

  // OverscrollController:
  void ReceivedEventACK(const blink::WebInputEvent& event,
                        bool processed) override {
    // Since we're only mocking this one method of OverscrollController and its
    // other methods are non-virtual, we'll delegate to it so that it doesn't
    // get into an inconsistent state.
    OverscrollController::ReceivedEventACK(event, processed);

    if (event.GetType() == blink::WebInputEvent::kGestureScrollUpdate &&
        processed) {
      content_scrolling_ = true;
      if (quit_closure_)
        std::move(quit_closure_).Run();
    }
  }

  // MockOverscrollController:
  void WaitForConsumedScroll() override {
    if (!content_scrolling_) {
      base::RunLoop run_loop;
      quit_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

 private:
  bool content_scrolling_;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(MockOverscrollControllerImpl);
};
}  // namespace

// static
MockOverscrollController* MockOverscrollController::Create(
    RenderWidgetHostView* rwhv) {
  std::unique_ptr<MockOverscrollControllerImpl> mock =
      std::make_unique<MockOverscrollControllerImpl>();
  MockOverscrollController* raw_mock = mock.get();

  RenderWidgetHostViewAura* rwhva =
      static_cast<RenderWidgetHostViewAura*>(rwhv);
  rwhva->SetOverscrollControllerForTesting(std::move(mock));

  return raw_mock;
}

#endif  // defined(USE_AURA)

ContextMenuFilter::ContextMenuFilter()
    : content::BrowserMessageFilter(FrameMsgStart),
      run_loop_(new base::RunLoop),
      quit_closure_(run_loop_->QuitClosure()) {}

bool ContextMenuFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (message.type() == FrameHostMsg_ContextMenu::ID) {
    FrameHostMsg_ContextMenu::Param params;
    FrameHostMsg_ContextMenu::Read(&message, &params);
    content::ContextMenuParams menu_params = std::get<0>(params);
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ContextMenuFilter::OnContextMenu, this, menu_params));
  }
  return false;
}

void ContextMenuFilter::Wait() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  run_loop_->Run();
  run_loop_ = nullptr;
}

ContextMenuFilter::~ContextMenuFilter() {}

void ContextMenuFilter::OnContextMenu(
    const content::ContextMenuParams& params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  last_params_ = params;
  std::move(quit_closure_).Run();
}

WebContents* GetEmbedderForGuest(content::WebContents* guest) {
  CHECK(guest);
  return static_cast<content::WebContentsImpl*>(guest)->GetOuterWebContents();
}

bool IsNetworkServiceRunningInProcess() {
  return base::FeatureList::IsEnabled(network::features::kNetworkService) &&
         (base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kSingleProcess) ||
          base::FeatureList::IsEnabled(features::kNetworkServiceInProcess));
}

int LoadBasicRequest(network::mojom::NetworkContext* network_context,
                     const GURL& url,
                     int process_id,
                     int render_frame_id,
                     int load_flags) {
  network::mojom::URLLoaderFactoryPtr url_loader_factory;
  network::mojom::URLLoaderFactoryParamsPtr url_loader_factory_params =
      network::mojom::URLLoaderFactoryParams::New();
  url_loader_factory_params->process_id = process_id;
  url_loader_factory_params->is_corb_enabled = false;
  network_context->CreateURLLoaderFactory(MakeRequest(&url_loader_factory),
                                          std::move(url_loader_factory_params));
  // |url_loader_factory| will receive error notification asynchronously if
  // |network_context| has already encountered error. However it's still false
  // at this point.
  EXPECT_FALSE(url_loader_factory.encountered_error());

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->render_frame_id = render_frame_id;
  request->load_flags = load_flags;

  content::SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(), simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  return simple_loader->NetError();
}

void EnsureCookiesFlushed(BrowserContext* browser_context) {
  BrowserContext::ForEachStoragePartition(
      browser_context, base::BindRepeating([](StoragePartition* partition) {
        base::RunLoop run_loop;
        partition->GetCookieManagerForBrowserProcess()->FlushCookieStore(
            run_loop.QuitClosure());
        run_loop.Run();
      }));
}

bool HasValidProcessForProcessGroup(const std::string& process_group_name) {
  return ServiceManagerContext::HasValidProcessForProcessGroup(
      process_group_name);
}

bool TestChildOrGuestAutoresize(bool is_guest,
                                RenderProcessHost* embedder_rph,
                                RenderWidgetHost* guest_rwh) {
  RenderProcessHostImpl* embedder_rph_impl =
      static_cast<RenderProcessHostImpl*>(embedder_rph);
  RenderWidgetHostImpl* guest_rwh_impl =
      static_cast<RenderWidgetHostImpl*>(guest_rwh);

  scoped_refptr<SynchronizeVisualPropertiesMessageFilter> filter(
      new SynchronizeVisualPropertiesMessageFilter());

  // Register the message filter for the guest or child. For guest, we must use
  // a special hook, as there are already message filters installed which will
  // supercede us.
  if (is_guest) {
    embedder_rph_impl->SetBrowserPluginMessageFilterSubFilterForTesting(
        filter.get());
  } else {
    embedder_rph_impl->AddFilter(filter.get());
  }

  viz::LocalSurfaceId current_id =
      guest_rwh_impl->GetView()->GetLocalSurfaceId();
  // The guest may not yet be fully attached / initted. If not, |current_id|
  // will be invalid, and we should wait for an ID before proceeding.
  if (!current_id.is_valid())
    current_id = filter->WaitForSurfaceId();

  // Enable auto-resize.
  gfx::Size min_size(10, 10);
  gfx::Size max_size(100, 100);
  guest_rwh_impl->SetAutoResize(true, min_size, max_size);
  guest_rwh_impl->GetView()->EnableAutoResize(min_size, max_size);

  // Enabling auto resize generates a surface ID, wait for it.
  current_id = filter->WaitForSurfaceId();

  // Fake an auto-resize update.
  viz::LocalSurfaceId local_surface_id(current_id.parent_sequence_number(),
                                       current_id.child_sequence_number() + 1,
                                       current_id.embed_token());
  cc::RenderFrameMetadata metadata;
  metadata.viewport_size_in_pixels = gfx::Size(75, 75);
  metadata.local_surface_id = local_surface_id;
  guest_rwh_impl->DidUpdateVisualProperties(metadata);

  // This won't generate a response, as we short-circuit auto-resizes, so cause
  // an additional update by disabling auto-resize.
  guest_rwh_impl->GetView()->DisableAutoResize(gfx::Size(75, 75));

  // Get the first delivered surface id and ensure it has the surface id which
  // we expect.
  return filter->WaitForSurfaceId() ==
         viz::LocalSurfaceId(current_id.parent_sequence_number() + 1,
                             current_id.child_sequence_number() + 1,
                             current_id.embed_token());
}

const uint32_t
    SynchronizeVisualPropertiesMessageFilter::kMessageClassesToFilter[2] = {
        FrameMsgStart, BrowserPluginMsgStart};

SynchronizeVisualPropertiesMessageFilter::
    SynchronizeVisualPropertiesMessageFilter()
    : content::BrowserMessageFilter(kMessageClassesToFilter,
                                    arraysize(kMessageClassesToFilter)),
      screen_space_rect_run_loop_(std::make_unique<base::RunLoop>()),
      screen_space_rect_received_(false) {}

void SynchronizeVisualPropertiesMessageFilter::WaitForRect() {
  screen_space_rect_run_loop_->Run();
}

void SynchronizeVisualPropertiesMessageFilter::ResetRectRunLoop() {
  last_rect_ = gfx::Rect();
  screen_space_rect_run_loop_.reset(new base::RunLoop);
  screen_space_rect_received_ = false;
}

viz::FrameSinkId SynchronizeVisualPropertiesMessageFilter::GetOrWaitForId() {
  // No-op if already quit.
  frame_sink_id_run_loop_.Run();
  return frame_sink_id_;
}

viz::LocalSurfaceId
SynchronizeVisualPropertiesMessageFilter::WaitForSurfaceId() {
  surface_id_run_loop_.reset(new base::RunLoop);
  surface_id_run_loop_->Run();
  return last_surface_id_;
}

SynchronizeVisualPropertiesMessageFilter::
    ~SynchronizeVisualPropertiesMessageFilter() {}

void SynchronizeVisualPropertiesMessageFilter::
    OnSynchronizeFrameHostVisualProperties(
        const viz::SurfaceId& surface_id,
        const FrameVisualProperties& resize_params) {
  OnSynchronizeVisualProperties(surface_id.local_surface_id(),
                                surface_id.frame_sink_id(), resize_params);
}

void SynchronizeVisualPropertiesMessageFilter::
    OnSynchronizeBrowserPluginVisualProperties(
        int browser_plugin_guest_instance_id,
        viz::LocalSurfaceId surface_id,
        FrameVisualProperties resize_params) {
  OnSynchronizeVisualProperties(surface_id, viz::FrameSinkId(), resize_params);
}

void SynchronizeVisualPropertiesMessageFilter::OnSynchronizeVisualProperties(
    const viz::LocalSurfaceId& local_surface_id,
    const viz::FrameSinkId& frame_sink_id,
    const FrameVisualProperties& resize_params) {
  gfx::Rect screen_space_rect_in_dip = resize_params.screen_space_rect;
  if (IsUseZoomForDSFEnabled()) {
    screen_space_rect_in_dip =
        gfx::Rect(gfx::ScaleToFlooredPoint(
                      resize_params.screen_space_rect.origin(),
                      1.f / resize_params.screen_info.device_scale_factor),
                  gfx::ScaleToCeiledSize(
                      resize_params.screen_space_rect.size(),
                      1.f / resize_params.screen_info.device_scale_factor));
  }
  // Track each rect updates.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &SynchronizeVisualPropertiesMessageFilter::OnUpdatedFrameRectOnUI,
          this, screen_space_rect_in_dip));

  // Track each surface id update.
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &SynchronizeVisualPropertiesMessageFilter::OnUpdatedSurfaceIdOnUI,
          this, local_surface_id));

  // Record the received value. We cannot check the current state of the child
  // frame, as it can only be processed on the UI thread, and we cannot block
  // here.
  frame_sink_id_ = frame_sink_id;

  // There can be several updates before a valid viz::FrameSinkId is ready. Do
  // not quit |run_loop_| until after we receive a valid one.
  if (!frame_sink_id_.is_valid())
    return;

  // We can't nest on the IO thread. So tests will wait on the UI thread, so
  // post there to exit the nesting.
  base::CreateSingleThreadTaskRunnerWithTraits({content::BrowserThread::UI})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&SynchronizeVisualPropertiesMessageFilter::
                                    OnUpdatedFrameSinkIdOnUI,
                                this));
}

void SynchronizeVisualPropertiesMessageFilter::OnUpdatedFrameRectOnUI(
    const gfx::Rect& rect) {
  last_rect_ = rect;
  if (!screen_space_rect_received_) {
    screen_space_rect_received_ = true;
    // Tests looking at the rect currently expect all received input to finish
    // processing before the test continutes.
    screen_space_rect_run_loop_->QuitWhenIdle();
  }
}

void SynchronizeVisualPropertiesMessageFilter::OnUpdatedFrameSinkIdOnUI() {
  frame_sink_id_run_loop_.Quit();
}

void SynchronizeVisualPropertiesMessageFilter::OnUpdatedSurfaceIdOnUI(
    viz::LocalSurfaceId surface_id) {
  last_surface_id_ = surface_id;
  if (surface_id_run_loop_) {
    surface_id_run_loop_->QuitWhenIdle();
  }
}

bool SynchronizeVisualPropertiesMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(SynchronizeVisualPropertiesMessageFilter, message)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SynchronizeVisualProperties,
                        OnSynchronizeFrameHostVisualProperties)
    IPC_MESSAGE_HANDLER(BrowserPluginHostMsg_SynchronizeVisualProperties,
                        OnSynchronizeBrowserPluginVisualProperties)
  IPC_END_MESSAGE_MAP()

  // We do not consume the message, so that we can verify the effects of it
  // being processed.
  return false;
}

}  // namespace content
