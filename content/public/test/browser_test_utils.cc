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
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/process/kill.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/client/frame_evictor.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/file_system/file_system_manager_impl.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/input/synthetic_touchscreen_pinch_gesture.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_metadata_provider_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/screen_orientation/screen_orientation_provider.h"
#include "content/browser/service_manager/service_manager_context.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/histogram_fetcher.h"
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
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_fileapi_operation_waiter.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "ipc/ipc_security_test_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "net/base/filename_util.h"
#include "net/base/io_buffer.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_util.h"
#include "net/filter/gzip_header.h"
#include "net/filter/gzip_source_stream.h"
#include "net/filter/mock_source_stream.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/python_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "storage/browser/file_system/file_system_context.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/latency/latency_info.h"
#include "ui/resources/grit/webui_resources.h"

#if defined(OS_WIN)
#include <uiautomation.h>
#include <wrl/client.h>
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#endif

#if defined(USE_AURA)
#include "content/browser/renderer_host/delegated_frame_host.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "ui/aura/test/window_event_dispatcher_test_api.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#endif  // USE_AURA

namespace content {
namespace {

// Specifying a prototype so that we can add the WARN_UNUSED_RESULT attribute.
bool ExecuteScriptHelper(RenderFrameHost* render_frame_host,
                         const std::string& script,
                         bool user_gesture,
                         int32_t world_id,
                         std::unique_ptr<base::Value>* result)
    WARN_UNUSED_RESULT;

// Executes the passed |script| in the frame specified by |render_frame_host|.
// If |result| is not NULL, stores the value that the evaluation of the script
// in |result|.  Returns true on success.
bool ExecuteScriptHelper(RenderFrameHost* render_frame_host,
                         const std::string& script,
                         bool user_gesture,
                         int32_t world_id,
                         std::unique_ptr<base::Value>* result) {
  // TODO(lukasza): Only get messages from the specific |render_frame_host|.
  DOMMessageQueue dom_message_queue(render_frame_host);

  base::string16 script16 = base::UTF8ToUTF16(script);
  if (world_id == ISOLATED_WORLD_ID_GLOBAL && user_gesture) {
    render_frame_host->ExecuteJavaScriptWithUserGestureForTests(script16,
                                                                world_id);
  } else {
    // Note that |user_gesture| here is ignored when the world is not main. We
    // allow a value of |true| because it's the default, but in blink, the
    // execution will occur with no user gesture.
    render_frame_host->ExecuteJavaScriptForTests(script16, base::NullCallback(),
                                                 world_id);
  }

  std::string json;
  if (!dom_message_queue.WaitForMessage(&json)) {
    DLOG(ERROR) << "Cannot communicate with DOMMessageQueue.";
    return false;
  }

  // Nothing more to do for callers that ignore the returned JS value.
  if (!result)
    return true;

  base::JSONReader::ValueWithError parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(
          json, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!parsed_json.value) {
    *result = nullptr;
    DLOG(ERROR) << parsed_json.error_message;
    return false;
  }
  *result = base::Value::ToUniquePtrValue(std::move(*parsed_json.value));

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

  if (type == blink::WebInputEvent::Type::kChar ||
      type == blink::WebInputEvent::Type::kRawKeyDown) {
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
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::Type::kRawKeyDown,
                      ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT,
                      ui::VKEY_CONTROL, modifiers);
  }
  if (shift) {
    modifiers |= blink::WebInputEvent::kShiftKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::Type::kRawKeyDown,
                      ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT,
                      ui::VKEY_SHIFT, modifiers);
  }
  if (alt) {
    modifiers |= blink::WebInputEvent::kAltKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::Type::kRawKeyDown,
                      ui::DomKey::ALT, ui::DomCode::ALT_LEFT, ui::VKEY_MENU,
                      modifiers);
  }
  if (command) {
    modifiers |= blink::WebInputEvent::kMetaKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::Type::kRawKeyDown,
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
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::Type::kKeyUp,
                      ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT,
                      ui::VKEY_CONTROL, modifiers);
  }

  if (shift) {
    modifiers &= ~blink::WebInputEvent::kShiftKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::Type::kKeyUp,
                      ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT,
                      ui::VKEY_SHIFT, modifiers);
  }

  if (alt) {
    modifiers &= ~blink::WebInputEvent::kAltKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::Type::kKeyUp,
                      ui::DomKey::ALT, ui::DomCode::ALT_LEFT, ui::VKEY_MENU,
                      modifiers);
  }

  if (command) {
    modifiers &= ~blink::WebInputEvent::kMetaKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::Type::kKeyUp,
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
  InjectRawKeyEvent(web_contents, blink::WebInputEvent::Type::kRawKeyDown, key,
                    code, key_code, modifiers);
  if (send_char) {
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::Type::kChar, key,
                      code, key_code, modifiers);
  }
  InjectRawKeyEvent(web_contents, blink::WebInputEvent::Type::kKeyUp, key, code,
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

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
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
  TestNavigationManagerThrottle(
      NavigationHandle* handle,
      base::OnceClosure on_will_start_request_closure,
      base::OnceClosure on_will_process_response_closure)
      : NavigationThrottle(handle),
        on_will_start_request_closure_(
            std::move(on_will_start_request_closure)),
        on_will_process_response_closure_(
            std::move(on_will_process_response_closure)) {}
  ~TestNavigationManagerThrottle() override {}

  const char* GetNameForLogging() override {
    return "TestNavigationManagerThrottle";
  }

 private:
  // NavigationThrottle:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    DCHECK(on_will_start_request_closure_);
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, std::move(on_will_start_request_closure_));
    return NavigationThrottle::DEFER;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    DCHECK(on_will_process_response_closure_);
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, std::move(on_will_process_response_closure_));
    return NavigationThrottle::DEFER;
  }

  base::OnceClosure on_will_start_request_closure_;
  base::OnceClosure on_will_process_response_closure_;
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
  auto source_stream = std::make_unique<net::MockSourceStream>();
  source_stream->AddReadResult(encoded.front_as<char>(), encoded.size(),
                               net::OK, net::MockSourceStream::SYNC);
  // Add an EOF.
  source_stream->AddReadResult(encoded.front_as<char>() + encoded.size(), 0,
                               net::OK, net::MockSourceStream::SYNC);
  std::unique_ptr<net::GzipSourceStream> filter = net::GzipSourceStream::Create(
      std::move(source_stream), net::SourceStream::TYPE_GZIP);
  scoped_refptr<net::IOBufferWithSize> dest_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(4096);
  while (true) {
    int rv = filter->Read(dest_buffer.get(), dest_buffer->size(),
                          net::CompletionOnceCallback());
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
class CommitOriginInterceptor : public DidCommitNavigationInterceptor {
 public:
  CommitOriginInterceptor(WebContents* web_contents,
                          const GURL& target_url,
                          const GURL& new_url,
                          const url::Origin& new_origin)
      : DidCommitNavigationInterceptor(web_contents),
        target_url_(target_url),
        new_url_(new_url),
        new_origin_(new_origin) {}
  ~CommitOriginInterceptor() override = default;

  // WebContentsObserver:
  void WebContentsDestroyed() override { delete this; }

 protected:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
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
  return NavigateToURL(web_contents, url, url);
}

bool NavigateToURL(WebContents* web_contents,
                   const GURL& url,
                   const GURL& expected_commit_url) {
  NavigateToURLBlockUntilNavigationsComplete(web_contents, url, 1);
  if (!IsLastCommittedEntryOfPageType(web_contents, PAGE_TYPE_NORMAL))
    return false;

  bool is_same_url = web_contents->GetLastCommittedURL() == expected_commit_url;
  if (!is_same_url) {
    DLOG(WARNING) << "Expected URL " << expected_commit_url << " but observed "
                  << web_contents->GetLastCommittedURL();
  }
  return is_same_url;
}

bool NavigateIframeToURL(WebContents* web_contents,
                         const std::string& iframe_id,
                         const GURL& url) {
  TestNavigationObserver load_observer(web_contents);
  bool result = BeginNavigateIframeToURL(web_contents, iframe_id, url);
  load_observer.Wait();
  return result;
}

bool BeginNavigateIframeToURL(WebContents* web_contents,
                              const std::string& iframe_id,
                              const GURL& url) {
  std::string script = base::StringPrintf(
      "setTimeout(\""
      "var iframes = document.getElementById('%s');iframes.src='%s';"
      "\",0)",
      iframe_id.c_str(), url.spec().c_str());
  return ExecuteScript(web_contents, script);
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

void RequestMouseLock(RenderWidgetHost* host,
                      bool user_gesture,
                      bool privileged,
                      bool request_unadjusted_movement) {
  static_cast<RenderWidgetHostImpl*>(host)->RequestMouseLock(
      user_gesture, privileged, request_unadjusted_movement,
      /*response=*/base::DoNothing());
}

void RunUntilInputProcessed(RenderWidgetHost* host) {
  base::RunLoop run_loop;
  RenderWidgetHostImpl::From(host)->WaitForInputProcessed(
      run_loop.QuitClosure());
  run_loop.Run();
}

std::string ReferrerPolicyToString(
    network::mojom::ReferrerPolicy referrer_policy) {
  switch (referrer_policy) {
    case network::mojom::ReferrerPolicy::kDefault:
      return "no-meta";
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      return "no-referrer-when-downgrade";
    case network::mojom::ReferrerPolicy::kOrigin:
      return "origin";
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return "origin-when-crossorigin";
    case network::mojom::ReferrerPolicy::kSameOrigin:
      return "same-origin";
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      return "strict-origin";
    case network::mojom::ReferrerPolicy::kAlways:
      return "always";
    case network::mojom::ReferrerPolicy::kNever:
      return "never";
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
      return "strict-origin-when-cross-origin";
  }
  NOTREACHED();
  return "";
}

mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>
BindFakeFrameWidgetInterfaces(RenderFrameHost* frame) {
  RenderWidgetHostImpl* render_widget_host_impl =
      static_cast<RenderFrameHostImpl*>(frame)->GetRenderWidgetHost();

  mojo::AssociatedRemote<blink::mojom::FrameWidgetHost> blink_frame_widget_host;
  auto blink_frame_widget_host_receiver =
      blink_frame_widget_host.BindNewEndpointAndPassDedicatedReceiver();

  mojo::AssociatedRemote<blink::mojom::FrameWidget> blink_frame_widget;
  auto blink_frame_widget_receiver =
      blink_frame_widget.BindNewEndpointAndPassDedicatedReceiver();

  render_widget_host_impl->BindFrameWidgetInterfaces(
      std::move(blink_frame_widget_host_receiver), blink_frame_widget.Unbind());

  return blink_frame_widget_receiver;
}

void SimulateActiveStateForWidget(RenderFrameHost* frame, bool active) {
  static_cast<RenderFrameHostImpl*>(frame)->GetRenderWidgetHost()->SetActive(
      active);
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
  WebContentsDestroyedWatcher watcher(web_contents);
  WaitForLoadStopWithoutSuccessCheck(web_contents);
  if (watcher.IsDestroyed()) {
    LOG(ERROR) << "WebContents was destroyed during waiting for load stop.";
    return false;
  }
  bool is_page_normal =
      IsLastCommittedEntryOfPageType(web_contents, PAGE_TYPE_NORMAL);
  if (!is_page_normal) {
    NavigationEntry* last_entry =
        web_contents->GetController().GetLastCommittedEntry();
    if (last_entry) {
      LOG(ERROR) << "Http status code = " << last_entry->GetHttpStatusCode()
                 << ", page type = " << last_entry->GetPageType();
    } else {
      LOG(ERROR) << "No committed entry.";
    }
  }
  return is_page_normal;
}

void PrepContentsForBeforeUnloadTest(WebContents* web_contents,
                                     bool trigger_user_activation) {
  for (auto* frame : web_contents->GetAllFrames()) {
    if (trigger_user_activation)
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
  return last_entry && last_entry->GetPageType() == page_type;
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
  EXPECT_TRUE(rph->Shutdown(RESULT_CODE_KILLED));
  watcher.Wait();
  EXPECT_FALSE(watcher.did_exit_normally());
  EXPECT_TRUE(web_contents->IsCrashed());
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
        base::BindRepeating(IsResizeComplete, &dispatcher_test, widget_host));
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
        base::BindRepeating(IsResizeComplete, widget_host));
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
  auto* web_contents_impl = static_cast<WebContentsImpl*>(web_contents);
  auto* rwhvb = static_cast<RenderWidgetHostViewBase*>(
      web_contents->GetRenderWidgetHostView());
  blink::WebMouseEvent mouse_event(blink::WebInputEvent::Type::kMouseDown,
                                   modifiers, ui::EventTimeForNow());
  mouse_event.button = button;
  mouse_event.SetPositionInWidget(point.x(), point.y());
  // Mac needs positionInScreen for events to plugins.
  gfx::Rect offset = web_contents->GetContainerBounds();
  mouse_event.SetPositionInScreen(point.x() + offset.x(),
                                  point.y() + offset.y());
  mouse_event.click_count = 1;
  web_contents_impl->GetInputEventRouter()->RouteMouseEvent(rwhvb, &mouse_event,
                                                            ui::LatencyInfo());
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  web_contents_impl->GetInputEventRouter()->RouteMouseEvent(rwhvb, &mouse_event,
                                                            ui::LatencyInfo());
}

void SimulateMouseClickOrTapElementWithId(content::WebContents* web_contents,
                                          const std::string& id) {
  // Get the center coordinates of the DOM element.
  const int x = EvalJs(web_contents,
                       JsReplace("const bounds = "
                                 "document.getElementById($1)."
                                 "getBoundingClientRect();"
                                 "Math.floor(bounds.left + bounds.width / 2)",
                                 id))
                    .ExtractInt();
  const int y = EvalJs(web_contents,
                       JsReplace("const bounds = "
                                 "document.getElementById($1)."
                                 "getBoundingClientRect();"
                                 "Math.floor(bounds.top + bounds.height / 2)",
                                 id))
                    .ExtractInt();
#if defined(OS_ANDROID)
  SimulateTapDownAt(web_contents, gfx::Point(x, y));
  SimulateTapAt(web_contents, gfx::Point(x, y));
#else
  SimulateMouseClickAt(web_contents, 0, blink::WebMouseEvent::Button::kLeft,
                       gfx::Point(x, y));
#endif  // defined(OS_ANDROID)
}

void SendMouseDownToWidget(RenderWidgetHost* target,
                           int modifiers,
                           blink::WebMouseEvent::Button button) {
  auto* view = static_cast<RenderWidgetHostImpl*>(target)->GetView();

  blink::WebMouseEvent mouse_event(blink::WebInputEvent::Type::kMouseDown,
                                   modifiers, ui::EventTimeForNow());
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
  SimulateMouseEvent(web_contents, type,
                     blink::WebMouseEvent::Button::kNoButton, point);
}

void SimulateMouseEvent(WebContents* web_contents,
                        blink::WebInputEvent::Type type,
                        blink::WebMouseEvent::Button button,
                        const gfx::Point& point) {
  auto* web_contents_impl = static_cast<WebContentsImpl*>(web_contents);
  auto* rwhvb = static_cast<RenderWidgetHostViewBase*>(
      web_contents->GetRenderWidgetHostView());
  blink::WebMouseEvent mouse_event(type, 0, ui::EventTimeForNow());
  mouse_event.button = button;
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
  blink::WebMouseWheelEvent wheel_event(blink::WebInputEvent::Type::kMouseWheel,
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

#if !defined(OS_MAC)
void SimulateMouseWheelCtrlZoomEvent(WebContents* web_contents,
                                     const gfx::Point& point,
                                     bool zoom_in,
                                     blink::WebMouseWheelEvent::Phase phase) {
  blink::WebMouseWheelEvent wheel_event(blink::WebInputEvent::Type::kMouseWheel,
                                        blink::WebInputEvent::kControlKey,
                                        ui::EventTimeForNow());

  wheel_event.SetPositionInWidget(point.x(), point.y());
  wheel_event.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;
  wheel_event.delta_y =
      (zoom_in ? 1.0 : -1.0) * ui::MouseWheelEvent::kWheelDelta;
  wheel_event.wheel_ticks_y = (zoom_in ? 1.0 : -1.0);
  wheel_event.phase = phase;
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->ForwardWheelEvent(wheel_event);
}

void SimulateTouchscreenPinch(WebContents* web_contents,
                              const gfx::PointF& anchor,
                              float scale_change,
                              base::OnceClosure on_complete) {
  SyntheticPinchGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  params.scale_factor = scale_change;
  params.anchor = anchor;

  auto pinch_gesture =
      std::make_unique<SyntheticTouchscreenPinchGesture>(params);
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetTopLevelRenderWidgetHostView()->GetRenderWidgetHost());
  widget_host->QueueSyntheticGesture(
      std::move(pinch_gesture),
      base::BindOnce(
          [](base::OnceClosure on_complete, SyntheticGesture::Result result) {
            std::move(on_complete).Run();
          },
          std::move(on_complete)));
}

#endif  // !defined(OS_MAC)

void SimulateGesturePinchSequence(WebContents* web_contents,
                                  const gfx::Point& point,
                                  float scale,
                                  blink::WebGestureDevice source_device) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());

  blink::WebGestureEvent pinch_begin(
      blink::WebInputEvent::Type::kGesturePinchBegin,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(), source_device);
  pinch_begin.SetPositionInWidget(gfx::PointF(point));
  pinch_begin.SetPositionInScreen(gfx::PointF(point));
  pinch_begin.SetNeedsWheelEvent(source_device ==
                                 blink::WebGestureDevice::kTouchpad);
  widget_host->ForwardGestureEvent(pinch_begin);

  blink::WebGestureEvent pinch_update(pinch_begin);
  pinch_update.SetType(blink::WebInputEvent::Type::kGesturePinchUpdate);
  pinch_update.data.pinch_update.scale = scale;
  pinch_update.SetNeedsWheelEvent(source_device ==
                                  blink::WebGestureDevice::kTouchpad);
  widget_host->ForwardGestureEvent(pinch_update);

  blink::WebGestureEvent pinch_end(pinch_begin);
  pinch_end.SetType(blink::WebInputEvent::Type::kGesturePinchEnd);
  pinch_end.SetNeedsWheelEvent(source_device ==
                               blink::WebGestureDevice::kTouchpad);
  widget_host->ForwardGestureEvent(pinch_end);
}

void SimulateGestureScrollSequence(WebContents* web_contents,
                                   const gfx::Point& point,
                                   const gfx::Vector2dF& delta) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());

  blink::WebGestureEvent scroll_begin(
      blink::WebGestureEvent::Type::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::WebGestureDevice::kTouchpad);
  scroll_begin.SetPositionInWidget(gfx::PointF(point));
  scroll_begin.data.scroll_begin.delta_x_hint = delta.x();
  scroll_begin.data.scroll_begin.delta_y_hint = delta.y();
  widget_host->ForwardGestureEvent(scroll_begin);

  blink::WebGestureEvent scroll_update(
      blink::WebGestureEvent::Type::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::WebGestureDevice::kTouchpad);
  scroll_update.SetPositionInWidget(gfx::PointF(point));
  scroll_update.data.scroll_update.delta_x = delta.x();
  scroll_update.data.scroll_update.delta_y = delta.y();
  scroll_update.data.scroll_update.velocity_x = 0;
  scroll_update.data.scroll_update.velocity_y = 0;
  widget_host->ForwardGestureEvent(scroll_update);

  blink::WebGestureEvent scroll_end(
      blink::WebGestureEvent::Type::kGestureScrollEnd,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::WebGestureDevice::kTouchpad);
  scroll_end.SetPositionInWidget(gfx::PointF(point));
  widget_host->ForwardGestureEvent(scroll_end);
}

void SimulateGestureFlingSequence(WebContents* web_contents,
                                  const gfx::Point& point,
                                  const gfx::Vector2dF& velocity) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());

  blink::WebGestureEvent scroll_begin(
      blink::WebGestureEvent::Type::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::WebGestureDevice::kTouchpad);
  scroll_begin.SetPositionInWidget(gfx::PointF(point));
  widget_host->ForwardGestureEvent(scroll_begin);

  blink::WebGestureEvent scroll_end(
      blink::WebGestureEvent::Type::kGestureScrollEnd,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::WebGestureDevice::kTouchpad);
  scroll_end.SetPositionInWidget(gfx::PointF(point));
  widget_host->ForwardGestureEvent(scroll_end);

  blink::WebGestureEvent fling_start(
      blink::WebGestureEvent::Type::kGestureFlingStart,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::WebGestureDevice::kTouchpad);
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
                                 blink::WebGestureDevice::kTouchscreen);
  gesture.SetPositionInWidget(gfx::PointF(point));
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->ForwardGestureEvent(gesture);
}

void SimulateTapDownAt(WebContents* web_contents, const gfx::Point& point) {
  SimulateTouchGestureAt(web_contents, point,
                         blink::WebGestureEvent::Type::kGestureTapDown);
}

void SimulateTapAt(WebContents* web_contents, const gfx::Point& point) {
  SimulateTouchGestureAt(web_contents, point,
                         blink::WebGestureEvent::Type::kGestureTap);
}

void SimulateTapWithModifiersAt(WebContents* web_contents,
                                unsigned modifiers,
                                const gfx::Point& point) {
  blink::WebGestureEvent tap(blink::WebGestureEvent::Type::kGestureTap,
                             modifiers, ui::EventTimeForNow(),
                             blink::WebGestureDevice::kTouchpad);
  tap.SetPositionInWidget(gfx::PointF(point));
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->ForwardGestureEvent(tap);
}

#if defined(USE_AURA)
void SimulateTouchEventAt(WebContents* web_contents,
                          ui::EventType event_type,
                          const gfx::Point& point) {
  ui::TouchEvent touch(event_type, point, base::TimeTicks(),
                       ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView())
      ->OnTouchEvent(&touch);
}

void SimulateLongTapAt(WebContents* web_contents, const gfx::Point& point) {
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());

  ui::TouchEvent touch_start(
      ui::ET_TOUCH_PRESSED, point, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
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

  ui::TouchEvent touch_end(ui::ET_TOUCH_RELEASED, point, base::TimeTicks(),
                           ui::PointerDetails(ui::EventPointerType::kTouch, 0));
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
  EXPECT_TRUE(ExecuteScriptAndExtractString(
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
          source_line = base::StrCat(
              {source_line.substr(0, max_length - elision_mark.length()),
               elision_mark});
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

EvalJsResult EvalRunnerScript(const ToRenderFrameHost& execution_target,
                              const std::string& script,
                              int options,
                              int32_t world_id,
                              const std::string& token) {
  const char* kSourceURL = "__const_std::string&_script__";
  bool use_automatic_reply = !(options & EXECUTE_SCRIPT_USE_MANUAL_REPLY);
  bool user_gesture = !(options & EXECUTE_SCRIPT_NO_USER_GESTURE);
  std::ostringstream error_stream;
  std::unique_ptr<base::Value> response;
  if (!execution_target.render_frame_host()->IsRenderFrameLive()) {
    error_stream << "Error: EvalJs won't work on an already-crashed frame.";
  } else if (!ExecuteScriptHelper(execution_target.render_frame_host(), script,
                                  user_gesture, world_id, &response)) {
    error_stream << "Internal Error: ExecuteScriptHelper failed";
  } else if (!response) {
    error_stream << "Internal Error: no value";
  } else {
    bool is_reply_from_script = response->is_list() &&
                                response->GetList().size() == 2 &&
                                response->GetList()[0].is_string() &&
                                response->GetList()[0].GetString() == token;

    bool is_error = is_reply_from_script && response->GetList()[1].is_string();
    bool is_automatic_success_reply =
        is_reply_from_script && response->GetList()[1].is_list() &&
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

}  // namespace

testing::AssertionResult ExecJs(const ToRenderFrameHost& execution_target,
                                const std::string& script,
                                int options,
                                int32_t world_id) {
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
                    int32_t world_id) {
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

  return EvalRunnerScript(execution_target, runner_script, options, world_id,
                          token);
}

EvalJsResult EvalJsWithManualReply(const ToRenderFrameHost& execution_target,
                                   const std::string& script,
                                   int options,
                                   int32_t world_id) {
  return EvalJs(execution_target, script,
                options | EXECUTE_SCRIPT_USE_MANUAL_REPLY, world_id);
}

EvalJsResult EvalJsAfterLifecycleUpdate(
    const ToRenderFrameHost& execution_target,
    const std::string& raf_script,
    const std::string& script,
    int options,
    int32_t world_id) {
  bool use_automatic_reply = !(options & EXECUTE_SCRIPT_USE_MANUAL_REPLY);
  bool resolve_promises = !(options & EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);
  std::string token = "EvalJs-" + base::GenerateGUID();
  const char* kSourceURL = "__const_std::string&_script__";
  std::string modified_raf_script;
  if (raf_script.length()) {
    modified_raf_script = base::StringPrintf("%s;\n//# sourceURL=%s",
                                             raf_script.c_str(), kSourceURL);
  }
  std::string modified_script =
      base::StringPrintf("%s;\n//# sourceURL=%s", script.c_str(), kSourceURL);

  // This runner_script is very similar to that used by EvalJs, except that
  // this one delays running the argument script until just before
  // (|raf_script|) and after (|script|) a rendering update.
  std::string runner_script = JsReplace(
      R"(Promise.all([$1, $2])
         .then(scripts => new Promise((resolve, reject) => {
               requestAnimationFrame(() => {
                 window.eval(scripts[0]);
                 setTimeout(() => {
                   resolve([window.eval(scripts[1])])
                 }) }) }) )
         .then((result) => $3 ? Promise.all(result) : result )
         .then((result) => $4 ? result : Promise.reject(),
               (error) => 'a JavaScript error:' +
                          (error && error.stack ? '\n' + error.stack
                                                : ' "' + error + '"'))
         .then((reply) => window.domAutomationController.send([$5, reply]));
      //# sourceURL=EvalJs-runner.js)",
      modified_raf_script, modified_script, resolve_promises,
      use_automatic_reply, token);

  return EvalRunnerScript(execution_target, runner_script, options, world_id,
                          token);
}

namespace {
void AddToSetIfFrameMatchesPredicate(
    std::set<RenderFrameHost*>* frame_set,
    base::OnceCallback<bool(RenderFrameHost*)> predicate,
    RenderFrameHost* host) {
  if (std::move(predicate).Run(host))
    frame_set->insert(host);
}
}

RenderFrameHost* FrameMatchingPredicate(
    WebContents* web_contents,
    base::RepeatingCallback<bool(RenderFrameHost*)> predicate) {
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

  bool should_wait_flag = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kWaitForDebuggerWebUI);

  if (should_wait_flag) {
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

std::string GetCookies(BrowserContext* browser_context,
                       const GURL& url,
                       net::CookieOptions::SameSiteCookieContext context) {
  std::string cookies;
  base::RunLoop run_loop;
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  BrowserContext::GetDefaultStoragePartition(browser_context)
      ->GetNetworkContext()
      ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
  net::CookieOptions options;
  options.set_same_site_cookie_context(context);
  cookie_manager->GetCookieList(
      url, options,
      base::BindOnce(
          [](std::string* cookies_out, base::RunLoop* run_loop,
             const net::CookieAccessResultList& cookies,
             const net::CookieAccessResultList& excluded_cookies) {
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
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  BrowserContext::GetDefaultStoragePartition(browser_context)
      ->GetNetworkContext()
      ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
  // Allow access to SameSite cookies in tests.
  net::CookieOptions options;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  cookie_manager->GetCookieList(
      url, options,
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::vector<net::CanonicalCookie>* cookies_out,
             const net::CookieAccessResultList& cookies,
             const net::CookieAccessResultList& excluded_cookies) {
            *cookies_out = net::cookie_util::StripAccessResults(cookies);
            run_loop->Quit();
          },
          &run_loop, &cookies));
  run_loop.Run();
  return cookies;
}

bool SetCookie(BrowserContext* browser_context,
               const GURL& url,
               const std::string& value,
               net::CookieOptions::SameSiteCookieContext context) {
  bool result = false;
  base::RunLoop run_loop;
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  BrowserContext::GetDefaultStoragePartition(browser_context)
      ->GetNetworkContext()
      ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
  std::unique_ptr<net::CanonicalCookie> cc(net::CanonicalCookie::Create(
      url, value, base::Time::Now(), base::nullopt /* server_time */));
  DCHECK(cc.get());

  net::CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(context);
  cookie_manager->SetCanonicalCookie(
      *cc.get(), url, options,
      base::BindOnce(
          [](bool* result, base::RunLoop* run_loop,
             net::CookieAccessResult set_cookie_access_result) {
            *result = set_cookie_access_result.status.IsInclude();
            run_loop->Quit();
          },
          &result, &run_loop));
  run_loop.Run();
  return result;
}

void FetchHistogramsFromChildProcesses() {
  // Wait for all the renderer processes to be initialized before fetching
  // histograms for the first time.
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    if (!it.GetCurrentValue()->IsReady()) {
      RenderProcessHost* render_process_host = it.GetCurrentValue();
      RenderProcessHostWatcher ready_watcher(
          render_process_host,
          RenderProcessHostWatcher::WATCH_FOR_PROCESS_READY);
      ready_watcher.Wait();
    }
  }

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
  embedded_test_server->RegisterRequestHandler(base::BindRepeating(
      &CrossSiteRedirectResponseHandler, embedded_test_server));
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

void RemoveWebContentsReceiverSet(WebContents* web_contents,
                                  const std::string& interface_name) {
  static_cast<WebContentsImpl*>(web_contents)
      ->RemoveReceiverSetForTesting(interface_name);
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
  // If an image annotation is set, it plays the same role as a name, so it
  // makes sense to check both in the same test helper.
  if (node->GetStringAttribute(ax::mojom::StringAttribute::kName) == name ||
      node->GetStringAttribute(ax::mojom::StringAttribute::kImageAnnotation) ==
          name)
    return true;
  for (unsigned i = 0; i < node->PlatformChildCount(); i++) {
    if (AccessibilityTreeContainsNodeWithName(node->PlatformGetChild(i), name))
      return true;
  }
  return false;
}

void WaitForAccessibilityTreeToChange(WebContents* web_contents) {
  AccessibilityNotificationWaiter accessibility_waiter(
      web_contents, ui::AXMode(), ax::mojom::Event::kNone);
  accessibility_waiter.WaitForNotification();
}

void WaitForAccessibilityTreeToContainNodeWithName(WebContents* web_contents,
                                                   const std::string& name) {
  WebContentsImpl* web_contents_impl = static_cast<WebContentsImpl*>(
      web_contents);
  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      web_contents_impl->GetMainFrame());
  BrowserAccessibilityManager* main_frame_manager =
      main_frame->browser_accessibility_manager();
  while (!main_frame_manager || !AccessibilityTreeContainsNodeWithName(
             main_frame_manager->GetRoot(), name)) {
    WaitForAccessibilityTreeToChange(web_contents);
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

ui::AXPlatformNodeDelegate* GetRootAccessibilityNode(
    WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  BrowserAccessibilityManager* manager =
      web_contents_impl->GetRootBrowserAccessibilityManager();
  return manager ? manager->GetRoot() : nullptr;
}

FindAccessibilityNodeCriteria::FindAccessibilityNodeCriteria() = default;

FindAccessibilityNodeCriteria::~FindAccessibilityNodeCriteria() = default;

ui::AXPlatformNodeDelegate* FindAccessibilityNode(
    WebContents* web_contents,
    const FindAccessibilityNodeCriteria& criteria) {
  ui::AXPlatformNodeDelegate* root = GetRootAccessibilityNode(web_contents);
  CHECK(root);
  return FindAccessibilityNodeInSubtree(root, criteria);
}

ui::AXPlatformNodeDelegate* FindAccessibilityNodeInSubtree(
    ui::AXPlatformNodeDelegate* node,
    const FindAccessibilityNodeCriteria& criteria) {
  auto* node_internal = BrowserAccessibility::FromAXPlatformNodeDelegate(node);
  DCHECK(node_internal);
  if ((!criteria.name ||
       node_internal->GetStringAttribute(ax::mojom::StringAttribute::kName) ==
           criteria.name.value()) &&
      (!criteria.role || node_internal->GetRole() == criteria.role.value())) {
    return node;
  }

  for (unsigned int i = 0; i < node_internal->PlatformChildCount(); ++i) {
    BrowserAccessibility* child = node_internal->PlatformGetChild(i);
    ui::AXPlatformNodeDelegate* result =
        FindAccessibilityNodeInSubtree(child, criteria);
    if (result)
      return result;
  }
  return nullptr;
}

#if defined(OS_WIN)
template <typename T>
Microsoft::WRL::ComPtr<T> QueryInterfaceFromNode(
    ui::AXPlatformNodeDelegate* node) {
  Microsoft::WRL::ComPtr<T> result;
  EXPECT_HRESULT_SUCCEEDED(
      node->GetNativeViewAccessible()->QueryInterface(__uuidof(T), &result));
  return result;
}

void UiaGetPropertyValueVtArrayVtUnknownValidate(
    PROPERTYID property_id,
    ui::AXPlatformNodeDelegate* target_node,
    const std::vector<std::string>& expected_names) {
  ASSERT_TRUE(target_node);

  base::win::ScopedVariant result_variant;
  Microsoft::WRL::ComPtr<IRawElementProviderSimple> node_provider =
      QueryInterfaceFromNode<IRawElementProviderSimple>(target_node);

  node_provider->GetPropertyValue(property_id, result_variant.Receive());
  ASSERT_EQ(VT_ARRAY | VT_UNKNOWN, result_variant.type());
  ASSERT_EQ(1u, SafeArrayGetDim(V_ARRAY(result_variant.ptr())));

  LONG lower_bound, upper_bound, size;
  ASSERT_HRESULT_SUCCEEDED(
      SafeArrayGetLBound(V_ARRAY(result_variant.ptr()), 1, &lower_bound));
  ASSERT_HRESULT_SUCCEEDED(
      SafeArrayGetUBound(V_ARRAY(result_variant.ptr()), 1, &upper_bound));
  size = upper_bound - lower_bound + 1;
  ASSERT_EQ(static_cast<LONG>(expected_names.size()), size);

  std::vector<std::string> names;
  for (LONG i = 0; i < size; ++i) {
    Microsoft::WRL::ComPtr<IUnknown> unknown_element;
    ASSERT_HRESULT_SUCCEEDED(
        SafeArrayGetElement(V_ARRAY(result_variant.ptr()), &i,
                            static_cast<void**>(&unknown_element)));
    ASSERT_NE(nullptr, unknown_element);

    Microsoft::WRL::ComPtr<IRawElementProviderSimple>
        raw_element_provider_simple;
    ASSERT_HRESULT_SUCCEEDED(unknown_element.As(&raw_element_provider_simple));
    ASSERT_NE(nullptr, raw_element_provider_simple);

    base::win::ScopedVariant name;
    ASSERT_HRESULT_SUCCEEDED(raw_element_provider_simple->GetPropertyValue(
        UIA_NamePropertyId, name.Receive()));
    ASSERT_EQ(VT_BSTR, name.type());
    names.push_back(base::UTF16ToUTF8(
        base::string16(V_BSTR(name.ptr()), SysStringLen(V_BSTR(name.ptr())))));
  }

  ASSERT_THAT(names, testing::UnorderedElementsAreArray(expected_names));
}
#endif

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

bool IsRenderWidgetHostFocused(const RenderWidgetHost* host) {
  return static_cast<const RenderWidgetHostImpl*>(host)->is_focused();
}

WebContents* GetFocusedWebContents(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  return web_contents_impl->GetFocusedWebContents();
}

namespace {

RenderFrameMetadataProviderImpl* RenderFrameMetadataProviderFromFrameTreeNode(
    FrameTreeNode* node) {
  DCHECK(node);
  DCHECK(node->current_frame_host());
  DCHECK(node->current_frame_host()->GetRenderWidgetHost());
  return node->current_frame_host()
      ->GetRenderWidgetHost()
      ->render_frame_metadata_provider();
}

RenderFrameMetadataProviderImpl* RenderFrameMetadataProviderFromWebContents(
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

TitleWatcher::~TitleWatcher() = default;

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
  if (base::Contains(expected_titles_, current_title)) {
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
      allow_renderer_crashes_(
          std::make_unique<ScopedAllowRendererCrashes>(render_process_host)),
      quit_closure_(run_loop_.QuitClosure()) {
  render_process_host_->AddObserver(this);
}

RenderProcessHostWatcher::RenderProcessHostWatcher(WebContents* web_contents,
                                                   WatchType type)
    : RenderProcessHostWatcher(web_contents->GetMainFrame()->GetProcess(),
                               type) {}

RenderProcessHostWatcher::~RenderProcessHostWatcher() {
  ClearProcessHost();
}

void RenderProcessHostWatcher::ClearProcessHost() {
  // Although we would like to make it so that from here on, renderer crashes
  // cause test failures, resetting ScopedAllowRendererCrashes inside the RPH
  // observers is too soon. The current crash is notified to the testing
  // framework *after* the observers run and we need this one to be ignored.
  if (render_process_host_)
    render_process_host_->RemoveObserver(this);
  render_process_host_ = nullptr;
}

void RenderProcessHostWatcher::Wait() {
  run_loop_.Run();

  DCHECK(allow_renderer_crashes_)
      << "RenderProcessHostWatcher::Wait() may only be called once";
  allow_renderer_crashes_.reset();
  // Call this here just in case something else quits the RunLoop.
  ClearProcessHost();
}

void RenderProcessHostWatcher::QuitRunLoop() {
  std::move(quit_closure_).Run();
  ClearProcessHost();
}

void RenderProcessHostWatcher::RenderProcessReady(RenderProcessHost* host) {
  if (type_ == WATCH_FOR_PROCESS_READY)
    QuitRunLoop();
}

void RenderProcessHostWatcher::RenderProcessExited(
    RenderProcessHost* host,
    const ChildProcessTerminationInfo& info) {
  did_exit_normally_ =
      info.status == base::TERMINATION_STATUS_NORMAL_TERMINATION;
  if (type_ == WATCH_FOR_PROCESS_EXIT)
    QuitRunLoop();
}

void RenderProcessHostWatcher::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  render_process_host_ = nullptr;
  if (type_ == WATCH_FOR_HOST_DESTRUCTION)
    QuitRunLoop();
}

RenderProcessHostKillWaiter::RenderProcessHostKillWaiter(
    RenderProcessHost* render_process_host,
    const std::string& uma_name)
    : exit_watcher_(render_process_host,
                    RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT),
      uma_name_(uma_name) {}

base::Optional<int> RenderProcessHostKillWaiter::Wait() {
  base::Optional<bad_message::BadMessageReason> result;

  // Wait for the renderer kill.
  exit_watcher_.Wait();
#if !defined(OS_ANDROID)
  // Getting termination status on android is not reliable. To avoid flakiness,
  // we can skip this check and just check bad message. On other platforms we
  // want to verify that the renderer got killed, rather than exiting normally.
  if (exit_watcher_.did_exit_normally()) {
    LOG(ERROR) << "Renderer unexpectedly exited normally.";
    return result;
  }
#endif

  // Find the logged UMA data (if present).
  std::vector<base::Bucket> uma_samples =
      histogram_tester_.GetAllSamples(uma_name_);
  // No UMA will be present if the kill was not triggered by the //content layer
  // (e.g. if it was triggered by bad_message::ReceivedBadMessage from //chrome
  // layer or from somewhere in the //components layer).
  if (uma_samples.empty()) {
    LOG(ERROR) << "Unexpectedly found no '" << uma_name_ << "' samples.";
    return result;
  }
  const base::Bucket& bucket = uma_samples.back();
  // Assuming that user of RenderProcessHostKillWatcher makes sure that only one
  // kill can happen while using the class.
  DCHECK_EQ(1u, uma_samples.size())
      << "Multiple renderer kills are unsupported";

  return bucket.min;
}

RenderProcessHostBadMojoMessageWaiter::RenderProcessHostBadMojoMessageWaiter(
    RenderProcessHost* render_process_host)
    : monitored_render_process_id_(render_process_host->GetID()),
      kill_waiter_(render_process_host,
                   "Stability.BadMessageTerminated.Content") {
  // base::Unretained is safe below, because the destructor unregisters the
  // callback.
  RenderProcessHostImpl::SetBadMojoMessageCallbackForTesting(
      base::BindRepeating(
          &RenderProcessHostBadMojoMessageWaiter::OnBadMojoMessage,
          base::Unretained(this)));
}

RenderProcessHostBadMojoMessageWaiter::
    ~RenderProcessHostBadMojoMessageWaiter() {
  RenderProcessHostImpl::SetBadMojoMessageCallbackForTesting(
      RenderProcessHostImpl::BadMojoMessageCallbackForTesting());
}

base::Optional<std::string> RenderProcessHostBadMojoMessageWaiter::Wait() {
  base::Optional<int> bad_message_reason = kill_waiter_.Wait();
  if (!bad_message_reason.has_value())
    return base::nullopt;
  if (bad_message_reason.value() != bad_message::RPH_MOJO_PROCESS_ERROR) {
    LOG(ERROR) << "Unexpected |bad_message_reason|: "
               << bad_message_reason.value();
    return base::nullopt;
  }

  return observed_mojo_error_;
}

void RenderProcessHostBadMojoMessageWaiter::OnBadMojoMessage(
    int render_process_id,
    const std::string& error) {
  if (render_process_id == monitored_render_process_id_)
    observed_mojo_error_ = error;
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

DOMMessageQueue::DOMMessageQueue(RenderFrameHost* render_frame_host)
    : DOMMessageQueue(WebContents::FromRenderFrameHost(render_frame_host)) {
  render_frame_host_ = render_frame_host;
}

DOMMessageQueue::~DOMMessageQueue() = default;

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

void DOMMessageQueue::RenderFrameDeleted(RenderFrameHost* render_frame_host) {
  if (!render_frame_host_)
    return;
  if (render_frame_host_ != render_frame_host)
    return;
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

void DOMMessageQueue::ClearQueue() {
  message_queue_ = base::queue<std::string>();
}

bool DOMMessageQueue::WaitForMessage(std::string* message) {
  DCHECK(message);
  if (!renderer_crashed_ && message_queue_.empty()) {
    // This will be quit when a new message comes in.
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
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
          base::BindRepeating(&WebContentsAddedObserver::WebContentsCreated,
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
  child_observer_ = std::make_unique<RenderViewCreatedObserver>(web_contents);

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

bool RequestFrame(WebContents* web_contents) {
  DCHECK(web_contents);
  return RenderWidgetHostImpl::From(
             web_contents->GetRenderViewHost()->GetWidget())
      ->RequestRepaintForTesting();
}

RenderFrameSubmissionObserver::RenderFrameSubmissionObserver(
    RenderFrameMetadataProviderImpl* render_frame_metadata_provider)
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

void RenderFrameSubmissionObserver::WaitForPageScaleFactor(
    float expected_page_scale_factor,
    const float tolerance) {
  while (std::abs(render_frame_metadata_provider_->LastRenderFrameMetadata()
                      .page_scale_factor -
                  expected_page_scale_factor) > tolerance) {
    WaitForMetadataChange();
  }
}

void RenderFrameSubmissionObserver::WaitForExternalPageScaleFactor(
    float expected_external_page_scale_factor,
    const float tolerance) {
  while (std::abs(render_frame_metadata_provider_->LastRenderFrameMetadata()
                      .external_page_scale_factor -
                  expected_external_page_scale_factor) > tolerance) {
    WaitForMetadataChange();
  }
}

void RenderFrameSubmissionObserver::WaitForScrollOffset(
    const gfx::Vector2dF& expected_offset) {
  while (render_frame_metadata_provider_->LastRenderFrameMetadata()
             .root_scroll_offset != expected_offset) {
    const auto& offset =
        render_frame_metadata_provider_->LastRenderFrameMetadata()
            .root_scroll_offset;
    constexpr float kEpsilon = 0.01f;
    if (offset.has_value()) {
      const auto diff = expected_offset - *offset;
      if (std::abs(diff.x()) <= kEpsilon && std::abs(diff.y()) <= kEpsilon) {
        break;
      }
    }
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

void RenderFrameSubmissionObserver::NotifyOnNextMetadataChange(
    base::OnceClosure closure) {
  DCHECK(closure);
  DCHECK(metadata_change_closure_.is_null());
  metadata_change_closure_ = std::move(closure);
}

void RenderFrameSubmissionObserver::Quit() {
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

void RenderFrameSubmissionObserver::Wait() {
  base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void RenderFrameSubmissionObserver::
    OnRenderFrameMetadataChangedBeforeActivation(
        const cc::RenderFrameMetadata& metadata) {}

void RenderFrameSubmissionObserver::
    OnRenderFrameMetadataChangedAfterActivation() {
  Quit();
  if (metadata_change_closure_)
    std::move(metadata_change_closure_).Run();
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
      routing_id_(render_widget_host_->GetProcess()->GetNextRoutingID()) {}

MainThreadFrameObserver::~MainThreadFrameObserver() = default;

void MainThreadFrameObserver::Wait() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  static_cast<RenderWidgetHostImpl*>(render_widget_host_)
      ->InsertVisualStateCallback(base::BindOnce(&MainThreadFrameObserver::Quit,
                                                 base::Unretained(this)));
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
}

void MainThreadFrameObserver::Quit(bool) {
  if (quit_closure_)
    std::move(quit_closure_).Run();
}

InputMsgWatcher::InputMsgWatcher(RenderWidgetHost* render_widget_host,
                                 blink::WebInputEvent::Type type)
    : render_widget_host_(render_widget_host),
      wait_for_type_(type),
      ack_result_(blink::mojom::InputEventResultState::kUnknown),
      ack_source_(blink::mojom::InputEventResultSource::kUnknown) {
  render_widget_host->AddInputEventObserver(this);
}

InputMsgWatcher::~InputMsgWatcher() {
  render_widget_host_->RemoveInputEventObserver(this);
}

void InputMsgWatcher::OnInputEventAck(
    blink::mojom::InputEventResultSource ack_source,
    blink::mojom::InputEventResultState ack_state,
    const blink::WebInputEvent& event) {
  if (event.GetType() == wait_for_type_) {
    ack_result_ = ack_state;
    ack_source_ = ack_source;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }
}

bool InputMsgWatcher::HasReceivedAck() const {
  return ack_result_ != blink::mojom::InputEventResultState::kUnknown;
}

blink::mojom::InputEventResultState InputMsgWatcher::WaitForAck() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::RunLoop run_loop;
  quit_closure_ = run_loop.QuitClosure();
  run_loop.Run();
  return ack_result_;
}

blink::mojom::InputEventResultState
InputMsgWatcher::GetAckStateWaitIfNecessary() {
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
      [](blink::WebInputEvent::Type expected_type,
         blink::mojom::InputEventResultSource source,
         blink::mojom::InputEventResultState state,
         const blink::WebInputEvent& event) {
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

void InputEventAckWaiter::OnInputEventAck(
    blink::mojom::InputEventResultSource source,
    blink::mojom::InputEventResultState state,
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
  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteRTF(rtf);
}

void BrowserTestClipboardScope::SetText(const std::string& text) {
  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteText(base::ASCIIToUTF16(text));
}

void BrowserTestClipboardScope::GetText(std::string* result) {
  ui::Clipboard::GetForCurrentThread()->ReadAsciiText(
      ui::ClipboardBuffer::kCopyPaste, /* data_dst = */ nullptr, result);
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
    : impl_(std::make_unique<FrameTreeNodeObserverImpl>(
          static_cast<RenderFrameHostImpl*>(owner_host)->frame_tree_node())) {}

FrameFocusedObserver::~FrameFocusedObserver() = default;

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
    : impl_(std::make_unique<FrameTreeNodeObserverImpl>(
          static_cast<RenderFrameHostImpl*>(owner_host)->frame_tree_node())) {}

FrameDeletedObserver::~FrameDeletedObserver() = default;

void FrameDeletedObserver::Wait() {
  impl_->Run();
}

TestNavigationManager::TestNavigationManager(WebContents* web_contents,
                                             const GURL& url)
    : WebContentsObserver(web_contents),
      url_(url),
      request_(nullptr),
      navigation_paused_(false),
      current_state_(NavigationState::INITIAL),
      desired_state_(NavigationState::STARTED) {}

TestNavigationManager::~TestNavigationManager() {
  if (navigation_paused_)
    request_->GetNavigationThrottleRunnerForTesting()->CallResumeForTesting();
}

bool TestNavigationManager::WaitForRequestStart() {
  // This is the default desired state. A browser-initiated navigation can reach
  // this state synchronously, so the TestNavigationManager is set to always
  // pause navigations at WillStartRequest. This ensures the user can always
  // call WaitForWillStartRequest.
  DCHECK(desired_state_ == NavigationState::STARTED);
  return WaitForDesiredState();
}

void TestNavigationManager::ResumeNavigation() {
  DCHECK(current_state_ == NavigationState::STARTED ||
         current_state_ == NavigationState::RESPONSE);
  DCHECK_EQ(current_state_, desired_state_);
  DCHECK(navigation_paused_);
  navigation_paused_ = false;
  request_->GetNavigationThrottleRunnerForTesting()->CallResumeForTesting();
}

NavigationHandle* TestNavigationManager::GetNavigationHandle() {
  return request_;
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

  request_ = NavigationRequest::From(handle);
  auto throttle = std::make_unique<TestNavigationManagerThrottle>(
      request_,
      base::BindOnce(&TestNavigationManager::OnWillStartRequest,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce(&TestNavigationManager::OnWillProcessResponse,
                     weak_factory_.GetWeakPtr()));
  request_->RegisterThrottleForTesting(std::move(throttle));
}

void TestNavigationManager::DidFinishNavigation(NavigationHandle* handle) {
  if (handle != request_)
    return;
  was_committed_ = handle->HasCommitted();
  was_successful_ = was_committed_ && !handle->IsErrorPage();
  current_state_ = NavigationState::FINISHED;
  navigation_paused_ = false;
  request_ = nullptr;
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
    request_->GetNavigationThrottleRunnerForTesting()->CallResumeForTesting();

  // Wait for the desired state if needed.
  if (current_state_ < desired_state_) {
    DCHECK(!quit_closure_);
    base::RunLoop run_loop(message_loop_type_);
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
    request_->GetNavigationThrottleRunnerForTesting()->CallResumeForTesting();
}

bool TestNavigationManager::ShouldMonitorNavigation(NavigationHandle* handle) {
  if (request_ || handle->GetURL() != url_)
    return false;
  if (current_state_ != NavigationState::INITIAL)
    return false;
  return true;
}

void TestNavigationManager::AllowNestableTasks() {
  message_loop_type_ = base::RunLoop::Type::kNestableTasksAllowed;
}

NavigationHandleCommitObserver::NavigationHandleCommitObserver(
    content::WebContents* web_contents,
    const GURL& url)
    : WebContentsObserver(web_contents),
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

WebContentsConsoleObserver::WebContentsConsoleObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}
WebContentsConsoleObserver::~WebContentsConsoleObserver() = default;

void WebContentsConsoleObserver::Wait() {
  run_loop_.Run();
}

void WebContentsConsoleObserver::SetFilter(Filter filter) {
  filter_ = std::move(filter);
}

void WebContentsConsoleObserver::SetPattern(std::string pattern) {
  DCHECK(!pattern.empty()) << "An empty pattern will never match.";
  pattern_ = std::move(pattern);
}

std::string WebContentsConsoleObserver::GetMessageAt(size_t index) const {
  if (index >= messages_.size()) {
    ADD_FAILURE() << "Tried to retrieve a non-existent message at index: "
                  << index;
    return std::string();
  }
  return base::UTF16ToUTF8(messages_[index].message);
}

void WebContentsConsoleObserver::OnDidAddMessageToConsole(
    blink::mojom::ConsoleMessageLevel log_level,
    const base::string16& message_contents,
    int32_t line_no,
    const base::string16& source_id) {
  Message message({log_level, message_contents, line_no, source_id});
  if (filter_ && !filter_.Run(message))
    return;

  if (!pattern_.empty() &&
      !base::MatchPattern(base::UTF16ToUTF8(message_contents), pattern_)) {
    return;
  }

  messages_.push_back(std::move(message));
  run_loop_.Quit();
}

namespace {
mojo::Remote<blink::mojom::FileSystemManager> GetFileSystemManager(
    RenderProcessHost* rph) {
  FileSystemManagerImpl* file_system = static_cast<RenderProcessHostImpl*>(rph)
                                           ->GetFileSystemManagerForTesting();
  mojo::Remote<blink::mojom::FileSystemManager> file_system_manager_remote;
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemManagerImpl::BindReceiver,
                     base::Unretained(file_system),
                     file_system_manager_remote.BindNewPipeAndPassReceiver()));
  return file_system_manager_remote;
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
  mojo::Remote<blink::mojom::FileSystemManager> file_system_manager =
      GetFileSystemManager(process);
  file_system_manager->Create(
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
  mojo::Remote<blink::mojom::FileSystemManager> file_system_manager =
      GetFileSystemManager(process);
  mojo::PendingRemote<blink::mojom::FileSystemOperationListener> listener;
  mojo::Receiver<blink::mojom::FileSystemOperationListener> receiver(
      &waiter, listener.InitWithNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::FileSystemCancellableOperation> op;

  file_system_manager->Write(file_path, blob_uuid, position,
                             op.BindNewPipeAndPassReceiver(),
                             std::move(listener));
  waiter.WaitForOperationToFinish();
}

void PwnMessageHelper::OpenURL(RenderFrameHost* render_frame_host,
                               const GURL& url) {
  auto params = content::mojom::OpenURLParams::New();
  params->url = url;
  params->disposition = WindowOpenDisposition::CURRENT_TAB;
  params->should_replace_current_entry = false;
  params->user_gesture = true;
  static_cast<mojom::FrameHost*>(
      static_cast<RenderFrameHostImpl*>(render_frame_host))
      ->OpenURL(std::move(params));
}

#if defined(USE_AURA)
namespace {

// This class interacts with the internals of the DelegatedFrameHost without
// exposing them in the header.
class EvictionStateWaiter : public DelegatedFrameHost::Observer {
 public:
  explicit EvictionStateWaiter(DelegatedFrameHost* delegated_frame_host)
      : delegated_frame_host_(delegated_frame_host) {
    delegated_frame_host_->AddObserverForTesting(this);
  }

  ~EvictionStateWaiter() override {
    delegated_frame_host_->RemoveObserverForTesting(this);
  }

  void WaitForEvictionState(DelegatedFrameHost::FrameEvictionState state) {
    if (delegated_frame_host_->frame_eviction_state() == state)
      return;

    waited_eviction_state_ = state;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // DelegatedFrameHost::Observer:
  void OnFrameEvictionStateChanged(
      DelegatedFrameHost::FrameEvictionState new_state) override {
    if (!quit_closure_.is_null() && (new_state == waited_eviction_state_))
      std::move(quit_closure_).Run();
  }

 private:
  DelegatedFrameHost* delegated_frame_host_;
  DelegatedFrameHost::FrameEvictionState waited_eviction_state_;
  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(EvictionStateWaiter);
};

}  // namespace

void VerifyStaleContentOnFrameEviction(
    RenderWidgetHostView* render_widget_host_view) {
  auto* render_widget_host_view_aura =
      static_cast<RenderWidgetHostViewAura*>(render_widget_host_view);
  DelegatedFrameHost* delegated_frame_host =
      render_widget_host_view_aura->GetDelegatedFrameHost();

  // Initially there should be no stale content set.
  EXPECT_FALSE(
      delegated_frame_host->stale_content_layer()->has_external_content());
  EXPECT_EQ(delegated_frame_host->frame_eviction_state(),
            DelegatedFrameHost::FrameEvictionState::kNotStarted);

  // Hide the view and evict the frame, and expect that stale content will be
  // set.
  EvictionStateWaiter waiter{delegated_frame_host};
  render_widget_host_view_aura->WasOccluded();
  static_cast<viz::FrameEvictorClient*>(delegated_frame_host)
      ->EvictDelegatedFrame();
  EXPECT_EQ(delegated_frame_host->frame_eviction_state(),
            DelegatedFrameHost::FrameEvictionState::kPendingEvictionRequests);
  // Wait until the stale frame content is copied and set onto the layer, i.e.
  // the eviction state changes from kPendingEvictionRequests back to
  // kNotStarted.
  waiter.WaitForEvictionState(
      DelegatedFrameHost::FrameEvictionState::kNotStarted);
  EXPECT_TRUE(
      delegated_frame_host->stale_content_layer()->has_external_content());
}

#endif  // defined(USE_AURA)

ContextMenuFilter::ContextMenuFilter()
    : BrowserMessageFilter(FrameMsgStart),
      run_loop_(std::make_unique<base::RunLoop>()),
      quit_closure_(run_loop_->QuitClosure()) {}

bool ContextMenuFilter::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (message.type() == FrameHostMsg_ContextMenu::ID) {
    FrameHostMsg_ContextMenu::Param params;
    FrameHostMsg_ContextMenu::Read(&message, &params);
    UntrustworthyContextMenuParams menu_params = std::get<0>(params);
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&ContextMenuFilter::OnContextMenu, this, menu_params));
  }
  return false;
}

void ContextMenuFilter::Wait() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  run_loop_->Run();
  run_loop_ = nullptr;
}

ContextMenuFilter::~ContextMenuFilter() = default;

void ContextMenuFilter::OnContextMenu(
    const content::UntrustworthyContextMenuParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  last_params_ = params;
  std::move(quit_closure_).Run();
}

UpdateUserActivationStateInterceptor::UpdateUserActivationStateInterceptor() =
    default;

UpdateUserActivationStateInterceptor::~UpdateUserActivationStateInterceptor() =
    default;

void UpdateUserActivationStateInterceptor::Init(
    content::RenderFrameHost* render_frame_host) {
  render_frame_host_ = render_frame_host;
  impl_ = static_cast<RenderFrameHostImpl*>(render_frame_host_)
              ->local_frame_host_receiver_for_testing()
              .SwapImplForTesting(this);
}

void UpdateUserActivationStateInterceptor::set_quit_handler(
    base::OnceClosure handler) {
  quit_handler_ = std::move(handler);
}

blink::mojom::LocalFrameHost*
UpdateUserActivationStateInterceptor::GetForwardingInterface() {
  return impl_;
}

void UpdateUserActivationStateInterceptor::UpdateUserActivationState(
    blink::mojom::UserActivationUpdateType update_type,
    blink::mojom::UserActivationNotificationType notification_type) {
  update_user_activation_state_ = true;
  if (quit_handler_)
    std::move(quit_handler_).Run();
  GetForwardingInterface()->UpdateUserActivationState(update_type,
                                                      notification_type);
}

WebContents* GetEmbedderForGuest(content::WebContents* guest) {
  CHECK(guest);
  return static_cast<WebContentsImpl*>(guest)->GetOuterWebContents();
}

namespace {

int LoadBasicRequest(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& url,
    int load_flags,
    const base::Optional<url::Origin>& request_initiator = base::nullopt,
    int render_frame_id = MSG_ROUTING_NONE) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->load_flags = load_flags;
  request->request_initiator = request_initiator;
  request->render_frame_id = render_frame_id;
  // Allow access to SameSite cookies in tests.
  request->site_for_cookies = net::SiteForCookies::FromUrl(url);

  SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory, simple_loader_helper.GetCallback());
  simple_loader_helper.WaitForCallback();

  return simple_loader->NetError();
}

}  // namespace

int LoadBasicRequest(network::mojom::NetworkContext* network_context,
                     const GURL& url,
                     int load_flags) {
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory;
  network::mojom::URLLoaderFactoryParamsPtr url_loader_factory_params =
      network::mojom::URLLoaderFactoryParams::New();
  url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
  url_loader_factory_params->is_corb_enabled = false;
  url::Origin origin = url::Origin::Create(url);
  url_loader_factory_params->isolation_info =
      net::IsolationInfo::CreateForInternalRequest(origin);
  network_context->CreateURLLoaderFactory(
      url_loader_factory.BindNewPipeAndPassReceiver(),
      std::move(url_loader_factory_params));
  // |url_loader_factory| will receive disconnect notification asynchronously if
  // |network_context| is already disconnected. However it's still false
  // at this point.
  EXPECT_TRUE(url_loader_factory.is_connected());

  return LoadBasicRequest(url_loader_factory.get(), url, load_flags);
}

int LoadBasicRequest(RenderFrameHost* frame, const GURL& url) {
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory;
  frame->CreateNetworkServiceDefaultFactory(
      url_loader_factory.BindNewPipeAndPassReceiver());
  return LoadBasicRequest(
      url_loader_factory.get(), url, 0 /* load_flags */,
      frame->GetLastCommittedOrigin() /* request_initiator */,
      frame->GetRoutingID());
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

bool TestGuestAutoresize(RenderProcessHost* embedder_rph,
                         RenderWidgetHost* guest_rwh) {
  RenderProcessHostImpl* embedder_rph_impl =
      static_cast<RenderProcessHostImpl*>(embedder_rph);
  RenderWidgetHostImpl* guest_rwh_impl =
      static_cast<RenderWidgetHostImpl*>(guest_rwh);

  auto filter =
      base::MakeRefCounted<SynchronizeVisualPropertiesMessageFilter>();

  embedder_rph_impl->AddFilter(filter.get());

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
  guest_rwh_impl->OnLocalSurfaceIdChanged(metadata);

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

SynchronizeVisualPropertiesMessageFilter::
    SynchronizeVisualPropertiesMessageFilter()
    : BrowserMessageFilter(FrameMsgStart),
      screen_space_rect_run_loop_(std::make_unique<base::RunLoop>()),
      screen_space_rect_received_(false),
      pinch_gesture_active_set_(false),
      pinch_gesture_active_cleared_(false),
      last_pinch_gesture_active_(false) {}

void SynchronizeVisualPropertiesMessageFilter::WaitForRect() {
  screen_space_rect_run_loop_->Run();
}

void SynchronizeVisualPropertiesMessageFilter::ResetRectRunLoop() {
  last_rect_ = gfx::Rect();
  screen_space_rect_run_loop_ = std::make_unique<base::RunLoop>();
  screen_space_rect_received_ = false;
}

viz::LocalSurfaceId
SynchronizeVisualPropertiesMessageFilter::WaitForSurfaceId() {
  surface_id_run_loop_ = std::make_unique<base::RunLoop>();
  surface_id_run_loop_->Run();
  return last_surface_id_;
}

SynchronizeVisualPropertiesMessageFilter::
    ~SynchronizeVisualPropertiesMessageFilter() {}

void SynchronizeVisualPropertiesMessageFilter::
    OnSynchronizeFrameHostVisualProperties(
        const blink::FrameVisualProperties& visual_properties) {
  OnSynchronizeVisualProperties(visual_properties);
}

void SynchronizeVisualPropertiesMessageFilter::OnSynchronizeVisualProperties(
    const blink::FrameVisualProperties& visual_properties) {
  // Monitor |is_pinch_gesture_active| to determine when pinch gestures begin
  // and end.
  if (visual_properties.is_pinch_gesture_active &&
      !last_pinch_gesture_active_) {
    pinch_gesture_active_set_ = true;
  }
  if (!visual_properties.is_pinch_gesture_active &&
      last_pinch_gesture_active_) {
    pinch_gesture_active_cleared_ = true;
    if (pinch_end_run_loop_)
      pinch_end_run_loop_->Quit();
  }
  last_pinch_gesture_active_ = visual_properties.is_pinch_gesture_active;

  gfx::Rect screen_space_rect_in_dip = visual_properties.screen_space_rect;
  if (IsUseZoomForDSFEnabled()) {
    screen_space_rect_in_dip =
        gfx::Rect(gfx::ScaleToFlooredPoint(
                      visual_properties.screen_space_rect.origin(),
                      1.f / visual_properties.screen_info.device_scale_factor),
                  gfx::ScaleToCeiledSize(
                      visual_properties.screen_space_rect.size(),
                      1.f / visual_properties.screen_info.device_scale_factor));
  }
  // Track each rect updates.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SynchronizeVisualPropertiesMessageFilter::OnUpdatedFrameRectOnUI,
          this, screen_space_rect_in_dip));

  // Track each surface id update.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SynchronizeVisualPropertiesMessageFilter::OnUpdatedSurfaceIdOnUI,
          this, visual_properties.local_surface_id));

  // We can't nest on the IO thread. So tests will wait on the UI thread, so
  // post there to exit the nesting.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SynchronizeVisualPropertiesMessageFilter::OnUpdatedFrameSinkIdOnUI,
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
  run_loop_.Quit();
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
  IPC_END_MESSAGE_MAP()

  // We do not consume the message, so that we can verify the effects of it
  // being processed.
  return false;
}

void SynchronizeVisualPropertiesMessageFilter::WaitForPinchGestureEnd() {
  if (pinch_gesture_active_cleared_)
    return;
  DCHECK(!pinch_end_run_loop_);
  pinch_end_run_loop_ = std::make_unique<base::RunLoop>();
  pinch_end_run_loop_->Run();
}

RenderWidgetHostMouseEventMonitor::RenderWidgetHostMouseEventMonitor(
    RenderWidgetHost* host)
    : host_(host), event_received_(false) {
  mouse_callback_ = base::BindRepeating(
      &RenderWidgetHostMouseEventMonitor::MouseEventCallback,
      base::Unretained(this));
  host_->AddMouseEventCallback(mouse_callback_);
}

RenderWidgetHostMouseEventMonitor::~RenderWidgetHostMouseEventMonitor() {
  host_->RemoveMouseEventCallback(mouse_callback_);
}

DidStartNavigationObserver::DidStartNavigationObserver(WebContents* contents)
    : WebContentsObserver(contents) {}
DidStartNavigationObserver::~DidStartNavigationObserver() = default;

void DidStartNavigationObserver::DidStartNavigation(NavigationHandle* handle) {
  if (observed_)
    return;
  observed_ = true;
  navigation_handle_ = handle;
  run_loop_.Quit();
}

void DidStartNavigationObserver::DidFinishNavigation(NavigationHandle* handle) {
  if (navigation_handle_ == handle)
    navigation_handle_ = nullptr;
}

ProxyDSFObserver::ProxyDSFObserver() {
  RenderFrameProxyHost::SetCreatedCallbackForTesting(base::BindRepeating(
      &ProxyDSFObserver::OnCreation, base::Unretained(this)));
}

ProxyDSFObserver::~ProxyDSFObserver() {
  RenderFrameProxyHost::SetCreatedCallbackForTesting(
      RenderFrameProxyHost::CreatedCallback());
}

void ProxyDSFObserver::WaitForOneProxyHostCreation() {
  if (!proxy_host_created_dsf_.empty())
    return;
  runner_ = std::make_unique<base::RunLoop>();
  runner_->Run();
}

void ProxyDSFObserver::OnCreation(RenderFrameProxyHost* rfph) {
  // Not all RenderFrameProxyHosts will be created with a
  // CrossProcessFrameConnector. We're only interested in the ones that do.
  if (auto* cpfc = rfph->cross_process_frame_connector()) {
    proxy_host_created_dsf_.push_back(cpfc->screen_info().device_scale_factor);
  }
  if (runner_)
    runner_->Quit();
}

bool CompareWebContentsOutputToReference(
    WebContents* web_contents,
    const base::FilePath& expected_path,
    const gfx::Size& snapshot_size,
    const cc::PixelComparator& comparator) {
  // Produce a frame of output first to ensure the system is in a consistent,
  // known state.
  {
    base::RunLoop run_loop;
    web_contents->GetMainFrame()->InsertVisualStateCallback(
        base::BindLambdaForTesting([&](bool visual_state_updated) {
          ASSERT_TRUE(visual_state_updated);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  auto* rwh = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());

  if (!rwh->GetView() || !rwh->GetView()->IsSurfaceAvailableForCopy()) {
    ADD_FAILURE() << "RWHV surface not available for copy.";
    return false;
  }

  bool snapshot_matches = false;
  {
    base::RunLoop run_loop;
    rwh->GetView()->CopyFromSurface(
        gfx::Rect(), gfx::Size(),
        base::BindLambdaForTesting([&](const SkBitmap& bitmap) {
          base::ScopedAllowBlockingForTesting allow_blocking;
          ASSERT_FALSE(bitmap.drawsNothing());

          SkBitmap clipped_bitmap;
          bitmap.extractSubset(
              &clipped_bitmap,
              SkIRect::MakeWH(snapshot_size.width(), snapshot_size.height()));

          snapshot_matches =
              cc::MatchesPNGFile(clipped_bitmap, expected_path, comparator);

          // When rebaselining the pixel test, the test may fail. However, the
          // reference file will still be overwritten.
          if (base::CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kRebaselinePixelTests)) {
            ASSERT_TRUE(cc::WritePNGFile(clipped_bitmap, expected_path,
                                         /*discard_transparency=*/false));
          }

          run_loop.Quit();
        }));
    run_loop.Run();
  }

  return snapshot_matches;
}

RenderFrameHostChangedCallbackRunner::RenderFrameHostChangedCallbackRunner(
    WebContents* content,
    RenderFrameHostChangedCallback callback)
    : WebContentsObserver(content), callback_(std::move(callback)) {}

RenderFrameHostChangedCallbackRunner::~RenderFrameHostChangedCallbackRunner() =
    default;

void RenderFrameHostChangedCallbackRunner::RenderFrameHostChanged(
    RenderFrameHost* old_host,
    RenderFrameHost* new_host) {
  if (callback_)
    std::move(callback_).Run(old_host, new_host);
}

}  // namespace content
