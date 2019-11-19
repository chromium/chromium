// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/blink_test_runner.h"

#include <stddef.h>

#include <algorithm>
#include <clocale>
#include <cmath>
#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/debugger.h"
#include "base/files/file_path.h"
#include "base/hash/md5.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/plugins/renderer/plugin_placeholder.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/use_zoom_for_dsf_policy.h"
#include "content/public/common/web_preferences.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/test/web_test_support.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/common/web_test/blink_test_messages.h"
#include "content/shell/common/web_test/web_test_messages.h"
#include "content/shell/renderer/web_test/blink_test_helpers.h"
#include "content/shell/renderer/web_test/web_test_render_thread_observer.h"
#include "content/shell/test_runner/app_banner_service.h"
#include "content/shell/test_runner/gamepad_controller.h"
#include "content/shell/test_runner/pixel_dump.h"
#include "content/shell/test_runner/web_test_interfaces.h"
#include "content/shell/test_runner/web_test_runner.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "media/base/audio_capturer_source.h"
#include "media/base/audio_parameters.h"
#include "media/capture/video_capturer_source.h"
#include "media/media_buildflags.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/mojom/app_banner/app_banner.mojom.h"
#include "third_party/blink/public/platform/file_path_conversion.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_context_menu_data.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_history_item.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_params.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/icc_profile.h"

using blink::Platform;
using blink::WebContextMenuData;
using blink::WebElement;
using blink::WebFrame;
using blink::WebHistoryItem;
using blink::WebLocalFrame;
using blink::WebPoint;
using blink::WebRect;
using blink::WebScriptSource;
using blink::WebSize;
using blink::WebString;
using blink::WebTestingSupport;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLRequest;
using blink::WebVector;
using blink::WebView;

namespace content {

namespace {

class UseSynchronousResizeModeVisitor : public RenderViewVisitor {
 public:
  explicit UseSynchronousResizeModeVisitor(bool enable) : enable_(enable) {}
  ~UseSynchronousResizeModeVisitor() override {}

  bool Visit(RenderView* render_view) override {
    UseSynchronousResizeMode(render_view, enable_);
    return true;
  }

 private:
  bool enable_;
};

class MockVideoCapturerSource : public media::VideoCapturerSource {
 public:
  MockVideoCapturerSource() = default;
  ~MockVideoCapturerSource() override {}

  media::VideoCaptureFormats GetPreferredFormats() override {
    const int supported_width = 640;
    const int supported_height = 480;
    const float supported_framerate = 60.0;
    return media::VideoCaptureFormats(
        1, media::VideoCaptureFormat(
               gfx::Size(supported_width, supported_height),
               supported_framerate, media::PIXEL_FORMAT_I420));
  }
  void StartCapture(const media::VideoCaptureParams& params,
                    const VideoCaptureDeliverFrameCB& new_frame_callback,
                    const RunningCallback& running_callback) override {
    running_callback.Run(true);
  }
  void StopCapture() override {}
};

class MockAudioCapturerSource : public media::AudioCapturerSource {
 public:
  MockAudioCapturerSource() = default;

  void Initialize(const media::AudioParameters& params,
                  CaptureCallback* callback) override {}
  void Start() override {}
  void Stop() override {}
  void SetVolume(double volume) override {}
  void SetAutomaticGainControl(bool enable) override {}
  void SetOutputDeviceForAec(const std::string& output_device_id) override {}

 protected:
  ~MockAudioCapturerSource() override {}
};

}  // namespace

BlinkTestRunner::BlinkTestRunner(RenderView* render_view)
    : RenderViewObserver(render_view),
      RenderViewObserverTracker<BlinkTestRunner>(render_view),
      test_config_(mojom::ShellTestConfiguration::New()) {}

BlinkTestRunner::~BlinkTestRunner() {}

// WebTestDelegate  -----------------------------------------------------------

void BlinkTestRunner::ClearEditCommand() {
  render_view()->ClearEditCommands();
}

void BlinkTestRunner::SetEditCommand(const std::string& name,
                                     const std::string& value) {
  render_view()->SetEditCommandForNextKeyEvent(name, value);
}

void BlinkTestRunner::PrintMessageToStderr(const std::string& message) {
  Send(new BlinkTestHostMsg_PrintMessageToStderr(routing_id(), message));
}

void BlinkTestRunner::PrintMessage(const std::string& message) {
  Send(new BlinkTestHostMsg_PrintMessage(routing_id(), message));
}

void BlinkTestRunner::PostTask(base::OnceClosure task) {
  GetTaskRunner()->PostTask(FROM_HERE, std::move(task));
}

void BlinkTestRunner::PostDelayedTask(base::OnceClosure task,
                                      base::TimeDelta delay) {
  GetTaskRunner()->PostDelayedTask(FROM_HERE, std::move(task), delay);
}

WebString BlinkTestRunner::RegisterIsolatedFileSystem(
    const blink::WebVector<blink::WebString>& absolute_filenames) {
  std::vector<base::FilePath> files;
  for (size_t i = 0; i < absolute_filenames.size(); ++i)
    files.push_back(blink::WebStringToFilePath(absolute_filenames[i]));
  std::string filesystem_id;
  Send(new WebTestHostMsg_RegisterIsolatedFileSystem(routing_id(), files,
                                                     &filesystem_id));
  return WebString::FromUTF8(filesystem_id);
}

long long BlinkTestRunner::GetCurrentTimeInMillisecond() {
  return base::TimeDelta(base::Time::Now() - base::Time::UnixEpoch())
             .ToInternalValue() /
         base::Time::kMicrosecondsPerMillisecond;
}

WebString BlinkTestRunner::GetAbsoluteWebStringFromUTF8Path(
    const std::string& utf8_path) {
  base::FilePath path = base::FilePath::FromUTF8Unsafe(utf8_path);
  if (!path.IsAbsolute()) {
    GURL base_url =
        net::FilePathToFileURL(test_config_->current_working_directory.Append(
            FILE_PATH_LITERAL("foo")));
    net::FileURLToFilePath(base_url.Resolve(utf8_path), &path);
  }
  return blink::FilePathToWebString(path);
}

WebURL BlinkTestRunner::LocalFileToDataURL(const WebURL& file_url) {
  base::FilePath local_path;
  if (!net::FileURLToFilePath(file_url, &local_path))
    return WebURL();

  std::string contents;
  Send(
      new WebTestHostMsg_ReadFileToString(routing_id(), local_path, &contents));

  std::string contents_base64;
  base::Base64Encode(contents, &contents_base64);

  const char data_url_prefix[] = "data:text/css:charset=utf-8;base64,";
  return WebURL(GURL(data_url_prefix + contents_base64));
}

WebURL BlinkTestRunner::RewriteWebTestsURL(const std::string& utf8_url,
                                           bool is_wpt_mode) {
  return content::RewriteWebTestsURL(utf8_url, is_wpt_mode);
}

test_runner::TestPreferences* BlinkTestRunner::Preferences() {
  return &prefs_;
}

void BlinkTestRunner::ApplyPreferences() {
  WebPreferences prefs = render_view()->GetWebkitPreferences();
  ExportWebTestSpecificPreferences(prefs_, &prefs);
  render_view()->SetWebkitPreferences(prefs);
  Send(new BlinkTestHostMsg_OverridePreferences(routing_id(), prefs));
}

void BlinkTestRunner::SetPopupBlockingEnabled(bool block_popups) {
  Send(
      new BlinkTestHostMsg_SetPopupBlockingEnabled(routing_id(), block_popups));
}

void BlinkTestRunner::UseUnfortunateSynchronousResizeMode(bool enable) {
  UseSynchronousResizeModeVisitor visitor(enable);
  RenderView::ForEach(&visitor);
}

void BlinkTestRunner::EnableAutoResizeMode(const WebSize& min_size,
                                           const WebSize& max_size) {
  content::EnableAutoResizeMode(render_view(), min_size, max_size);
}

void BlinkTestRunner::DisableAutoResizeMode(const WebSize& new_size) {
  content::DisableAutoResizeMode(render_view(), new_size);
  ForceResizeRenderView(render_view(), new_size);
}

void BlinkTestRunner::ResetAutoResizeMode() {
  // An empty size indicates to keep the size as is. Resetting races with the
  // browser setting up the new test (one is a mojo IPC (OnSetTestConfiguration)
  // and one is legacy (OnReset)) so we can not clobber the size here.
  content::DisableAutoResizeMode(render_view(), gfx::Size());
  // Does not call ForceResizeRenderView() here intentionally. This is between
  // tests, and the next test will set up a size.
}

void BlinkTestRunner::NavigateSecondaryWindow(const GURL& url) {
  Send(new BlinkTestHostMsg_NavigateSecondaryWindow(routing_id(), url));
}

void BlinkTestRunner::InspectSecondaryWindow() {
  Send(new WebTestHostMsg_InspectSecondaryWindow(routing_id()));
}

void BlinkTestRunner::ClearAllDatabases() {
  Send(new WebTestHostMsg_ClearAllDatabases(routing_id()));
}

void BlinkTestRunner::SetDatabaseQuota(int quota) {
  Send(new WebTestHostMsg_SetDatabaseQuota(routing_id(), quota));
}

void BlinkTestRunner::SimulateWebNotificationClick(
    const std::string& title,
    const base::Optional<int>& action_index,
    const base::Optional<base::string16>& reply) {
  Send(new WebTestHostMsg_SimulateWebNotificationClick(routing_id(), title,
                                                       action_index, reply));
}

void BlinkTestRunner::SimulateWebNotificationClose(const std::string& title,
                                                   bool by_user) {
  Send(new WebTestHostMsg_SimulateWebNotificationClose(routing_id(), title,
                                                       by_user));
}

void BlinkTestRunner::SimulateWebContentIndexDelete(const std::string& id) {
  Send(new WebTestHostMsg_SimulateWebContentIndexDelete(routing_id(), id));
}

void BlinkTestRunner::SetDeviceScaleFactor(float factor) {
  content::SetDeviceScaleFactor(render_view(), factor);
}

std::unique_ptr<blink::WebInputEvent>
BlinkTestRunner::TransformScreenToWidgetCoordinates(
    test_runner::WebWidgetTestProxy* web_widget_test_proxy,
    const blink::WebInputEvent& event) {
  return content::TransformScreenToWidgetCoordinates(web_widget_test_proxy,
                                                     event);
}

test_runner::WebWidgetTestProxy* BlinkTestRunner::GetWebWidgetTestProxy(
    blink::WebLocalFrame* frame) {
  return content::GetWebWidgetTestProxy(frame);
}

void BlinkTestRunner::EnableUseZoomForDSF() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableUseZoomForDSF);
}

bool BlinkTestRunner::IsUseZoomForDSFEnabled() {
  return content::IsUseZoomForDSFEnabled();
}

void BlinkTestRunner::SetDeviceColorSpace(const std::string& name) {
  content::SetDeviceColorSpace(render_view(), GetTestingColorSpace(name));
}

void BlinkTestRunner::SetBluetoothFakeAdapter(const std::string& adapter_name,
                                              base::OnceClosure callback) {
  GetBluetoothFakeAdapterSetter().Set(adapter_name, std::move(callback));
}

void BlinkTestRunner::SetBluetoothManualChooser(bool enable) {
  Send(new BlinkTestHostMsg_SetBluetoothManualChooser(routing_id(), enable));
}

void BlinkTestRunner::GetBluetoothManualChooserEvents(
    base::OnceCallback<void(const std::vector<std::string>&)> callback) {
  get_bluetooth_events_callbacks_.push_back(std::move(callback));
  Send(new BlinkTestHostMsg_GetBluetoothManualChooserEvents(routing_id()));
}

void BlinkTestRunner::SendBluetoothManualChooserEvent(
    const std::string& event,
    const std::string& argument) {
  Send(new BlinkTestHostMsg_SendBluetoothManualChooserEvent(routing_id(), event,
                                                            argument));
}

void BlinkTestRunner::SetFocus(blink::WebView* web_view, bool focus) {
  RenderView* render_view = RenderView::FromWebView(web_view);
  if (render_view)  // Check whether |web_view| has been already closed.
    SetFocusAndActivate(render_view, focus);
}

void BlinkTestRunner::SetBlockThirdPartyCookies(bool block) {
  Send(new WebTestHostMsg_BlockThirdPartyCookies(routing_id(), block));
}

std::string BlinkTestRunner::PathToLocalResource(const std::string& resource) {
#if defined(OS_WIN)
  if (base::StartsWith(resource, "/tmp/", base::CompareCase::SENSITIVE)) {
    // We want a temp file.
    GURL base_url = net::FilePathToFileURL(test_config_->temp_path);
    return base_url.Resolve(resource.substr(sizeof("/tmp/") - 1)).spec();
  }
#endif

  // Some web tests use file://// which we resolve as a UNC path. Normalize
  // them to just file:///.
  std::string result = resource;
  static const size_t kFileLen = sizeof("file:///") - 1;
  while (base::StartsWith(base::ToLowerASCII(result), "file:////",
                          base::CompareCase::SENSITIVE)) {
    result = result.substr(0, kFileLen) + result.substr(kFileLen + 1);
  }
  return RewriteWebTestsURL(result, false /* is_wpt_mode */).GetString().Utf8();
}

void BlinkTestRunner::SetLocale(const std::string& locale) {
  setlocale(LC_ALL, locale.c_str());
  // Number to string conversions require C locale, regardless of what
  // all the other subsystems are set to.
  setlocale(LC_NUMERIC, "C");
}

void BlinkTestRunner::OnWebTestRuntimeFlagsChanged(
    const base::DictionaryValue& changed_values) {
  // Ignore changes that happen before we got the initial, accumulated
  // web flag changes in either OnReplicateTestConfiguration or
  // OnSetTestConfiguration.
  test_runner::WebTestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  if (!interfaces->TestIsRunning())
    return;

  Send(new WebTestHostMsg_WebTestRuntimeFlagsChanged(changed_values));
}

void BlinkTestRunner::TestFinished() {
  test_runner::WebTestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  // We might get multiple TestFinished calls, ensure to only process the dump
  // once.
  if (!interfaces->TestIsRunning())
    return;
  interfaces->SetTestIsRunning(false);

  // If we're not in the main frame, then ask the browser to redirect the call
  // to the main frame instead.
  if (!is_main_window_ || !render_view()->GetMainRenderFrame()) {
    Send(new WebTestHostMsg_TestFinishedInSecondaryRenderer());
    return;
  }

  // Now we know that we're in the main frame, we should generate dump results.
  // Clean out the lifecycle if needed before capturing the web tree
  // dump and pixels from the compositor.
  auto* web_frame = render_view()->GetWebView()->MainFrame()->ToWebLocalFrame();
  web_frame->FrameWidget()->UpdateAllLifecyclePhases(
      blink::WebWidget::LifecycleUpdateReason::kTest);

  // Initialize a new dump results object which we will populate in the calls
  // below.
  dump_result_ = mojom::WebTestDump::New();

  bool browser_should_dump_back_forward_list =
      interfaces->TestRunner()->ShouldDumpBackForwardList();

  if (interfaces->TestRunner()->ShouldDumpAsAudio()) {
    CaptureLocalAudioDump();

    Send(new WebTestHostMsg_InitiateCaptureDump(
        routing_id(), browser_should_dump_back_forward_list,
        /*browser_should_capture_pixels=*/false));
    return;
  }

  // TODO(vmpstr): Sometimes the web isn't stable, which means that if we
  // just ask the browser to ask us to do a dump, the layout would be
  // different compared to if we do it now. This probably needs to be
  // rebaselined. But for now, just capture a local web first.
  CaptureLocalLayoutDump();

  if (!interfaces->TestRunner()->ShouldGeneratePixelResults()) {
    Send(new WebTestHostMsg_InitiateCaptureDump(
        routing_id(), browser_should_dump_back_forward_list,
        /*browser_should_capture_pixels=*/false));
    return;
  }

  if (interfaces->TestRunner()->CanDumpPixelsFromRenderer()) {
    // This does the capture in the renderer when possible, otherwise
    // we will ask the browser to initiate it.
    CaptureLocalPixelsDump();
  } else {
    // If the browser should capture pixels, then we shouldn't be waiting
    // for layout dump results. Any test can only require the browser to
    // dump one or the other at this time.
    DCHECK(!waiting_for_layout_dump_results_);
    if (interfaces->TestRunner()->ShouldDumpSelectionRect()) {
      dump_result_->selection_rect =
          web_frame->GetSelectionBoundsRectForTesting();
    }
  }
  Send(new WebTestHostMsg_InitiateCaptureDump(
      routing_id(), browser_should_dump_back_forward_list,
      !interfaces->TestRunner()->CanDumpPixelsFromRenderer()));
}

void BlinkTestRunner::CaptureLocalAudioDump() {
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureLocalAudioDump");
  test_runner::WebTestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  dump_result_->audio.emplace();
  interfaces->TestRunner()->GetAudioData(&*dump_result_->audio);
}

void BlinkTestRunner::CaptureLocalLayoutDump() {
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureLocalLayoutDump");
  test_runner::WebTestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  std::string layout;
  if (interfaces->TestRunner()->HasCustomTextDump(&layout)) {
    dump_result_->layout.emplace(layout + "\n");
  } else if (!interfaces->TestRunner()->IsRecursiveLayoutDumpRequested()) {
    dump_result_->layout.emplace(interfaces->TestRunner()->DumpLayout(
        render_view()->GetMainRenderFrame()->GetWebFrame()));
  } else {
    // TODO(vmpstr): Since CaptureDump is called from the browser, we can be
    // smart and move this logic directly to the browser.
    waiting_for_layout_dump_results_ = true;
    Send(new BlinkTestHostMsg_InitiateLayoutDump(routing_id()));
  }
}

void BlinkTestRunner::CaptureLocalPixelsDump() {
  TRACE_EVENT0("shell", "BlinkTestRunner::CaptureLocalPixelsDump");

  // Test finish should only be processed in the BlinkTestRunner associated
  // with the current, non-swapped-out RenderView.
  DCHECK(render_view()->GetWebView()->MainFrame()->IsWebLocalFrame());

  waiting_for_pixels_dump_result_ = true;

  test_runner::WebTestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  interfaces->TestRunner()->DumpPixelsAsync(
      render_view(), base::BindOnce(&BlinkTestRunner::OnPixelsDumpCompleted,
                                    base::Unretained(this)));
}

void BlinkTestRunner::OnLayoutDumpCompleted(std::string completed_layout_dump) {
  CHECK(waiting_for_layout_dump_results_);
  dump_result_->layout.emplace(completed_layout_dump);
  waiting_for_layout_dump_results_ = false;
  CaptureDumpComplete();
}

void BlinkTestRunner::OnPixelsDumpCompleted(const SkBitmap& snapshot) {
  CHECK(waiting_for_pixels_dump_result_);
  DCHECK_NE(0, snapshot.info().width());
  DCHECK_NE(0, snapshot.info().height());

  // The snapshot arrives from the GPU process via shared memory. Because MSan
  // can't track initializedness across processes, we must assure it that the
  // pixels are in fact initialized.
  MSAN_UNPOISON(snapshot.getPixels(), snapshot.computeByteSize());
  base::MD5Digest digest;
  base::MD5Sum(snapshot.getPixels(), snapshot.computeByteSize(), &digest);
  std::string actual_pixel_hash = base::MD5DigestToBase16(digest);

  dump_result_->actual_pixel_hash = actual_pixel_hash;
  if (actual_pixel_hash != test_config_->expected_pixel_hash)
    dump_result_->pixels = snapshot;

  waiting_for_pixels_dump_result_ = false;
  CaptureDumpComplete();
}

void BlinkTestRunner::CaptureDumpComplete() {
  // Abort if we're still waiting for some results.
  if (waiting_for_layout_dump_results_ || waiting_for_pixels_dump_result_)
    return;

  // Abort if the browser didn't ask us for the dump yet.
  if (!dump_callback_)
    return;

  std::move(dump_callback_).Run(std::move(dump_result_));
}

void BlinkTestRunner::CloseRemainingWindows() {
  Send(new BlinkTestHostMsg_CloseRemainingWindows(routing_id()));
}

void BlinkTestRunner::DeleteAllCookies() {
  Send(new WebTestHostMsg_DeleteAllCookies(routing_id()));
}

int BlinkTestRunner::NavigationEntryCount() {
  return GetLocalSessionHistoryLength(render_view());
}

void BlinkTestRunner::GoToOffset(int offset) {
  Send(new BlinkTestHostMsg_GoToOffset(routing_id(), offset));
}

void BlinkTestRunner::Reload() {
  Send(new BlinkTestHostMsg_Reload(routing_id()));
}

void BlinkTestRunner::LoadURLForFrame(const WebURL& url,
                                      const std::string& frame_name) {
  Send(new BlinkTestHostMsg_LoadURLForFrame(routing_id(), url, frame_name));
}

bool BlinkTestRunner::AllowExternalPages() {
  return test_config_->allow_external_pages;
}

void BlinkTestRunner::FetchManifest(
    blink::WebView* view,
    base::OnceCallback<void(const blink::WebURL&, const blink::Manifest&)>
        callback) {
  ::content::FetchManifest(view, std::move(callback));
}

void BlinkTestRunner::SetPermission(const std::string& name,
                                    const std::string& value,
                                    const GURL& origin,
                                    const GURL& embedding_origin) {
  blink::mojom::PermissionStatus status;
  if (value == "granted") {
    status = blink::mojom::PermissionStatus::GRANTED;
  } else if (value == "prompt") {
    status = blink::mojom::PermissionStatus::ASK;
  } else if (value == "denied") {
    status = blink::mojom::PermissionStatus::DENIED;
  } else {
    NOTREACHED();
    status = blink::mojom::PermissionStatus::DENIED;
  }

  Send(new WebTestHostMsg_SetPermission(routing_id(), name, status, origin,
                                        embedding_origin));
}

void BlinkTestRunner::ResetPermissions() {
  Send(new WebTestHostMsg_ResetPermissions(routing_id()));
}

void BlinkTestRunner::DispatchBeforeInstallPromptEvent(
    const std::vector<std::string>& event_platforms,
    base::OnceCallback<void(bool)> callback) {
  app_banner_service_.reset(new test_runner::AppBannerService());
  render_view()->GetMainRenderFrame()->BindLocalInterface(
      blink::mojom::AppBannerController::Name_,
      app_banner_service_->controller()
          .BindNewPipeAndPassReceiver()
          .PassPipe());
  app_banner_service_->SendBannerPromptRequest(event_platforms,
                                               std::move(callback));
}

void BlinkTestRunner::ResolveBeforeInstallPromptPromise(
    const std::string& platform) {
  if (app_banner_service_) {
    app_banner_service_->ResolvePromise(platform);
    app_banner_service_.reset(nullptr);
  }
}

blink::WebPlugin* BlinkTestRunner::CreatePluginPlaceholder(
    const blink::WebPluginParams& params) {
  if (params.mime_type != "application/x-plugin-placeholder-test")
    return nullptr;

  plugins::PluginPlaceholder* placeholder = new plugins::PluginPlaceholder(
      render_view()->GetMainRenderFrame(), params, "<div>Test content</div>");
  return placeholder->plugin();
}

void BlinkTestRunner::RunIdleTasks(base::OnceClosure callback) {
  SchedulerRunIdleTasks(std::move(callback));
}

void BlinkTestRunner::ForceTextInputStateUpdate(WebLocalFrame* frame) {
  ForceTextInputStateUpdateForRenderFrame(RenderFrame::FromWebFrame(frame));
}

// RenderViewObserver  --------------------------------------------------------

void BlinkTestRunner::DidClearWindowObject(WebLocalFrame* frame) {
  WebTestingSupport::InjectInternalsObject(frame);
}

bool BlinkTestRunner::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BlinkTestRunner, message)
    IPC_MESSAGE_HANDLER(BlinkTestMsg_Reset, OnReset)
    IPC_MESSAGE_HANDLER(BlinkTestMsg_TestFinishedInSecondaryRenderer,
                        OnTestFinishedInSecondaryRenderer)
    IPC_MESSAGE_HANDLER(BlinkTestMsg_ReplyBluetoothManualChooserEvents,
                        OnReplyBluetoothManualChooserEvents)
    IPC_MESSAGE_HANDLER(BlinkTestMsg_LayoutDumpCompleted, OnLayoutDumpCompleted)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

// Public methods - -----------------------------------------------------------

void BlinkTestRunner::Reset(bool for_new_test) {
  prefs_.Reset();
  waiting_for_reset_ = false;

  WebFrame* main_frame = render_view()->GetWebView()->MainFrame();

  render_view()->ClearEditCommands();
  if (for_new_test) {
    if (main_frame->IsWebLocalFrame()) {
      WebLocalFrame* local_main_frame = main_frame->ToWebLocalFrame();
      local_main_frame->SetName(WebString());
      GetWebWidgetTestProxy(local_main_frame)->EndSyntheticGestures();
    }
    main_frame->ClearOpener();
  }

  // Resetting the internals object also overrides the WebPreferences, so we
  // have to sync them to WebKit again.
  if (main_frame->IsWebLocalFrame()) {
    WebTestingSupport::ResetInternalsObject(main_frame->ToWebLocalFrame());
    render_view()->SetWebkitPreferences(render_view()->GetWebkitPreferences());
  }
}

void BlinkTestRunner::CaptureDump(
    mojom::WebTestControl::CaptureDumpCallback callback) {
  // TODO(vmpstr): This is only called on the main frame. One suggestion is to
  // split the interface on which this call lives so that it is only accessible
  // to the main frame (as opposed to all frames).
  DCHECK(is_main_window_ && render_view()->GetMainRenderFrame());

  dump_callback_ = std::move(callback);
  CaptureDumpComplete();
}

void BlinkTestRunner::DidCommitNavigationInMainFrame() {
  WebFrame* main_frame = render_view()->GetWebView()->MainFrame();
  if (!waiting_for_reset_ || !main_frame->IsWebLocalFrame())
    return;
  GURL url = main_frame->ToWebLocalFrame()->GetDocumentLoader()->GetUrl();
  if (!url.IsAboutBlank())
    return;
  waiting_for_reset_ = false;
  Send(new BlinkTestHostMsg_ResetDone(routing_id()));
}

// Private methods  -----------------------------------------------------------

mojom::WebTestBluetoothFakeAdapterSetter&
BlinkTestRunner::GetBluetoothFakeAdapterSetter() {
  if (!bluetooth_fake_adapter_setter_) {
    RenderThread::Get()->BindHostReceiver(
        bluetooth_fake_adapter_setter_.BindNewPipeAndPassReceiver());
  }
  return *bluetooth_fake_adapter_setter_;
}

void BlinkTestRunner::OnSetupSecondaryRenderer() {
  DCHECK(!is_main_window_);

  test_runner::WebTestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  interfaces->SetTestIsRunning(true);
  ForceResizeRenderView(render_view(), WebSize(800, 600));
}

void BlinkTestRunner::ApplyTestConfiguration(
    mojom::ShellTestConfigurationPtr params) {
  test_runner::WebTestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();

  test_config_ = params.Clone();

  is_main_window_ = true;
  interfaces->SetMainView(render_view()->GetWebView());

  interfaces->SetTestIsRunning(true);
  interfaces->ConfigureForTestWithURL(params->test_url, params->protocol_mode);
}

void BlinkTestRunner::OnReplicateTestConfiguration(
    mojom::ShellTestConfigurationPtr params) {
  ApplyTestConfiguration(std::move(params));
}

void BlinkTestRunner::OnSetTestConfiguration(
    mojom::ShellTestConfigurationPtr params) {
  mojom::ShellTestConfigurationPtr local_params = params.Clone();
  ApplyTestConfiguration(std::move(params));

  ForceResizeRenderView(render_view(),
                        WebSize(local_params->initial_size.width(),
                                local_params->initial_size.height()));

  WebTestRenderThreadObserver::GetInstance()
      ->test_interfaces()
      ->TestRunner()
      ->SetFocus(render_view()->GetWebView(), true);
}

void BlinkTestRunner::OnReset() {
  // BlinkTestMsg_Reset should always be sent to the *current* view.
  DCHECK(render_view()->GetWebView()->MainFrame()->IsWebLocalFrame());
  WebLocalFrame* main_frame =
      render_view()->GetWebView()->MainFrame()->ToWebLocalFrame();

  WebTestRenderThreadObserver::GetInstance()->test_interfaces()->ResetAll();
  Reset(true /* for_new_test */);
  // Navigating to about:blank will make sure that no new loads are initiated
  // by the renderer.
  waiting_for_reset_ = true;

  auto request = blink::WebURLRequest(GURL(url::kAboutBlankURL));
  request.SetMode(network::mojom::RequestMode::kNavigate);
  request.SetRedirectMode(network::mojom::RedirectMode::kManual);
  request.SetRequestContext(blink::mojom::RequestContextType::INTERNAL);
  request.SetRequestorOrigin(blink::WebSecurityOrigin::CreateUniqueOpaque());
  main_frame->StartNavigation(request);
}

void BlinkTestRunner::OnTestFinishedInSecondaryRenderer() {
  DCHECK(is_main_window_ && render_view()->GetMainRenderFrame());

  // Avoid a situation where TestFinished is called twice, because
  // of a racey test finish in 2 secondary renderers.
  test_runner::WebTestInterfaces* interfaces =
      WebTestRenderThreadObserver::GetInstance()->test_interfaces();
  if (!interfaces->TestIsRunning())
    return;

  TestFinished();
}

void BlinkTestRunner::OnReplyBluetoothManualChooserEvents(
    const std::vector<std::string>& events) {
  DCHECK(!get_bluetooth_events_callbacks_.empty());
  base::OnceCallback<void(const std::vector<std::string>&)> callback =
      std::move(get_bluetooth_events_callbacks_.front());
  get_bluetooth_events_callbacks_.pop_front();
  std::move(callback).Run(events);
}

void BlinkTestRunner::OnDestruct() {
  delete this;
}

scoped_refptr<base::SingleThreadTaskRunner> BlinkTestRunner::GetTaskRunner() {
  if (render_view()->GetWebView()->MainFrame()->IsWebLocalFrame()) {
    WebLocalFrame* main_frame =
        render_view()->GetWebView()->MainFrame()->ToWebLocalFrame();
    return main_frame->GetTaskRunner(blink::TaskType::kInternalTest);
  }
  return blink::scheduler::GetSingleThreadTaskRunnerForTesting();
}

}  // namespace content
