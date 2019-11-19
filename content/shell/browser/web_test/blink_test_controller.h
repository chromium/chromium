// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_BLINK_TEST_CONTROLLER_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_BLINK_TEST_CONTROLLER_H_

#include <map>
#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/web_preferences.h"
#include "content/shell/browser/web_test/leak_detector.h"
#include "content/shell/common/web_test.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/gfx/geometry/size.h"

class SkBitmap;

namespace content {

class DevToolsProtocolTestBindings;
class RenderFrameHost;
class Shell;
class WebTestBluetoothChooserFactory;
class WebTestDevToolsBindings;
struct TestInfo;

class BlinkTestResultPrinter {
 public:
  BlinkTestResultPrinter(std::ostream* output, std::ostream* error);
  ~BlinkTestResultPrinter();

  void reset() { state_ = DURING_TEST; }
  bool output_finished() const { return state_ == AFTER_TEST; }
  void set_capture_text_only(bool capture_text_only) {
    capture_text_only_ = capture_text_only;
  }

  void set_encode_binary_data(bool encode_binary_data) {
    encode_binary_data_ = encode_binary_data;
  }

  void PrintTextHeader();
  void PrintTextBlock(const std::string& block);
  void PrintTextFooter();

  void PrintImageHeader(const std::string& actual_hash,
                        const std::string& expected_hash);
  void PrintImageBlock(const std::vector<unsigned char>& png_image);
  void PrintImageFooter();

  void PrintAudioHeader();
  void PrintAudioBlock(const std::vector<unsigned char>& audio_data);
  void PrintAudioFooter();

  void AddMessageToStderr(const std::string& message);
  void AddMessage(const std::string& message);
  void AddMessageRaw(const std::string& message);
  void AddErrorMessage(const std::string& message);

  void CloseStderr();
  void StartStateDump();

 private:
  void PrintEncodedBinaryData(const std::vector<unsigned char>& data);

  enum State {
    DURING_TEST,
    DURING_STATE_DUMP,
    IN_TEXT_BLOCK,
    IN_AUDIO_BLOCK,
    IN_IMAGE_BLOCK,
    AFTER_TEST
  };
  State state_;

  bool capture_text_only_;
  bool encode_binary_data_;

  std::ostream* output_;
  std::ostream* error_;

  DISALLOW_COPY_AND_ASSIGN(BlinkTestResultPrinter);
};

class BlinkTestController : public WebContentsObserver,
                            public RenderProcessHostObserver,
                            public NotificationObserver,
                            public GpuDataManagerObserver {
 public:
  static BlinkTestController* Get();

  BlinkTestController();
  ~BlinkTestController() override;

  // True if the controller is ready for testing.
  bool PrepareForWebTest(const TestInfo& test_info);
  // True if the controller was reset successfully.
  bool ResetAfterWebTest();

  // IPC messages forwarded from elsewhere.
  void OnWebTestRuntimeFlagsChanged(
      int sender_process_host_id,
      const base::DictionaryValue& changed_web_test_runtime_flags);
  void OnTestFinishedInSecondaryRenderer();
  void OnInitiateCaptureDump(bool capture_navigation_history,
                             bool capture_pixels);
  void OnInspectSecondaryWindow();

  // Makes sure that the potentially new renderer associated with |frame| is 1)
  // initialized for the test, 2) kept up to date wrt test flags and 3)
  // monitored for crashes.
  void HandleNewRenderFrameHost(RenderFrameHost* frame);

  void SetTempPath(const base::FilePath& temp_path);
  void RendererUnresponsive();
  void OverrideWebkitPrefs(WebPreferences* prefs);
  void OpenURL(const GURL& url);
  bool IsMainWindow(WebContents* web_contents) const;
  std::unique_ptr<BluetoothChooser> RunBluetoothChooser(
      RenderFrameHost* frame,
      const BluetoothChooser::EventHandler& event_handler);

  BlinkTestResultPrinter* printer() { return printer_.get(); }
  void set_printer(BlinkTestResultPrinter* printer) { printer_.reset(printer); }

  void DevToolsProcessCrashed();

  // WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message) override;
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override;
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;

  // RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(
      RenderProcessHost* render_process_host) override;
  void RenderProcessExited(RenderProcessHost* render_process_host,
                           const ChildProcessTerminationInfo& info) override;

  // NotificationObserver implementation.
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

  // GpuDataManagerObserver implementation.
  void OnGpuProcessCrashed(base::TerminationStatus exit_code) override;

  const base::DictionaryValue& accumulated_web_test_runtime_flags_changes()
      const {
    return accumulated_web_test_runtime_flags_changes_;
  }

 private:
  enum TestPhase { BETWEEN_TESTS, DURING_TEST, CLEAN_UP };

  // Node structure to construct a RenderFrameHost tree.
  struct Node {
    Node();
    explicit Node(RenderFrameHost* host);
    Node(Node&& other);
    ~Node();

    RenderFrameHost* render_frame_host = nullptr;
    GlobalFrameRoutingId render_frame_host_id;
    std::vector<Node*> children;

    DISALLOW_COPY_AND_ASSIGN(Node);
  };

  static BlinkTestController* instance_;

  Shell* SecondaryWindow();
  void LoadDevToolsJSTest();
  void DiscardMainWindow();

  // Message handlers.
  void OnAudioDump(const std::vector<unsigned char>& audio_dump);
  void OnImageDump(const std::string& actual_pixel_hash, const SkBitmap& image);
  void OnTextDump(const std::string& dump);
  void OnInitiateLayoutDump();
  void OnDumpFrameLayoutResponse(int frame_tree_node_id,
                                 const std::string& dump);
  void OnPrintMessageToStderr(const std::string& message);
  void OnPrintMessage(const std::string& message);
  void OnOverridePreferences(const WebPreferences& prefs);
  void OnSetPopupBlockingEnabled(bool block_popups);
  void OnTestFinished();
  void OnNavigateSecondaryWindow(const GURL& url);
  void OnGoToOffset(int offset);
  void OnReload();
  void OnLoadURLForFrame(const GURL& url, const std::string& frame_name);
  void OnCaptureSessionHistory();
  void OnCloseRemainingWindows();
  void OnResetDone();
  void OnLeakDetectionDone(const LeakDetector::LeakDetectionReport& report);
  void OnSetBluetoothManualChooser(bool enable);
  void OnGetBluetoothManualChooserEvents();
  void OnSendBluetoothManualChooserEvent(const std::string& event,
                                         const std::string& argument);
  void OnBlockThirdPartyCookies(bool block);
  mojo::AssociatedRemote<mojom::WebTestControl>& GetWebTestControlRemote(
      RenderFrameHost* frame);
  void HandleWebTestControlError(const GlobalFrameRoutingId& key);

  void OnCleanupFinished();
  void OnCaptureDumpCompleted(mojom::WebTestDumpPtr dump);
  void OnPixelDumpCaptured(const SkBitmap& snapshot);
  void ReportResults();
  void EnqueueSurfaceCopyRequest();

  // CompositeAllFramesThen() first builds a frame tree based on
  // frame->GetParent(). Then, it builds a queue of frames in depth-first order,
  // so that compositing happens from the leaves up. Finally,
  // CompositeNodeQueueThen() is used to composite one frame at a time,
  // asynchronously, continuing on to the next frame once each composite
  // finishes. Once all nodes have been composited, the final callback is run.
  // Each call to CompositeWithRaster() is an asynchronous Mojo call, to avoid
  // reentrancy problems.
  void CompositeAllFramesThen(base::OnceCallback<void()> callback);

 private:
  Node* BuildFrameTree(WebContents* web_contents);
  void CompositeNodeQueueThen(base::OnceCallback<void()> callback);
  void BuildDepthFirstQueue(Node* node);

 public:
  std::unique_ptr<BlinkTestResultPrinter> printer_;

  base::FilePath current_working_directory_;
  base::FilePath temp_path_;

  Shell* main_window_;
  Shell* secondary_window_;
  Shell* devtools_window_;

  std::unique_ptr<WebTestDevToolsBindings> devtools_bindings_;
  std::unique_ptr<DevToolsProtocolTestBindings>
      devtools_protocol_test_bindings_;

  // The PID of the render process of the render view host of main_window_.
  int current_pid_;

  // Tracks if (during the current test) we have already sent *initial* test
  // configuration to a renderer process (*initial* test configuration is
  // associated with some steps that should only be executed *once* per test -
  // for example resizing the window and setting the focus).
  bool did_send_initial_test_configuration_;

  // What phase of running an individual test we are currently in.
  TestPhase test_phase_;

  // Per test config.
  std::string expected_pixel_hash_;
  gfx::Size initial_size_;
  GURL test_url_;
  bool protocol_mode_;

  // Stores the default test-adapted WebPreferences which is then used to fully
  // reset the main window's preferences if and when it is reused.
  WebPreferences default_prefs_;

  // True if the WebPreferences of newly created RenderViewHost should be
  // overridden with prefs_.
  bool should_override_prefs_;
  WebPreferences prefs_;

  NotificationRegistrar registrar_;

  bool crash_when_leak_found_;
  std::unique_ptr<LeakDetector> leak_detector_;

  std::unique_ptr<WebTestBluetoothChooserFactory> bluetooth_chooser_factory_;

  // Map from frame_tree_node_id into frame-specific dumps.
  std::map<int, std::string> frame_to_layout_dump_map_;
  // Number of WebTestControl.DumpFrameLayout responses we are waiting for.
  int pending_layout_dumps_;

  // Renderer processes are observed to detect crashes.
  ScopedObserver<RenderProcessHost, RenderProcessHostObserver>
      render_process_host_observer_{this};
  std::set<RenderProcessHost*> all_observed_render_process_hosts_;
  std::set<RenderProcessHost*> main_window_render_process_hosts_;

  // Changes reported by OnWebTestRuntimeFlagsChanged that have accumulated
  // since PrepareForWebTest (i.e. changes that need to be send to a fresh
  // renderer created while test is in progress).
  base::DictionaryValue accumulated_web_test_runtime_flags_changes_;

  std::string navigation_history_dump_;
  base::Optional<SkBitmap> pixel_dump_;
  std::string actual_pixel_hash_;
  mojom::WebTestDumpPtr main_frame_dump_;
  bool waiting_for_pixel_results_ = false;
  bool waiting_for_main_frame_dump_ = false;

  std::vector<std::unique_ptr<Node>> composite_all_frames_node_storage_;
  std::queue<Node*> composite_all_frames_node_queue_;

  // Map from one frame to one mojo pipe.
  std::map<GlobalFrameRoutingId, mojo::AssociatedRemote<mojom::WebTestControl>>
      web_test_control_map_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BlinkTestController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BlinkTestController);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_BLINK_TEST_CONTROLLER_H_
