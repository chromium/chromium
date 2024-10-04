// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/341324165): Fix and remove.
#pragma allow_unsafe_buffers
#endif

#include "content/web_test/browser/web_test_control_host.h"

#include <stddef.h>
#include <string.h>

#include <iostream>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/paint/skia_paint_canvas.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/custom_handlers/simple_protocol_handler_registry_factory.h"
#include "content/browser/aggregation_service/aggregation_service.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/in_memory_federated_permission_context.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/content_index_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/blink_test_browser_support.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_content_index_provider.h"
#include "content/shell/browser/shell_devtools_frontend.h"
#include "content/test/mock_platform_notification_service.h"
#include "content/test/storage_partition_test_helpers.h"
#include "content/web_test/browser/devtools_protocol_test_bindings.h"
#include "content/web_test/browser/fake_bluetooth_chooser.h"
#include "content/web_test/browser/test_info_extractor.h"
#include "content/web_test/browser/web_test_bluetooth_chooser_factory.h"
#include "content/web_test/browser/web_test_browser_context.h"
#include "content/web_test/browser/web_test_content_browser_client.h"
#include "content/web_test/browser/web_test_devtools_bindings.h"
#include "content/web_test/browser/web_test_first_device_bluetooth_chooser.h"
#include "content/web_test/browser/web_test_permission_manager.h"
#include "content/web_test/browser/web_test_pressure_manager.h"
#include "content/web_test/common/web_test_constants.h"
#include "content/web_test/common/web_test_string_util.h"
#include "content/web_test/common/web_test_switches.h"
#include "ipc/ipc_channel_proxy.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "services/device/public/cpp/compute_pressure/buildflags.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/file_system/isolated_context.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/page_state/page_state.h"
#include "third_party/blink/public/common/page_state/page_state_serialization.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/common/unique_name/unique_name_helper.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"
#include "url/gurl.h"
#include "url/url_constants.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/foundation_util.h"
#endif

namespace content {

namespace {

// The URL used in between two web tests.
const char kAboutBlankResetWebTest[] = "about:blank?reset-web-test";

std::string DumpFrameState(const blink::ExplodedFrameState& frame_state,
                           size_t indent,
                           bool is_current_index) {
  std::string result;
  if (is_current_index) {
    constexpr const char kCurrentMarker[] = "curr->";
    result.append(kCurrentMarker);
    result.append(indent - strlen(kCurrentMarker), ' ');
  } else {
    result.append(indent, ' ');
  }

  std::string url = web_test_string_util::NormalizeWebTestURLForTextOutput(
      base::UTF16ToUTF8(frame_state.url_string.value_or(std::u16string())));
  result.append(url);
  if (frame_state.target && !frame_state.target->empty()) {
    std::string unique_name = base::UTF16ToUTF8(*frame_state.target);
    result.append(" (in frame \"");
    result.append(
        blink::UniqueNameHelper::ExtractStableNameForTesting(unique_name));
    result.append("\")");
  }
  result.append("\n");

  std::vector<blink::ExplodedFrameState> sorted_children = frame_state.children;
  std::sort(sorted_children.begin(), sorted_children.end(),
            [](const blink::ExplodedFrameState& lhs,
               const blink::ExplodedFrameState& rhs) {
              // Child nodes should always have a target (aka unique name).
              DCHECK(lhs.target);
              DCHECK(rhs.target);
              std::string lhs_name =
                  blink::UniqueNameHelper::ExtractStableNameForTesting(
                      base::UTF16ToUTF8(*lhs.target));
              std::string rhs_name =
                  blink::UniqueNameHelper::ExtractStableNameForTesting(
                      base::UTF16ToUTF8(*rhs.target));
              if (!base::EqualsCaseInsensitiveASCII(lhs_name, rhs_name))
                return base::CompareCaseInsensitiveASCII(lhs_name, rhs_name) <
                       0;

              return lhs.item_sequence_number < rhs.item_sequence_number;
            });
  for (const auto& child : sorted_children)
    result += DumpFrameState(child, indent + 4, false);

  return result;
}

std::string DumpNavigationEntry(NavigationEntry* navigation_entry,
                                bool is_current_index) {
  // This is silly, but it's currently the best way to extract the information.
  blink::PageState page_state = navigation_entry->GetPageState();
  blink::ExplodedPageState exploded_page_state;
  CHECK(
      blink::DecodePageState(page_state.ToEncodedData(), &exploded_page_state));
  return DumpFrameState(exploded_page_state.top, 8, is_current_index);
}

std::string DumpHistoryForWebContents(WebContents* web_contents) {
  std::string result;
  const int current_index =
      web_contents->GetController().GetCurrentEntryIndex();
  for (int i = 0; i < web_contents->GetController().GetEntryCount(); ++i) {
    result += DumpNavigationEntry(
        web_contents->GetController().GetEntryAtIndex(i), i == current_index);
  }
  return result;
}

std::vector<std::string> DumpTitleWasSet(WebContents* web_contents) {
  WebTestControlHost* control_host = WebTestControlHost::Get();
  bool load =
      control_host->web_test_runtime_flags().dump_frame_load_callbacks();

  bool title_changed =
      control_host->web_test_runtime_flags().dump_title_changes();

  std::vector<std::string> logs;

  if (load) {
    // TitleWasSet is only available on top-level frames.
    std::string log = "main frame";
    logs.emplace_back(
        log + " - TitleWasSet: " + base::UTF16ToUTF8(web_contents->GetTitle()));
  }

  if (title_changed) {
    logs.emplace_back("TITLE CHANGED: '" +
                      base::UTF16ToUTF8(web_contents->GetTitle()) + "'");
  }
  return logs;
}

std::string DumpFailLoad(WebContents* web_contents,
                         RenderFrameHost* render_frame_host) {
  WebTestControlHost* control_host = WebTestControlHost::Get();
  bool result =
      control_host->web_test_runtime_flags().dump_frame_load_callbacks();

  if (!result)
    return std::string();

  std::string log = (web_contents->GetPrimaryMainFrame() == render_frame_host)
                        ? "main frame "
                        : "frame ";
  std::string name = GetFrameNameFromBrowserForWebTests(render_frame_host);
  log += !name.empty() ? "\"" + name + "\"" : "(anonymous)";
  return log + " - DidFailLoad";
}

// Draws a selection rect into a bitmap.
void DrawSelectionRect(const SkBitmap& bitmap, const gfx::Rect& wr) {
  // Render a red rectangle bounding selection rect
  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setColor(0xFFFF0000);  // Fully opaque red
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  flags.setStrokeWidth(1.0f);
  SkIRect rect;  // Bounding rect
  rect.setXYWH(wr.x(), wr.y(), wr.width(), wr.height());
  canvas.drawIRect(rect, flags);
}

// Applies settings that differ between web tests and regular mode. Some
// of the defaults are controlled via command line flags which are
// automatically set for web tests.
void ApplyWebTestDefaultPreferences(blink::web_pref::WebPreferences* prefs) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  prefs->allow_universal_access_from_file_urls = false;
  prefs->dom_paste_enabled = true;
  prefs->javascript_can_access_clipboard = true;
  prefs->tabs_to_links = false;
  prefs->hyperlink_auditing_enabled = false;
  prefs->allow_running_insecure_content = false;
  prefs->disable_reading_from_canvas = false;
  prefs->strict_mixed_content_checking = false;
  prefs->strict_powerful_feature_restrictions = false;
  prefs->webgl_errors_to_console_enabled = false;
  prefs->enable_scroll_animator =
      !command_line.HasSwitch(switches::kDisableSmoothScrolling);
  prefs->minimum_logical_font_size = 9;
  prefs->accelerated_2d_canvas_enabled =
      command_line.HasSwitch(switches::kEnableAccelerated2DCanvas);
  prefs->smart_insert_delete_enabled = true;
  // On iOS platform, kEnableViewport is enabled by default (see
  // content_main.cc::RunContentProcess). When the viewport is enabled,
  // the visual viewport always provides its own scrollbars even if
  // the test includes <body style=overflow:hidden>.
  // To ensure the testing expectations are consistent with MacPort
  // and to avoid this scrollbar behavior in web-platform-tests,
  // set viewport_enabled to false for iOS.
#if BUILDFLAG(IS_IOS)
  prefs->viewport_enabled = false;
#else
  prefs->viewport_enabled = command_line.HasSwitch(switches::kEnableViewport);
#endif
  prefs->default_minimum_page_scale_factor = 1.f;
  prefs->default_maximum_page_scale_factor = 4.f;
  prefs->presentation_receiver =
      command_line.HasSwitch(switches::kForcePresentationReceiverForTesting);
  prefs->translate_service_available = true;

#if BUILDFLAG(IS_MAC)
  prefs->editing_behavior = blink::mojom::EditingBehavior::kEditingMacBehavior;
#else
  prefs->editing_behavior =
      blink::mojom::EditingBehavior::kEditingWindowsBehavior;
#endif

#if BUILDFLAG(IS_APPLE)
  prefs->cursive_font_family_map[blink::web_pref::kCommonScript] =
      u"Apple Chancery";
  prefs->fantasy_font_family_map[blink::web_pref::kCommonScript] = u"Papyrus";
  prefs->serif_font_family_map[blink::web_pref::kCommonScript] = u"Times";
  prefs->standard_font_family_map[blink::web_pref::kCommonScript] = u"Times";
  prefs->fixed_font_family_map[blink::web_pref::kCommonScript] = u"Menlo";
#else
  prefs->cursive_font_family_map[blink::web_pref::kCommonScript] =
      u"Comic Sans MS";
  prefs->fantasy_font_family_map[blink::web_pref::kCommonScript] = u"Impact";
  prefs->serif_font_family_map[blink::web_pref::kCommonScript] =
      u"times new roman";
  prefs->standard_font_family_map[blink::web_pref::kCommonScript] =
      u"times new roman";
  prefs->fixed_font_family_map[blink::web_pref::kCommonScript] = u"Courier";
#endif
  prefs->sans_serif_font_family_map[blink::web_pref::kCommonScript] =
      u"Helvetica";
}

}  // namespace

// WebTestResultPrinter ----------------------------------------------------

WebTestResultPrinter::WebTestResultPrinter(std::ostream* output,
                                           std::ostream* error)
    : output_(output), error_(error) {}

void WebTestResultPrinter::StartStateDump() {
  state_ = DURING_STATE_DUMP;
}

void WebTestResultPrinter::PrintTextHeader() {
  if (state_ != DURING_STATE_DUMP)
    return;
  if (!capture_text_only_)
    *output_ << "Content-Type: text/plain\n";
  state_ = IN_TEXT_BLOCK;
}

void WebTestResultPrinter::PrintTextBlock(const std::string& block) {
  if (state_ != IN_TEXT_BLOCK)
    return;
  *output_ << block;
}

void WebTestResultPrinter::PrintTextFooter() {
  if (state_ != IN_TEXT_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    output_->flush();
  }
  state_ = IN_IMAGE_BLOCK;
}

void WebTestResultPrinter::PrintImageHeader(const std::string& actual_hash,
                                            const std::string& expected_hash) {
  if (state_ != IN_IMAGE_BLOCK || capture_text_only_)
    return;
  *output_ << "\nActualHash: " << actual_hash << "\n";
  if (!expected_hash.empty())
    *output_ << "\nExpectedHash: " << expected_hash << "\n";
}

void WebTestResultPrinter::PrintImageBlock(
    const std::vector<unsigned char>& png_image) {
  if (state_ != IN_IMAGE_BLOCK || capture_text_only_)
    return;
  *output_ << "Content-Type: image/png\n";
  if (encode_binary_data_) {
    PrintEncodedBinaryData(png_image);
    return;
  }

  *output_ << "Content-Length: " << png_image.size() << "\n";
  output_->write(reinterpret_cast<const char*>(&png_image[0]),
                 png_image.size());
}

void WebTestResultPrinter::PrintImageFooter() {
  if (state_ != IN_IMAGE_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    output_->flush();
  }
  state_ = AFTER_TEST;
}

void WebTestResultPrinter::PrintAudioHeader() {
  DCHECK_EQ(state_, DURING_STATE_DUMP);
  if (!capture_text_only_)
    *output_ << "Content-Type: audio/wav\n";
  state_ = IN_AUDIO_BLOCK;
}

void WebTestResultPrinter::PrintAudioBlock(
    const std::vector<unsigned char>& audio_data) {
  if (state_ != IN_AUDIO_BLOCK || capture_text_only_)
    return;
  if (encode_binary_data_) {
    PrintEncodedBinaryData(audio_data);
    return;
  }

  *output_ << "Content-Length: " << audio_data.size() << "\n";
  output_->write(reinterpret_cast<const char*>(&audio_data[0]),
                 audio_data.size());
}

void WebTestResultPrinter::PrintAudioFooter() {
  if (state_ != IN_AUDIO_BLOCK)
    return;
  if (!capture_text_only_) {
    *output_ << "#EOF\n";
    output_->flush();
  }
  state_ = IN_IMAGE_BLOCK;
}

void WebTestResultPrinter::AddMessageToStderr(const std::string& message) {
  *error_ << message;
}

void WebTestResultPrinter::AddMessage(const std::string& message) {
  AddMessageRaw(message + "\n");
}

void WebTestResultPrinter::AddMessageRaw(const std::string& message) {
  if (state_ != DURING_TEST)
    return;
  *output_ << message;
}

void WebTestResultPrinter::AddErrorMessage(const std::string& message) {
  if (!capture_text_only_)
    *error_ << message << "\n";
  if (state_ != DURING_TEST && state_ != DURING_STATE_DUMP)
    return;
  PrintTextHeader();
  *output_ << message << "\n";
  PrintTextFooter();
  PrintImageFooter();
}

void WebTestResultPrinter::PrintEncodedBinaryData(
    const std::vector<unsigned char>& data) {
  *output_ << "Content-Transfer-Encoding: base64\n";

  std::string data_base64 = base::Base64Encode(
      std::string_view(reinterpret_cast<const char*>(&data[0]), data.size()));

  *output_ << "Content-Length: " << data_base64.length() << "\n";
  output_->write(data_base64.c_str(), data_base64.length());
}

void WebTestResultPrinter::CloseStderr() {
  if (state_ != AFTER_TEST)
    return;
  if (!capture_text_only_) {
    *error_ << "#EOF\n";
    error_->flush();
  }
}

// WebTestWindowObserver -----------------------------------------------------

class WebTestControlHost::WebTestWindowObserver : WebContentsObserver {
 public:
  WebTestWindowObserver(WebContents* web_contents,
                        WebTestControlHost* web_test_control)
      : WebContentsObserver(web_contents), web_test_control_(web_test_control) {
    // If the WebContents was already set up before given to the Shell, it may
    // have a set of RenderFrames already, and we need to notify about them
    // here.
    web_contents->ForEachRenderFrameHost(
        [&](RenderFrameHost* render_frame_host) {
          if (render_frame_host->IsRenderFrameLive())
            RenderFrameCreated(render_frame_host);
        });
  }

 private:
  void WebContentsDestroyed() override {
    // Deletes |this| and removes the pointer to it from WebTestControlHost.
    web_test_control_->test_opened_window_observers_.erase(web_contents());
  }

  void RenderFrameCreated(RenderFrameHost* render_frame_host) override {
    web_test_control_->HandleNewRenderFrameHost(render_frame_host);
  }

  const raw_ptr<WebTestControlHost> web_test_control_;
};

// WebTestControlHost -------------------------------------------------------

WebTestControlHost* WebTestControlHost::instance_ = nullptr;

// static
WebTestControlHost* WebTestControlHost::Get() {
  return instance_;
}

WebTestControlHost::WebTestControlHost() {
  CHECK(!instance_);
  instance_ = this;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLeakDetection)) {
    leak_detector_ = std::make_unique<LeakDetector>();
    std::string switchValue =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kEnableLeakDetection);
    crash_when_leak_found_ = switchValue == switches::kCrashOnFailure;
  }

  printer_ = std::make_unique<WebTestResultPrinter>(&std::cout, &std::cerr);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEncodeBinary))
    printer_->set_encode_binary_data(true);

  // Print text only (without binary dumps and headers/footers for run_web_tests
  // protocol) until we enter the protocol mode (see TestInfo::protocol_mode).
  printer_->set_capture_text_only(true);

  InjectTestSharedWorkerService(ShellContentBrowserClient::Get()
                                    ->browser_context()
                                    ->GetDefaultStoragePartition());

  GpuDataManager::GetInstance()->AddObserver(this);
}

WebTestControlHost::~WebTestControlHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(instance_ == this);
  CHECK(test_phase_ == BETWEEN_TESTS);
  GpuDataManager::GetInstance()->RemoveObserver(this);
  instance_ = nullptr;
  // The |main_window_| and |secondary_window_| are leaked here, but the
  // WebTestBrowserMainRunner will close all Shell windows including those.
}

void WebTestControlHost::PrepareForWebTest(const TestInfo& test_info) {
  TRACE_EVENT0("shell", "WebTestControlHost::PrepareForWebTest");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  current_working_directory_ = test_info.current_working_directory;
  expected_pixel_hash_ = test_info.expected_pixel_hash;
  wpt_print_mode_ = test_info.wpt_print_mode;
  bool is_devtools_js_test = false;
  test_url_ = WebTestDevToolsBindings::MapTestURLIfNeeded(test_info.url,
                                                          &is_devtools_js_test);
  bool is_devtools_protocol_test = false;
  test_url_ = DevToolsProtocolTestBindings::MapTestURLIfNeeded(
      test_url_, &is_devtools_protocol_test);

  protocol_mode_ = test_info.protocol_mode;
  if (protocol_mode_)
    printer_->set_capture_text_only(false);
  printer_->reset();

  accumulated_web_test_runtime_flags_changes_.clear();
  web_test_runtime_flags_.Reset();
  main_window_render_view_hosts_.clear();
  main_window_render_process_hosts_.clear();
  all_observed_render_process_hosts_.clear();
  render_process_host_observations_.RemoveAllObservations();
  frame_to_layout_dump_map_.clear();

  if (!test_info.trace_file.empty()) {
    tracing_controller_.emplace(test_info.trace_file);
    tracing_controller_->StartTracing();
  }

  ShellBrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();

  browser_context->GetClientHintsControllerDelegate()->ResetForTesting();

  const gfx::Size window_size = Shell::GetShellDefaultSize();

  if (!main_window_) {
    TRACE_EVENT0("shell",
                 "WebTestControlHost::PrepareForWebTest::CreateMainWindow");
    main_window_ = content::Shell::CreateNewWindow(
        browser_context, GURL(url::kAboutBlankURL), nullptr, window_size);
    WebContentsObserver::Observe(main_window_->web_contents());

    default_prefs_ = main_window_->web_contents()->GetOrCreateWebPreferences();
    default_accept_languages_ = main_window_->web_contents()
                                    ->GetMutableRendererPrefs()
                                    ->accept_languages;
  } else {
    // Set a different size first to reset the possibly inconsistent state
    // caused by the previous test using unfortunate synchronous resize mode.
    // This forces SetSize() not to early return which would otherwise happen
    // when we set the size to |window_size| which is the same as its current
    // size. See http://crbug.com/1011191 for more details.
    // TODO(crbug.com/41067256): This resize to half-size could go away if
    // testRunner.useUnfortunateSynchronousResizeMode() goes away.
    main_window_->web_contents()->GetRenderWidgetHostView()->DisableAutoResize(
        gfx::Size());
    main_window_->ResizeWebContentForTests(
        gfx::ScaleToCeiledSize(window_size, 0.5f, 1));
    main_window_->ResizeWebContentForTests(window_size);

    SetAcceptLanguages(default_accept_languages_);

    main_window_->web_contents()->SetWebPreferences(default_prefs_);

    main_window_->web_contents()->WasShown();
  }

  // Tests should always start with the browser controls hidden.
  // TODO(danakj): We no longer run web tests on android, and this is an android
  // feature, so maybe this isn't needed anymore.
  main_window_->web_contents()->UpdateBrowserControlsState(
      cc::BrowserControlsState::kBoth, cc::BrowserControlsState::kHidden, false,
      std::nullopt);

  // We did not track the |main_window_| RenderFrameHost during the creation of
  // |main_window_|, since we need the pointer value in this class set first. So
  // we update the |test_phase_| here allowing us to now track the RenderFrames
  // in that window, and call HandleNewRenderFrameHost() explicitly.
  test_phase_ = DURING_TEST;
  HandleNewRenderFrameHost(main_window_->web_contents()->GetPrimaryMainFrame());

  if (is_devtools_protocol_test) {
    std::string log;
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kInspectorProtocolLog)) {
      base::FilePath log_path =
          base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
              switches::kInspectorProtocolLog);
      base::ScopedAllowBlockingForTesting allow_blocking;
      if (!base::ReadFileToString(log_path, &log)) {
        printer_->AddErrorMessage(base::StringPrintf(
            "FAIL: Failed to read the inspector-protocol-log file %s",
            log_path.AsUTF8Unsafe().c_str()));
      }
    }

    devtools_protocol_test_bindings_ =
        std::make_unique<DevToolsProtocolTestBindings>(
            main_window_->web_contents(), log);
  }

  // We don't go down the normal system path of focusing RenderWidgetHostView
  // because on mac headless, there are no system windows and that path does
  // not do anything. Instead we go through the Shell::ActivateContents() path
  // which knows how to perform the activation correctly on all platforms and in
  // headless mode.
  main_window_->ActivateContents(main_window_->web_contents());

  RenderViewHost* main_render_view_host =
      main_window_->web_contents()->GetPrimaryMainFrame()->GetRenderViewHost();
  {
    TRACE_EVENT0("shell", "WebTestControlHost::PrepareForWebTest::Flush");
    // Round-trip through the InputHandler mojom interface to the compositor
    // thread, in order to ensure that any input events (moving the mouse at the
    // start of the test, focus coming from ActivateContents() above, etc) are
    // handled and bounced if appropriate to the main thread, before we continue
    // and start the test. This will ensure they are handled on the main thread
    // before the test runs, which would otherwise race against them.
    main_render_view_host->GetWidget()->FlushForTesting();
  }

  if (is_devtools_js_test) {
    secondary_window_ = content::Shell::CreateNewWindow(
        ShellContentBrowserClient::Get()->browser_context(),
        GURL(url::kAboutBlankURL), nullptr, window_size);
    // This navigates the secondary (devtools inspector) window, and then
    // navigates the main window once that has loaded to a devtools html test
    // page, based on the test url.
    devtools_bindings_ = std::make_unique<WebTestDevToolsBindings>(
        main_window_->web_contents(), secondary_window_->web_contents(),
        test_url_);
  } else {
    // Loading the URL will immediately start the web test. Manually call
    // LoadURLWithParams on the WebContents to avoid extraneous calls from
    // content::Shell such as SetFocus(), which could race with the web
    // test.
    NavigationController::LoadURLParams params(test_url_);

    // Using PAGE_TRANSITION_TYPED replicates an omnibox navigation.
    params.transition_type =
        ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED);

    // Clear history to purge the prior navigation to about:blank.
    params.should_clear_history_list = true;
    main_window_->web_contents()->GetController().LoadURLWithParams(params);
  }
}

void WebTestControlHost::ResetBrowserAfterWebTest() {
  TRACE_EVENT0("shell", "WebTestControlHost::ResetBrowserAfterWebTest");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Close any windows opened by the test to avoid them polluting the next
  // test.
  CloseTestOpenedWindows();

  // Close IPC channels to avoid unexpected messages or replies after the test
  // is done. New channels will be opened for the next test.
  web_test_render_frame_map_.clear();
  receiver_bindings_.Clear();

  // StopTracing() must be called before the printer_ calls, to ensure the trace
  // file is flushed to disk before control returns to the test runner
  if (tracing_controller_.has_value()) {
    tracing_controller_->StopTracing();
    tracing_controller_.reset();
  }

  printer_->PrintTextFooter();
  printer_->PrintImageFooter();
  printer_->CloseStderr();
  test_phase_ = BETWEEN_TESTS;
  expected_pixel_hash_.clear();
  test_url_ = GURL();
  prefs_ = blink::web_pref::WebPreferences();
  lcpp_hint_ = std::nullopt;
  should_override_prefs_ = false;
  WebTestContentBrowserClient::Get()->SetPopupBlockingEnabled(true);
  WebTestContentBrowserClient::Get()->ResetMockClipboardHosts();
  WebTestContentBrowserClient::Get()->ResetFakeBluetoothDelegate();
  WebTestContentBrowserClient::Get()->ResetWebSensorProviderAutomation();
  WebTestContentBrowserClient::Get()
      ->GetWebTestBrowserContext()
      ->GetWebTestPermissionManager()
      ->ResetPermissions();
  check_for_leaked_windows_ = false;
  renderer_dump_result_ = nullptr;
  navigation_history_dump_ = "";
  layout_dump_.reset();
  waiting_for_layout_dumps_ = 0;
  pixel_dump_.reset();
  actual_pixel_hash_ = "";
  waiting_for_pixel_results_ = false;
  composite_all_frames_node_queue_ =
      std::queue<raw_ptr<Node, CtnExperimental>>();
  composite_all_frames_node_storage_.clear();
  next_pointer_lock_action_ = NextPointerLockAction::kWillSucceed;

  BlockThirdPartyCookies(false);
  SetBluetoothManualChooser(false);
  SetDatabaseQuota(content::kDefaultDatabaseQuota);

  ShellBrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  static_cast<InMemoryFederatedPermissionContext*>(
      browser_context->GetFederatedIdentityPermissionContext())
      ->ResetForTesting();

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  // Delete any ScopedVirtualPressureSourceForDevTools and
  // WebTestPressureManager instances created by WebTestContentBrowserClient.
  // At this point all other windows have been closed and their WebContents
  // have been destroyed, so we only need to worry about |main_window_|.
  //
  // Note that if other windows were using WebTestPressureManager there might
  // be a race condition between ScopedVirtualPressureSourceForDevTools and
  // WebContentsPressureManagerProxy because both the latter and
  // WebTestPressureManager inherit from WebContentsUserData so their
  // destruction order can vary. This is not a problem though -- in the worst
  // case, some virtual pressure sources will remain valid but unused in
  // //services during content_shell's lifetime.
  if (main_window_) {
    main_window_->web_contents()->RemoveUserData(
        WebTestPressureManager::UserDataKey());
  }
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)

  // Delete all cookies, Attribution Reporting data and Aggregation service data
  {
    StoragePartition* storage_partition =
        browser_context->GetDefaultStoragePartition();
    storage_partition->GetCookieManagerForBrowserProcess()->DeleteCookies(
        network::mojom::CookieDeletionFilter::New(), base::DoNothing());

    if (auto* attribution_manager =
            AttributionManager::FromBrowserContext(browser_context)) {
      attribution_manager->ClearData(
          /*delete_begin=*/base::Time::Min(), /*delete_end=*/base::Time::Max(),
          /*filter=*/StoragePartition::StorageKeyMatcherFunction(),
          /*filter_builder=*/nullptr,
          /*delete_rate_limit_data=*/true,
          /*done=*/base::DoNothing());
    }

    if (auto* aggregation_service =
            AggregationService::GetService(browser_context)) {
      aggregation_service->ClearData(
          /*delete_begin=*/base::Time::Min(), /*delete_end=*/base::Time::Max(),
          /*filter=*/StoragePartition::StorageKeyMatcherFunction(),
          /*done=*/base::DoNothing());
    }
  }

  ui::SelectFileDialog::SetFactory(nullptr);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (writable_directory_for_tests_.IsValid()) {
      if (!writable_directory_for_tests_.Delete())
        LOG(ERROR) << "Failed to delete temporary directory";
    }
  }

  weak_factory_.InvalidateWeakPtrs();
}

void WebTestControlHost::DidCreateOrAttachWebContents(
    WebContents* web_contents) {
  auto result = test_opened_window_observers_.emplace(
      web_contents,
      std::make_unique<WebTestWindowObserver>(web_contents, this));
  CHECK(result.second);  // The WebContents should not already be in the map!
}

void WebTestControlHost::SetTempPath(const base::FilePath& temp_path) {
  temp_path_ = temp_path;
}

void WebTestControlHost::OverrideWebkitPrefs(
    blink::web_pref::WebPreferences* prefs) {
  if (should_override_prefs_) {
    *prefs = prefs_;
  } else {
    ApplyWebTestDefaultPreferences(prefs);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceDarkMode)) {
    prefs->preferred_color_scheme = blink::mojom::PreferredColorScheme::kDark;
  } else {
    prefs->preferred_color_scheme = blink::mojom::PreferredColorScheme::kLight;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceHighContrast)) {
    prefs->in_forced_colors = true;
    prefs->preferred_contrast = blink::mojom::PreferredContrast::kMore;
  } else {
    prefs->in_forced_colors = false;
    prefs->preferred_contrast = blink::mojom::PreferredContrast::kNoPreference;
  }
}

void WebTestControlHost::OpenURL(const GURL& url) {
  if (test_phase_ != DURING_TEST)
    return;

  Shell::CreateNewWindow(main_window_->web_contents()->GetBrowserContext(), url,
                         main_window_->web_contents()->GetSiteInstance(),
                         gfx::Size());
}

void WebTestControlHost::InitiateCaptureDump(
    mojom::WebTestRendererDumpResultPtr renderer_dump_result,
    bool capture_navigation_history,
    bool capture_pixels) {
  if (test_phase_ != DURING_TEST)
    return;

  renderer_dump_result_ = std::move(renderer_dump_result);

  if (capture_navigation_history) {
    for (auto* window : Shell::windows()) {
      WebContents* web_contents = window->web_contents();
      // Only dump the main test window, and windows that it opened. This avoids
      // devtools windows specifically.
      if (window == main_window_ || web_contents->HasOpener()) {
        navigation_history_dump_ +=
            "\n============== Back Forward List ==============\n";
        navigation_history_dump_ += DumpHistoryForWebContents(web_contents);
        navigation_history_dump_ +=
            "===============================================\n";
      }
    }
  }

  // Grab a layout dump if the renderer was not able to provide one.
  if (!renderer_dump_result_->layout) {
    DCHECK_EQ(0, waiting_for_layout_dumps_);

    main_window_->web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
        [&](RenderFrameHost* render_frame_host) {
          if (!render_frame_host->IsRenderFrameLive())
            return;

          ++waiting_for_layout_dumps_;
          GetWebTestRenderFrameRemote(render_frame_host)
              ->DumpFrameLayout(
                  base::BindOnce(&WebTestControlHost::OnDumpFrameLayoutResponse,
                                 weak_factory_.GetWeakPtr(),
                                 render_frame_host->GetFrameTreeNodeId()));
        });
  }

  if (capture_pixels) {
    waiting_for_pixel_results_ = true;
    CompositeAllFramesThen(
        base::BindOnce(&WebTestControlHost::EnqueueSurfaceCopyRequest,
                       weak_factory_.GetWeakPtr()));
  }

  // Try to report results now, if we aren't waiting for anything.
  ReportResults();
}

void WebTestControlHost::TestFinishedInSecondaryRenderer() {
  GetWebTestRenderFrameRemote(
      main_window_->web_contents()->GetPrimaryMainFrame())
      ->TestFinishedFromSecondaryRenderer();
}

// Enqueue an image copy output request.
void WebTestControlHost::EnqueueSurfaceCopyRequest() {
  // Under fuzzing, the renderer may close the |main_window_| while we're
  // capturing test results, as demonstrated by https://crbug.com/1098835.
  // We must handle this bad behaviour.
  if (!main_window_) {
    // DiscardMainWindow has already called OnTestFinished().
    CHECK_EQ(test_phase_, CLEAN_UP);
    return;
  }

  auto* rwhv = main_window_->web_contents()->GetRenderWidgetHostView();
  rwhv->CopyFromSurface(gfx::Rect(), gfx::Size(),
                        base::BindOnce(&WebTestControlHost::OnPixelDumpCaptured,
                                       weak_factory_.GetWeakPtr()));
}

void WebTestControlHost::CompositeAllFramesThen(
    base::OnceCallback<void()> callback) {
  // Only allow a single call to CompositeAllFramesThen(), without a call to
  // ResetBrowserAfterWebTest() in between. More than once risks overlapping
  // calls, due to the asynchronous nature of CompositeNodeQueueThen(), which
  // can lead to use-after-free, e.g. crbug.com/899465.
  if (!composite_all_frames_node_storage_.empty() ||
      !composite_all_frames_node_queue_.empty()) {
    return;
  }
  // Build the frame storage and depth first queue.
  Node* root = BuildFrameTree(main_window_->web_contents());
  BuildDepthFirstQueue(root);
  // Now asynchronously run through the node queue.
  CompositeNodeQueueThen(std::move(callback));
}

void WebTestControlHost::CompositeNodeQueueThen(
    base::OnceCallback<void()> callback) {
  RenderFrameHost* frame = nullptr;
  while (!frame) {
    if (composite_all_frames_node_queue_.empty()) {
      // Done with the queue - call the callback.
      std::move(callback).Run();
      return;
    }

    frame = composite_all_frames_node_queue_.front()->render_frame_host;
    GlobalRenderFrameHostId routing_id =
        composite_all_frames_node_queue_.front()->render_frame_host_id;
    composite_all_frames_node_queue_.pop();

    if (!RenderFrameHost::FromID(routing_id.child_id,
                                 routing_id.frame_routing_id)) {
      // The frame is gone. Frames can get detached by a parent frame during or
      // in between SynchronouslyCompositeAfterTest() calls, after the test
      // claims it has finished. That would be bad test behaviour but the fuzzer
      // can do it. See crbug.com/899465 for an example of this problem.
      frame = nullptr;
    } else if (!frame->IsRenderFrameLive()) {
      // The renderer is gone. Frames can also crash the renderer after the test
      // claims to be finished.
      frame = nullptr;
    } else if (frame->GetParent() &&
               static_cast<SiteInstanceImpl*>(
                   frame->GetParent()->GetSiteInstance())
                       ->group() ==
                   static_cast<SiteInstanceImpl*>(frame->GetSiteInstance())
                       ->group()) {
      // The frame is not a local root, so nothing to do.
      frame = nullptr;
    }
  }

  GetWebTestRenderFrameRemote(frame)->SynchronouslyCompositeAfterTest(
      base::BindOnce(&WebTestControlHost::CompositeNodeQueueThen,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void WebTestControlHost::BuildDepthFirstQueue(Node* node) {
  for (content::WebTestControlHost::Node* child : node->children) {
    BuildDepthFirstQueue(child);
  }
  composite_all_frames_node_queue_.push(node);
}

WebTestControlHost::Node* WebTestControlHost::BuildFrameTree(
    WebContents* web_contents) {
  // Returns a Node for a given RenderFrameHost, or nullptr if doesn't exist.
  auto node_for_frame = [this](RenderFrameHost* rfh) {
    auto it = base::ranges::find(composite_all_frames_node_storage_, rfh,
                                 &Node::render_frame_host);
    return it == composite_all_frames_node_storage_.end() ? nullptr : it->get();
  };

  //  Collect all live frames in web_contents.
  std::vector<RenderFrameHost*> frames;
  web_contents->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&](RenderFrameHost* render_frame_host) {
        if (render_frame_host->IsRenderFrameLive())
          frames.push_back(render_frame_host);
      });

  // Add all of the frames to storage.
  for (auto* frame : frames) {
    DCHECK(!node_for_frame(frame)) << "Frame seen multiple times.";
    composite_all_frames_node_storage_.emplace_back(
        std::make_unique<Node>(frame));
  }

  // Construct a tree rooted at |root|.
  Node* root = nullptr;
  for (auto* frame : frames) {
    Node* node = node_for_frame(frame);
    DCHECK(node);
    if (!frame->GetParentOrOuterDocument()) {
      DCHECK(!root) << "Multiple roots found.";
      root = node;
    } else {
      Node* parent = node_for_frame(frame->GetParentOrOuterDocument());
      DCHECK(parent);
      parent->children.push_back(node);
    }
  }
  DCHECK(root) << "No root found.";

  return root;
}

bool WebTestControlHost::IsMainWindow(WebContents* web_contents) const {
  return main_window_ && web_contents == main_window_->web_contents();
}

std::unique_ptr<BluetoothChooser> WebTestControlHost::RunBluetoothChooser(
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  // TODO(crbug.com/40426301): Remove |bluetooth_chooser_factory_| once
  // all of the Web Bluetooth tests are migrated to external/wpt/.
  if (bluetooth_chooser_factory_) {
    return bluetooth_chooser_factory_->RunBluetoothChooser(frame,
                                                           event_handler);
  }

  auto next_fake_bluetooth_chooser =
      WebTestContentBrowserClient::Get()->GetNextFakeBluetoothChooser();
  if (next_fake_bluetooth_chooser) {
    const url::Origin origin = frame->GetLastCommittedOrigin();
    DCHECK(!origin.opaque());
    next_fake_bluetooth_chooser->OnRunBluetoothChooser(event_handler, origin);
    return next_fake_bluetooth_chooser;
  }

  return std::make_unique<WebTestFirstDeviceBluetoothChooser>(event_handler);
}

void WebTestControlHost::RequestPointerLock(WebContents* web_contents) {
  if (next_pointer_lock_action_ == NextPointerLockAction::kTestWillRespond)
    return;

  web_contents->GotResponseToPointerLockRequest(
      next_pointer_lock_action_ == NextPointerLockAction::kWillSucceed
          ? blink::mojom::PointerLockResult::kSuccess
          : blink::mojom::PointerLockResult::kPermissionDenied);

  next_pointer_lock_action_ = NextPointerLockAction::kWillSucceed;
}

void WebTestControlHost::PluginCrashed(const base::FilePath& plugin_path,
                                       base::ProcessId plugin_pid) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  printer_->AddErrorMessage(
      base::StringPrintf("#CRASHED - plugin (pid %" CrPRIdPid ")", plugin_pid));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(base::IgnoreResult(&WebTestControlHost::DiscardMainWindow),
                     weak_factory_.GetWeakPtr()));
}

void WebTestControlHost::TitleWasSet(NavigationEntry* entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::string> logs = DumpTitleWasSet(main_window_->web_contents());
  if (logs.empty())
    return;
  for (auto log : logs)
    printer_->AddMessage(log);
}

void WebTestControlHost::DidFailLoad(RenderFrameHost* render_frame_host,
                                     const GURL& validated_url,
                                     int error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::string log =
      DumpFailLoad(main_window_->web_contents(), render_frame_host);
  if (log.empty())
    return;
  printer_->AddMessage(log);
}

void WebTestControlHost::WebContentsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  printer_->AddErrorMessage("FAIL: main window was destroyed");
  DiscardMainWindow();
}

void WebTestControlHost::DidUpdateFaviconURL(
    RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  if (web_test_runtime_flags_.dump_icon_changes()) {
    std::string log = IsMainWindow(web_contents()) ? "main frame " : "frame ";
    printer_->AddMessageRaw(log + "- didChangeIcons\n");
  }
}

void WebTestControlHost::RenderFrameHostChanged(RenderFrameHost* old_host,
                                                RenderFrameHost* new_host) {
  if (!old_host || !old_host->IsInPrimaryMainFrame())
    return;

  GetWebTestRenderFrameRemote(old_host)->OnDeactivated();
}

void WebTestControlHost::RenderViewDeleted(RenderViewHost* render_view_host) {
  main_window_render_view_hosts_.erase(render_view_host);
}

void WebTestControlHost::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  if (lcpp_hint_) {
    navigation_handle->SetLCPPNavigationHint(lcpp_hint_.value());
  }
}

void WebTestControlHost::ReadyToCommitNavigation(
    NavigationHandle* navigation_handle) {
  NavigationRequest* request = NavigationRequest::From(navigation_handle);
  RenderFrameHostImpl* rfh =
      request->GetRenderFrameHostRestoredFromBackForwardCache();
  if (rfh)
    GetWebTestRenderFrameRemote(rfh)->OnReactivated();

  if (navigation_handle->IsInPrimaryMainFrame() &&
      next_non_blank_nav_is_new_test_ &&
      navigation_handle->GetURL() != GURL(kAboutBlankResetWebTest)) {
    GetWebTestRenderFrameRemote(navigation_handle->GetRenderFrameHost())
        ->BlockTestUntilStart();
  }
}

void WebTestControlHost::RenderProcessHostDestroyed(
    RenderProcessHost* render_process_host) {
  render_process_host_observations_.RemoveObservation(render_process_host);
  all_observed_render_process_hosts_.erase(render_process_host);
  main_window_render_process_hosts_.erase(render_process_host);
}

void WebTestControlHost::RenderProcessExited(
    RenderProcessHost* render_process_host,
    const ChildProcessTerminationInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (info.status) {
    case base::TerminationStatus::TERMINATION_STATUS_NORMAL_TERMINATION:
    case base::TerminationStatus::TERMINATION_STATUS_STILL_RUNNING:
      break;

    case base::TerminationStatus::TERMINATION_STATUS_ABNORMAL_TERMINATION:
    case base::TerminationStatus::TERMINATION_STATUS_LAUNCH_FAILED:
    case base::TerminationStatus::TERMINATION_STATUS_PROCESS_CRASHED:
    case base::TerminationStatus::TERMINATION_STATUS_PROCESS_WAS_KILLED:
    default: {
      const base::Process& process = render_process_host->GetProcess();
      if (process.IsValid()) {
        printer_->AddErrorMessage(std::string("#CRASHED - renderer (pid ") +
                                  base::NumberToString(process.Pid()) + ")");
      } else {
        printer_->AddErrorMessage("#CRASHED - renderer");
      }

      DiscardMainWindow();
      break;
    }
  }
}

void WebTestControlHost::OnGpuProcessCrashed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  printer_->AddErrorMessage("#CRASHED - gpu");
  DiscardMainWindow();
}

void WebTestControlHost::DiscardMainWindow() {
  // We can get here for 2 different reasons:
  // 1. The |main_window_| Shell is destroying, and the WebContents inside it
  // has been destroyed. Then we dare not call Shell::Close() on the
  // |main_window_|.
  // 2. Some other fatal error has occurred. We can't tell this apart from the
  // Shell destroying, since that is also something a test can do.
  //
  // Since we can't tell at this point if |main_window_| is okay to use, we
  // don't touch it, and we stop observing its WebContents.
  WebContentsObserver::Observe(nullptr);
  main_window_ = nullptr;

  // We don't want to leak any open windows when we finish a test, and the next
  // test will create its own |main_window_|. So at this point we close all
  // Shell windows, to avoid using the potentially-bad pointer.
  CloseAllWindows();

  if (test_phase_ == DURING_TEST) {
    // Then we immediately end the current test instead of timing out. This is
    // like ReportResults() except we report only messages added to the
    // |printer_| and no other test results.
    printer_->StartStateDump();
    printer_->PrintTextHeader();
    printer_->PrintTextFooter();
    OnTestFinished();
  } else {
    // Given that main_window_ is null, this is (at the time of writing)
    // equivalent to calling Shell::QuitMainMessageLoopForTesting(), but it
    // seems cleaner to call it.
    PrepareRendererForNextWebTest();
  }
}

void WebTestControlHost::HandleNewRenderFrameHost(RenderFrameHost* frame) {
  // When creating the main window, we don't have a |main_window_| pointer yet.
  // So we will explicitly call this for the main window after moving  to
  // DURING_TEST.
  if (test_phase_ != DURING_TEST)
    return;

  // Consider a prerender as main window as well since it may be activated to
  // become the main window.
  const bool main_window =
      (FrameTreeNode::From(frame)->frame_tree().is_primary() ||
       FrameTreeNode::From(frame)->frame_tree().is_prerendering()) &&
      WebContents::FromRenderFrameHost(frame) == main_window_->web_contents();

  RenderProcessHost* process_host = frame->GetProcess();
  RenderViewHost* view_host = frame->GetRenderViewHost();

  // If this the first time this renderer contains parts of the main test
  // window, we need to make sure that it gets configured correctly (including
  // letting it know that it's part of the main test window).
  // We consider the renderer as new when we see either a new RenderProcessHost
  // or a new RenderViewHost, as it is possible that a new renderer (with a new
  // RenderViewHost) reuses a renderer process, and it's also possible that we
  // reuse RenderViewHosts (in some fetch tests).
  // TODO(rakina): Understand the fetch tests to figure out if it's possible to
  // remove RenderProcessHost tracking here.
  if (main_window &&
      (!base::Contains(main_window_render_view_hosts_, view_host) ||
       !base::Contains(main_window_render_process_hosts_, process_host))) {
    // When we find the main window's main frame for the first time, we mark the
    // test as starting for the renderer.
    const bool starting_test = main_window_render_process_hosts_.empty();
    DCHECK_EQ(main_window_render_process_hosts_.empty(),
              main_window_render_view_hosts_.empty());

    main_window_render_view_hosts_.insert(view_host);
    main_window_render_process_hosts_.insert(process_host);

    // Make sure the new renderer process_host has a test configuration shared
    // with other renderers.
    mojom::WebTestRunTestConfigurationPtr params =
        mojom::WebTestRunTestConfiguration::New();
    params->allow_external_pages = false;
    params->current_working_directory = current_working_directory_;
    params->temp_path = temp_path_;
    params->test_url = test_url_;
    params->allow_external_pages =
        base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kAllowExternalPages);
    params->expected_pixel_hash = expected_pixel_hash_;
    params->wpt_print_mode = wpt_print_mode_;
    params->protocol_mode = protocol_mode_;

    GetWebTestRenderFrameRemote(frame)->SetTestConfiguration(std::move(params),
                                                             starting_test);
  }

  // Is this a previously unknown renderer process_host?
  if (!render_process_host_observations_.IsObservingSource(process_host)) {
    render_process_host_observations_.AddObservation(process_host);
    all_observed_render_process_hosts_.insert(process_host);

    if (!main_window) {
      GetWebTestRenderFrameRemote(frame)
          ->SetupRendererProcessForNonTestWindow();
    }

    GetWebTestRenderFrameRemote(frame)->ReplicateWebTestRuntimeFlagsChanges(
        accumulated_web_test_runtime_flags_changes_.Clone());
    GetWebTestRenderFrameRemote(frame)->ReplicateWorkQueueStates(
        work_queue_states_.Clone());
  }
}

void WebTestControlHost::OnTestFinished() {
  CHECK_EQ(test_phase_, DURING_TEST);

  test_phase_ = CLEAN_UP;
  if (!printer_->output_finished())
    printer_->PrintImageFooter();
  if (main_window_)
    main_window_->web_contents()->ExitFullscreen(/*will_cause_resize=*/false);
  devtools_bindings_.reset();
  devtools_protocol_test_bindings_.reset();
  accumulated_web_test_runtime_flags_changes_.clear();
  web_test_runtime_flags_.Reset();
  work_queue_states_.clear();

  ShellBrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      2, base::BindOnce(&WebTestControlHost::PrepareRendererForNextWebTest,
                        weak_factory_.GetWeakPtr()));

  StoragePartition* storage_partition =
      browser_context->GetDefaultStoragePartition();
  storage_partition->GetServiceWorkerContext()->ClearAllServiceWorkersForTest(
      barrier_closure);
  storage_partition->ClearBluetoothAllowedDevicesMapForTesting();

  // TODO(nhiroki): Add a comment about the reason why we terminate all shared
  // workers here.
  TerminateAllSharedWorkers(ShellContentBrowserClient::Get()
                                ->browser_context()
                                ->GetDefaultStoragePartition(),
                            barrier_closure);
}

void WebTestControlHost::OnDumpFrameLayoutResponse(
    FrameTreeNodeId frame_tree_node_id,
    const std::string& dump) {
  // Store the result.
  auto pair = frame_to_layout_dump_map_.emplace(frame_tree_node_id, dump);
  bool insertion_took_place = pair.second;
  DCHECK(insertion_took_place);

  // See if we need to wait for more responses.
  if (--waiting_for_layout_dumps_ > 0)
    return;

  // Stitch the frame-specific results in the right order.
  std::string stitched_layout_dump;
  web_contents()->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      [&](RenderFrameHost* render_frame_host) {
        auto it = frame_to_layout_dump_map_.find(
            render_frame_host->GetFrameTreeNodeId());
        if (it != frame_to_layout_dump_map_.end()) {
          stitched_layout_dump.append(it->second);
        }
      });

  layout_dump_.emplace(std::move(stitched_layout_dump));
  ReportResults();
}

void WebTestControlHost::OnPixelDumpCaptured(const SkBitmap& snapshot) {
  // In the test: test_runner/notify_done_and_defered_close_dump_surface.html,
  // the |main_window_| is closed while waiting for the pixel dump. When this
  // happens, every window is closed and while pumping the message queue,
  // OnPixelDumpCaptured is called with an empty snapshot. It is also possible
  // to use a redirect to capture an empty snapshot - see crbug.com/1443169.
  if (!main_window_ || snapshot.drawsNothing()) {
    return;
  }
  pixel_dump_ = snapshot;
  waiting_for_pixel_results_ = false;
  ReportResults();
}

void WebTestControlHost::ReportResults() {
  if (waiting_for_layout_dumps_ || waiting_for_pixel_results_)
    return;

  printer_->StartStateDump();

  // Audio results only come from the renderer.
  if (renderer_dump_result_->audio)
    OnAudioDump(*renderer_dump_result_->audio);

  // Use the browser-generated |layout_dump_| if present, else use the
  // renderer's.
  if (layout_dump_)
    OnTextDump(*layout_dump_);
  else if (renderer_dump_result_->layout)
    OnTextDump(*renderer_dump_result_->layout);
  else
    NOTREACHED_IN_MIGRATION();

  // Use the browser-generated |pixel_dump_| if present, else use the
  // renderer's.
  if (pixel_dump_) {
    // See if we need to draw the selection bounds rect on top of the snapshot.
    if (!renderer_dump_result_->selection_rect.IsEmpty())
      DrawSelectionRect(*pixel_dump_, renderer_dump_result_->selection_rect);
    // The snapshot arrives from the GPU process via shared memory. Because MSan
    // can't track initializedness across processes, we must assure it that the
    // pixels are in fact initialized.
    MSAN_UNPOISON(pixel_dump_->getPixels(), pixel_dump_->computeByteSize());
    base::MD5Digest digest;
    auto bytes =
        base::span(static_cast<const uint8_t*>(pixel_dump_->getPixels()),
                   pixel_dump_->computeByteSize());
    base::MD5Sum(bytes, &digest);
    actual_pixel_hash_ = base::MD5DigestToBase16(digest);

    OnImageDump(actual_pixel_hash_, *pixel_dump_);
  } else if (!renderer_dump_result_->actual_pixel_hash.empty()) {
    OnImageDump(renderer_dump_result_->actual_pixel_hash,
                renderer_dump_result_->pixels);
  }

  OnTestFinished();
}

void WebTestControlHost::OnImageDump(const std::string& actual_pixel_hash,
                                     const SkBitmap& image) {
  printer_->PrintImageHeader(actual_pixel_hash, expected_pixel_hash_);

  // Only encode and dump the png if the hashes don't match. Encoding the
  // image is really expensive.
  if (actual_pixel_hash != expected_pixel_hash_) {
    std::vector<unsigned char> png;

    bool discard_transparency = true;
    if (web_test_runtime_flags().dump_drag_image())
      discard_transparency = false;

    gfx::PNGCodec::ColorFormat pixel_format;
    switch (image.info().colorType()) {
      case kBGRA_8888_SkColorType:
        pixel_format = gfx::PNGCodec::FORMAT_BGRA;
        break;
      case kRGBA_8888_SkColorType:
        pixel_format = gfx::PNGCodec::FORMAT_RGBA;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        return;
    }

    std::vector<gfx::PNGCodec::Comment> comments;
    // Used by
    // //third_party/blink/tools/blinkpy/common/read_checksum_from_png.py
    comments.push_back(gfx::PNGCodec::Comment("checksum", actual_pixel_hash));
    bool success = gfx::PNGCodec::Encode(
        static_cast<const unsigned char*>(image.getPixels()), pixel_format,
        gfx::Size(image.width(), image.height()),
        static_cast<int>(image.rowBytes()), discard_transparency, comments,
        &png);
    if (success)
      printer_->PrintImageBlock(png);
  }
  printer_->PrintImageFooter();
}

void WebTestControlHost::OnAudioDump(const std::vector<unsigned char>& dump) {
  printer_->PrintAudioHeader();
  printer_->PrintAudioBlock(dump);
  printer_->PrintAudioFooter();
}

void WebTestControlHost::OnTextDump(const std::string& dump) {
  printer_->PrintTextHeader();
  printer_->PrintTextBlock(dump);
  if (!navigation_history_dump_.empty())
    printer_->PrintTextBlock(navigation_history_dump_);
  printer_->PrintTextFooter();
}

void WebTestControlHost::PrintMessageToStderr(const std::string& message) {
  printer_->AddMessageToStderr(message);
}

void WebTestControlHost::PrintMessage(const std::string& message) {
  printer_->AddMessageRaw(message);
}

void WebTestControlHost::OverridePreferences(
    const blink::web_pref::WebPreferences& prefs) {
  should_override_prefs_ = true;
  prefs_ = prefs;

  // Notifies the WebContents that Blink preferences changed so
  // immediately apply the new settings and to avoid re-usage of cached
  // preferences that are now stale. WebContents::UpdateWebPreferences is
  // not used here because it would send an unneeded preferences update to the
  // renderer.
  main_window_->web_contents()->OnWebPreferencesChanged();
}

void WebTestControlHost::SetPopupBlockingEnabled(bool block_popups) {
  WebTestContentBrowserClient::Get()->SetPopupBlockingEnabled(block_popups);
}

void WebTestControlHost::SimulateScreenOrientationChanged() {
  content::WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(main_window_->web_contents());
  web_contents->DidChangeScreenOrientation();
}

void WebTestControlHost::SetPermission(const std::string& name,
                                       blink::mojom::PermissionStatus status,
                                       const GURL& origin,
                                       const GURL& embedding_origin) {
  blink::PermissionType type;
  if (name == "midi") {
    type = blink::PermissionType::MIDI;
  } else if (name == "midi-sysex") {
    type = blink::PermissionType::MIDI_SYSEX;
  } else if (name == "push-messaging" || name == "notifications") {
    type = blink::PermissionType::NOTIFICATIONS;
  } else if (name == "geolocation") {
    type = blink::PermissionType::GEOLOCATION;
  } else if (name == "protected-media-identifier") {
    type = blink::PermissionType::PROTECTED_MEDIA_IDENTIFIER;
  } else if (name == "background-sync") {
    type = blink::PermissionType::BACKGROUND_SYNC;
  } else if (name == "clipboard-read-write") {
    type = blink::PermissionType::CLIPBOARD_READ_WRITE;
  } else if (name == "clipboard-sanitized-write") {
    type = blink::PermissionType::CLIPBOARD_SANITIZED_WRITE;
  } else if (name == "payment-handler") {
    type = blink::PermissionType::PAYMENT_HANDLER;
  } else if (name == "accelerometer" || name == "gyroscope" ||
             name == "magnetometer" || name == "ambient-light-sensor") {
    type = blink::PermissionType::SENSORS;
  } else if (name == "background-fetch") {
    type = blink::PermissionType::BACKGROUND_FETCH;
  } else if (name == "periodic-background-sync") {
    type = blink::PermissionType::PERIODIC_BACKGROUND_SYNC;
  } else if (name == "wake-lock-screen") {
    type = blink::PermissionType::WAKE_LOCK_SCREEN;
  } else if (name == "wake-lock-system") {
    type = blink::PermissionType::WAKE_LOCK_SYSTEM;
  } else if (name == "nfc") {
    type = blink::PermissionType::NFC;
  } else if (name == "storage-access") {
    type = blink::PermissionType::STORAGE_ACCESS_GRANT;
  } else if (name == "top-level-storage-access") {
    type = blink::PermissionType::TOP_LEVEL_STORAGE_ACCESS;
  } else {
    NOTREACHED_IN_MIGRATION();
    type = blink::PermissionType::NOTIFICATIONS;
  }

  WebTestContentBrowserClient::Get()
      ->GetWebTestBrowserContext()
      ->GetWebTestPermissionManager()
      ->SetPermission(type, status, origin, embedding_origin,
                      base::DoNothing());
}

void WebTestControlHost::GetWritableDirectory(
    GetWritableDirectoryCallback reply) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  if (!writable_directory_for_tests_.IsValid()) {
    if (!writable_directory_for_tests_.CreateUniqueTempDir()) {
      LOG(ERROR) << "Failed to create temporary directory, test might not work "
                    "correctly";
    }
  }
  std::move(reply).Run(writable_directory_for_tests_.GetPath());
}

namespace {

// A fake ui::SelectFileDialog, which will select a single pre-determined path.
class FakeSelectFileDialog : public ui::SelectFileDialog {
 public:
  FakeSelectFileDialog(base::FilePath result,
                       Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy)
      : ui::SelectFileDialog(listener, std::move(policy)),
        result_(std::move(result)) {}

 protected:
  ~FakeSelectFileDialog() override = default;

  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      const GURL* caller) override {
    listener_->FileSelected(ui::SelectedFileInfo(result_), 0);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return false;
  }
  void ListenerDestroyed() override { listener_ = nullptr; }
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

 private:
  base::FilePath result_;
};

class FakeSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit FakeSelectFileDialogFactory(base::FilePath result)
      : result_(std::move(result)) {}
  ~FakeSelectFileDialogFactory() override = default;

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    return new FakeSelectFileDialog(result_, listener, std::move(policy));
  }

 private:
  base::FilePath result_;
};

}  // namespace

void WebTestControlHost::SetFilePathForMockFileDialog(
    const base::FilePath& path) {
  ui::SelectFileDialog::SetFactory(
      std::make_unique<FakeSelectFileDialogFactory>(path));
}

void WebTestControlHost::FocusDevtoolsSecondaryWindow() {
  CHECK(secondary_window_);
  // We don't go down the normal system path of focusing RenderWidgetHostView
  // because on mac headless, there are no system windows and that path does
  // not do anything. Instead we go through the Shell::ActivateContents() path
  // which knows how to perform the activation correctly on all platforms and in
  // headless mode.
  secondary_window_->ActivateContents(secondary_window_->web_contents());
}

void WebTestControlHost::SetTrustTokenKeyCommitments(
    const std::string& raw_commitments,
    base::OnceClosure callback) {
  GetNetworkService()->SetTrustTokenKeyCommitments(raw_commitments,
                                                   std::move(callback));
}

void WebTestControlHost::ClearTrustTokenState(base::OnceClosure callback) {
  BrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  StoragePartition* storage_partition =
      browser_context->GetDefaultStoragePartition();
  storage_partition->GetNetworkContext()->ClearTrustTokenData(
      nullptr,  // A wildcard filter.
      std::move(callback));
}

void WebTestControlHost::SetDatabaseQuota(int32_t quota) {
  auto run_on_io_thread = [](scoped_refptr<storage::QuotaManager> quota_manager,
                             int32_t quota) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (quota == kDefaultDatabaseQuota) {
      // Reset quota to settings with a zero refresh interval to force
      // QuotaManager to refresh settings immediately.
      storage::QuotaSettings default_settings;
      default_settings.refresh_interval = base::TimeDelta();
      quota_manager->SetQuotaSettings(default_settings);
    } else {
      DCHECK_GE(quota, 0);
      quota_manager->SetQuotaSettings(storage::GetHardCodedSettings(quota));
    }
  };

  BrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  StoragePartition* storage_partition =
      browser_context->GetDefaultStoragePartition();
  scoped_refptr<storage::QuotaManager> quota_manager =
      base::WrapRefCounted(storage_partition->GetQuotaManager());

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(run_on_io_thread, std::move(quota_manager), quota));
}

void WebTestControlHost::ClearAllDatabases() {
  auto run_on_database_sequence =
      [](scoped_refptr<storage::DatabaseTracker> db_tracker) {
        DCHECK(db_tracker->task_runner()->RunsTasksInCurrentSequence());
        db_tracker->DeleteDataModifiedSince(base::Time(), base::DoNothing());
      };

  BrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  StoragePartition* storage_partition =
      browser_context->GetDefaultStoragePartition();
  scoped_refptr<storage::DatabaseTracker> db_tracker =
      base::WrapRefCounted(storage_partition->GetDatabaseTracker());

  if (db_tracker) {
    base::SequencedTaskRunner* task_runner = db_tracker->task_runner();
    task_runner->PostTask(FROM_HERE, base::BindOnce(run_on_database_sequence,
                                                    std::move(db_tracker)));
  }
}

void WebTestControlHost::SimulateWebNotificationClick(
    const std::string& title,
    int32_t action_index,
    const std::optional<std::u16string>& reply) {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  auto* service = context->GetPlatformNotificationService();
  static_cast<MockPlatformNotificationService*>(service)->SimulateClick(
      title,
      action_index == std::numeric_limits<int32_t>::min()
          ? std::optional<int>()
          : std::optional<int>(action_index),
      reply);
}

void WebTestControlHost::SimulateWebNotificationClose(const std::string& title,
                                                      bool by_user) {
  auto* client = WebTestContentBrowserClient::Get();
  auto* context = client->GetWebTestBrowserContext();
  auto* service = context->GetPlatformNotificationService();
  static_cast<MockPlatformNotificationService*>(service)->SimulateClose(
      title, by_user);
}

void WebTestControlHost::SimulateWebContentIndexDelete(const std::string& id) {
  BrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  auto* content_index_provider = static_cast<ShellContentIndexProvider*>(
      browser_context->GetContentIndexProvider());

  std::pair<int64_t, url::Origin> registration_data =
      content_index_provider->GetRegistrationDataFromId(id);

  StoragePartition* storage_partition =
      browser_context->GetStoragePartitionForUrl(
          registration_data.second.GetURL(),
          /*can_create=*/false);
  storage_partition->GetContentIndexContext()->OnUserDeletedItem(
      registration_data.first, registration_data.second, id);
}

void WebTestControlHost::WebTestRuntimeFlagsChanged(
    base::Value::Dict changed_web_test_runtime_flags) {
  const int render_process_id = receiver_bindings_.current_context();

  // Stash the accumulated changes for future, not-yet-created renderers.
  accumulated_web_test_runtime_flags_changes_.Merge(
      changed_web_test_runtime_flags.Clone());
  web_test_runtime_flags_.tracked_dictionary().ApplyUntrackedChanges(
      accumulated_web_test_runtime_flags_changes_);

  base::flat_map<int, mojom::WebTestRenderFrame*> process_to_frame_map;

  // Propagate the changes to all the renderer processes, we only
  // need to send it once per process so we build a list of the first
  // frame we find per process.
  for (auto& item : web_test_render_frame_map_) {
    if (item.first.child_id == render_process_id) {
      continue;
    }
    process_to_frame_map.emplace(item.first.child_id, item.second.get());
  }

  // Then we send the new flags to those frames.
  for (auto [id, frame] : process_to_frame_map) {
    frame->ReplicateWebTestRuntimeFlagsChanges(
        changed_web_test_runtime_flags.Clone());
  }
}

void WebTestControlHost::RegisterIsolatedFileSystem(
    const std::vector<base::FilePath>& file_paths,
    RegisterIsolatedFileSystemCallback callback) {
  const int render_process_id = receiver_bindings_.current_context();

  ChildProcessSecurityPolicy* policy =
      ChildProcessSecurityPolicy::GetInstance();

  storage::IsolatedContext::FileInfoSet file_info_set;
  for (auto& path : file_paths) {
    file_info_set.AddPath(path, nullptr);
    if (!policy->CanReadFile(render_process_id, path))
      policy->GrantReadFile(render_process_id, path);
  }

  std::string filesystem_id =
      storage::IsolatedContext::GetInstance()->RegisterDraggedFileSystem(
          file_info_set);
  policy->GrantReadFileSystem(render_process_id, filesystem_id);

  std::move(callback).Run(filesystem_id);
}

void WebTestControlHost::DropPointerLock() {
  main_window_->web_contents()->DropPointerLockForTesting();
}

void WebTestControlHost::SetPointerLockWillFail() {
  next_pointer_lock_action_ = NextPointerLockAction::kWillFail;
}

void WebTestControlHost::SetPointerLockWillRespondAsynchronously() {
  next_pointer_lock_action_ = NextPointerLockAction::kTestWillRespond;
}

void WebTestControlHost::AllowPointerLock() {
  DCHECK_EQ(next_pointer_lock_action_, NextPointerLockAction::kTestWillRespond);
  main_window_->web_contents()->GotResponseToPointerLockRequest(
      blink::mojom::PointerLockResult::kSuccess);
  next_pointer_lock_action_ = NextPointerLockAction::kWillSucceed;
}

void WebTestControlHost::WorkItemAdded(mojom::WorkItemPtr work_item) {
  // TODO(peria): Check if |work_item| comes from the main window's main frame.
  // TODO(peria): Reject the item if the work queue is frozen.
  work_queue_.push_back(std::move(work_item));
}

void WebTestControlHost::RequestWorkItem() {
  DCHECK(main_window_);
  auto* frame = main_window_->web_contents()->GetPrimaryMainFrame();
  if (work_queue_.empty()) {
    work_queue_states_.SetByDottedPath(kDictKeyWorkQueueHasItems, false);
    GetWebTestRenderFrameRemote(frame)->ReplicateWorkQueueStates(
        work_queue_states_.Clone());
  } else {
    GetWebTestRenderFrameRemote(frame)->ProcessWorkItem(
        work_queue_.front()->Clone());
    work_queue_.pop_front();
  }
}

void WebTestControlHost::WorkQueueStatesChanged(
    base::Value::Dict changed_work_queue_states) {
  work_queue_states_.Merge(std::move(changed_work_queue_states));
}

void WebTestControlHost::SetAcceptLanguages(
    const std::string& accept_languages) {
  if (web_contents()->GetMutableRendererPrefs()->accept_languages ==
      accept_languages) {
    return;
  }

  web_contents()->GetMutableRendererPrefs()->accept_languages =
      accept_languages;
  web_contents()->SyncRendererPrefs();
}

void WebTestControlHost::EnableAutoResize(const gfx::Size& min_size,
                                          const gfx::Size& max_size) {
  web_contents()->GetRenderWidgetHostView()->EnableAutoResize(min_size,
                                                              max_size);
}

void WebTestControlHost::DisableAutoResize(const gfx::Size& new_size) {
  web_contents()->GetRenderWidgetHostView()->DisableAutoResize(new_size);
  main_window_->ResizeWebContentForTests(new_size);
}

void WebTestControlHost::SetLCPPNavigationHint(
    blink::mojom::LCPCriticalPathPredictorNavigationTimeHintPtr hint) {
  lcpp_hint_ = *hint.get();
}

void WebTestControlHost::SetRegisterProtocolHandlerMode(
    mojom::WebTestControlHost::AutoResponseMode mode) {
  custom_handlers::ProtocolHandlerRegistry* registry =
      custom_handlers::SimpleProtocolHandlerRegistryFactory::
          GetForBrowserContext(web_contents()->GetBrowserContext(), true);
  CHECK(registry);

  switch (mode) {
    case WebTestControlHost::AutoResponseMode::kNone:
      registry->SetRphRegistrationMode(
          custom_handlers::RphRegistrationMode::kNone);
      return;
    case WebTestControlHost::AutoResponseMode::kAutoAccept:
      registry->SetRphRegistrationMode(
          custom_handlers::RphRegistrationMode::kAutoAccept);
      return;
    case WebTestControlHost::AutoResponseMode::kAutoReject:
      registry->SetRphRegistrationMode(
          custom_handlers::RphRegistrationMode::kAutoReject);
      return;
  }
  NOTREACHED();
}

void WebTestControlHost::GoToOffset(int offset) {
  main_window_->GoBackOrForward(offset);
}

void WebTestControlHost::Reload() {
  main_window_->Reload();
}

void WebTestControlHost::LoadURLForFrame(const GURL& url,
                                         const std::string& frame_name) {
  main_window_->LoadURLForFrame(url, frame_name, ui::PAGE_TRANSITION_LINK);
}

void WebTestControlHost::SetMainWindowHidden(bool hidden) {
  if (hidden)
    main_window_->web_contents()->WasHidden();
  else
    main_window_->web_contents()->WasShown();
}

void WebTestControlHost::SetFrameWindowHidden(
    const blink::LocalFrameToken& frame_token,
    bool hidden) {
  if (hidden) {
    GetWebContentsFromCurrentContext(frame_token)->WasHidden();
  } else {
    GetWebContentsFromCurrentContext(frame_token)->WasShown();
  }
}

WebContents* WebTestControlHost::GetWebContentsFromCurrentContext(
    const blink::LocalFrameToken& frame_token) {
  const int render_process_id = receiver_bindings_.current_context();
  auto* rfh =
      RenderFrameHostImpl::FromFrameToken(render_process_id, frame_token);
  CHECK(rfh);
  return WebContents::FromRenderFrameHost(rfh);
}

void WebTestControlHost::CheckForLeakedWindows() {
  check_for_leaked_windows_ = true;
}

void WebTestControlHost::PrepareRendererForNextWebTest() {
  // If the window is gone, due to crashes or whatever, we need to make
  // progress.
  if (!main_window_) {
    PrepareRendererForNextWebTestDone();
    return;
  }

  content::WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(main_window_->web_contents());

  // TODO(arthursonzogni): Not sure if this line is needed. It cancels pending
  // navigations and pending subresources requests. I guess it increases the
  // odds of transitionning from one test to another with no side effects.
  // Consider removing it to understand what happens without.
  web_contents->Stop();

  // Disable back/forward cache before the current test page navigates away so
  // that the test page does not remain in the back/forward cache after the
  // test.
  BackForwardCache::DisableForRenderFrameHost(
      web_contents->GetPrimaryMainFrame(),
      BackForwardCache::DisabledReason(
          BackForwardCache::DisabledSource::kTesting, 0,
          "disabled for web_test not to cache the test page after the test "
          "ends.",
          /*context=*/"", "disabled"));

  // Flush all the back/forward cache to avoid side effects in the next test.
  for (auto* shell : Shell::windows()) {
    shell->web_contents()->GetController().GetBackForwardCache().Flush();
  }

  // Navigate to about:blank in between two consecutive web tests.
  //
  // Note: this navigation might happen in a new process, depending on the
  // COOP policy of the previous document.

  // Avoid sending LCPP hint on the about:blank navigation.
  lcpp_hint_ = std::nullopt;

  NavigationController::LoadURLParams params((GURL(kAboutBlankResetWebTest)));
  params.transition_type = ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED);
  params.should_clear_history_list = true;
  params.initiator_origin = url::Origin();  // Opaque initiator.
  // We should always reset the browsing instance, but it slows down tests
  // significantly. For efficiency, this is limited to tests known to be
  // affected.
  params.force_new_browsing_instance =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kResetBrowsingInstanceBetweenTests);
  web_contents->GetController().LoadURLWithParams(params);

  // The navigation might have to wait for before unload handler to execute. The
  // remaining of the logic continues in:
  // |WebTestControlHost::DidFinishNavigation|.
}

void WebTestControlHost::FlushInputAndStartTest(WeakDocumentPtr doc) {
  RenderFrameHost* rfh = doc.AsRenderFrameHostIfValid();
  if (!rfh) {
    return;
  }

  // Ensures any synthetic input (e.g. mouse enter/leave/move events as a
  // result of navigation) have been handled by the renderer.
  rfh->GetRenderWidgetHost()->FlushForTesting();
  GetWebTestRenderFrameRemote(rfh)->StartTest();
}

void WebTestControlHost::DidFinishNavigation(NavigationHandle* navigation) {
  if (navigation->GetURL() == GURL(kAboutBlankResetWebTest)) {
    // During fuzzing, the |main_window_| might close itself using
    // window.close(). This might happens after the end of the test, during the
    // cleanup phase. In this case, the pending about:blank navigation might be
    // canceled, within the |main_window_| destructor. It is no longer safe to
    // access |main_window_| here. See https://crbug.com/1221183
    if (!navigation->HasCommitted()) {
      return;
    }

    next_non_blank_nav_is_new_test_ = true;

    // Request additional web test specific cleanup in the renderer process:
    content::WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(main_window_->web_contents());
    GetWebTestRenderFrameRemote(web_contents->GetPrimaryMainFrame())
        ->ResetRendererAfterWebTest();

    PrepareRendererForNextWebTestDone();
  } else if (navigation->IsInPrimaryMainFrame() &&
             !navigation->GetURL().IsAboutBlank() &&
             next_non_blank_nav_is_new_test_) {
    next_non_blank_nav_is_new_test_ = false;

    if (navigation->HasCommitted()) {
      // If the browser is injecting synthetic mouse moves, it does so at
      // CommitPending time by posting a task to perform the dispatch. Hence,
      // that task must already be queued (or complete) by this time. Post the
      // flush input task to ensure it runs after the synthetic mouse event
      // dispatch task. See comments on next_non_blank_nav_is_new_test_ for
      // more details.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &WebTestControlHost::FlushInputAndStartTest,
              weak_factory_.GetWeakPtr(),
              navigation->GetRenderFrameHost()->GetWeakDocumentPtr()));
    }
  }
}

void WebTestControlHost::PrepareRendererForNextWebTestDone() {
  if (leak_detector_ && main_window_) {
    // When doing leak detection, we don't want to count opened windows as
    // leaks, unless the test specifies that it expects to have closed them
    // all and wants to look for them as leaks.
    if (!check_for_leaked_windows_)
      CloseTestOpenedWindows();

    RenderViewHost* rvh = main_window_->web_contents()
                              ->GetPrimaryMainFrame()
                              ->GetRenderViewHost();
    RenderProcessHost* rph = rvh->GetProcess();
    CHECK(rph->GetProcess().IsValid());
    leak_detector_->TryLeakDetection(
        rph,
        base::BindOnce(&WebTestControlHost::OnLeakDetectionDone,
                       weak_factory_.GetWeakPtr(), rph->GetProcess().Pid()));
    return;
  }

  Shell::QuitMainMessageLoopForTesting();
}

void WebTestControlHost::OnLeakDetectionDone(
    int pid,
    const LeakDetector::LeakDetectionReport& report) {
  if (report.leaked) {
    printer_->StartStateDump();
    printer_->AddErrorMessage(base::StringPrintf("#LEAK - renderer pid %d (%s)",
                                                 pid, report.detail.c_str()));
    CHECK(!crash_when_leak_found_);
    DiscardMainWindow();
  }

  Shell::QuitMainMessageLoopForTesting();
}

void WebTestControlHost::CloseTestOpenedWindows() {
  DevToolsAgentHost::DetachAllClients();
  std::vector<Shell*> open_windows(Shell::windows());
  for (auto* shell : open_windows) {
    if (shell != main_window_)
      shell->Close();
  }
  secondary_window_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

void WebTestControlHost::CloseAllWindows() {
  DevToolsAgentHost::DetachAllClients();
  while (!Shell::windows().empty())
    Shell::windows().back()->Close();
  main_window_ = nullptr;
  secondary_window_ = nullptr;
  base::RunLoop().RunUntilIdle();
}

void WebTestControlHost::SetBluetoothManualChooser(bool enable) {
  if (enable) {
    bluetooth_chooser_factory_ =
        std::make_unique<WebTestBluetoothChooserFactory>();
  } else {
    bluetooth_chooser_factory_.reset();
  }
}

void WebTestControlHost::GetBluetoothManualChooserEvents(
    GetBluetoothManualChooserEventsCallback reply) {
  if (!bluetooth_chooser_factory_) {
    printer_->AddErrorMessage(
        "FAIL: Must call setBluetoothManualChooser before "
        "getBluetoothManualChooserEvents.");
    std::move(reply).Run({});
    return;
  }
  std::move(reply).Run(bluetooth_chooser_factory_->GetAndResetEvents());
}

void WebTestControlHost::SendBluetoothManualChooserEvent(
    const std::string& event_name,
    const std::string& argument) {
  if (!bluetooth_chooser_factory_) {
    printer_->AddErrorMessage(
        "FAIL: Must call setBluetoothManualChooser before "
        "sendBluetoothManualChooserEvent.");
    return;
  }
  BluetoothChooserEvent event;
  if (event_name == "cancelled") {
    event = BluetoothChooserEvent::CANCELLED;
  } else if (event_name == "selected") {
    event = BluetoothChooserEvent::SELECTED;
  } else if (event_name == "rescan") {
    event = BluetoothChooserEvent::RESCAN;
  } else {
    printer_->AddErrorMessage(base::StringPrintf(
        "FAIL: Unexpected sendBluetoothManualChooserEvent() event name '%s'.",
        event_name.c_str()));
    return;
  }
  bluetooth_chooser_factory_->SendEvent(event, argument);
}

void WebTestControlHost::BlockThirdPartyCookies(bool block) {
  ShellBrowserContext* browser_context =
      ShellContentBrowserClient::Get()->browser_context();
  StoragePartition* storage_partition =
      browser_context->GetDefaultStoragePartition();
  storage_partition->GetCookieManagerForBrowserProcess()
      ->BlockThirdPartyCookies(block);
}

void WebTestControlHost::BindWebTestControlHostForRenderer(
    int render_process_id,
    mojo::PendingAssociatedReceiver<mojom::WebTestControlHost> receiver) {
  receiver_bindings_.Add(this, std::move(receiver), render_process_id);
}

void WebTestControlHost::BindNonAssociatedWebTestControlHost(
    mojo::PendingReceiver<mojom::NonAssociatedWebTestControlHost> receiver) {
  non_associated_receiver_bindings_.Add(this, std::move(receiver));
}

mojo::AssociatedRemote<mojom::WebTestRenderFrame>&
WebTestControlHost::GetWebTestRenderFrameRemote(RenderFrameHost* frame) {
  GlobalRenderFrameHostId key(frame->GetProcess()->GetID(),
                              frame->GetRoutingID());
  if (!base::Contains(web_test_render_frame_map_, key)) {
    mojo::AssociatedRemote<mojom::WebTestRenderFrame>& new_ptr =
        web_test_render_frame_map_[key];
    frame->GetRemoteAssociatedInterfaces()->GetInterface(&new_ptr);
    new_ptr.set_disconnect_handler(
        base::BindOnce(&WebTestControlHost::HandleWebTestRenderFrameRemoteError,
                       weak_factory_.GetWeakPtr(), key));
  }
  DCHECK(web_test_render_frame_map_[key].get());
  return web_test_render_frame_map_[key];
}

void WebTestControlHost::HandleWebTestRenderFrameRemoteError(
    const GlobalRenderFrameHostId& key) {
  web_test_render_frame_map_.erase(key);
}

WebTestControlHost::Node::Node(RenderFrameHost* host)
    : render_frame_host(host),
      render_frame_host_id(host->GetProcess()->GetID(), host->GetRoutingID()) {}

WebTestControlHost::Node::Node(Node&& other) = default;
WebTestControlHost::Node& WebTestControlHost::Node::operator=(Node&& other) =
    default;

WebTestControlHost::Node::~Node() = default;

}  // namespace content
