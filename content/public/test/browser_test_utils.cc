// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/public/test/browser_test_utils.h"

#include <stddef.h>

#include <cstdint>
#include <set>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/process/kill.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/optional_util.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/test/pixel_test_utils.h"
#include "components/input/render_widget_host_input_event_router.h"
#include "components/viz/client/frame_evictor.h"
#include "content/browser/file_system/file_system_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_frame_metadata_provider_impl.h"
#include "content/browser/renderer_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/screen_orientation/screen_orientation_provider.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/frame.mojom.h"
#include "content/common/input/synthetic_touchscreen_pinch_gesture.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/synchronize_visual_properties_interceptor.h"
#include "content/public/test/test_fileapi_operation_waiter.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "content/test/mock_commit_deferring_condition.h"
#include "ipc/ipc_security_test_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "net/base/filename_util.h"
#include "net/base/io_buffer.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_access_result.h"
#include "net/cookies/cookie_constants.h"
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
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "storage/browser/blob/blob_url_registry.h"
#include "storage/browser/file_system/file_system_context.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"
#include "third_party/blink/public/common/frame/frame_visual_properties.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-test-utils.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom.h"
#include "third_party/blink/public/mojom/keyboard_lock/keyboard_lock.mojom-shared.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/latency/latency_info.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/browser/media/captured_surface_controller.h"
#include "content/public/test/mock_captured_surface_controller.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/webui/grit/ash_webui_common_resources.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <combaseapi.h>
#include <wrl/client.h>

#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"

#include <uiautomation.h>
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

void BuildSimpleWebKeyEvent(blink::WebInputEvent::Type type,
                            ui::DomKey key,
                            ui::DomCode code,
                            ui::KeyboardCode key_code,
                            input::NativeWebKeyboardEvent* event) {
  event->dom_key = key;
  event->dom_code = static_cast<int>(code);
  event->native_key_code = ui::KeycodeConverter::DomCodeToNativeKeycode(code);
  event->windows_key_code = key_code;
  event->is_system_key = false;
  event->skip_if_unhandled = true;

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
  input::NativeWebKeyboardEvent event(type, modifiers, base::TimeTicks::Now());
  BuildSimpleWebKeyEvent(type, key, code, key_code, &event);
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  RenderWidgetHostImpl* main_frame_rwh =
      web_contents_impl->GetPrimaryMainFrame()->GetRenderWidgetHost();
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
    return nullptr;
  }
  std::string params = request.relative_url.substr(length_of_chosen_prefix);

  // A hostname to redirect to must be included in the URL, therefore at least
  // one '/' character is expected.
  size_t slash = params.find('/');
  if (slash == std::string::npos)
    return nullptr;

  // Replace the host of the URL with the one passed in the URL.
  GURL::Replacements replace_host;
  replace_host.SetHostStr(std::string_view(params).substr(0, slash));
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
      base::RepeatingClosure on_will_redirect_request_closure,
      base::OnceClosure on_will_process_response_closure)
      : NavigationThrottle(handle),
        on_will_start_request_closure_(
            std::move(on_will_start_request_closure)),
        on_will_redirect_request_closure_(
            std::move(on_will_redirect_request_closure)),
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

  NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override {
    CHECK(on_will_redirect_request_closure_);
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                        on_will_redirect_request_closure_);
    return NavigationThrottle::DEFER;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    DCHECK(on_will_process_response_closure_);
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, std::move(on_will_process_response_closure_));
    return NavigationThrottle::DEFER;
  }

  base::OnceClosure on_will_start_request_closure_;
  base::RepeatingClosure on_will_redirect_request_closure_;
  base::OnceClosure on_will_process_response_closure_;
};

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool HasGzipHeader(const base::RefCountedMemory& maybe_gzipped) {
  net::GZipHeader header;
  net::GZipHeader::Status header_status = net::GZipHeader::INCOMPLETE_HEADER;
  const char* header_end = nullptr;
  while (header_status == net::GZipHeader::INCOMPLETE_HEADER) {
    auto chars = base::as_chars(base::span(maybe_gzipped));
    header_status = header.ReadMore(chars.data(), chars.size(), &header_end);
  }
  return header_status == net::GZipHeader::COMPLETE_HEADER;
}

void AppendGzippedResource(const base::RefCountedMemory& encoded,
                           std::string* to_append) {
  auto source_stream = std::make_unique<net::MockSourceStream>();
  auto encoded_chars = base::as_chars(base::span(encoded));
  source_stream->AddReadResult(encoded_chars.data(), encoded_chars.size(),
                               net::OK, net::MockSourceStream::SYNC);
  // Add an EOF.
  auto end = encoded_chars.last(0u);
  source_stream->AddReadResult(end.data(), end.size(), net::OK,
                               net::MockSourceStream::SYNC);
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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
      return navigator.mediaDevices.enumerateDevices()
      .then(function(devices) {
        if (devices.some((device) => device.kind == 'videoinput')) {
          return 'has-video-input-device';
        }
        return 'no-video-input-devices';
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

  CommitOriginInterceptor(const CommitOriginInterceptor&) = delete;
  CommitOriginInterceptor& operator=(const CommitOriginInterceptor&) = delete;

  ~CommitOriginInterceptor() override = default;

  // WebContentsObserver:
  void WebContentsDestroyed() override { delete this; }

 protected:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    if ((**params).url == target_url_) {
      (**params).url = new_url_;
      (**params).origin = new_origin_;
    }
    return true;
  }

 private:
  GURL target_url_;
  GURL new_url_;
  url::Origin new_origin_;
};

// Observer which waits for a visual update in a RenderWidgetHost to meet some
// desired conditions.
class ResizeObserver : public RenderWidgetHostObserver {
 public:
  ResizeObserver(RenderWidgetHost* widget_host,
                 base::RepeatingCallback<bool()> is_complete_callback)
      : widget_host_(widget_host),
        is_complete_callback_(std::move(is_complete_callback)) {
    widget_host_->AddObserver(this);
  }

  ~ResizeObserver() override { widget_host_->RemoveObserver(this); }

  // RenderWidgetHostObserver:
  void RenderWidgetHostDidUpdateVisualProperties(
      RenderWidgetHost* widget_host) override {
    if (is_complete_callback_.Run())
      run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

 private:
  raw_ptr<RenderWidgetHost> widget_host_;
  base::RunLoop run_loop_;
  base::RepeatingCallback<bool()> is_complete_callback_;
};

// Observer for RenderFrameProxyHost by setting itself through
// RenderFrameProxyHost::SetObserverForTesting.
class ProxyHostObserver : public RenderFrameProxyHost::TestObserver {
 public:
  using CreatedCallback = base::RepeatingCallback<void(RenderFrameProxyHost*)>;

  ProxyHostObserver() = default;
  ~ProxyHostObserver() override = default;

  void Reset() { created_callback_ = CreatedCallback(); }

  void set_created_callback(CreatedCallback callback) {
    created_callback_ = std::move(callback);
  }

 private:
  // RenderFrameProxyHost::TestObserver:
  void OnCreated(RenderFrameProxyHost* rfph) override {
    if (created_callback_)
      created_callback_.Run(rfph);
  }

  // Callback which runs on RenderFrameProxyHost is created.
  CreatedCallback created_callback_;
};

ProxyHostObserver* GetProxyHostObserver() {
  static base::NoDestructor<ProxyHostObserver> observer;
  return observer.get();
}

bool IsRequestCompatibleWithSpeculativeRFH(NavigationRequest* request) {
  return request->state() <=
             NavigationRequest::NavigationState::WILL_PROCESS_RESPONSE &&
         request->GetAssociatedRFHType() ==
             NavigationRequest::AssociatedRenderFrameHostType::NONE;
}

}  // namespace

bool WaiterHelper::WaitInternal() {
  if (event_received_) {
    return true;
  }
  run_loop_.Run();
  return event_received_;
}

bool WaiterHelper::Wait() {
  bool result = WaitInternal();
  event_received_ = false;
  return result;
}

void WaiterHelper::OnEvent() {
  event_received_ = true;
  run_loop_.Quit();
}

bool NavigateToURL(WebContents* web_contents, const GURL& url) {
  return NavigateToURL(web_contents, url, url);
}

bool NavigateToURL(WebContents* web_contents,
                   const GURL& url,
                   const GURL& expected_commit_url) {
  NavigateToURLBlockUntilNavigationsComplete(
      web_contents, url, 1,
      /*ignore_uncommitted_navigations=*/false);
  if (!IsLastCommittedEntryOfPageType(web_contents, PAGE_TYPE_NORMAL))
    return false;

  bool is_same_url = web_contents->GetLastCommittedURL() == expected_commit_url;
  if (!is_same_url) {
    DLOG(WARNING) << "Expected URL " << expected_commit_url << " but observed "
                  << web_contents->GetLastCommittedURL();
  }
  return is_same_url;
}

bool NavigateToURLFromRenderer(const ToRenderFrameHost& adapter,
                               const GURL& url) {
  return NavigateToURLFromRenderer(adapter, url, url);
}

bool NavigateToURLFromRenderer(const ToRenderFrameHost& adapter,
                               const GURL& url,
                               const GURL& expected_commit_url) {
  RenderFrameHost* rfh = adapter.render_frame_host();
  TestFrameNavigationObserver nav_observer(rfh);
  if (!BeginNavigateToURLFromRenderer(adapter, url))
    return false;
  nav_observer.Wait();
  return nav_observer.last_committed_url() == expected_commit_url &&
         nav_observer.last_navigation_succeeded();
}

bool NavigateToURLFromRendererWithoutUserGesture(
    const ToRenderFrameHost& adapter,
    const GURL& url) {
  return NavigateToURLFromRendererWithoutUserGesture(adapter, url, url);
}

bool NavigateToURLFromRendererWithoutUserGesture(
    const ToRenderFrameHost& adapter,
    const GURL& url,
    const GURL& expected_commit_url) {
  RenderFrameHost* rfh = adapter.render_frame_host();
  TestFrameNavigationObserver nav_observer(rfh);
  if (!ExecJs(rfh, JsReplace("location = $1", url),
              EXECUTE_SCRIPT_NO_USER_GESTURE)) {
    return false;
  }
  nav_observer.Wait();
  return nav_observer.last_committed_url() == expected_commit_url;
}

bool BeginNavigateToURLFromRenderer(const ToRenderFrameHost& adapter,
                                    const GURL& url) {
  ExecuteScriptAsync(adapter, JsReplace("location = $1", url));
  DidStartNavigationObserver observer(
      WebContents::FromRenderFrameHost(adapter.render_frame_host()));
  observer.Wait();
  return observer.observed();
}

bool NavigateIframeToURL(WebContents* web_contents,
                         std::string_view iframe_id,
                         const GURL& url) {
  TestNavigationObserver load_observer(web_contents);
  bool result = BeginNavigateIframeToURL(web_contents, iframe_id, url);
  load_observer.Wait();
  return result;
}

bool BeginNavigateIframeToURL(WebContents* web_contents,
                              std::string_view iframe_id,
                              const GURL& url) {
  std::string script =
      base::StrCat({"setTimeout(\"var iframes = document.getElementById('",
                    iframe_id, "');iframes.src='", url.spec(), "';\",0)"});
  return ExecJs(web_contents, script, EXECUTE_SCRIPT_NO_USER_GESTURE);
}

void NavigateToURLBlockUntilNavigationsComplete(
    WebContents* web_contents,
    const GURL& url,
    int number_of_navigations,
    bool ignore_uncommitted_navigations) {
  // This mimics behavior of Shell::LoadURL...
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

  NavigateToURLBlockUntilNavigationsComplete(web_contents, params,
                                             number_of_navigations,
                                             ignore_uncommitted_navigations);
}

void NavigateToURLBlockUntilNavigationsComplete(
    WebContents* web_contents,
    const NavigationController::LoadURLParams& params,
    int number_of_navigations,
    bool ignore_uncommitted_navigations) {
  // Prepare for the navigation.
  WaitForLoadStop(web_contents);
  TestNavigationObserver same_tab_observer(
      web_contents, number_of_navigations,
      MessageLoopRunner::QuitMode::IMMEDIATE,
      /*ignore_uncommitted_navigations=*/ignore_uncommitted_navigations);
  if (!blink::IsRendererDebugURL(params.url) && number_of_navigations == 1)
    same_tab_observer.set_expected_initial_url(params.url);

  web_contents->GetController().LoadURLWithParams(params);
  web_contents->GetOutermostWebContents()->Focus();

  // Wait until the expected number of navigations finish.
  same_tab_observer.Wait();
}

GURL GetFileUrlWithQuery(const base::FilePath& path,
                         std::string_view query_string) {
  GURL url = net::FilePathToFileURL(path);
  if (!query_string.empty()) {
    GURL::Replacements replacements;
    replacements.SetQueryStr(query_string);
    return url.ReplaceComponents(replacements);
  }
  return url;
}

void ResetTouchAction(RenderWidgetHost* host) {
  static_cast<input::InputRouterImpl*>(
      static_cast<RenderWidgetHostImpl*>(host)->input_router())
      ->ForceResetTouchActionForTest();
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
  NOTREACHED_IN_MIGRATION();
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
  static_cast<RenderFrameHostImpl*>(frame)
      ->GetRenderWidgetHost()
      ->delegate()
      ->SendActiveState(active);
}

std::optional<uint64_t> GetVisitedLinkSaltForNavigation(
    NavigationHandle* navigation_handle) {
  return static_cast<NavigationRequest*>(navigation_handle)
      ->commit_params()
      .visited_link_salt;
}

void WaitForLoadStopWithoutSuccessCheck(WebContents* web_contents) {
  // In many cases, the load may have finished before we get here.  Only wait if
  // the tab still has a pending navigation.
  if (web_contents->IsLoading()) {
    LoadStopObserver load_stop_observer(web_contents);
    load_stop_observer.Wait();
  }
}

bool IsLastCommittedPageNormal(WebContents* web_contents) {
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

bool WaitForLoadStop(WebContents* web_contents) {
  TRACE_EVENT0("test", "content::WaitForLoadStop");
  WebContentsDestroyedWatcher watcher(web_contents);
  WaitForLoadStopWithoutSuccessCheck(web_contents);
  if (watcher.IsDestroyed()) {
    LOG(ERROR) << "WebContents was destroyed during waiting for load stop.";
    return false;
  }
  return IsLastCommittedPageNormal(web_contents);
}

bool WaitForNavigationFinished(WebContents* web_contents,
                               TestNavigationObserver& observer) {
  TRACE_EVENT0("test", "content::WaitForNavigationFinished");
  WebContentsDestroyedWatcher watcher(web_contents);
  observer.WaitForNavigationFinished();
  if (watcher.IsDestroyed()) {
    LOG(ERROR)
        << "WebContents was destroyed during waiting for navigation finished.";
    return false;
  }
  return IsLastCommittedPageNormal(web_contents);
}

void PrepContentsForBeforeUnloadTest(WebContents* web_contents,
                                     bool trigger_user_activation) {
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [trigger_user_activation](RenderFrameHost* render_frame_host) {
        if (trigger_user_activation) {
          render_frame_host->ExecuteJavaScriptWithUserGestureForTests(
              std::u16string(), base::NullCallback(), ISOLATED_WORLD_ID_GLOBAL);
        }

        // Disable the hang monitor, otherwise there will be a race between
        // the beforeunload dialog and the beforeunload hang timer.
        render_frame_host->DisableBeforeUnloadHangMonitorForTesting();
      });
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
  RenderProcessHost* rph = web_contents->GetPrimaryMainFrame()->GetProcess();
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

bool CanCommitURLForTesting(int child_id, const GURL& url) {
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  return policy->CanCommitURL(child_id, url);
}

void SimulateUnresponsiveRenderer(WebContents* web_contents,
                                  RenderWidgetHost* widget) {
  static_cast<WebContentsImpl*>(web_contents)
      ->RendererUnresponsive(RenderWidgetHostImpl::From(widget),
                             base::DoNothing());
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
      web_contents->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget());
  if (!IsResizeComplete(&dispatcher_test, widget_host)) {
    ResizeObserver resize_observer(
        widget_host,
        base::BindRepeating(IsResizeComplete, &dispatcher_test, widget_host));
    resize_observer.Wait();
  }
}
#elif BUILDFLAG(IS_ANDROID)
bool IsResizeComplete(RenderWidgetHostImpl* widget_host) {
  return !widget_host->visual_properties_ack_pending_for_testing();
}

void WaitForResizeComplete(WebContents* web_contents) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  if (!IsResizeComplete(widget_host)) {
    ResizeObserver resize_observer(
        widget_host, base::BindRepeating(IsResizeComplete, widget_host));
    resize_observer.Wait();
  }
}
#endif

void NotifyCopyableViewInWebContents(WebContents* web_contents,
                                     base::OnceClosure done_callback) {
  NotifyCopyableViewInFrame(web_contents->GetPrimaryMainFrame(),
                            std::move(done_callback));
}

void NotifyCopyableViewInFrame(RenderFrameHost* render_frame_host,
                               base::OnceClosure done_callback) {
  RenderWidgetHostImpl* rwhi = static_cast<RenderWidgetHostImpl*>(
      render_frame_host->GetView()->GetRenderWidgetHost());

  // Note: this function intentionally avoids using RunLoops, which would make
  // the code easier to read, so that it can be used on Android which doesn't
  // support nested run loops.

  auto first_frame_done = base::BindOnce(
      [](base::WeakPtr<RenderWidgetHostImpl> rwhi,
         base::OnceClosure done_callback, bool success) {
        // This is invoked when the first `CompositorFrame` is submitted from
        // the renderer to the GPU. However, we want to wait until the Viz
        // process has received the new `CompositorFrame` so that the previously
        // submitted frame is available for copy. Waiting for a second frame to
        // be submitted guarantees this, since the second frame cannot be sent
        // until the first frame was ACKed by Viz.

        if (!rwhi || !success) {
          std::move(done_callback).Run();
          return;
        }

        // Force a redraw to ensure the callback below goes through the complete
        // compositing pipeline.
        rwhi->ForceRedrawForTesting();
        rwhi->InsertVisualStateCallback(base::BindOnce(
            [](base::WeakPtr<RenderWidgetHostImpl> rwhi,
               base::OnceClosure final_done_callback, bool success) {
              if (rwhi) {
                // `IsSurfaceAvailableForCopy` actually only checks if the
                // browser currently embeds a surface or not (as opposed to
                // sending a IPC to the GPU). However if the browser does not
                // embed any surface, we won't be able to issue any copy
                // requests.
                ASSERT_TRUE(rwhi->GetView()->IsSurfaceAvailableForCopy());
              }
              std::move(final_done_callback).Run();
            },
            rwhi->GetWeakPtr(), std::move(done_callback)));
      },
      rwhi->GetWeakPtr(), std::move(done_callback));

  rwhi->InsertVisualStateCallback(std::move(first_frame_done));
}

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

gfx::PointF GetCenterCoordinatesOfElementWithId(
    const ToRenderFrameHost& adapter,
    std::string_view id) {
  float x =
      EvalJs(adapter, JsReplace("const bounds = "
                                "document.getElementById($1)."
                                "getBoundingClientRect();"
                                "Math.floor(bounds.left + bounds.width / 2)",
                                id))
          .ExtractDouble();
  float y =
      EvalJs(adapter, JsReplace("const bounds = "
                                "document.getElementById($1)."
                                "getBoundingClientRect();"
                                "Math.floor(bounds.top + bounds.height / 2)",
                                id))
          .ExtractDouble();
  return gfx::PointF(x, y);
}

void SimulateMouseClickOrTapElementWithId(content::WebContents* web_contents,
                                          std::string_view id) {
  gfx::Point point = gfx::ToFlooredPoint(
      GetCenterCoordinatesOfElementWithId(web_contents, id));

#if BUILDFLAG(IS_ANDROID)
  SimulateTapDownAt(web_contents, point);
  SimulateTapAt(web_contents, point);
#else
  SimulateMouseClickAt(web_contents, 0, blink::WebMouseEvent::Button::kLeft,
                       point);
#endif  // BUILDFLAG(IS_ANDROID)
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
      web_contents->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget());
  widget_host->ForwardWheelEvent(wheel_event);
}

#if !BUILDFLAG(IS_MAC)
void SimulateMouseWheelCtrlZoomEvent(RenderWidgetHost* render_widget_host,
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
  RenderWidgetHostImpl* widget_host =
      RenderWidgetHostImpl::From(render_widget_host);
  widget_host->ForwardWheelEvent(wheel_event);
}

void SimulateTouchscreenPinch(WebContents* web_contents,
                              const gfx::PointF& anchor,
                              float scale_change,
                              base::OnceClosure on_complete) {
  SyntheticPinchGestureParams params;
  params.gesture_source_type = content::mojom::GestureSourceType::kTouchInput;
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

#endif  // !BUILDFLAG(IS_MAC)

void SimulateGesturePinchSequence(RenderWidgetHost* render_widget_host,
                                  const gfx::Point& point,
                                  float scale,
                                  blink::WebGestureDevice source_device) {
  RenderWidgetHostImpl* widget_host =
      RenderWidgetHostImpl::From(render_widget_host);

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

void SimulateGesturePinchSequence(WebContents* web_contents,
                                  const gfx::Point& point,
                                  float scale,
                                  blink::WebGestureDevice source_device) {
  RenderWidgetHost* widget_host =
      web_contents->GetPrimaryMainFrame()->GetRenderWidgetHost();
  SimulateGesturePinchSequence(widget_host, point, scale, source_device);
}

void SimulateGestureScrollSequence(RenderWidgetHost* render_widget_host,
                                   const gfx::Point& point,
                                   const gfx::Vector2dF& delta) {
  blink::WebGestureEvent scroll_begin(
      blink::WebGestureEvent::Type::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::WebGestureDevice::kTouchpad);
  scroll_begin.SetPositionInWidget(gfx::PointF(point));
  scroll_begin.data.scroll_begin.delta_x_hint = delta.x();
  scroll_begin.data.scroll_begin.delta_y_hint = delta.y();
  render_widget_host->ForwardGestureEvent(scroll_begin);

  blink::WebGestureEvent scroll_update(
      blink::WebGestureEvent::Type::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::WebGestureDevice::kTouchpad);
  scroll_update.SetPositionInWidget(gfx::PointF(point));
  scroll_update.data.scroll_update.delta_x = delta.x();
  scroll_update.data.scroll_update.delta_y = delta.y();
  render_widget_host->ForwardGestureEvent(scroll_update);

  blink::WebGestureEvent scroll_end(
      blink::WebGestureEvent::Type::kGestureScrollEnd,
      blink::WebInputEvent::kNoModifiers, ui::EventTimeForNow(),
      blink::WebGestureDevice::kTouchpad);
  scroll_end.SetPositionInWidget(gfx::PointF(point));
  render_widget_host->ForwardGestureEvent(scroll_end);
}

void SimulateGestureScrollSequence(WebContents* web_contents,
                                   const gfx::Point& point,
                                   const gfx::Vector2dF& delta) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetPrimaryMainFrame()->GetRenderWidgetHost());

  SimulateGestureScrollSequence(widget_host, point, delta);
}

void SimulateGestureEvent(RenderWidgetHost* render_widget_host,
                          const blink::WebGestureEvent& gesture_event,
                          const ui::LatencyInfo& latency) {
  RenderWidgetHostViewBase* view =
      static_cast<RenderWidgetHostViewBase*>(render_widget_host->GetView());
  view->ProcessGestureEvent(gesture_event, latency);
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
      web_contents->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget());
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
      web_contents->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget());
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
      ui::EventType::kTouchPressed, point, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhva->OnTouchEvent(&touch_start);

  ui::GestureEventDetails tap_down_details(ui::EventType::kGestureTapDown);
  tap_down_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent tap_down(point.x(), point.y(), 0, ui::EventTimeForNow(),
                            tap_down_details, touch_start.unique_event_id());
  rwhva->OnGestureEvent(&tap_down);

  ui::GestureEventDetails long_press_details(ui::EventType::kGestureLongPress);
  long_press_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent long_press(point.x(), point.y(), 0, ui::EventTimeForNow(),
                              long_press_details,
                              touch_start.unique_event_id());
  rwhva->OnGestureEvent(&long_press);

  ui::TouchEvent touch_end(ui::EventType::kTouchReleased, point,
                           base::TimeTicks(),
                           ui::PointerDetails(ui::EventPointerType::kTouch, 0));
  rwhva->OnTouchEvent(&touch_end);

  ui::GestureEventDetails long_tap_details(ui::EventType::kGestureLongTap);
  long_tap_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent long_tap(point.x(), point.y(), 0, ui::EventTimeForNow(),
                            long_tap_details, touch_end.unique_event_id());
  rwhva->OnGestureEvent(&long_tap);
}

// Observer which waits for the selection bounds in a RenderWidgetHostViewAura
// to meet some desired conditions.
class SelectionBoundsWaiter : public TextInputManager::Observer {
 public:
  using Predicate = base::RepeatingCallback<bool()>;

  SelectionBoundsWaiter(RenderWidgetHostViewAura* rwhva, Predicate predicate)
      : predicate_(std::move(predicate)) {
    text_input_manager_observation_.Observe(rwhva->GetTextInputManager());
  }
  SelectionBoundsWaiter(const SelectionBoundsWaiter&) = delete;
  SelectionBoundsWaiter& operator=(const SelectionBoundsWaiter&) = delete;
  virtual ~SelectionBoundsWaiter() = default;

  // TextInputManager::Observer:
  void OnSelectionBoundsChanged(
      TextInputManager* text_input_manager,
      RenderWidgetHostViewBase* updated_view) override {
    if (predicate_.Run()) {
      run_loop_.Quit();
    }
  }

  void Wait() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;
  Predicate predicate_;
  base::ScopedObservation<TextInputManager, TextInputManager::Observer>
      text_input_manager_observation_{this};
};

NonZeroCaretSizeWaiter::NonZeroCaretSizeWaiter(WebContents* web_contents) {
  RenderWidgetHostViewAura* rwhva =
      static_cast<content::RenderWidgetHostViewAura*>(
          web_contents->GetRenderWidgetHostView());
  selection_bounds_waiter_ = std::make_unique<SelectionBoundsWaiter>(
      rwhva, base::BindLambdaForTesting([rwhva]() {
        return !rwhva->GetCaretBounds().size().IsZero();
      }));
}

NonZeroCaretSizeWaiter::~NonZeroCaretSizeWaiter() = default;

void NonZeroCaretSizeWaiter::Wait() {
  selection_bounds_waiter_->Wait();
}

CaretBoundsUpdateWaiter::CaretBoundsUpdateWaiter(WebContents* web_contents) {
  RenderWidgetHostViewAura* rwhva =
      static_cast<content::RenderWidgetHostViewAura*>(
          web_contents->GetRenderWidgetHostView());
  const gfx::Rect current_caret_bounds = rwhva->GetCaretBounds();
  selection_bounds_waiter_ = std::make_unique<SelectionBoundsWaiter>(
      rwhva, base::BindLambdaForTesting([rwhva, current_caret_bounds]() {
        return rwhva->GetCaretBounds() != current_caret_bounds;
      }));
}

CaretBoundsUpdateWaiter::~CaretBoundsUpdateWaiter() = default;

void CaretBoundsUpdateWaiter::Wait() {
  selection_bounds_waiter_->Wait();
}

BoundingBoxUpdateWaiter::BoundingBoxUpdateWaiter(WebContents* web_contents) {
  RenderWidgetHostViewAura* rwhva =
      static_cast<content::RenderWidgetHostViewAura*>(
          web_contents->GetRenderWidgetHostView());
  const gfx::Rect current_bounding_box = rwhva->GetSelectionBoundingBox();
  selection_bounds_waiter_ = std::make_unique<SelectionBoundsWaiter>(
      rwhva, base::BindLambdaForTesting([rwhva, current_bounding_box]() {
        return rwhva->GetSelectionBoundingBox() != current_bounding_box;
      }));
}

BoundingBoxUpdateWaiter::~BoundingBoxUpdateWaiter() = default;

void BoundingBoxUpdateWaiter::Wait() {
  selection_bounds_waiter_->Wait();
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

void SimulateProxyHostPostMessage(RenderFrameHost* source_render_frame_host,
                                  RenderFrameHost* target_render_frame_host,
                                  blink::TransferableMessage message) {
  RenderFrameProxyHost* proxy_host =
      static_cast<RenderFrameHostImpl*>(target_render_frame_host)
          ->browsing_context_state()
          ->GetRenderFrameProxyHost(
              static_cast<SiteInstanceImpl*>(
                  source_render_frame_host->GetSiteInstance())
                  ->group());
  CHECK(proxy_host);

  proxy_host->RouteMessageEvent(
      source_render_frame_host->GetFrameToken(),
      source_render_frame_host->GetLastCommittedOrigin(),
      base::UTF8ToUTF16(
          target_render_frame_host->GetLastCommittedOrigin().Serialize()),
      std::move(message));
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
  return EvalJs(web_contents, kHasVideoInputDeviceOnSystem).ExtractString() ==
         kHasVideoInputDevice;
}

RenderFrameHost* ConvertToRenderFrameHost(WebContents* web_contents) {
  return web_contents->GetPrimaryMainFrame();
}

RenderFrameHost* ConvertToRenderFrameHost(RenderFrameHost* render_frame_host) {
  return render_frame_host;
}

void ExecuteScriptAsync(const ToRenderFrameHost& adapter,
                        std::string_view script) {
  // Prerendering pages will never have user gesture.
  if (adapter.render_frame_host()->GetLifecycleState() ==
      RenderFrameHost::LifecycleState::kPrerendering) {
    ExecuteScriptAsyncWithoutUserGesture(adapter, script);
  } else {
    adapter.render_frame_host()->ExecuteJavaScriptWithUserGestureForTests(
        base::UTF8ToUTF16(script), base::NullCallback(),
        ISOLATED_WORLD_ID_GLOBAL);
  }
}

void ExecuteScriptAsyncWithoutUserGesture(const ToRenderFrameHost& adapter,
                                          std::string_view script) {
  adapter.render_frame_host()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script), base::NullCallback(),
      ISOLATED_WORLD_ID_GLOBAL);
}

// EvalJsResult methods.
EvalJsResult::EvalJsResult(base::Value value, std::string_view error)
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

base::Value EvalJsResult::ExtractList() const {
  CHECK(error.empty())
      << "Can't ExtractList() because the script encountered a problem: "
      << error;
  CHECK(value.is_list()) << "Can't ExtractList() because script result: "
                         << value << "is not a list.";
  return value.Clone();
}

std::ostream& operator<<(std::ostream& os, const EvalJsResult& bar) {
  if (!bar.error.empty()) {
    os << bar.error;
  } else {
    os << bar.value;
  }
  return os;
}

namespace {

// Parse a JS stack trace out of |js_error|, detect frames that match
// |source_name|, and interleave the appropriate lines of source code from
// |source| into the error report. This is meant to be useful for scripts that
// are passed to ExecJs/EvalJs functions, and hence dynamically generated.
//
// An adjustment of |column_adjustment_for_line_one| characters is subtracted
// when mapping positions from line 1 of |source|. This is to offset the effect
// of boilerplate added by the script runner.
//
// TODO(nick): Elide snippets to 80 chars, since it is common for sources to not
// include newlines.
std::string AnnotateAndAdjustJsStackTraces(std::string_view js_error,
                                           std::string source_name,
                                           std::string_view source,
                                           int column_adjustment_for_line_one) {
  // Escape wildcards in |source_name| for use in MatchPattern.
  base::ReplaceChars(source_name, "\\", "\\\\", &source_name);
  base::ReplaceChars(source_name, "*", "\\*", &source_name);
  base::ReplaceChars(source_name, "?", "\\?", &source_name);

  // This vector maps line numbers to the corresponding text in |source|.
  const std::vector<std::string_view> source_lines = base::SplitStringPiece(
      source, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  // |source_frame_pattern| should match any line that looks like a stack frame
  // from a source file named |source_name|.
  const std::string source_frame_pattern =
      base::StringPrintf("    at *%s:*:*", source_name.c_str());

  // This is the amount of indentation that is applied to the lines of inserted
  // annotations.
  const std::string indent(8, ' ');
  const std::string_view elision_mark = "";

  // Loop over each line of |js_error|, and append each to |annotated_error| --
  // possibly rewriting to include extra context.
  std::ostringstream annotated_error;
  for (std::string_view error_line : base::SplitStringPiece(
           js_error, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL)) {
    // Does this look like a stack frame whose URL source matches |source_name|?
    if (base::MatchPattern(error_line, source_frame_pattern)) {
      // When a match occurs, annotate the stack trace with the corresponding
      // line from |source|, along with a ^^^ underneath, indicating the column
      // position.
      std::vector<std::string_view> error_line_parts = base::SplitStringPiece(
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
        std::string source_line(source_lines[line_number - 1]);

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

// Waits for a response from ExecuteJavaScriptForTests, simulating an
// error if the target renderer is destroyed while executing the script.
class ExecuteJavaScriptForTestsWaiter : public WebContentsObserver {
 public:
  explicit ExecuteJavaScriptForTestsWaiter(const ToRenderFrameHost& adapter)
      : WebContentsObserver(
            WebContents::FromRenderFrameHost(adapter.render_frame_host())),
        render_frame_host_(adapter.render_frame_host()) {}

  blink::mojom::LocalFrame::JavaScriptExecuteRequestForTestsCallback
  GetCallback() {
    return base::BindOnce(&ExecuteJavaScriptForTestsWaiter::SetValue,
                          weak_ptr_factory_.GetWeakPtr());
  }

  bool Wait() {
    if (!has_value_)
      run_loop_.Run();
    return has_value_;
  }

  blink::mojom::JavaScriptExecutionResultType GetResultType() {
    DCHECK(has_value_);
    return type_;
  }

  const base::Value& GetResult() {
    DCHECK(has_value_);
    return value_;
  }

  // WebContentsObserver
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
        status == base::TERMINATION_STATUS_STILL_RUNNING) {
      return;
    }
    UpdateAfterScriptFailed("Renderer terminated.");
  }
  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    if (render_frame_host_ != render_frame_host)
      return;
    UpdateAfterScriptFailed("RenderFrame deleted.");
  }

 private:
  void UpdateAfterScriptFailed(const std::string& msg) {
    render_frame_host_ = nullptr;
    if (has_value_)
      return;
    SetValue(blink::mojom::JavaScriptExecutionResultType::kException,
             base::Value(msg));
  }

  void SetValue(blink::mojom::JavaScriptExecutionResultType type,
                base::Value value) {
    DCHECK(!has_value_);
    has_value_ = true;
    type_ = type;
    value_ = value.Clone();
    run_loop_.Quit();
  }

  raw_ptr<RenderFrameHost> render_frame_host_;
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
  bool has_value_ = false;
  blink::mojom::JavaScriptExecutionResultType type_;
  base::Value value_;

  base::WeakPtrFactory<ExecuteJavaScriptForTestsWaiter> weak_ptr_factory_{this};
};

EvalJsResult EvalJsRunner(
    const ToRenderFrameHost& execution_target,
    std::string_view script,
    std::string_view source_url,
    int options,
    int32_t world_id,
    base::OnceClosure after_script_invoke = base::DoNothing()) {
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(execution_target.render_frame_host());
  if (!rfh->IsRenderFrameLive()) {
    return EvalJsResult(
        base::Value(), "Error: EvalJs won't work on an already-crashed frame.");
  }

  bool user_gesture = rfh->GetLifecycleState() !=
                          RenderFrameHost::LifecycleState::kPrerendering &&
                      !(options & EXECUTE_SCRIPT_NO_USER_GESTURE) &&
                      world_id == ISOLATED_WORLD_ID_GLOBAL;
  bool resolve_promises = !(options & EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);
  bool honor_js_content_settings =
      options & EXECUTE_SCRIPT_HONOR_JS_CONTENT_SETTINGS;

  ExecuteJavaScriptForTestsWaiter waiter(rfh);
  rfh->ExecuteJavaScriptForTests(base::UTF8ToUTF16(script), user_gesture,
                                 resolve_promises, honor_js_content_settings,
                                 world_id, waiter.GetCallback());

  std::move(after_script_invoke).Run();

  bool has_value = waiter.Wait();
  if (!has_value) {
    return EvalJsResult(base::Value(),
                        "Timeout waiting for Javascript to execute.");
  }

  using blink::mojom::JavaScriptExecutionResultType;
  JavaScriptExecutionResultType result_type = waiter.GetResultType();
  const base::Value& result_value = waiter.GetResult();

  if (result_type == JavaScriptExecutionResultType::kException) {
    // Parse the stack trace here, and interleave lines of source code from
    // |script| to aid debugging.
    CHECK(result_value.is_string() && !result_value.GetString().empty());
    std::string error_text =
        "a JavaScript error: \"" + result_value.GetString() + "\"";
    return EvalJsResult(base::Value(),
                        AnnotateAndAdjustJsStackTraces(
                            error_text, std::string(source_url), script, 0));
  }

  return EvalJsResult(result_value.Clone(), std::string());
}

}  // namespace

::testing::AssertionResult ExecJs(const ToRenderFrameHost& execution_target,
                                  std::string_view script,
                                  int options,
                                  int32_t world_id) {
  // TODO(nick): Do we care enough about folks shooting themselves in the foot
  // here with e.g. ASSERT_TRUE(ExecJs("window == window.top")) -- when they
  // mean EvalJs -- to fail a CHECK() when eval_result.value.is_bool()?
  EvalJsResult eval_result =
      EvalJs(execution_target, script, options, world_id);

  // NOTE: |eval_result.value| is intentionally ignored by ExecJs().
  if (!eval_result.error.empty())
    return ::testing::AssertionFailure() << eval_result.error;
  return ::testing::AssertionSuccess();
}

EvalJsResult EvalJs(const ToRenderFrameHost& execution_target,
                    std::string_view script,
                    int options,
                    int32_t world_id,
                    base::OnceClosure after_script_invoke) {
  TRACE_EVENT1("test", "EvalJs", "script", script);

  // The sourceURL= parameter provides a string that replaces <anonymous> in
  // stack traces, if an Error is thrown. 'std::string' is meant to communicate
  // that this is a dynamic argument originating from C++ code.
  //
  // Wrapping the script in braces makes it run in a block scope so that
  // let/const don't leak outside the code being run, but vars will float to
  // the outer scope.
  const char* kSourceURL = "__const_std::string&_script__";
  std::string modified_script =
      base::StrCat({"{", script, "\n}\n//# sourceURL=", kSourceURL});

  return EvalJsRunner(execution_target, modified_script, kSourceURL, options,
                      world_id, std::move(after_script_invoke));
}

EvalJsResult EvalJsAfterLifecycleUpdate(
    const ToRenderFrameHost& execution_target,
    std::string_view raf_script,
    std::string_view script,
    int options,
    int32_t world_id) {
  TRACE_EVENT2("test", "EvalJsAfterLifecycleUpdate", "raf_script", raf_script,
               "script", script);

  const char* kSourceURL = "__const_std::string&_script__";
  const char* kWrapperURL = "__const_std::string&_EvalJsAfterLifecycleUpdate__";
  std::string modified_raf_script;
  if (raf_script.length()) {
    modified_raf_script =
        base::StrCat({raf_script, ";\n//# sourceURL=", kSourceURL});
  }
  std::string modified_script =
      base::StrCat({script, ";\n//# sourceURL=", kSourceURL});

  // This runner_script delays running the argument scripts until just before
  // (|raf_script|) and after (|script|) a rendering update.
  std::string runner_script = JsReplace(
      R"(new Promise((resolve, reject) => {
           requestAnimationFrame(() => {
             try { window.eval($1); } catch (e) { reject(e); }
             setTimeout(() => {
               try { resolve(window.eval($2)); } catch (e) { reject(e); }
             });
           });
         })
         //# sourceURL=$3)",
      modified_raf_script, modified_script, kWrapperURL);

  EvalJsResult result = EvalJsRunner(execution_target, runner_script,
                                     kWrapperURL, options, world_id);

  if (base::StartsWith(result.error, "a JavaScript error: \"EvalError: Refused",
                       base::CompareCase::SENSITIVE)) {
    return EvalJsResult(
        base::Value(),
        "EvalJsAfterLifecycleUpdate encountered an EvalError, because eval() "
        "is blocked by the document's CSP on this page. To test content that "
        "is protected by CSP, consider using EvalJsAfterLifecycleUpdate in an "
        "isolated world. Details: " +
            result.error);
  }
  return result;
}

RenderFrameHost* FrameMatchingPredicateOrNullptr(
    Page& page,
    base::RepeatingCallback<bool(RenderFrameHost*)> predicate) {
  std::set<RenderFrameHost*> frame_set;
  page.GetMainDocument().ForEachRenderFrameHost(
      [&predicate, &frame_set](RenderFrameHost* rfh) {
        if (predicate.Run(rfh))
          frame_set.insert(rfh);
      });
  EXPECT_LE(frame_set.size(), 1u);
  return frame_set.size() == 1 ? *frame_set.begin() : nullptr;
}

RenderFrameHost* FrameMatchingPredicate(
    Page& page,
    base::RepeatingCallback<bool(RenderFrameHost*)> predicate) {
  content::RenderFrameHost* rfh =
      FrameMatchingPredicateOrNullptr(page, std::move(predicate));
  EXPECT_TRUE(rfh);
  return rfh;
}

bool FrameMatchesName(std::string_view name, RenderFrameHost* frame) {
  return frame->GetFrameName() == name;
}

bool FrameIsChildOfMainFrame(RenderFrameHost* frame) {
  return frame->GetParent() && !frame->GetParent()->GetParent();
}

bool FrameHasSourceUrl(const GURL& url, RenderFrameHost* frame) {
  return frame->GetLastCommittedURL() == url;
}

RenderFrameHost* ChildFrameAt(const ToRenderFrameHost& adapter, size_t index) {
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(adapter.render_frame_host());
  if (index >= rfh->frame_tree_node()->child_count())
    return nullptr;
  return rfh->frame_tree_node()->child_at(index)->current_frame_host();
}

bool HasOriginKeyedProcess(RenderFrameHost* frame) {
  return static_cast<RenderFrameHostImpl*>(frame)
      ->GetSiteInstance()
      ->GetSiteInfo()
      .requires_origin_keyed_process();
}

bool HasSandboxedSiteInstance(RenderFrameHost* frame) {
  return static_cast<RenderFrameHostImpl*>(frame)
      ->GetSiteInstance()
      ->GetSiteInfo()
      .is_sandboxed();
}

std::vector<RenderFrameHost*> CollectAllRenderFrameHosts(
    RenderFrameHost* starting_rfh) {
  std::vector<RenderFrameHost*> visited_frames;
  starting_rfh->ForEachRenderFrameHost(
      [&](RenderFrameHost* rfh) { visited_frames.push_back(rfh); });
  return visited_frames;
}

std::vector<RenderFrameHost*> CollectAllRenderFrameHosts(Page& page) {
  return CollectAllRenderFrameHosts(&page.GetMainDocument());
}

std::vector<RenderFrameHost*> CollectAllRenderFrameHosts(
    WebContents* web_contents) {
  std::vector<RenderFrameHost*> visited_frames;
  web_contents->ForEachRenderFrameHost(
      [&](RenderFrameHost* rfh) { visited_frames.push_back(rfh); });
  return visited_frames;
}

std::vector<WebContents*> GetAllWebContents() {
  std::vector<WebContentsImpl*> all_wci = WebContentsImpl::GetAllWebContents();
  std::vector<WebContents*> all_wc;
  base::ranges::transform(all_wci, std::back_inserter(all_wc),
                          [](WebContentsImpl* wc) { return wc; });

  return all_wc;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool ExecuteWebUIResourceTest(WebContents* web_contents) {
  // Inject WebUI test runner script.
  std::string script;
  scoped_refptr<base::RefCountedMemory> bytes =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          IDR_ASH_WEBUI_COMMON_WEBUI_RESOURCE_TEST_JS);

  if (HasGzipHeader(*bytes)) {
    AppendGzippedResource(*bytes, &script);
  } else {
    auto chars = base::as_chars(base::span(*bytes));
    script.append(chars.data(), chars.size());
  }

  script.append("\n");
  ExecuteScriptAsync(web_contents, script);

  DOMMessageQueue message_queue(web_contents);

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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::string GetCookies(BrowserContext* browser_context,
                       const GURL& url,
                       net::CookieOptions::SameSiteCookieContext context,
                       net::CookiePartitionKeyCollection key_collection) {
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  browser_context->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
  net::CookieOptions options;
  options.set_same_site_cookie_context(context);
  base::test::TestFuture<const net::CookieAccessResultList&,
                         const net::CookieAccessResultList&>
      future;
  cookie_manager->GetCookieList(url, options, key_collection,
                                future.GetCallback());
  return net::CanonicalCookie::BuildCookieLine(std::get<0>(future.Get()));
}

std::vector<net::CanonicalCookie> GetCanonicalCookies(
    BrowserContext* browser_context,
    const GURL& url,
    net::CookiePartitionKeyCollection key_collection) {
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  browser_context->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
  // Allow access to SameSite cookies in tests.
  net::CookieOptions options;
  options.set_same_site_cookie_context(
      net::CookieOptions::SameSiteCookieContext::MakeInclusive());
  base::test::TestFuture<const net::CookieAccessResultList&,
                         const net::CookieAccessResultList&>
      future;
  cookie_manager->GetCookieList(url, options, key_collection,
                                future.GetCallback());
  return net::cookie_util::StripAccessResults(std::get<0>(future.Get()));
}

bool SetCookie(BrowserContext* browser_context,
               const GURL& url,
               const std::string& value,
               net::CookieOptions::SameSiteCookieContext context,
               net::CookiePartitionKey* cookie_partition_key) {
  if (cookie_partition_key) {
    DCHECK(base::Contains(base::ToLowerASCII(value), ";partitioned"));
  }
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  browser_context->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());
  std::unique_ptr<net::CanonicalCookie> cc(
      net::CanonicalCookie::CreateForTesting(
          url, value, base::Time::Now(), std::nullopt /* server_time */,
          base::OptionalFromPtr(cookie_partition_key)));
  DCHECK(cc.get());

  net::CookieOptions options;
  options.set_include_httponly();
  options.set_same_site_cookie_context(context);
  base::test::TestFuture<net::CookieAccessResult> future;
  cookie_manager->SetCanonicalCookie(*cc.get(), url, options,
                                     future.GetCallback());
  return future.Get().status.IsInclude();
}

uint32_t DeleteCookies(BrowserContext* browser_context,
                       network::mojom::CookieDeletionFilter filter) {
  mojo::Remote<network::mojom::CookieManager> cookie_manager;
  browser_context->GetDefaultStoragePartition()
      ->GetNetworkContext()
      ->GetCookieManager(cookie_manager.BindNewPipeAndPassReceiver());

  base::test::TestFuture<uint32_t> future;
  cookie_manager->DeleteCookies(
      network::mojom::CookieDeletionFilter::New(filter), future.GetCallback());
  return future.Get();
}

void FetchHistogramsFromChildProcesses() {
  // Wait for all initialized processes to be ready before fetching histograms
  // for the first time.
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd(); it.Advance()) {
    RenderProcessHost* process = it.GetCurrentValue();
    if (process->IsInitializedAndNotDead() && !process->IsReady()) {
      RenderProcessHostWatcher ready_watcher(
          process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_READY);
      ready_watcher.Wait();
    }
  }

  base::RunLoop run_loop;

  FetchHistogramsAsynchronously(
      base::SingleThreadTaskRunner::GetCurrentDefault(), run_loop.QuitClosure(),
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

void SetFileSystemAccessPermissionContext(
    BrowserContext* browser_context,
    FileSystemAccessPermissionContext* permission_context) {
  static_cast<content::FileSystemAccessManagerImpl*>(
      browser_context->GetDefaultStoragePartition()
          ->GetFileSystemAccessEntryFactory())
      ->SetPermissionContextForTesting(permission_context);
}

bool WaitForRenderFrameReady(RenderFrameHost* rfh) {
  if (!rfh)
    return false;
  std::string result =
      EvalJs(rfh,
             "(async function() {"
             "  if (document.readyState != 'complete') {"
             "    await new Promise((resolve) =>"
             "      document.addEventListener('readystatechange', event => {"
             "        if (document.readyState == 'complete') {"
             "          resolve();"
             "        }"
             "      }));"
             "  }"
             "})().then(() => 'pageLoadComplete');",
             EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractString();
  EXPECT_EQ("pageLoadComplete", result);
  return "pageLoadComplete" == result;
}

void WaitForAccessibilityFocusChange() {
  base::RunLoop run_loop;
  ui::BrowserAccessibilityManager::SetFocusChangeCallbackForTesting(
      run_loop.QuitClosure());
  run_loop.Run();
}

ui::AXNodeData GetFocusedAccessibilityNodeInfo(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  ui::BrowserAccessibilityManager* manager =
      web_contents_impl->GetRootBrowserAccessibilityManager();
  if (!manager)
    return ui::AXNodeData();
  ui::BrowserAccessibility* focused_node = manager->GetFocus();
  return focused_node->GetData();
}

bool AccessibilityTreeContainsNodeWithName(ui::BrowserAccessibility* node,
                                           std::string_view name) {
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
  ASSERT_TRUE(accessibility_waiter.WaitForNotification());
}

void WaitForAccessibilityTreeToContainNodeWithName(WebContents* web_contents,
                                                   std::string_view name) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      web_contents_impl->GetPrimaryMainFrame());
  ui::BrowserAccessibilityManager* main_frame_manager =
      main_frame->browser_accessibility_manager();
  while (!main_frame_manager ||
         !AccessibilityTreeContainsNodeWithName(
             main_frame_manager->GetBrowserAccessibilityRoot(), name)) {
    WaitForAccessibilityTreeToChange(web_contents);
    main_frame_manager = main_frame->browser_accessibility_manager();
  }
}

ui::AXTreeUpdate GetAccessibilityTreeSnapshot(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  ui::BrowserAccessibilityManager* manager =
      web_contents_impl->GetRootBrowserAccessibilityManager();
  if (!manager)
    return ui::AXTreeUpdate();
  return manager->SnapshotAXTreeForTesting();
}

ui::AXTreeUpdate GetAccessibilityTreeSnapshotFromId(
    const ui::AXTreeID& tree_id) {
  ui::BrowserAccessibilityManager* manager =
      ui::BrowserAccessibilityManager::FromID(tree_id);
  return manager ? manager->SnapshotAXTreeForTesting() : ui::AXTreeUpdate();
}

ui::AXPlatformNodeDelegate* GetRootAccessibilityNode(
    WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  ui::BrowserAccessibilityManager* manager =
      web_contents_impl->GetRootBrowserAccessibilityManager();
  return manager ? manager->GetBrowserAccessibilityRoot() : nullptr;
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
  auto* node_internal =
      ui::BrowserAccessibility::FromAXPlatformNodeDelegate(node);
  DCHECK(node_internal);
  if ((!criteria.name ||
       node_internal->GetStringAttribute(ax::mojom::StringAttribute::kName) ==
           criteria.name.value()) &&
      (!criteria.role || node_internal->GetRole() == criteria.role.value())) {
    return node;
  }

  for (unsigned int i = 0; i < node_internal->PlatformChildCount(); ++i) {
    ui::BrowserAccessibility* child = node_internal->PlatformGetChild(i);
    ui::AXPlatformNodeDelegate* result =
        FindAccessibilityNodeInSubtree(child, criteria);
    if (result)
      return result;
  }
  return nullptr;
}

#if BUILDFLAG(IS_WIN)
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
    names.push_back(base::WideToUTF8(
        std::wstring(V_BSTR(name.ptr()), SysStringLen(V_BSTR(name.ptr())))));
  }

  ASSERT_THAT(names, ::testing::UnorderedElementsAreArray(expected_names));
}
#endif

RenderWidgetHost* GetKeyboardLockWidget(WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)->GetKeyboardLockWidget();
}

RenderWidgetHost* GetMouseLockWidget(WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)
      ->mouse_lock_widget_for_testing();
}

void RequestKeyboardLock(
    WebContents* web_contents,
    std::optional<base::flat_set<ui::DomCode>> codes,
    base::OnceCallback<void(blink::mojom::KeyboardLockRequestResult)>
        callback) {
  DCHECK(!codes.has_value() || !codes.value().empty());
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  RenderWidgetHostImpl* render_widget_host_impl =
      web_contents_impl->GetPrimaryMainFrame()->GetRenderWidgetHost();
  render_widget_host_impl->Focus();
  render_widget_host_impl->RequestKeyboardLock(std::move(codes),
                                               std::move(callback));
}

void CancelKeyboardLock(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  RenderWidgetHostImpl* render_widget_host_impl =
      web_contents_impl->GetPrimaryMainFrame()->GetRenderWidgetHost();
  render_widget_host_impl->CancelKeyboardLock();
}

ScreenOrientationDelegate* GetScreenOrientationDelegate() {
  return ScreenOrientationProvider::GetDelegateForTesting();
}

std::vector<RenderWidgetHostView*> GetInputEventRouterRenderWidgetHostViews(
    WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)
      ->GetRenderWidgetHostViewsForTests();
}

RenderWidgetHost* GetFocusedRenderWidgetHost(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  return web_contents_impl->GetFocusedRenderWidgetHost(
      web_contents_impl->GetPrimaryMainFrame()->GetRenderWidgetHost());
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

RenderFrameMetadataProviderImpl* RenderFrameMetadataProviderFromRenderFrameHost(
    RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);
  DCHECK(render_frame_host->GetRenderWidgetHost());
  // This helper should return a valid provider since it's used for
  // RenderFrameSubmissionObserver ctor.
  DCHECK(RenderWidgetHostImpl::From(render_frame_host->GetRenderWidgetHost())
             ->render_frame_metadata_provider());
  return RenderWidgetHostImpl::From(render_frame_host->GetRenderWidgetHost())
      ->render_frame_metadata_provider();
}

}  // namespace

TitleWatcher::TitleWatcher(WebContents* web_contents,
                           std::u16string_view expected_title)
    : WebContentsObserver(web_contents) {
  expected_titles_.emplace_back(expected_title);
}

void TitleWatcher::AlsoWaitForTitle(std::u16string_view expected_title) {
  expected_titles_.emplace_back(expected_title);
}

TitleWatcher::~TitleWatcher() = default;

const std::u16string& TitleWatcher::WaitAndGetTitle() {
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
  const std::u16string& current_title = web_contents()->GetTitle();
  if (base::Contains(expected_titles_, current_title)) {
    observed_title_ = current_title;
    run_loop_.Quit();
  }
}

RenderProcessHostWatcher::RenderProcessHostWatcher(
    RenderProcessHost* render_process_host,
    WatchType type)
    : type_(type),
      did_exit_normally_(true),
      allow_renderer_crashes_(
          std::make_unique<ScopedAllowRendererCrashes>(render_process_host)),
      quit_closure_(run_loop_.QuitClosure()) {
  observation_.Observe(render_process_host);
}

RenderProcessHostWatcher::RenderProcessHostWatcher(WebContents* web_contents,
                                                   WatchType type)
    : RenderProcessHostWatcher(
          web_contents->GetPrimaryMainFrame()->GetProcess(),
          type) {}
RenderProcessHostWatcher::~RenderProcessHostWatcher() = default;

void RenderProcessHostWatcher::Wait() {
  run_loop_.Run();

  DCHECK(allow_renderer_crashes_)
      << "RenderProcessHostWatcher::Wait() may only be called once";
  allow_renderer_crashes_.reset();
  // Call this here just in case something else quits the RunLoop.
  observation_.Reset();
}

void RenderProcessHostWatcher::QuitRunLoop() {
  std::move(quit_closure_).Run();
  observation_.Reset();
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
  if (type_ == WATCH_FOR_HOST_DESTRUCTION)
    QuitRunLoop();
}

RenderProcessHostKillWaiter::RenderProcessHostKillWaiter(
    RenderProcessHost* render_process_host,
    std::string_view uma_name)
    : exit_watcher_(render_process_host,
                    RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT),
      uma_name_(uma_name) {}

std::optional<int> RenderProcessHostKillWaiter::Wait() {
  std::optional<bad_message::BadMessageReason> result;

  // Wait for the renderer kill.
  exit_watcher_.Wait();
#if !BUILDFLAG(IS_ANDROID)
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

std::optional<std::string> RenderProcessHostBadMojoMessageWaiter::Wait() {
  std::optional<int> bad_message_reason = kill_waiter_.Wait();
  if (!bad_message_reason.has_value())
    return std::nullopt;
  if (bad_message_reason.value() != bad_message::RPH_MOJO_PROCESS_ERROR) {
    LOG(ERROR) << "Unexpected |bad_message_reason|: "
               << bad_message_reason.value();
    return std::nullopt;
  }

  return observed_mojo_error_;
}

void RenderProcessHostBadMojoMessageWaiter::OnBadMojoMessage(
    int render_process_id,
    const std::string& error) {
  if (render_process_id == monitored_render_process_id_)
    observed_mojo_error_ = error;
}

class DOMMessageQueue::MessageObserver : public WebContentsObserver {
 public:
  MessageObserver(DOMMessageQueue* queue, WebContents* contents)
      : WebContentsObserver(contents),
        queue_(queue),
        render_frame_host_(nullptr),
        watching_frame_(false) {}

  MessageObserver(DOMMessageQueue* queue, RenderFrameHost* render_frame_host)
      : WebContentsObserver(
            WebContents::FromRenderFrameHost(render_frame_host)),
        queue_(queue),
        render_frame_host_(render_frame_host),
        watching_frame_(true) {}

  ~MessageObserver() override = default;

 private:
  void DomOperationResponse(RenderFrameHost* rfh,
                            const std::string& result) override {
    if (!watching_frame_ || render_frame_host_ == rfh) {
      queue_->OnDomMessageReceived(result);
    }
  }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    queue_->PrimaryMainFrameRenderProcessGone(status);
  }

  void RenderFrameDeleted(RenderFrameHost* render_frame_host) override {
    if (render_frame_host_ != render_frame_host) {
      return;
    }
    render_frame_host_ = nullptr;
    queue_->RenderFrameDeleted();
  }

  void WebContentsDestroyed() override {
    queue_->OnBackingWebContentsDestroyed(this);
  }

  raw_ptr<DOMMessageQueue> queue_;
  raw_ptr<RenderFrameHost> render_frame_host_;
  bool watching_frame_;
};

DOMMessageQueue::DOMMessageQueue() {
  // TODO(crbug.com/40746969): Remove the need to listen for this
  // notification.
  for (auto* contents : WebContentsImpl::GetAllWebContents()) {
    observers_.emplace(std::make_unique<MessageObserver>(this, contents));
  }
  web_contents_creation_subscription_ =
      RegisterWebContentsCreationCallback(base::BindRepeating(
          &DOMMessageQueue::OnWebContentsCreated, base::Unretained(this)));
}

DOMMessageQueue::DOMMessageQueue(WebContents* web_contents) {
  observers_.emplace(std::make_unique<MessageObserver>(this, web_contents));
}

DOMMessageQueue::DOMMessageQueue(RenderFrameHost* render_frame_host) {
  observers_.emplace(
      std::make_unique<MessageObserver>(this, render_frame_host));
}

DOMMessageQueue::~DOMMessageQueue() = default;

void DOMMessageQueue::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  VLOG(0) << "DOMMessageQueue::RenderProcessGone " << status;
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
      break;
    default:
      renderer_crashed_ = true;
      if (callback_) {
        std::move(callback_).Run();
      }
      break;
  }
}

void DOMMessageQueue::RenderFrameDeleted() {
  if (callback_) {
    std::move(callback_).Run();
  }
}

void DOMMessageQueue::ClearQueue() {
  message_queue_ = base::queue<std::string>();
}

void DOMMessageQueue::OnDomMessageReceived(const std::string& message) {
  message_queue_.push(message);
  if (callback_) {
    std::move(callback_).Run();
  }
}

void DOMMessageQueue::OnWebContentsCreated(WebContents* contents) {
  observers_.emplace(std::make_unique<MessageObserver>(this, contents));
}

void DOMMessageQueue::OnBackingWebContentsDestroyed(MessageObserver* observer) {
  for (auto& entry : observers_) {
    if (entry.get() == observer) {
      observers_.erase(entry);
      break;
    }
  }
}

void DOMMessageQueue::SetOnMessageAvailableCallback(
    base::OnceClosure callback) {
  CHECK(!callback_);
  if (!message_queue_.empty() || renderer_crashed_) {
    std::move(callback).Run();
  } else {
    callback_ = std::move(callback);
  }
}

bool DOMMessageQueue::WaitForMessage(std::string* message) {
  DCHECK(message);
  if (!renderer_crashed_ && message_queue_.empty()) {
    // This will be quit when a new message comes in.
    base::RunLoop run_loop{base::RunLoop::Type::kNestableTasksAllowed};
    callback_ = run_loop.QuitClosure();
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

bool DOMMessageQueue::HasMessages() {
  return !message_queue_.empty();
}

WebContentsAddedObserver::WebContentsAddedObserver()
    : creation_subscription_(RegisterWebContentsCreationCallback(
          base::BindRepeating(&WebContentsAddedObserver::WebContentsCreated,
                              base::Unretained(this)))) {}

WebContentsAddedObserver::~WebContentsAddedObserver() = default;

void WebContentsAddedObserver::WebContentsCreated(WebContents* web_contents) {
  DCHECK(!web_contents_);
  web_contents_ = web_contents;

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

bool RequestFrame(WebContents* web_contents) {
  DCHECK(web_contents);
  return RenderWidgetHostImpl::From(web_contents->GetPrimaryMainFrame()
                                        ->GetRenderViewHost()
                                        ->GetWidget())
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
          RenderFrameMetadataProviderFromRenderFrameHost(
              node->current_frame_host())) {}

RenderFrameSubmissionObserver::RenderFrameSubmissionObserver(
    WebContents* web_contents)
    : RenderFrameSubmissionObserver(
          RenderFrameMetadataProviderFromRenderFrameHost(
              web_contents->GetPrimaryMainFrame())) {}

RenderFrameSubmissionObserver::RenderFrameSubmissionObserver(
    RenderFrameHost* rfh)
    : RenderFrameSubmissionObserver(
          RenderFrameMetadataProviderFromRenderFrameHost(rfh)) {}

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
    const gfx::PointF& expected_offset) {
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

void RenderFrameSubmissionObserver::OnRenderFrameMetadataChangedAfterActivation(
    base::TimeTicks activation_time) {
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

void InputMsgWatcher::OnInputEvent(const blink::WebInputEvent& event) {
  last_sent_event_type_ = event.GetType();
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
    : render_widget_host_(
          static_cast<RenderWidgetHostImpl*>(render_widget_host)->GetWeakPtr()),
      predicate_(predicate) {
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
  if (render_widget_host_) {
    render_widget_host_->RemoveInputEventObserver(this);
  }
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
  ~FrameTreeNodeObserverImpl() override {
    if (owner_)
      owner_->RemoveObserver(this);
  }

  void Run() { run_loop_.Run(); }

  void OnFrameTreeNodeFocused(FrameTreeNode* node) override {
    if (node == owner_)
      run_loop_.Quit();
  }

  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override {
    if (node == owner_)
      owner_ = nullptr;
  }

 private:
  raw_ptr<FrameTreeNode> owner_;
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
  explicit FrameTreeNodeObserverImpl(FrameTreeNode* owner)
      : frame_tree_node_id_(owner->frame_tree_node_id()), owner_(owner) {
    owner->AddObserver(this);
  }
  ~FrameTreeNodeObserverImpl() override = default;

  void Run() {
    if (!IsDestroyed()) {
      run_loop_.Run();
    }
  }

  bool IsDestroyed() const { return owner_ == nullptr; }

  FrameTreeNodeId frame_tree_node_id() const { return frame_tree_node_id_; }

 private:
  // FrameTreeNode::Observer:
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override {
    if (node == owner_) {
      owner_ = nullptr;
      run_loop_.Quit();
    }
  }

  const content::FrameTreeNodeId frame_tree_node_id_;
  raw_ptr<FrameTreeNode> owner_;
  base::RunLoop run_loop_;
};

FrameDeletedObserver::FrameDeletedObserver(RenderFrameHost* owner_host)
    : impl_(std::make_unique<FrameTreeNodeObserverImpl>(
          static_cast<RenderFrameHostImpl*>(owner_host)->frame_tree_node())) {}

FrameDeletedObserver::~FrameDeletedObserver() = default;

void FrameDeletedObserver::Wait() {
  impl_->Run();
}

bool FrameDeletedObserver::IsDeleted() const {
  return impl_->IsDestroyed();
}

content::FrameTreeNodeId FrameDeletedObserver::GetFrameTreeNodeId() const {
  return impl_->frame_tree_node_id();
}

TestNavigationManager::TestNavigationManager(WebContents* web_contents,
                                             const GURL& url)
    : WebContentsObserver(web_contents), url_(url) {}

TestNavigationManager::~TestNavigationManager() {
  ResumeIfPaused();
}

bool TestNavigationManager::WaitForFirstYieldAfterDidStartNavigation() {
  TRACE_EVENT(
      "test",
      "TestNavigationManager::WaitForFirstYieldAfterDidStartNavigation");
  if (current_state_ >= NavigationState::WILL_START)
    return true;

  DCHECK_EQ(desired_state_, NavigationState::WILL_START);
  // Ignore the result because DidStartNavigation will update |desired_state_|
  // we check below.
  (void)WaitForDesiredState();
  // This returns false if the runloop was terminated by a timeout rather than
  // reaching the |WILL_START|.
  return current_state_ >= NavigationState::WILL_START;
}

bool TestNavigationManager::WaitForRequestStart() {
  TRACE_EVENT("test", "TestNavigationManager::WaitForRequestStart");
  desired_state_ = NavigationState::REQUEST_STARTED;
  return WaitForDesiredState();
}

bool TestNavigationManager::WaitForLoaderStart() {
  TRACE_EVENT("test", "TestNavigationManager::WaitForLoaderStart");
  desired_state_ = NavigationState::LOADER_STARTED;
  return WaitForDesiredState();
}

bool TestNavigationManager::WaitForRequestRedirected() {
  desired_state_ = NavigationState::REDIRECTED;
  return WaitForDesiredState();
}

void TestNavigationManager::ResumeNavigation() {
  TRACE_EVENT("test", "TestNavigationManager::ResumeNavigation");
  CHECK(current_state_ == NavigationState::REQUEST_STARTED ||
        current_state_ == NavigationState::REDIRECTED ||
        current_state_ == NavigationState::RESPONSE);
  CHECK_EQ(current_state_, desired_state_);
  CHECK(navigation_paused_);
  ResumeIfPaused();
}

NavigationHandle* TestNavigationManager::GetNavigationHandle() {
  return request_;
}

ukm::SourceId TestActivationManager::next_page_ukm_source_id() const {
  EXPECT_NE(ukm::kInvalidSourceId, next_page_ukm_source_id_);
  return next_page_ukm_source_id_;
}

bool TestNavigationManager::WaitForResponse() {
  TRACE_EVENT("test", "TestNavigationManager::WaitForResponse");
  desired_state_ = NavigationState::RESPONSE;
  return WaitForDesiredState();
}

bool TestNavigationManager::WaitForNavigationFinished() {
  TRACE_EVENT("test", "TestNavigationManager::WaitForNavigationFinished");
  desired_state_ = NavigationState::FINISHED;
  return WaitForDesiredState();
}

void TestNavigationManager::WaitForSpeculativeRenderFrameHostCreation() {
  TRACE_EVENT(
      "test",
      "TestNavigationManager::WaitForSpeculativeRenderFrameHostCreation");
  if (current_state_ < NavigationState::REQUEST_STARTED) {
    CHECK(WaitForRequestStart());
  }
  if (!speculative_rfh_created_) {
    base::RunLoop run_loop(message_loop_type_);
    wait_rfh_closure_ = run_loop.QuitClosure();
    ResumeNavigation();
    run_loop.Run();
  }
}

void TestNavigationManager::DidStartNavigation(NavigationHandle* handle) {
  if (!ShouldMonitorNavigation(handle))
    return;

  DCHECK(!handle->IsPageActivation())
      << "For PageActivating navigations, use TestActivationManager.";

  request_ = NavigationRequest::From(handle);
  auto throttle = std::make_unique<TestNavigationManagerThrottle>(
      request_,
      base::BindOnce(&TestNavigationManager::OnWillStartRequest,
                     weak_factory_.GetWeakPtr()),
      base::BindRepeating(&TestNavigationManager::OnWillRedirectRequest,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(&TestNavigationManager::OnWillProcessResponse,
                     weak_factory_.GetWeakPtr()));
  request_->RegisterThrottleForTesting(std::move(throttle));

  current_state_ = NavigationState::WILL_START;

  OnNavigationStateChanged();

  // This is the default desired state. A browser-initiated navigation can
  // reach WillStartRequest state synchronously, so the TestNavigationManager
  // is set to always pause navigations at WillStartRequest. This ensures the
  // navigation will defer and the user can always call
  // WaitForRequestStart.
  if (!request_->IsPageActivation() &&
      desired_state_ == NavigationState::WILL_START) {
    desired_state_ = NavigationState::REQUEST_STARTED;
  }
}

void TestNavigationManager::DidUpdateNavigationHandleTiming(
    NavigationHandle* handle) {
  if (handle != request_ ||
      handle->GetNavigationHandleTiming().loader_start_time.is_null() ||
      current_state_ >= NavigationState::LOADER_STARTED) {
    return;
  }

  CHECK(!handle->IsPageActivation())
      << "For PageActivating navigations, use TestActivationManager.";

  current_state_ = NavigationState::LOADER_STARTED;

  OnNavigationStateChanged();
}

void TestNavigationManager::DidRedirectNavigation(NavigationHandle* handle) {
  if (handle != request_) {
    return;
  }

  CHECK(!handle->IsPageActivation())
      << "For PageActivating navigations, use TestActivationManager.";

  current_state_ = NavigationState::REDIRECTED;

  OnNavigationStateChanged();
}

void TestNavigationManager::DidFinishNavigation(NavigationHandle* handle) {
  if (handle != request_)
    return;
  was_committed_ = handle->HasCommitted();
  was_successful_ = was_committed_ && !handle->IsErrorPage();
  current_state_ = NavigationState::FINISHED;
  navigation_paused_ = false;
  request_ = nullptr;

  // Invalidate the WeakPtrs so that the throttle callbacks will not be
  // called after this point. We need to do this because
  // TestNavigationManagerThrottle posts tasks for these callbacks and we
  // may get a call to this function before those tasks run.
  weak_factory_.InvalidateWeakPtrs();
  OnNavigationStateChanged();
}

void TestNavigationManager::OnWillStartRequest() {
  current_state_ = NavigationState::REQUEST_STARTED;
  navigation_paused_ = true;
  OnNavigationStateChanged();
}

void TestNavigationManager::OnWillRedirectRequest() {
  current_state_ = NavigationState::REDIRECTED;
  navigation_paused_ = true;
  OnNavigationStateChanged();
}

void TestNavigationManager::OnWillProcessResponse() {
  current_state_ = NavigationState::RESPONSE;
  navigation_paused_ = true;
  OnNavigationStateChanged();
}

void TestNavigationManager::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  RenderFrameHostImpl* host_impl =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  NavigationRequest* request =
      host_impl->frame_tree_node()->navigation_request();
  if (host_impl->lifecycle_state() ==
          RenderFrameHostImpl::LifecycleStateImpl::kSpeculative &&
      IsRequestCompatibleWithSpeculativeRFH(request) &&
      request->GetURL() == url_ &&
      (request == request_ || request_ == nullptr)) {
    DCHECK(host_impl->frame_tree_node()->HasNavigation());
    speculative_rfh_created_ = true;
    created_speculative_rfh_ =
        std::make_unique<RenderFrameHostWrapper>(render_frame_host);
    if (wait_rfh_closure_) {
      std::move(wait_rfh_closure_).Run();
    }
  }
}

RenderFrameHost* TestNavigationManager::GetCreatedSpeculativeRFH() {
  if (!created_speculative_rfh_) {
    return nullptr;
  }
  return created_speculative_rfh_->get();
}

// TODO(csharrison): Remove CallResumeForTesting method calls in favor of doing
// it through the throttle.
bool TestNavigationManager::WaitForDesiredState() {
  // If the desired state has already been reached, just return.
  if (current_state_ == desired_state_)
    return true;

  // Resume the navigation if it was paused.
  ResumeIfPaused();

  // Wait for the desired state if needed.
  if (current_state_ < desired_state_) {
    DCHECK(!state_quit_closure_);
    base::RunLoop run_loop(message_loop_type_);
    state_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Return false if the navigation did not reach the state specified by the
  // user.
  return current_state_ == desired_state_;
}

void TestNavigationManager::OnNavigationStateChanged() {
  TRACE_EVENT("test", "TestNavigationManager::OnNavigationStateChanged", "this",
              this);

  // If the state the user was waiting for has been reached, exit the message
  // loop.
  if (current_state_ >= desired_state_) {
    if (state_quit_closure_) {
      std::move(state_quit_closure_).Run();
    }
    return;
  }

  // Otherwise, the navigation should be resumed if it was previously paused.
  ResumeIfPaused();
}

void TestNavigationManager::ResumeIfPaused() {
  TRACE_EVENT("test", "TestNavigationManager::ResumeIfPaused", "this", this);

  if (!navigation_paused_)
    return;

  navigation_paused_ = false;

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

void TestNavigationManager::WriteIntoTrace(
    perfetto::TracedValue context) const {
  perfetto::TracedDictionary dict = std::move(context).WriteDictionary();
  dict.Add("url", url_);
  dict.Add("navigation_request", request_);
  dict.Add("navigation_paused", navigation_paused_);
  dict.Add("current_state", current_state_);
  dict.Add("desired_state", desired_state_);
}

namespace {

// A helper CommitDeferringCondition instantiated and inserted into all
// navigations from TestActivationManager. It delegates WillCommitNavigation
// method of the CommitDeferringCondition back to the TestActivationManager so
// that the manager can see and decide how to proceed with the condition for
// every occurring navigation.
class TestActivationManagerCondition : public CommitDeferringCondition {
  using WillCommitCallback =
      base::OnceCallback<Result(CommitDeferringCondition&, base::OnceClosure)>;

 public:
  TestActivationManagerCondition(NavigationHandle& handle,
                                 WillCommitCallback on_will_commit_navigation)
      : CommitDeferringCondition(handle),
        on_will_commit_navigation_(std::move(on_will_commit_navigation)) {}
  ~TestActivationManagerCondition() override = default;

  TestActivationManagerCondition(const TestActivationManagerCondition&) =
      delete;
  TestActivationManagerCondition& operator=(
      const TestActivationManagerCondition&) = delete;

  Result WillCommitNavigation(base::OnceClosure resume) override {
    return std::move(on_will_commit_navigation_).Run(*this, std::move(resume));
  }
  const char* TraceEventName() const override {
    return "TestActivationManagerCondition";
  }

 private:
  WillCommitCallback on_will_commit_navigation_;
};

// We need this wrapper since the TestActivationManager can be destroyed while
// navigations are ongoing so we need to pass the callback with a WeakPtr.
// However, we can't bind a WeakPtr to a method that returns non-void so we use
// this wrapper to provide the default return value.
CommitDeferringCondition::Result ConditionCallbackWeakWrapper(
    base::WeakPtr<TestActivationManager> manager,
    base::RepeatingCallback<
        CommitDeferringCondition::Result(TestActivationManager*,
                                         CommitDeferringCondition&,
                                         base::OnceClosure)> manager_func,
    CommitDeferringCondition& condition,
    base::OnceClosure resume_callback) {
  // If the manager was destroyed, we don't need to pause navigation any longer
  // so just proceed.
  if (!manager)
    return CommitDeferringCondition::Result::kProceed;

  return manager_func.Run(manager.get(), condition, std::move(resume_callback));
}

}  // namespace

class TestActivationManager::ConditionInserter {
  using WillCommitCallback =
      base::RepeatingCallback<CommitDeferringCondition::Result(
          CommitDeferringCondition&,
          base::OnceClosure)>;

 public:
  explicit ConditionInserter(WillCommitCallback callback,
                             CommitDeferringConditionRunner::InsertOrder order)
      : condition_callback_(std::move(callback)),
        generator_id_(
            CommitDeferringConditionRunner::InstallConditionGeneratorForTesting(
                base::BindRepeating(&ConditionInserter::Install,
                                    base::Unretained(this)),
                order)) {}
  ~ConditionInserter() {
    CommitDeferringConditionRunner::UninstallConditionGeneratorForTesting(
        generator_id_);
  }

 private:
  std::unique_ptr<CommitDeferringCondition> Install(
      NavigationHandle& handle,
      CommitDeferringCondition::NavigationType navigation_type) {
    // TestActivationManager should only pause during checks for an activating
    // navigation. It's possible for a navigation to start off as activating
    // but become non-activating after the initial checks. In that case,
    // CommitDeferringConditions will be run a second time as non-activating so
    // we'll avoid registering the second time with this early out.
    // TODO(bokan): We can't check navigation_type here because BFCache is
    // considered kOther. Ideally all page activations would be a single type.
    // crbug.com/1226442.
    auto* request = NavigationRequest::From(&handle);
    if (!request->IsServedFromBackForwardCache() &&
        !request->is_running_potential_prerender_activation_checks()) {
      return nullptr;
    }

    return std::make_unique<TestActivationManagerCondition>(
        handle, condition_callback_);
  }

  WillCommitCallback condition_callback_;
  const int generator_id_;
};

TestActivationManager::TestActivationManager(WebContents* web_contents,
                                             const GURL& url)
    : WebContentsObserver(web_contents), url_(url) {
  first_condition_inserter_ = std::make_unique<ConditionInserter>(
      base::BindRepeating(
          &ConditionCallbackWeakWrapper, weak_factory_.GetWeakPtr(),
          base::BindRepeating(&TestActivationManager::FirstConditionCallback)),
      CommitDeferringConditionRunner::InsertOrder::kBefore);
  last_condition_inserter_ = std::make_unique<ConditionInserter>(
      base::BindRepeating(
          &ConditionCallbackWeakWrapper, weak_factory_.GetWeakPtr(),
          base::BindRepeating(&TestActivationManager::LastConditionCallback)),
      CommitDeferringConditionRunner::InsertOrder::kAfter);
}

TestActivationManager::~TestActivationManager() {
  DCHECK(!quit_closure_);
  if (is_paused())
    std::move(resume_callback_).Run();
}

bool TestActivationManager::WaitForBeforeChecks() {
  TRACE_EVENT("test", "TestActivationManager::WaitForBeforeChecks");
  desired_state_ = ActivationState::kBeforeChecks;
  return WaitForDesiredState();
}

bool TestActivationManager::WaitForAfterChecks() {
  TRACE_EVENT("test", "TestActivationManager::WaitForAfterChecks");
  desired_state_ = ActivationState::kAfterChecks;
  return WaitForDesiredState();
}

void TestActivationManager::WaitForNavigationFinished() {
  TRACE_EVENT("test", "TestActivationManager::WaitForNavigationFinished");
  desired_state_ = ActivationState::kFinished;
  bool finished = WaitForDesiredState();
  DCHECK(finished);
}

void TestActivationManager::ResumeActivation() {
  TRACE_EVENT("test", "TestActivationManager::ResumeActivation");
  DCHECK(is_paused());

  // Set desired_state_ to kFinished so the navigation can proceed to finish
  // unless it yields somewhere and/or the caller calls another WaitFor method.
  desired_state_ = ActivationState::kFinished;
  std::move(resume_callback_).Run();
}

NavigationHandle* TestActivationManager::GetNavigationHandle() {
  return request_;
}

void TestActivationManager::SetCallbackCalledAfterActivationIsReady(
    base::OnceClosure callback) {
  DCHECK(!callback_in_last_condition);
  callback_in_last_condition = std::move(callback);
}

CommitDeferringCondition::Result TestActivationManager::FirstConditionCallback(
    CommitDeferringCondition& condition,
    base::OnceClosure resume_callback) {
  if (condition.GetNavigationHandle().GetURL() != url_)
    return CommitDeferringCondition::Result::kProceed;

  DCHECK(!is_tracking_activation_)
      << "Second request for watched URL: " << url_;
  is_tracking_activation_ = true;

  DCHECK(!request_);
  request_ = NavigationRequest::From(&condition.GetNavigationHandle());
  DCHECK(request_->is_running_potential_prerender_activation_checks() ||
         request_->IsServedFromBackForwardCache())
      << "TestActivationManager should only be used for for page "
         "activations. For regular navigations, use TestNavigationManager.";

  DCHECK_EQ(current_state_, ActivationState::kInitial);
  current_state_ = ActivationState::kBeforeChecks;

  if (current_state_ < desired_state_)
    return CommitDeferringCondition::Result::kProceed;

  resume_callback_ = std::move(resume_callback);
  StopWaitingIfNeeded();

  // Always defer here so test code gets a chance to set WaitFor... before the
  // navigation finishes.
  return CommitDeferringCondition::Result::kDefer;
}

CommitDeferringCondition::Result TestActivationManager::LastConditionCallback(
    CommitDeferringCondition& condition,
    base::OnceClosure resume_callback) {
  if (callback_in_last_condition) {
    std::move(callback_in_last_condition).Run();
  }

  if (request_ != &condition.GetNavigationHandle())
    return CommitDeferringCondition::Result::kProceed;

  DCHECK(is_tracking_activation_);

  current_state_ = ActivationState::kAfterChecks;
  if (current_state_ < desired_state_)
    return CommitDeferringCondition::Result::kProceed;

  resume_callback_ = std::move(resume_callback);
  StopWaitingIfNeeded();

  return CommitDeferringCondition::Result::kDefer;
}

void TestActivationManager::DidFinishNavigation(NavigationHandle* handle) {
  if (handle == request_) {
    DCHECK(is_tracking_activation_);
    was_committed_ = handle->HasCommitted();
    was_successful_ = was_committed_ && !handle->IsErrorPage();
    was_activated_ = was_successful_ && handle->IsPageActivation();
    next_page_ukm_source_id_ = handle->GetNextPageUkmSourceId();
    request_ = nullptr;
    current_state_ = ActivationState::kFinished;
    StopWaitingIfNeeded();
  }
}

bool TestActivationManager::WaitForDesiredState() {
  DCHECK_LE(current_state_, desired_state_);

  // If the desired state has already been reached, just return.
  if (current_state_ == desired_state_)
    return true;

  // Resume the navigation if it was paused.
  if (is_paused())
    ResumeActivation();

  // Wait for the desired state if needed.
  if (current_state_ < desired_state_) {
    DCHECK(!quit_closure_);
    base::RunLoop run_loop(base::RunLoop::Type::kDefault);
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Return false if the navigation did not reach the state specified by the
  // user.
  return current_state_ == desired_state_;
}

void TestActivationManager::StopWaitingIfNeeded() {
  if (current_state_ == desired_state_ && quit_closure_)
    std::move(quit_closure_).Run();
}

NavigationHandleCommitObserver::NavigationHandleCommitObserver(
    content::WebContents* web_contents,
    const GURL& url)
    : WebContentsObserver(web_contents), url_(url) {}

void NavigationHandleCommitObserver::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (handle->GetURL() != url_)
    return;
  has_committed_ = true;
  was_same_document_ = handle->IsSameDocument();
  was_renderer_initiated_ = handle->IsRendererInitiated();
  navigation_type_ =
      NavigationRequest::From(handle)->common_params().navigation_type;
}

WebContentsConsoleObserver::WebContentsConsoleObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {}
WebContentsConsoleObserver::~WebContentsConsoleObserver() = default;

bool WebContentsConsoleObserver::Wait() {
  return waiter_helper_.Wait();
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
    RenderFrameHost* source_frame,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message_contents,
    int32_t line_no,
    const std::u16string& source_id,
    const std::optional<std::u16string>& untrusted_stack_trace) {
  Message message(
      {source_frame, log_level, message_contents, line_no, source_id});
  if (filter_ && !filter_.Run(message))
    return;

  if (!pattern_.empty() &&
      !base::MatchPattern(base::UTF16ToUTF8(message_contents), pattern_)) {
    return;
  }

  messages_.push_back(std::move(message));
  waiter_helper_.OnEvent();
}

namespace {
static constexpr int kEnableLogMessageId = 0;
static constexpr char kEnableLogMessage[] = R"({"id":0,"method":"Log.enable"})";
static constexpr int kDisableLogMessageId = 1;
static constexpr char kDisableLogMessage[] =
    R"({"id":1,"method":"Log.disable"})";
}  // namespace

DevToolsInspectorLogWatcher::DevToolsInspectorLogWatcher(
    WebContents* web_contents) {
  host_ = DevToolsAgentHost::GetOrCreateFor(web_contents);
  host_->AttachClient(this);

  host_->DispatchProtocolMessage(
      this, base::as_bytes(
                base::make_span(kEnableLogMessage, strlen(kEnableLogMessage))));

  run_loop_enable_log_.Run();
}

DevToolsInspectorLogWatcher::~DevToolsInspectorLogWatcher() {
  host_->DetachClient(this);
}

void DevToolsInspectorLogWatcher::DispatchProtocolMessage(
    DevToolsAgentHost* host,
    base::span<const uint8_t> message) {
  std::string_view message_str(reinterpret_cast<const char*>(message.data()),
                                message.size());
  auto parsed_message =
      std::move(base::JSONReader::Read(message_str)->GetDict());
  std::optional<int> command_id = parsed_message.FindInt("id");
  if (command_id.has_value()) {
    switch (command_id.value()) {
      case kEnableLogMessageId:
        run_loop_enable_log_.Quit();
        break;
      case kDisableLogMessageId:
        run_loop_disable_log_.Quit();
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
    return;
  }

  std::string* notification = parsed_message.FindString("method");
  if (notification && *notification == "Log.entryAdded") {
    std::string* text =
        parsed_message.FindStringByDottedPath("params.entry.text");
    DCHECK(text);
    last_message_ = *text;
    std::string* url =
        parsed_message.FindStringByDottedPath("params.entry.url");
    if (url) {
      last_url_ = GURL(*url);
    }
  }
}

void DevToolsInspectorLogWatcher::AgentHostClosed(DevToolsAgentHost* host) {}

void DevToolsInspectorLogWatcher::FlushAndStopWatching() {
  host_->DispatchProtocolMessage(
      this, base::as_bytes(base::make_span(kDisableLogMessage,
                                           strlen(kDisableLogMessage))));
  run_loop_disable_log_.Run();
}

namespace {
mojo::Remote<blink::mojom::FileSystemManager> GetFileSystemManager(
    RenderProcessHost* rph,
    const blink::StorageKey& storage_key) {
  FileSystemManagerImpl* file_system = static_cast<RenderProcessHostImpl*>(rph)
                                           ->GetFileSystemManagerForTesting();
  mojo::Remote<blink::mojom::FileSystemManager> file_system_manager_remote;
  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemManagerImpl::BindReceiver,
                     base::Unretained(file_system), storage_key,
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
                                        bool recursive,
                                        const blink::StorageKey& storage_key) {
  TestFileapiOperationWaiter waiter;
  mojo::Remote<blink::mojom::FileSystemManager> file_system_manager =
      GetFileSystemManager(process, storage_key);
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
                                       int64_t position,
                                       const blink::StorageKey& storage_key) {
  TestFileapiOperationWaiter waiter;
  mojo::Remote<blink::mojom::FileSystemManager> file_system_manager =
      GetFileSystemManager(process, storage_key);
  mojo::PendingRemote<blink::mojom::FileSystemOperationListener> listener;
  mojo::Receiver<blink::mojom::FileSystemOperationListener> receiver(
      &waiter, listener.InitWithNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::FileSystemCancellableOperation> op;

  file_system_manager->Write(
      file_path, process->GetBrowserContext()->GetBlobRemote(blob_uuid),
      position, op.BindNewPipeAndPassReceiver(), std::move(listener));
  waiter.WaitForOperationToFinish();
}

void PwnMessageHelper::OpenURL(RenderFrameHost* render_frame_host,
                               const GURL& url) {
  auto params = blink::mojom::OpenURLParams::New();
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

  EvictionStateWaiter(const EvictionStateWaiter&) = delete;
  EvictionStateWaiter& operator=(const EvictionStateWaiter&) = delete;

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
  raw_ptr<DelegatedFrameHost> delegated_frame_host_;
  DelegatedFrameHost::FrameEvictionState waited_eviction_state_;
  base::OnceClosure quit_closure_;
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
      ->EvictDelegatedFrame(delegated_frame_host->GetFrameEvictorForTesting()
                                ->CollectSurfaceIdsForEviction());
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

// static
void BlobURLStoreInterceptor::Intercept(GURL target_url,
                                        storage::BlobUrlRegistry* registry,
                                        mojo::ReceiverId receiver_id) {
  auto interceptor = base::WrapUnique(new BlobURLStoreInterceptor(target_url));
  auto* raw_interceptor = interceptor.get();
  auto impl = registry->receivers_for_testing().SwapImplForTesting(
      receiver_id, std::move(interceptor));
  raw_interceptor->url_store_ = std::move(impl);
}

blink::mojom::BlobURLStore* BlobURLStoreInterceptor::GetForwardingInterface() {
  return url_store_.get();
}

void BlobURLStoreInterceptor::Register(
    mojo::PendingRemote<blink::mojom::Blob> blob,
    const GURL& url,
    // TODO(crbug.com/40775506): Remove these once experiment is over.
    const base::UnguessableToken& unsafe_agent_cluster_id,
    const std::optional<net::SchemefulSite>& unsafe_top_level_site,
    RegisterCallback callback) {
  GetForwardingInterface()->Register(
      std::move(blob), target_url_, unsafe_agent_cluster_id,
      unsafe_top_level_site, std::move(callback));
}

BlobURLStoreInterceptor::BlobURLStoreInterceptor(GURL target_url)
    : target_url_(target_url) {}
BlobURLStoreInterceptor::~BlobURLStoreInterceptor() = default;

namespace {

int LoadBasicRequest(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& url,
    int load_flags,
    const std::optional<url::Origin>& request_initiator = std::nullopt) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->load_flags = load_flags;
  request->request_initiator = request_initiator;
  // Allow access to SameSite cookies in tests.
  request->site_for_cookies = net::SiteForCookies::FromUrl(url);

  SimpleURLLoaderTestHelper simple_loader_helper;
  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      network::SimpleURLLoader::Create(std::move(request),
                                       TRAFFIC_ANNOTATION_FOR_TESTS);

  simple_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory, simple_loader_helper.GetCallbackDeprecated());
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
  url_loader_factory_params->is_orb_enabled = false;
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
      frame->GetLastCommittedOrigin() /* request_initiator */);
}

void EnsureCookiesFlushed(BrowserContext* browser_context) {
  browser_context->ForEachLoadedStoragePartition(
      [](StoragePartition* partition) {
        base::RunLoop run_loop;
        partition->GetCookieManagerForBrowserProcess()->FlushCookieStore(
            run_loop.QuitClosure());
        run_loop.Run();
      });
}

bool TestGuestAutoresize(WebContents* embedder_web_contents,
                         RenderFrameHost* guest_main_frame) {
  RenderFrameProxyHost* subframe_proxy_host =
      FrameTreeNode::From(guest_main_frame)
          ->render_manager()
          ->GetProxyToOuterDelegate();
  RenderWidgetHostImpl* guest_rwh_impl = static_cast<RenderWidgetHostImpl*>(
      guest_main_frame->GetRenderWidgetHost());

  auto interceptor = std::make_unique<SynchronizeVisualPropertiesInterceptor>(
      subframe_proxy_host);

  viz::LocalSurfaceId current_id =
      guest_rwh_impl->GetView()->GetLocalSurfaceId();
  // The guest may not yet be fully attached / initted. If not, |current_id|
  // will be invalid, and we should wait for an ID before proceeding.
  if (!current_id.is_valid())
    current_id = interceptor->WaitForSurfaceId();

  // Enable auto-resize.
  gfx::Size min_size(10, 10);
  gfx::Size max_size(100, 100);
  guest_rwh_impl->SetAutoResize(true, min_size, max_size);
  guest_rwh_impl->GetView()->EnableAutoResize(min_size, max_size);

  // Enabling auto resize generates a surface ID, wait for it.
  current_id = interceptor->WaitForSurfaceId();

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
  return interceptor->WaitForSurfaceId() ==
         viz::LocalSurfaceId(current_id.parent_sequence_number() + 1,
                             current_id.child_sequence_number() + 1,
                             current_id.embed_token());
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
  // Set callback and observer to track the creation of RenderFrameProxyHosts.
  ProxyHostObserver* observer = GetProxyHostObserver();
  observer->set_created_callback(base::BindRepeating(
      &ProxyDSFObserver::OnCreation, base::Unretained(this)));
  RenderFrameProxyHost::SetObserverForTesting(observer);
}

ProxyDSFObserver::~ProxyDSFObserver() {
  // Stop observing RenderFrameProxyHosts.
  GetProxyHostObserver()->Reset();
  RenderFrameProxyHost::SetObserverForTesting(nullptr);
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
    proxy_host_created_dsf_.push_back(
        cpfc->screen_infos().current().device_scale_factor);
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
    web_contents->GetPrimaryMainFrame()->InsertVisualStateCallback(
        base::BindLambdaForTesting([&](bool visual_state_updated) {
          ASSERT_TRUE(visual_state_updated);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  auto* rwh = RenderWidgetHostImpl::From(
      web_contents->GetPrimaryMainFrame()->GetRenderViewHost()->GetWidget());

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

DidFinishNavigationObserver::DidFinishNavigationObserver(
    WebContents* web_contents,
    base::RepeatingCallback<void(NavigationHandle*)> callback)
    : WebContentsObserver(web_contents), callback_(callback) {}

DidFinishNavigationObserver::DidFinishNavigationObserver(
    RenderFrameHost* render_frame_host,
    base::RepeatingCallback<void(NavigationHandle*)> callback)
    : DidFinishNavigationObserver(
          WebContents::FromRenderFrameHost(render_frame_host),
          callback) {}

DidFinishNavigationObserver::~DidFinishNavigationObserver() = default;

void DidFinishNavigationObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  callback_.Run(navigation_handle);
}

// Since the loading state might be switched more than once due to navigation
// restart, the following helper methods for history traversal need to call
// `WaitForNavigationFinished()` to ensure the navigation is finally committed
// and `WaitForLoadStop()` to ensure the `WebContents` finishes the last
// loading.
bool HistoryGoToIndex(WebContents* wc, int index) {
  TestNavigationObserver observer(wc);
  wc->GetController().GoToIndex(index);
  WaitForNavigationFinished(wc, observer);
  return WaitForLoadStop(wc);
}

bool HistoryGoToOffset(WebContents* wc, int offset) {
  TestNavigationObserver observer(wc);
  wc->GetController().GoToOffset(offset);
  WaitForNavigationFinished(wc, observer);
  return WaitForLoadStop(wc);
}

bool HistoryGoBack(WebContents* wc) {
  TestNavigationObserver observer(wc);
  wc->GetController().GoBack();
  WaitForNavigationFinished(wc, observer);
  return WaitForLoadStop(wc);
}

bool HistoryGoForward(WebContents* wc) {
  TestNavigationObserver observer(wc);
  wc->GetController().GoForward();
  WaitForNavigationFinished(wc, observer);
  return WaitForLoadStop(wc);
}

CreateAndLoadWebContentsObserver::CreateAndLoadWebContentsObserver(
    int num_expected_contents)
    : creation_subscription_(
          RegisterWebContentsCreationCallback(base::BindRepeating(
              &CreateAndLoadWebContentsObserver::OnWebContentsCreated,
              base::Unretained(this)))),
      num_expected_contents_(num_expected_contents) {
  EXPECT_GE(num_expected_contents, 1);
}

CreateAndLoadWebContentsObserver::~CreateAndLoadWebContentsObserver() = default;

void CreateAndLoadWebContentsObserver::OnWebContentsCreated(
    WebContents* web_contents) {
  ++num_new_contents_seen_;
  if (num_new_contents_seen_ < num_expected_contents_) {
    return;
  }

  // If there is already a WebContents, then this will fail the test later.
  if (num_new_contents_seen_ > num_expected_contents_) {
    ADD_FAILURE() << "Unexpected WebContents creation";
    // If we're called before Wait(), then `contents_creation_quit_closure_`
    // has not been set. If we're called after, then we'll clear this when
    // we see the creation of the expected contents and it won't be set again.
    EXPECT_FALSE(contents_creation_quit_closure_);
    return;
  }

  web_contents_ = web_contents;
  load_stop_observer_.emplace(web_contents_);

  if (contents_creation_quit_closure_)
    std::move(contents_creation_quit_closure_).Run();
}

WebContents* CreateAndLoadWebContentsObserver::Wait() {
  // Wait for a new WebContents if we haven't gotten one yet.
  if (!load_stop_observer_) {
    base::RunLoop run_loop;
    contents_creation_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  load_stop_observer_->Wait();

  // Do this after waiting for load to complete, since only the specified number
  // of WebContents should be created before Wait() returns. If an additional
  // one is created while the expected contents is loading, then we still fail
  // the test.
  EXPECT_EQ(num_expected_contents_, num_new_contents_seen_);
  creation_subscription_ = base::CallbackListSubscription();

  return web_contents_;
}

CookieChangeObserver::CookieChangeObserver(content::WebContents* web_contents,
                                           int num_expected_calls)
    : content::WebContentsObserver(web_contents),
      run_loop_(base::RunLoop::Type::kNestableTasksAllowed),
      num_expected_calls_(num_expected_calls) {}

CookieChangeObserver::~CookieChangeObserver() = default;

void CookieChangeObserver::Wait() {
  run_loop_.Run();
}

void CookieChangeObserver::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  OnCookieAccessed(details);
}

void CookieChangeObserver::OnCookiesAccessed(
    content::NavigationHandle* navigation,
    const content::CookieAccessDetails& details) {
  OnCookieAccessed(details);
}

void CookieChangeObserver::OnCookieAccessed(
    const content::CookieAccessDetails& details) {
  if (details.type == CookieAccessDetails::Type::kRead) {
    num_read_seen_++;
  } else if (details.type == CookieAccessDetails::Type::kChange) {
    num_write_seen_++;
  }

  if (++num_seen_ == num_expected_calls_) {
    run_loop_.Quit();
  }
}

SpeculativeRenderFrameHostObserver::SpeculativeRenderFrameHostObserver(
    content::WebContents* web_contents,
    const GURL& url)
    : content::WebContentsObserver(web_contents), url_(url) {}

SpeculativeRenderFrameHostObserver::~SpeculativeRenderFrameHostObserver() =
    default;

void SpeculativeRenderFrameHostObserver::Wait() {
  run_loop_.Run();
}

void SpeculativeRenderFrameHostObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  RenderFrameHostImpl* host_impl =
      static_cast<RenderFrameHostImpl*>(render_frame_host);
  NavigationRequest* request =
      host_impl->frame_tree_node()->navigation_request();
  if (host_impl->lifecycle_state() ==
          RenderFrameHostImpl::LifecycleStateImpl::kSpeculative &&
      IsRequestCompatibleWithSpeculativeRFH(request) &&
      request->GetURL() == url_) {
    run_loop_.Quit();
  }
}

SpareRenderProcessHostStartedObserver::SpareRenderProcessHostStartedObserver() {
  scoped_observation_.Observe(&SpareRenderProcessHostManager::Get());
}

SpareRenderProcessHostStartedObserver::
    ~SpareRenderProcessHostStartedObserver() = default;

void SpareRenderProcessHostStartedObserver::OnSpareRenderProcessHostReady(
    RenderProcessHost* host) {
  spare_render_process_host_ = host;
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

RenderProcessHost*
SpareRenderProcessHostStartedObserver::WaitForSpareRenderProcessStarted() {
  base::RunLoop loop;
  quit_closure_ = loop.QuitClosure();
  if (!spare_render_process_host_) {
    loop.Run();
  }

  RenderProcessHost* host = std::exchange(spare_render_process_host_, nullptr);
  scoped_observation_.Reset();
  return host;
}

base::CallbackListSubscription RegisterWebContentsCreationCallback(
    base::RepeatingCallback<void(WebContents*)> callback) {
  return WebContentsImpl::FriendWrapper::AddCreatedCallbackForTesting(callback);
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
void SetCapturedSurfaceControllerFactoryForTesting(
    base::RepeatingCallback<std::unique_ptr<MockCapturedSurfaceController>(
        GlobalRenderFrameHostId,
        WebContentsMediaCaptureId)> factory) {
  using FactoryType =
      ::base::RepeatingCallback<std::unique_ptr<CapturedSurfaceController>(
          GlobalRenderFrameHostId, WebContentsMediaCaptureId,
          base::RepeatingCallback<void(int)>)>;
  using MockFactoryType =
      ::base::RepeatingCallback<std::unique_ptr<MockCapturedSurfaceController>(
          GlobalRenderFrameHostId, WebContentsMediaCaptureId)>;

  FactoryType wrapped_factory = base::BindRepeating(
      [](MockFactoryType mock_factory, GlobalRenderFrameHostId rfh_id,
         WebContentsMediaCaptureId captured_wc_id,
         base::RepeatingCallback<void(int)> on_zoom_level_change_callback) {
        std::unique_ptr<MockCapturedSurfaceController> mock_controller =
            mock_factory.Run(rfh_id, captured_wc_id);
        std::unique_ptr<CapturedSurfaceController> wrapped_object =
            base::WrapUnique<CapturedSurfaceController>(
                mock_controller.release());
        return wrapped_object;
      },
      factory);

  MediaStreamManager::GetInstance()
      ->SetCapturedSurfaceControllerFactoryForTesting(wrapped_factory);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

}  // namespace content
