// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/accessibility/android/accessibility_state.h"
#endif

namespace content {

namespace {

bool SkipUrlMatch(const std::vector<std::string>& skip_urls,
                  const std::string& url) {
  for (const auto& skip_url : skip_urls) {
    if (base::Contains(url, skip_url)) {
      return true;
    }
  }
  return false;
}

bool ShouldHaveChildTree(const ui::AXNode& node,
                         const std::vector<std::string>& skip_urls) {
  const ui::AXNodeData& data = node.data();
  if (data.GetRestriction() == ax::mojom::Restriction::kDisabled) {
    DCHECK(!data.HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId));
    return false;  // A disabled child tree owner won't have a child tree.
  }

  if (node.IsInvisibleOrIgnored()) {
    return false;
  }

  // If it has an embedding element role or a child tree id, then expect some
  // child tree content. In some cases IsEmbeddingElement(role) will be false,
  // if an ARIA role was used, e.g. <iframe role="region">.
  if (data.HasStringAttribute(ax::mojom::StringAttribute::kChildTreeId)) {
    return true;
  }
  if (!ui::IsEmbeddingElement(node.GetRole())) {
    return false;
  }
  std::string url = node.GetStringAttribute(ax::mojom::StringAttribute::kUrl);
  return (!url.empty() && !SkipUrlMatch(skip_urls, url));
}

class AXTreeChangeWaiter : public ui::AXTreeObserver {
 public:
  AXTreeChangeWaiter()
      : loop_runner_(std::make_unique<base::RunLoop>()),
        loop_runner_quit_closure_(loop_runner_->QuitClosure()) {}

  void OnStringAttributeChanged(ui::AXTree* tree,
                                ui::AXNode* node,
                                ax::mojom::StringAttribute attr,
                                const std::string& old_value,
                                const std::string& new_value) override {
    if (attr == ax::mojom::StringAttribute::kChildTreeId) {
      tree->RemoveObserver(this);
      loop_runner_quit_closure_.Run();
    }
  }

  void OnChildTreeConnectionChanged(ui::AXNode* host_node) override {
    host_node->tree()->RemoveObserver(this);
    loop_runner_quit_closure_.Run();
  }

  void WaitForChange(ui::AXTree* tree) {
    tree->AddObserver(this);
    loop_runner_->Run();
    loop_runner_.reset();
    loop_runner_quit_closure_.Reset();
  }

 private:
  std::unique_ptr<base::RunLoop> loop_runner_;
  base::RepeatingClosure loop_runner_quit_closure_;
};

void WaitForChildTrees(const ui::AXNode& node,
                       const std::vector<std::string>& skip_urls) {
  while (true) {
    size_t num_children = node.GetChildCountCrossingTreeBoundary();
    if (!num_children && ShouldHaveChildTree(node, skip_urls)) {
      AXTreeChangeWaiter waiter;
      waiter.WaitForChange(node.tree());
      continue;
    }

    // Any node that is the connection point for a child tree should have
    // exactly one child.
    DCHECK(!ShouldHaveChildTree(node, skip_urls) || num_children == 1u)
        << "AXNode (" << node << ") has an unexpected number of "
        << "children :" << num_children;

    for (size_t i = 0; i < num_children; i++) {
      WaitForChildTrees(*node.GetChildAtIndexCrossingTreeBoundary(i),
                        skip_urls);
    }
    break;
  }
}

bool IsLoadedDocWithUrl(const ui::BrowserAccessibility* node,
                        const std::string& url) {
  return node->GetRole() == ax::mojom::Role::kRootWebArea &&
         node->GetStringAttribute(ax::mojom::StringAttribute::kUrl) == url &&
         node->manager()->GetTreeData().loaded;
}

// Recursively searches accessibility nodes in the subtree of |node| that
// represent a fully loaded web document with the given |url|. If less than
// |num_expected| occurrences are found, it returns the remainder. Otherwise,
// it stops searching when reaching |num_expected| occurrences, and returns 0.
unsigned SearchLoadedDocsWithUrlInAccessibilityTree(
    const ui::BrowserAccessibility* node,
    const std::string& url,
    unsigned num_expected) {
  if (!num_expected) {
    return 0;
  }

  if (IsLoadedDocWithUrl(node, url)) {
    num_expected -= 1;
    if (!num_expected) {
      return 0;
    }
  }

  for (const auto* child : node->AllChildren()) {
    num_expected =
        SearchLoadedDocsWithUrlInAccessibilityTree(child, url, num_expected);
    if (!num_expected) {
      return 0;
    }
  }
  return num_expected;
}

}  // namespace

using ui::AXPropertyFilter;
using ui::AXTreeFormatter;

// DumpAccessibilityTestBase
DumpAccessibilityTestBase::DumpAccessibilityTestBase()
    : enable_accessibility_after_navigating_(false), test_helper_(GetParam()) {}

DumpAccessibilityTestBase::~DumpAccessibilityTestBase() {}

void DumpAccessibilityTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  IsolateAllSitesForTesting(command_line);
}

void DumpAccessibilityTestBase::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
}

void DumpAccessibilityTestBase::SetUp() {
  // Each test pass may require custom feature setup.
  test_helper_.InitializeFeatureList();

  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  ChooseFeatures(&enabled_features, &disabled_features);

  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

  // The <input type="color"> popup tested in
  // AccessibilityInputColorWithPopupOpen requires the ability to read pixels
  // from a Canvas, so we need to be able to produce pixel output.
  EnablePixelOutput();

  ContentBrowserTest::SetUp();
}

void DumpAccessibilityTestBase::TearDown() {
  ContentBrowserTest::TearDown();
  scoped_feature_list_.Reset();
  test_helper_.ResetFeatureList();
}

void DumpAccessibilityTestBase::SignalRunTestOnMainThread(int) {
  LOG(INFO) << "\n\nFinal accessibility tree upon the test termination:\n"
            << DumpUnfilteredAccessibilityTreeAsString();
}

void DumpAccessibilityTestBase::ChooseFeatures(
    std::vector<base::test::FeatureRef>* enabled_features,
    std::vector<base::test::FeatureRef>* disabled_features) {
  // For the best test coverage during development of this feature, enable the
  // code that expposes document markers on AXInlineTextBox objects and the
  // corresponding code in AXPosition on the browser that collects those
  // markers.
  enabled_features->emplace_back(features::kUseAXPositionForDocumentMarkers);
  // For improved test coverage ahead of a finch trial, enable the feature that
  // prunes redundant text for inline text boxes.
  enabled_features->emplace_back(
      features::kAccessibilityPruneRedundantInlineText);
  // For improved test coverage ahead of a finch trial, enable the feature that
  // prunes redundant (next|previous) on line IDs.
  enabled_features->emplace_back(
      features::kAccessibilityPruneRedundantInlineConnectivity);
}

std::string DumpAccessibilityTestBase::DumpTreeAsString() const {
  std::unique_ptr<AXTreeFormatter> formatter(CreateFormatter());
  formatter->SetPropertyFilters(scenario_.property_filters,
                                AXTreeFormatter::kFiltersDefaultSet);
  formatter->SetNodeFilters(scenario_.node_filters);
  std::string actual_contents =
      formatter->Format(GetRootAccessibilityNode(GetWebContents()));
  return base::EscapeNonASCII(actual_contents);
}

std::string
DumpAccessibilityTestBase::DumpUnfilteredAccessibilityTreeAsString() {
  std::unique_ptr<AXTreeFormatter> formatter(CreateFormatter());
  formatter->SetPropertyFilters({{"*", AXPropertyFilter::ALLOW}});
  formatter->set_show_ids(true);
  return formatter->Format(GetRootAccessibilityNode(GetWebContents()));
}

void DumpAccessibilityTestBase::RunTest(
    ui::AXMode mode,
    const base::FilePath file_path,
    const char* file_dir,
    const base::FilePath::StringType& expectations_qualifier) {
  RunTestForPlatform(mode, file_path, file_dir, expectations_qualifier);
}

void DumpAccessibilityTestBase::RunTest(
    const base::FilePath file_path,
    const char* file_dir,
    const base::FilePath::StringType& expectations_qualifier) {
  RunTestForPlatform(ui::kAXModeComplete, file_path, file_dir,
                     expectations_qualifier);
}

// TODO(accessibility) Consider renaming these things to
// WaitForAccessibiltiyClean(), Action::kRequestAccessibilityCleanNotification,
// Event::kAccessibilityClean, etc. because this can be used multiple times
// per test.
void DumpAccessibilityTestBase::WaitForEndOfTest(ui::AXMode mode) const {
  // To make sure we've handled all accessibility events, add a sentinel by
  // calling SignalEndOfTest on each frame and waiting for a kEndOfTest event
  // in response.
  auto hosts = content::CollectAllRenderFrameHosts(GetWebContents());
  for (auto* host : hosts) {
    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kSignalEndOfTest;
    host->AccessibilityPerformAction(action_data);
  }

  AccessibilityNotificationWaiter waiter(GetWebContents(), mode,
                                         ax::mojom::Event::kEndOfTest);
  ASSERT_TRUE(waiter.WaitForNotification(true));
}

void DumpAccessibilityTestBase::PerformAndWaitForDefaultActions(
    ui::AXMode mode) {
  // Only perform actions the first call, as they are only allowed once per
  // test, e.g. only perform the action once if this is  script is executed
  // multiple times.

  if (has_performed_default_actions_) {
    return;
  }

  has_performed_default_actions_ = true;

  // Perform default action on any elements specified by the test.
  for (const auto& str : scenario_.default_action_on) {
    // TODO(accessibility) Consider waiting for kEndOfTest instead (but change
    // the name to something more like kAccessibilityClean).
    AccessibilityNotificationWaiter waiter(GetWebContents(), mode,
                                           ax::mojom::Event::kClicked);
    ui::BrowserAccessibility* action_element;

    // TODO(accessibility) base/strings/string_split.h might be cleaner here.
    size_t parent_node_delimiter_index = str.find(",");
    if (parent_node_delimiter_index != std::string::npos) {
      auto node_name = str.substr(0, parent_node_delimiter_index);
      auto parent_node_name = str.substr(parent_node_delimiter_index + 1);

      ui::BrowserAccessibility* parent_node = FindNode(parent_node_name);
      DCHECK(parent_node) << "Parent node name provided but not found";
      action_element = FindNode(node_name, parent_node);
    } else {
      action_element = FindNode(str);
    }

    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kDoDefault;
    action_element->AccessibilityPerformAction(action_data);

    ASSERT_TRUE(waiter.WaitForNotification());
  }
}

void DumpAccessibilityTestBase::WaitForExpectedText(ui::AXMode mode) {
  // If the original page has a @WAIT-FOR directive, don't break until
  // the text we're waiting for appears in the full text dump of the
  // accessibility tree, either.

  for (;;) {
    VLOG(1) << "Top of WaitForExpectedText() loop";
    // Check to see if the @WAIT-FOR text has appeared yet.
    bool all_wait_for_strings_found = true;
    std::string tree_dump = DumpTreeAsString();
    for (const auto& str : scenario_.wait_for) {
      if (!base::Contains(tree_dump, str)) {
        VLOG(1) << "Still waiting on this text to be found: " << str;
        all_wait_for_strings_found = false;
        break;
      }
    }

    // If the @WAIT-FOR text has appeared, we're done.
    if (all_wait_for_strings_found) {
      break;
    }

    // Block until the next accessibility notification in any frame.
    VLOG(1) << "Waiting until the next accessibility event";
    AccessibilityNotificationWaiter accessibility_waiter(GetWebContents());
    ASSERT_TRUE(accessibility_waiter.WaitForNotification());
  }
}

void DumpAccessibilityTestBase::WaitForFinalTreeContents(ui::AXMode mode) {
  // If @DEFAULT-ACTION-ON:[name] is used, perform the action and wait until it
  // is complete.
  PerformAndWaitForDefaultActions(mode);

  if (scenario_.wait_for.size()) {
    // Wait for expected text from @WAIT-FOR.
    WaitForExpectedText(mode);
  } else {
    // Wait until all accessibility events and dirty objects have been
    // processed.
    WaitForEndOfTest(mode);
  }
}

void DumpAccessibilityTestBase::RunTestForPlatform(
    ui::AXMode mode,
    const base::FilePath file_path,
    const char* file_dir,
    const base::FilePath::StringType& expectations_qualifier) {
  // Ignore the hovered state (set when the mouse is hovering over
  // an object) because it makes test output change based on the mouse position.
  ui::BrowserAccessibility::ignore_hovered_state_for_testing_ = true;

  // For Android, set a consistent user preference for how password display.
#if BUILDFLAG(IS_ANDROID)
  ui::AccessibilityState::ForceRespectDisplayedPasswordTextForTesting();
#endif

  // Normally some accessibility events that would be fired are suppressed or
  // delayed, depending on what has focus or the type of event. For testing,
  // we want all events to fire immediately to make tests predictable and not
  // flaky.
  ui::BrowserAccessibilityManager::NeverSuppressOrDelayEventsForTesting();

  // Enable the behavior whereby all focused nodes will be exposed to the
  // platform accessibility layer. This behavior is currently disabled in
  // production code, but is enabled in tests so that it could be tested
  // thoroughly before it is turned on for all code.
  // TODO(nektar): Turn this on in a followup patch.
  // ui::AXTree::SetFocusedNodeShouldNeverBeIgnored();

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  std::optional<ui::AXInspectScenario> scenario =
      test_helper_.ParseScenario(file_path, DefaultFilters());
  if (!scenario) {
    ADD_FAILURE()
        << "Failed to process a testing file. The file might not exist: "
        << file_path.LossyDisplayName();
    return;
  }
  scenario_ = std::move(*scenario);

  std::optional<std::vector<std::string>> expected_lines;

  // Get expectation lines from expectation file if any.
  base::FilePath expected_file =
      test_helper_.GetExpectationFilePath(file_path, expectations_qualifier);
  if (!expected_file.empty()) {
    expected_lines = test_helper_.LoadExpectationFile(expected_file);
  }

  // Get the test URL.
  GURL url(embedded_test_server()->GetURL(
      "a.test",
      "/" + std::string(file_dir) + "/" + file_path.BaseName().MaybeAsASCII()));
  WebContentsImpl* web_contents = GetWebContents();

  // Start with no AXMode, so that in case the test was run with
  // --force-renderer-accessibility, we can still set the correct mode for the
  // test, e.g. form controls mode.
  BrowserAccessibilityState::GetInstance()->DisableAccessibility();

  if (enable_accessibility_after_navigating_ &&
      web_contents->GetAccessibilityMode().is_mode_off()) {
    // Load the url, then enable accessibility.
    EXPECT_TRUE(NavigateToURL(shell(), url));
    AccessibilityNotificationWaiter accessibility_waiter(
        web_contents, mode, ax::mojom::Event::kNone);
    static_cast<BrowserAccessibilityStateImpl*>(
        BrowserAccessibilityState::GetInstance())
        ->SetAXModeChangeAllowed(false);
    ASSERT_TRUE(accessibility_waiter.WaitForNotification());
  } else {
    // Enable accessibility, then load the test html and wait for the
    // "load complete" AX event.
    AccessibilityNotificationWaiter accessibility_waiter(
        web_contents, mode, ax::mojom::Event::kLoadComplete);
    static_cast<BrowserAccessibilityStateImpl*>(
        BrowserAccessibilityState::GetInstance())
        ->SetAXModeChangeAllowed(false);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    // TODO(crbug.com/40844856): Investigate why this does not return
    // true.
    ASSERT_TRUE(accessibility_waiter.WaitForNotification());
  }

  WaitForAllFramesLoaded(mode);

  // Call the subclass to dump the output.
  std::vector<std::string> actual_lines = Dump(mode);

  // Execute and wait for specified string
  for (const auto& function_name : scenario_.execute) {
    DLOG(INFO) << "executing: " << function_name;
    const std::string str =
        EvalJs(web_contents->GetPrimaryMainFrame(), function_name)
            .ExtractString();
    // If no string is specified, do not wait.
    bool wait_for_string = str != "";
    while (wait_for_string) {
      // Loop until specified string is found.
      std::string tree_dump = DumpUnfilteredAccessibilityTreeAsString();
      if (base::Contains(tree_dump, str)) {
        wait_for_string = false;
        // Append an additional dump if the specified string was found.
        std::vector<std::string> additional_dump = Dump(mode);
        actual_lines.emplace_back("=== Start Continuation ===");
        actual_lines.insert(actual_lines.end(), additional_dump.begin(),
                            additional_dump.end());
        break;
      }
      // Block until the next accessibility notification in any frame.
      VLOG(1) << "Still waiting on this text to be found: " << str;
      VLOG(1) << "Waiting until the next accessibility event";
      // TODO(aleventhal) Try waiting for kEndOfTest to make sure all events
      // after code execution are captured.
      AccessibilityNotificationWaiter accessibility_waiter(web_contents);
      ASSERT_TRUE(accessibility_waiter.WaitForNotification());
    }
  }

  // No expected lines indicate the test is marked to skip the expectations
  // checks or it has no expectation file. If we reach this point, then it means
  // no crashes during the test run and we can consider the test as succeeding.
  if (!expected_lines) {
    EXPECT_TRUE(true);
    return;
  }

  // Validate against the expectation file.
  bool matches_expectation = test_helper_.ValidateAgainstExpectation(
      file_path, expected_file, actual_lines, *expected_lines);
  EXPECT_TRUE(matches_expectation);
  if (!matches_expectation) {
    OnDiffFailed();
  }
}

std::map<std::string, unsigned> DumpAccessibilityTestBase::CollectAllFrameUrls(
    const std::vector<std::string>& skip_urls) {
  std::map<std::string, unsigned> all_frame_urls;
  // Get the url of every frame in the frame tree.
  for (FrameTreeNode* node : GetWebContents()->GetPrimaryFrameTree().Nodes()) {
    // Ignore about:blank urls because of the case where a parent frame A
    // has a child iframe B and it writes to the document using
    // contentDocument.open() on the child frame B.
    //
    // In this scenario, B's contentWindow.location.href matches A's url,
    // but B's url in the browser frame tree is still "about:blank".

    std::string url = node->current_url().spec();
    if (url != url::kAboutBlankURL && url != url::kAboutSrcdocURL &&
        !url.empty() && !SkipUrlMatch(skip_urls, url)) {
      all_frame_urls[url] += 1;
    }
  }
  return all_frame_urls;
}

void DumpAccessibilityTestBase::WaitForAllFramesLoaded(ui::AXMode mode) {
  // Wait for the accessibility tree to fully load for all frames,
  // by searching for the WEB_AREA node in the accessibility tree
  // with the url of each frame in our frame tree. If all frames
  // haven't loaded yet, set up a listener for accessibility events
  // on any frame and block until the next one is received.
  WebContentsImpl* web_contents = GetWebContents();
  for (;;) {
    VLOG(1) << "Top of WaitForAllFramesLoaded() loop";
    RenderFrameHostImpl* main_frame =
        static_cast<RenderFrameHostImpl*>(web_contents->GetPrimaryMainFrame());
    ui::BrowserAccessibilityManager* manager =
        main_frame->browser_accessibility_manager();
    if (manager) {
      ui::BrowserAccessibility* accessibility_root =
          manager->GetBrowserAccessibilityRoot();

      WaitForChildTrees(*accessibility_root->node(),
                        scenario_.no_load_expected);

      bool all_expected_urls_loaded = true;
      // A test may change the url for a frame, for example by setting
      // window.location.href, so collect the current list of urls.
      const std::map<std::string, unsigned> all_frame_urls =
          CollectAllFrameUrls(scenario_.no_load_expected);
      for (const auto& [url, num_expected] : all_frame_urls) {
        if (unsigned num_remaining = SearchLoadedDocsWithUrlInAccessibilityTree(
                accessibility_root, url, num_expected)) {
          VLOG(1) << "Still waiting on " << num_remaining
                  << " frame(s) to load: " << url;
          all_expected_urls_loaded = false;
          break;
        }
      }
      if (all_expected_urls_loaded) {
        break;
      }
    }

    // Block until the next accessibility notification in any frame.
    VLOG(1) << "Waiting until the next accessibility event";
    AccessibilityNotificationWaiter accessibility_waiter(web_contents);
    ASSERT_TRUE(accessibility_waiter.WaitForNotification());
  }
}

ui::BrowserAccessibility* DumpAccessibilityTestBase::FindNode(
    const std::string& name,
    ui::BrowserAccessibility* search_root) const {
  if (!search_root) {
    search_root = GetManager()->GetBrowserAccessibilityRoot();
  }

  CHECK(search_root);
  ui::BrowserAccessibility* node = FindNodeInSubtree(*search_root, name);
  return node;
}

ui::BrowserAccessibilityManager* DumpAccessibilityTestBase::GetManager() const {
  return GetWebContents()->GetRootBrowserAccessibilityManager();
}

WebContentsImpl* DumpAccessibilityTestBase::GetWebContents() const {
  return static_cast<WebContentsImpl*>(shell()->web_contents());
}

std::unique_ptr<AXTreeFormatter> DumpAccessibilityTestBase::CreateFormatter()
    const {
  return AXInspectFactory::CreateFormatter(GetParam());
}

std::pair<EvalJsResult, std::vector<std::string>>
DumpAccessibilityTestBase::CaptureEvents(InvokeAction invoke_action,
                                         ui::AXMode mode) {
  // Create a new Event Recorder for the run.
  ui::BrowserAccessibilityManager* manager = GetManager();
  ui::AXTreeSelector selector(manager->GetBrowserAccessibilityRoot()
                                  ->GetTargetForNativeAccessibilityEvent());
  std::unique_ptr<ui::AXEventRecorder> event_recorder =
      AXInspectFactory::CreateRecorder(GetParam(), manager,
                                       base::GetCurrentProcId(), selector);
  event_recorder->SetOnlyWebEvents(true);

  event_recorder->ListenToEvents(base::BindRepeating(
      &DumpAccessibilityTestBase::OnEventRecorded, base::Unretained(this)));

  LOG(INFO) << "-------------- Start listening to events --------------";

  // If @DEFAULT-ACTION-ON:[name] is used, perform the action and wait until
  // it is complete.
  PerformAndWaitForDefaultActions(mode);

  // Create a waiter that waits for any one accessibility event.
  // This will ensure that after calling the go() function, we
  // block until we've received an accessibility event generated as
  // a result of this function.
  AccessibilityNotificationWaiter waiter(GetWebContents());

  // Run any script, e.g. go().
  // If an action was performed, we already waited for the kClicked event in
  // PerformAndWaitForDefaultActions(), which means the action is already
  // completed.
  EvalJsResult action_result = std::move(invoke_action).Run();

  // If we didn't already wait for a default action to complete, then
  // wait for at least one event. This may unblock either when |waiter|
  // observes either an ax::mojom::Event or ui::AXEventGenerator::Event, or
  // when |event_recorder| records a platform event.
  // TODO(crbug.com/40844856): Investigate why this does not return
  // true.
  if (scenario_.default_action_on.empty()) {
    EXPECT_TRUE(waiter.WaitForNotification());
  }

  // More than one accessibility event could have been generated.
  // To make sure we've received all accessibility events, add a
  // sentinel by calling SignalEndOfTest and waiting for a kEndOfTest
  // event in response.
  WaitForEndOfTest(mode);
  event_recorder->WaitForDoneRecording();

  LOG(INFO) << "-------------- Stop listening to events --------------";

  // Dump the event logs, running them through any filters specified
  // in the HTML file.
  std::vector<std::string> event_logs = event_recorder->GetEventLogs();

  // Sort the logs so that results are predictable. There are too many
  // nondeterministic things that affect the exact order of events fired,
  // so these tests shouldn't be used to make assertions about event order.
  std::sort(event_logs.begin(), event_logs.end());

  return std::make_pair(std::move(action_result), std::move(event_logs));
}

ui::BrowserAccessibility* DumpAccessibilityTestBase::FindNodeInSubtree(
    ui::BrowserAccessibility& node,
    const std::string& name) const {
  if (node.GetStringAttribute(ax::mojom::StringAttribute::kName) == name) {
    return &node;
  }

  for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
    ui::BrowserAccessibility* result =
        FindNodeInSubtree(*node.PlatformGetChild(i), name);
    if (result) {
      return result;
    }
  }
  return nullptr;
}

ui::BrowserAccessibility* DumpAccessibilityTestBase::FindNodeByStringAttribute(
    const ax::mojom::StringAttribute attr,
    const std::string& value) const {
  ui::BrowserAccessibility* root = GetManager()->GetBrowserAccessibilityRoot();

  CHECK(root);
  return FindNodeByStringAttributeInSubtree(*root, attr, value);
}

ui::BrowserAccessibility*
DumpAccessibilityTestBase::FindNodeByStringAttributeInSubtree(
    ui::BrowserAccessibility& node,
    const ax::mojom::StringAttribute attr,
    const std::string& value) const {
  if (node.GetStringAttribute(attr) == value) {
    return &node;
  }

  for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
    if (ui::BrowserAccessibility* result = FindNodeByStringAttributeInSubtree(
            *node.PlatformGetChild(i), attr, value)) {
      return result;
    }
  }
  return nullptr;
}

void DumpAccessibilityTestBase::UseHttpsTestServer() {
  https_test_server_ = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_test_server_.get()->AddDefaultHandlers(GetTestDataFilePath());
  https_test_server_.get()->SetSSLConfig(
      net::EmbeddedTestServer::CERT_TEST_NAMES);
}

}  // namespace content
