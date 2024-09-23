// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_BROWSER_WEB_TEST_CONTROL_HOST_H_
#define CONTENT_WEB_TEST_BROWSER_WEB_TEST_CONTROL_HOST_H_

#include <map>
#include <memory>
#include <ostream>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/web_test/browser/leak_detector.h"
#include "content/web_test/browser/web_test_tracing_controller.h"
#include "content/web_test/common/web_test.mojom.h"
#include "content/web_test/common/web_test_runtime_flags.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/lcp_critical_path_predictor/lcp_critical_path_predictor.mojom.h"
#include "ui/gfx/geometry/size.h"

class SkBitmap;

namespace content {
class DevToolsProtocolTestBindings;
class RenderFrameHost;
class Shell;
class WebTestBluetoothChooserFactory;
class WebTestDevToolsBindings;
struct TestInfo;

class WebTestResultPrinter {
 public:
  WebTestResultPrinter(std::ostream* output, std::ostream* error);

  WebTestResultPrinter(const WebTestResultPrinter&) = delete;
  WebTestResultPrinter& operator=(const WebTestResultPrinter&) = delete;

  ~WebTestResultPrinter() = default;

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
  enum State {
    DURING_TEST,
    DURING_STATE_DUMP,
    IN_TEXT_BLOCK,
    IN_AUDIO_BLOCK,
    IN_IMAGE_BLOCK,
    AFTER_TEST
  };

  void PrintEncodedBinaryData(const std::vector<unsigned char>& data);

  const raw_ptr<std::ostream> output_;
  const raw_ptr<std::ostream> error_;

  State state_ = DURING_TEST;

  bool capture_text_only_ = false;
  bool encode_binary_data_ = false;
};

class WebTestControlHost : public WebContentsObserver,
                           public RenderProcessHostObserver,
                           public GpuDataManagerObserver,
                           public mojom::WebTestControlHost,
                           public mojom::NonAssociatedWebTestControlHost {
 public:
  static WebTestControlHost* Get();

  WebTestControlHost();
  ~WebTestControlHost() override;

  WebTestControlHost(const WebTestControlHost&) = delete;
  WebTestControlHost& operator=(const WebTestControlHost&) = delete;

  void PrepareForWebTest(const TestInfo& test_info);
  void ResetBrowserAfterWebTest();

  // Allows WebTestControlHost to track all WebContents created by tests, either
  // by Javascript or by C++ code in the browser.
  void DidCreateOrAttachWebContents(WebContents* web_contents);

  void SetTempPath(const base::FilePath& temp_path);
  void OverrideWebkitPrefs(blink::web_pref::WebPreferences* prefs);
  void OpenURL(const GURL& url);
  bool IsMainWindow(WebContents* web_contents) const;
  std::unique_ptr<BluetoothChooser> RunBluetoothChooser(
      RenderFrameHost* frame,
      const BluetoothChooser::EventHandler& event_handler);
  void RequestPointerLock(WebContents* web_contents);

  WebTestResultPrinter* printer() { return printer_.get(); }
  void set_printer(WebTestResultPrinter* printer) { printer_.reset(printer); }

  // WebTestControlHost implementation.
  void PrintMessageToStderr(const std::string& message) override;

  void DevToolsProcessCrashed();

  // Called when a renderer wants to bind a connection to the
  // WebTestControlHost.
  void BindWebTestControlHostForRenderer(
      int render_process_id,
      mojo::PendingAssociatedReceiver<mojom::WebTestControlHost> receiver);

  void BindNonAssociatedWebTestControlHost(
      mojo::PendingReceiver<mojom::NonAssociatedWebTestControlHost> receiver);

  const WebTestRuntimeFlags& web_test_runtime_flags() const {
    return web_test_runtime_flags_;
  }

 private:
  enum TestPhase { BETWEEN_TESTS, DURING_TEST, CLEAN_UP };

  // Node structure to construct a RenderFrameHost tree.
  struct Node {
    explicit Node(RenderFrameHost* host);
    ~Node();

    Node(Node&& other);
    Node& operator=(Node&& other);

    raw_ptr<RenderFrameHost, AcrossTasksDanglingUntriaged> render_frame_host =
        nullptr;
    GlobalRenderFrameHostId render_frame_host_id;
    std::vector<raw_ptr<Node, VectorExperimental>> children;
  };

  class WebTestWindowObserver;

  // WebContentsObserver implementation.
  void PluginCrashed(const base::FilePath& plugin_path,
                     base::ProcessId plugin_pid) override;
  void TitleWasSet(NavigationEntry* entry) override;
  void DidFailLoad(RenderFrameHost* render_frame_host,
                   const GURL& validated_url,
                   int error_code) override;
  void WebContentsDestroyed() override;
  void DidUpdateFaviconURL(
      RenderFrameHost* render_frame_host,
      const std::vector<blink::mojom::FaviconURLPtr>& candidates) override;
  void RenderFrameHostChanged(RenderFrameHost* old_host,
                              RenderFrameHost* new_host) override;
  void RenderViewDeleted(RenderViewHost* render_view_host) override;
  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation) override;

  // RenderProcessHostObserver implementation.
  void RenderProcessHostDestroyed(
      RenderProcessHost* render_process_host) override;
  void RenderProcessExited(RenderProcessHost* render_process_host,
                           const ChildProcessTerminationInfo& info) override;

  // GpuDataManagerObserver implementation.
  void OnGpuProcessCrashed() override;

  // WebTestControlHost implementation.
  void InitiateCaptureDump(
      mojom::WebTestRendererDumpResultPtr renderer_dump_result,
      bool capture_navigation_history,
      bool capture_pixels) override;
  void TestFinishedInSecondaryRenderer() override;
  void PrintMessage(const std::string& message) override;
  void Reload() override;
  void OverridePreferences(
      const blink::web_pref::WebPreferences& web_preferences) override;
  void SetMainWindowHidden(bool hidden) override;
  void SetFrameWindowHidden(const blink::LocalFrameToken& frame_token,
                            bool hidden) override;
  void CheckForLeakedWindows() override;
  void GoToOffset(int offset) override;
  void SendBluetoothManualChooserEvent(const std::string& event,
                                       const std::string& argument) override;
  void SetBluetoothManualChooser(bool enable) override;
  void GetBluetoothManualChooserEvents(
      GetBluetoothManualChooserEventsCallback reply) override;
  void SetPopupBlockingEnabled(bool block_popups) override;
  void LoadURLForFrame(const GURL& url, const std::string& frame_name) override;
  void SimulateScreenOrientationChanged() override;
  void SetPermission(const std::string& name,
                     blink::mojom::PermissionStatus status,
                     const GURL& origin,
                     const GURL& embedding_origin) override;
  void BlockThirdPartyCookies(bool block) override;
  void GetWritableDirectory(GetWritableDirectoryCallback reply) override;
  void SetFilePathForMockFileDialog(const base::FilePath& path) override;
  void FocusDevtoolsSecondaryWindow() override;
  void SetTrustTokenKeyCommitments(const std::string& raw_commitments,
                                   base::OnceClosure callback) override;
  void ClearTrustTokenState(base::OnceClosure callback) override;
  void SetDatabaseQuota(int32_t quota) override;
  void ClearAllDatabases() override;
  void SimulateWebNotificationClick(
      const std::string& title,
      int32_t action_index,
      const std::optional<std::u16string>& reply) override;
  void SimulateWebNotificationClose(const std::string& title,
                                    bool by_user) override;
  void SimulateWebContentIndexDelete(const std::string& id) override;
  void WebTestRuntimeFlagsChanged(
      base::Value::Dict changed_web_test_runtime_flags) override;
  void RegisterIsolatedFileSystem(
      const std::vector<base::FilePath>& file_paths,
      RegisterIsolatedFileSystemCallback callback) override;
  void DropPointerLock() override;
  void SetPointerLockWillFail() override;
  void SetPointerLockWillRespondAsynchronously() override;
  void AllowPointerLock() override;
  void WorkItemAdded(mojom::WorkItemPtr work_item) override;
  void RequestWorkItem() override;
  void WorkQueueStatesChanged(
      base::Value::Dict changed_work_queue_states) override;
  void SetAcceptLanguages(const std::string& accept_languages) override;
  void EnableAutoResize(const gfx::Size& min_size,
                        const gfx::Size& max_size) override;
  void DisableAutoResize(const gfx::Size& new_size) override;
  void SetLCPPNavigationHint(
      blink::mojom::LCPCriticalPathPredictorNavigationTimeHintPtr hint)
      override;
  // Sets the Protocol Handler Registry in automation mode to avoid the
  // permission prompt in tests.
  void SetRegisterProtocolHandlerMode(
      mojom::WebTestControlHost::AutoResponseMode mode) override;

  void DiscardMainWindow();
  void FlushInputAndStartTest(WeakDocumentPtr rfh);
  // Closes all windows opened by the test. This is every window but the main
  // window, since it is created by the test harness and reused between tests.
  void CloseTestOpenedWindows();
  // Closes all windows, including the main window.
  void CloseAllWindows();

  // Makes sure that the potentially new renderer associated with |frame| is 1)
  // initialized for the test, 2) kept up to date wrt test flags and 3)
  // monitored for crashes.
  void HandleNewRenderFrameHost(RenderFrameHost* frame);

  // Message handlers.
  void OnAudioDump(const std::vector<unsigned char>& audio_dump);
  void OnImageDump(const std::string& actual_pixel_hash, const SkBitmap& image);
  void OnTextDump(const std::string& dump);
  void OnDumpFrameLayoutResponse(FrameTreeNodeId frame_tree_node_id,
                                 const std::string& dump);
  void OnTestFinished();
  void OnCaptureSessionHistory();
  void OnLeakDetectionDone(int pid,
                           const LeakDetector::LeakDetectionReport& report);

  // In between two tests, do some cleanup. Navigate to about:blank and
  // request blink to reset states.
  //
  // Note: If the current document had COOP:same-origin defined, the navigation
  // to about:blank will have to happen in a different browsing context group.
  // The process used might be a different one.
  void PrepareRendererForNextWebTest();
  void PrepareRendererForNextWebTestDone();

  void OnPixelDumpCaptured(const SkBitmap& snapshot);
  void ReportResults();
  void EnqueueSurfaceCopyRequest();

  // Returns the WebContents associated with `frame_token`. Must only be called
  // when processing messages received on the mojom::WebTestControlHost
  // interface from renderer processes. `frame_token` must correspond to a valid
  // RenderFrameHost.
  // The return value can not be null.
  WebContents* GetWebContentsFromCurrentContext(
      const blink::LocalFrameToken& frame_token);

  mojo::AssociatedRemote<mojom::WebTestRenderFrame>&
  GetWebTestRenderFrameRemote(RenderFrameHost* frame);
  void HandleWebTestRenderFrameRemoteError(const GlobalRenderFrameHostId& key);

  // CompositeAllFramesThen() first builds a frame tree based on
  // frame->GetParent(). Then, it builds a queue of frames in depth-first order,
  // so that compositing happens from the leaves up. Finally,
  // CompositeNodeQueueThen() is used to composite one frame at a time,
  // asynchronously, continuing on to the next frame once each composite
  // finishes. Once all nodes have been composited, the final callback is run.
  // Each call to CompositeWithRaster() is an asynchronous Mojo call, to avoid
  // reentrancy problems.
  void CompositeAllFramesThen(base::OnceCallback<void()> callback);

  Node* BuildFrameTree(WebContents* web_contents);
  void CompositeNodeQueueThen(base::OnceCallback<void()> callback);
  void BuildDepthFirstQueue(Node* node);

#if BUILDFLAG(IS_MAC)
  // Bypasses system APIs to force a resize on the RenderWidgetHostView when in
  // headless web tests.
  static void PlatformResizeWindowMac(Shell* shell, const gfx::Size& size);
#endif

  static WebTestControlHost* instance_;

  std::unique_ptr<WebTestResultPrinter> printer_;

  base::FilePath current_working_directory_;
  base::FilePath temp_path_;

  raw_ptr<Shell> main_window_ = nullptr;
  raw_ptr<Shell, DanglingUntriaged> secondary_window_ = nullptr;

  std::unique_ptr<WebTestDevToolsBindings> devtools_bindings_;
  std::unique_ptr<DevToolsProtocolTestBindings>
      devtools_protocol_test_bindings_;

  // What phase of running an individual test we are currently in.
  TestPhase test_phase_ = BETWEEN_TESTS;

  // Per test config.
  std::string expected_pixel_hash_;
  GURL test_url_;
  bool wpt_print_mode_;
  bool protocol_mode_ = false;

  // Stores the default test-adapted WebPreferences which is then used to fully
  // reset the main window's preferences if and when it is reused.
  blink::web_pref::WebPreferences default_prefs_;
  std::string default_accept_languages_;

  // True if the WebPreferences of newly created RenderViewHost should be
  // overridden with prefs_.
  bool should_override_prefs_ = false;
  blink::web_pref::WebPreferences prefs_;

  // When populated, simulate a LCPP backend by sending this hint data along
  // navigations (typically reload of the same page).
  // This is set by the LCPP web_tests via
  // NonAssociatedWebTestControlHost::SetLCPPNavigationHint mojom interface.
  // This is reset before switching to the next test page.
  std::optional<blink::mojom::LCPCriticalPathPredictorNavigationTimeHint>
      lcpp_hint_;

  bool crash_when_leak_found_ = false;
  std::unique_ptr<LeakDetector> leak_detector_;

  std::unique_ptr<WebTestBluetoothChooserFactory> bluetooth_chooser_factory_;

  // Observe windows opened by tests.
  base::flat_map<WebContents*, std::unique_ptr<WebTestWindowObserver>>
      test_opened_window_observers_;

  // Renderer processes are observed to detect crashes.
  base::ScopedMultiSourceObservation<RenderProcessHost,
                                     RenderProcessHostObserver>
      render_process_host_observations_{this};
  std::set<raw_ptr<RenderProcessHost, SetExperimental>>
      all_observed_render_process_hosts_;
  std::set<raw_ptr<RenderProcessHost, SetExperimental>>
      main_window_render_process_hosts_;
  std::set<raw_ptr<RenderViewHost, SetExperimental>>
      main_window_render_view_hosts_;

  // Changes reported by WebTestRuntimeFlagsChanged() that have accumulated
  // since PrepareForWebTest (i.e. changes that need to be sent to a fresh
  // renderer created while test is in progress).
  base::Value::Dict accumulated_web_test_runtime_flags_changes_;

  // A snasphot of the current runtime flags.
  WebTestRuntimeFlags web_test_runtime_flags_;

  // Work items to be processed in the TestRunner on the renderer process
  // that hosts the main window's main frame.
  base::circular_deque<mojom::WorkItemPtr> work_queue_;

  // Properties of the work queue.
  base::Value::Dict work_queue_states_;

  mojom::WebTestRendererDumpResultPtr renderer_dump_result_;
  std::string navigation_history_dump_;
  std::optional<SkBitmap> pixel_dump_;
  std::optional<std::string> layout_dump_;
  std::string actual_pixel_hash_;
  // By default a test that opens other windows will have them closed at the end
  // of the test before checking for leaks. It may specify that it has closed
  // any windows it opened, and thus look for leaks from them with this flag.
  bool check_for_leaked_windows_ = false;
  bool waiting_for_pixel_results_ = false;
  int waiting_for_layout_dumps_ = 0;

  // Map from frame_tree_node_id into frame-specific dumps while collecting
  // text dumps from all frames, before stitching them together.
  std::map<FrameTreeNodeId, std::string> frame_to_layout_dump_map_;

  std::vector<std::unique_ptr<Node>> composite_all_frames_node_storage_;
  std::queue<raw_ptr<Node, CtnExperimental>> composite_all_frames_node_queue_;

  // Map from one frame to one mojo pipe.
  std::map<GlobalRenderFrameHostId,
           mojo::AssociatedRemote<mojom::WebTestRenderFrame>>
      web_test_render_frame_map_;

  // The set of bindings that receive messages on the mojom::WebTestControlHost
  // interface from renderer processes. There should be one per renderer
  // process, and we store it with the |render_process_id| attached to it
  // so that we can tell where a given message came from if needed.
  mojo::AssociatedReceiverSet<mojom::WebTestControlHost,
                              int /*render_process_id*/>
      receiver_bindings_;

  mojo::ReceiverSet<mojom::NonAssociatedWebTestControlHost>
      non_associated_receiver_bindings_;

  base::ScopedTempDir writable_directory_for_tests_;

  std::optional<WebTestTracingController> tracing_controller_;

  enum class NextPointerLockAction {
    kWillSucceed,
    kTestWillRespond,
    kWillFail,
  };
  NextPointerLockAction next_pointer_lock_action_ =
      NextPointerLockAction::kWillSucceed;

  // When navigating to a new web test, the control host blocks the renderer
  // from starting the test while some initialization to a known state occurs
  // (e.g. flushing synthetic input events associated with navigation).
  //
  // It does so by resetting this bit to `true` at the end of each web test.
  // When this bit is set, the next ReadyToCommitNavigation seen will call
  // BlockTestUntilStart in the renderer to block the renderer parser,
  // preventing the test from running. When that navigation finishes in
  // DidFinishNavigation, this bit is turned off and this class performs
  // initialization. Once the initialization is complete, StartTest is run in
  // the renderer, unblocking the parser.
  bool next_non_blank_nav_is_new_test_ = true;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<WebTestControlHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_BROWSER_WEB_TEST_CONTROL_HOST_H_
