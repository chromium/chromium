// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/dump_accessibility_browsertest_base.h"

#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_event_recorder.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/dump_accessibility_test_helper.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ui_base_features.h"

namespace content {

namespace {

// Searches recursively and returns true if an accessibility node is found
// that represents a fully loaded web document with the given url.
bool AccessibilityTreeContainsLoadedDocWithUrl(BrowserAccessibility* node,
                                               const std::string& url) {
  if (node->GetRole() == ax::mojom::Role::kRootWebArea &&
      node->GetStringAttribute(ax::mojom::StringAttribute::kUrl) == url) {
    // Ensure the doc has finished loading and has a non-zero size.
    return node->manager()->GetTreeData().loaded &&
           (node->GetData().relative_bounds.bounds.width() > 0 &&
            node->GetData().relative_bounds.bounds.height() > 0);
  }
  if (node->GetRole() == ax::mojom::Role::kWebArea &&
      node->GetStringAttribute(ax::mojom::StringAttribute::kUrl) == url) {
    // Ensure the doc has finished loading.
    return node->manager()->GetTreeData().loaded;
  }

  for (unsigned i = 0; i < node->PlatformChildCount(); i++) {
    if (AccessibilityTreeContainsLoadedDocWithUrl(node->PlatformGetChild(i),
                                                  url)) {
      return true;
    }
  }
  return false;
}

}  // namespace

typedef AccessibilityTreeFormatter::PropertyFilter PropertyFilter;
typedef AccessibilityTreeFormatter::NodeFilter NodeFilter;

DumpAccessibilityTestBase::DumpAccessibilityTestBase()
    : formatter_factory_(nullptr),
      event_recorder_factory_(nullptr),
      enable_accessibility_after_navigating_(false) {}

DumpAccessibilityTestBase::~DumpAccessibilityTestBase() {}

void DumpAccessibilityTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  IsolateAllSitesForTesting(command_line);

  // Each test pass might require custom command-line setup
  auto passes = AccessibilityTreeFormatter::GetTestPasses();
  size_t current_pass = GetParam();
  CHECK_LT(current_pass, passes.size());
  if (passes[current_pass].set_up_command_line)
    passes[current_pass].set_up_command_line(command_line);
}

void DumpAccessibilityTestBase::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
}

void DumpAccessibilityTestBase::SetUp() {
  std::vector<base::Feature> enabled_features;
  std::vector<base::Feature> disabled_features;
  ChooseFeatures(&enabled_features, &disabled_features);

  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);

  // The <input type="color"> popup tested in
  // AccessibilityInputColorWithPopupOpen requires the ability to read pixels
  // from a Canvas, so we need to be able to produce pixel output.
  EnablePixelOutput();

  ContentBrowserTest::SetUp();
}

void DumpAccessibilityTestBase::ChooseFeatures(
    std::vector<base::Feature>* enabled_features,
    std::vector<base::Feature>* disabled_features) {

  // Enable exposing "display: none" nodes to the browser process for testing.
  enabled_features->emplace_back(
      features::kEnableAccessibilityExposeDisplayNone);

  // For the best test coverage during development of this feature, enable the
  // code that expposes document markers on AXInlineTextBox objects and the
  // corresponding code in AXPosition on the browser that collects those
  // markers.
  enabled_features->emplace_back(features::kUseAXPositionForDocumentMarkers);

  enabled_features->emplace_back(blink::features::kPortals);

  // TODO(dmazzoni): DumpAccessibilityTree expectations are based on the
  // assumption that the accessibility labels feature is off. (There are
  // also several tests that explicitly enable the feature.) It'd be better
  // if DumpAccessibilityTree tests assumed that the feature is on by
  // default instead.  http://crbug.com/940330
  disabled_features->emplace_back(features::kExperimentalAccessibilityLabels);
}

std::string
DumpAccessibilityTestBase::DumpUnfilteredAccessibilityTreeAsString() {
  std::unique_ptr<AccessibilityTreeFormatter> formatter(formatter_factory_());
  std::vector<PropertyFilter> property_filters;
  property_filters.emplace_back("*", PropertyFilter::ALLOW);
  formatter->SetPropertyFilters(property_filters);
  formatter->set_show_ids(true);
  std::string ax_tree_dump;
  formatter->FormatAccessibilityTreeForTesting(
      GetRootAccessibilityNode(shell()->web_contents()), &ax_tree_dump);
  return ax_tree_dump;
}

void DumpAccessibilityTestBase::ParseHtmlForExtraDirectives(
    const std::string& test_html,
    std::vector<std::string>* no_load_expected,
    std::vector<std::string>* wait_for,
    std::vector<std::string>* execute,
    std::vector<std::string>* run_until,
    std::vector<std::string>* default_action_on) {
  for (const std::string& line : base::SplitString(
           test_html, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    const std::string& allow_empty_str = formatter_->GetAllowEmptyString();
    const std::string& allow_str = formatter_->GetAllowString();
    const std::string& deny_str = formatter_->GetDenyString();
    const std::string& deny_node_str = formatter_->GetDenyNodeString();
    const std::string& no_load_expected_str = "@NO-LOAD-EXPECTED:";
    const std::string& wait_str = "@WAIT-FOR:";
    const std::string& execute_str = "@EXECUTE-AND-WAIT-FOR:";
    const std::string& until_str = formatter_->GetRunUntilEventString();
    const std::string& default_action_on_str = "@DEFAULT-ACTION-ON:";
    if (base::StartsWith(line, allow_empty_str, base::CompareCase::SENSITIVE)) {
      property_filters_.emplace_back(
          line.substr(allow_empty_str.size()), PropertyFilter::ALLOW_EMPTY);
    } else if (base::StartsWith(line, allow_str,
                                base::CompareCase::SENSITIVE)) {
      property_filters_.emplace_back(line.substr(allow_str.size()), PropertyFilter::ALLOW);
    } else if (base::StartsWith(line, deny_str, base::CompareCase::SENSITIVE)) {
      property_filters_.emplace_back(line.substr(deny_str.size()), PropertyFilter::DENY);
    } else if (base::StartsWith(line, deny_node_str,
                                base::CompareCase::SENSITIVE)) {
      const auto& node_filter = line.substr(deny_node_str.size());
      const auto& parts = base::SplitString(
          node_filter, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      // Silently skip over parsing errors like the rest of the enclosing code.
      if (parts.size() == 2) {
        node_filters_.emplace_back(parts[0], parts[1]);
      }
    } else if (base::StartsWith(line, no_load_expected_str,
                                base::CompareCase::SENSITIVE)) {
      no_load_expected->push_back(line.substr(no_load_expected_str.size()));
    } else if (base::StartsWith(line, wait_str, base::CompareCase::SENSITIVE)) {
      wait_for->push_back(line.substr(wait_str.size()));
    } else if (base::StartsWith(line, execute_str,
                                base::CompareCase::SENSITIVE)) {
      execute->push_back(line.substr(execute_str.size()));
    } else if (base::StartsWith(line, until_str,
                                base::CompareCase::SENSITIVE)) {
      run_until->push_back(line.substr(until_str.size()));
    } else if (base::StartsWith(line, default_action_on_str,
                                base::CompareCase::SENSITIVE)) {
      default_action_on->push_back(line.substr(default_action_on_str.size()));
    }
  }
}

void DumpAccessibilityTestBase::RunTest(const base::FilePath file_path,
                                        const char* file_dir) {
  // Get all the tree formatters; the test is run independently on each one.
  auto formatters = AccessibilityTreeFormatter::GetTestPasses();
  auto event_recorders = AccessibilityEventRecorder::GetTestPasses();
  CHECK(event_recorders.size() == formatters.size());

  // The current test number is supplied as a test parameter.
  size_t current_pass = GetParam();
  CHECK_LT(current_pass, formatters.size());
  CHECK_EQ(std::string(formatters[current_pass].name),
           std::string(event_recorders[current_pass].name));

  formatter_factory_ = formatters[current_pass].create_formatter;
  event_recorder_factory_ = event_recorders[current_pass].create_recorder;

  RunTestForPlatform(file_path, file_dir);

  formatter_factory_ = nullptr;
  event_recorder_factory_ = nullptr;
}

void DumpAccessibilityTestBase::RunTestForPlatform(
    const base::FilePath file_path,
    const char* file_dir) {
  formatter_ = formatter_factory_();
  DumpAccessibilityTestHelper test_helper(formatter_.get());

  // Disable the "hot tracked" state (set when the mouse is hovering over
  // an object) because it makes test output change based on the mouse position.
  BrowserAccessibilityStateImpl::GetInstance()
      ->set_disable_hot_tracking_for_testing(true);

  // Normally some accessibility events that would be fired are suppressed or
  // delayed, depending on what has focus or the type of event. For testing,
  // we want all events to fire immediately to make tests predictable and not
  // flaky.
  BrowserAccessibilityManager::NeverSuppressOrDelayEventsForTesting();

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  // Exit without running the test if we can't find an expectation file.
  // This is used to skip certain tests on certain platforms.
  // We have to check for this in advance in order to avoid waiting on a
  // WAIT-FOR directive in the source file that's looking for something not
  // supported on the current platform.
  base::FilePath expected_file = test_helper.GetExpectationFilePath(file_path);
  if (expected_file.empty()) {
    LOG(INFO) << "No expectation file present, ignoring test on this "
                 "platform.";
    return;
  }

  base::Optional<std::vector<std::string>> expected_lines =
      test_helper.LoadExpectationFile(expected_file);
  if (!expected_lines) {
    LOG(INFO) << "Skipping this test on this platform.";
    return;
  }

  std::string html_contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Exit without running the test if the source file is missing, since that
    // was the behavior prior to http://crrev.com/c/1661175.
    // It would be preferable if we were to fail the test instead.
    // http://crbug.com/975830
    if (!base::ReadFileToString(file_path, &html_contents)) {
      LOG(INFO) << "File not found: " << file_path.LossyDisplayName();
      LOG(INFO) << "Skipping test.";
      return;
    }
  }

  // Parse filters and other directives in the test file.
  std::vector<std::string> no_load_expected;
  std::vector<std::string> wait_for;
  std::vector<std::string> execute;
  std::vector<std::string> run_until;
  std::vector<std::string> default_action_on;
  property_filters_.clear();
  node_filters_.clear();
  formatter_->AddDefaultFilters(&property_filters_);
  AddDefaultFilters(&property_filters_);
  ParseHtmlForExtraDirectives(html_contents, &no_load_expected, &wait_for,
                              &execute, &run_until, &default_action_on);

  // Get the test URL.
  GURL url(embedded_test_server()->GetURL("/" + std::string(file_dir) + "/" +
                                          file_path.BaseName().MaybeAsASCII()));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  if (enable_accessibility_after_navigating_ &&
      web_contents->GetAccessibilityMode().is_mode_off()) {
    // Load the url, then enable accessibility.
    EXPECT_TRUE(NavigateToURL(shell(), url));
    AccessibilityNotificationWaiter accessibility_waiter(
        web_contents, ui::kAXModeComplete, ax::mojom::Event::kNone);
    accessibility_waiter.WaitForNotification();
  } else {
    // Enable accessibility, then load the test html and wait for the
    // "load complete" AX event.
    AccessibilityNotificationWaiter accessibility_waiter(
        web_contents, ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    accessibility_waiter.WaitForNotification();
  }

  // Perform default action on any elements specified by the test.
  for (const auto& str : default_action_on) {
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           ui::kAXModeComplete,
                                           ax::mojom::Event::kClicked);
    BrowserAccessibility* action_element;

    size_t parent_node_delimiter_index = str.find(",");
    if (parent_node_delimiter_index != std::string::npos) {
      auto node_name = str.substr(0, parent_node_delimiter_index);
      auto parent_node_name = str.substr(parent_node_delimiter_index + 1);

      BrowserAccessibility* parent_node = FindNode(parent_node_name);
      DCHECK(parent_node) << "Parent node name provided but not found";
      action_element = FindNode(node_name, parent_node);
    } else {
      action_element = FindNode(str);
    }

    ui::AXActionData action_data;
    action_data.action = ax::mojom::Action::kDoDefault;
    action_element->AccessibilityPerformAction(action_data);

    waiter.WaitForNotification();
  }

  WaitForAXTreeLoaded(web_contents, no_load_expected, wait_for);

  // Call the subclass to dump the output.
  std::vector<std::string> actual_lines = Dump(run_until);

  // Execute and wait for specified string
  for (const auto& function_name : execute) {
    DLOG(INFO) << "executing: " << function_name;
    base::Value result =
        ExecuteScriptAndGetValue(web_contents->GetMainFrame(), function_name);
    const std::string& str = result.is_string() ? result.GetString() : "";
    // If no string is specified, do not wait.
    bool wait_for_string = str != "";
    while (wait_for_string) {
      // Loop until specified string is found.
      std::string tree_dump = DumpUnfilteredAccessibilityTreeAsString();
      if (tree_dump.find(str) != std::string::npos) {
        wait_for_string = false;
        // Append an additional dump if the specified string was found.
        std::vector<std::string> additional_dump = Dump(run_until);
        actual_lines.emplace_back("=== Start Continuation ===");
        actual_lines.insert(actual_lines.end(), additional_dump.begin(),
                            additional_dump.end());
        break;
      }
      // Block until the next accessibility notification in any frame.
      VLOG(1) << "Still waiting on this text to be found: " << str;
      VLOG(1) << "Waiting until the next accessibility event";
      AccessibilityNotificationWaiter accessibility_waiter(
          web_contents, ui::AXMode(), ax::mojom::Event::kNone);
      accessibility_waiter.WaitForNotification();
    }
  }

  // Validate against the expectation file.
  bool matches_expectation = test_helper.ValidateAgainstExpectation(
      file_path, expected_file, actual_lines, *expected_lines);
  EXPECT_TRUE(matches_expectation);
  if (!matches_expectation)
    OnDiffFailed();
}

void DumpAccessibilityTestBase::WaitForAXTreeLoaded(
    WebContentsImpl* web_contents,
    const std::vector<std::string>& no_load_expected,
    const std::vector<std::string>& wait_for) {
  // Get the url of every frame in the frame tree.
  FrameTree* frame_tree = web_contents->GetFrameTree();
  std::vector<std::string> all_frame_urls;
  for (FrameTreeNode* node : frame_tree->Nodes()) {
    // Ignore about:blank urls because of the case where a parent frame A
    // has a child iframe B and it writes to the document using
    // contentDocument.open() on the child frame B.
    //
    // In this scenario, B's contentWindow.location.href matches A's url,
    // but B's url in the browser frame tree is still "about:blank".
    //
    // We also ignore frame tree nodes created for portals in the outer
    // WebContents as the node doesn't have a url set.

    std::string url = node->current_url().spec();

    // sometimes we expect a url to never load, in these cases, don't wait.
    bool skip_url = false;
    for (std::string no_load_url : no_load_expected) {
      if (url.find(no_load_url) != std::string::npos) {
        skip_url = true;
        break;
      }
    }
    if (!skip_url && url != url::kAboutBlankURL && !url.empty() &&
        node->frame_owner_element_type() !=
            blink::mojom::FrameOwnerElementType::kPortal) {
      all_frame_urls.push_back(url);
    }
  }

  // Wait for the accessibility tree to fully load for all frames,
  // by searching for the WEB_AREA node in the accessibility tree
  // with the url of each frame in our frame tree. Note that this
  // doesn't support cases where there are two iframes with the
  // exact same url. If all frames haven't loaded yet, set up a
  // listener for accessibility events on any frame and block
  // until the next one is received.
  //
  // If the original page has a @WAIT-FOR directive, don't break until
  // the text we're waiting for appears in the full text dump of the
  // accessibility tree, either.
  for (;;) {
    VLOG(1) << "Top of loop";
    RenderFrameHostImpl* main_frame =
        static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame());
    BrowserAccessibilityManager* manager =
        main_frame->browser_accessibility_manager();
    if (manager) {
      BrowserAccessibility* accessibility_root = manager->GetRoot();

      // Check to see if all frames have loaded.
      bool all_frames_loaded = true;
      for (const auto& url : all_frame_urls) {
        if (!AccessibilityTreeContainsLoadedDocWithUrl(accessibility_root,
                                                       url)) {
          VLOG(1) << "Still waiting on this frame to load: " << url;
          all_frames_loaded = false;
          break;
        }
      }

      // Check to see if the @WAIT-FOR text has appeared yet.
      bool all_wait_for_strings_found = true;
      std::string tree_dump = DumpUnfilteredAccessibilityTreeAsString();
      for (const auto& str : wait_for) {
        if (tree_dump.find(str) == std::string::npos) {
          VLOG(1) << "Still waiting on this text to be found: " << str;
          all_wait_for_strings_found = false;
          break;
        }
      }

      // If all frames have loaded and the @WAIT-FOR text has appeared,
      // we're done.
      if (all_frames_loaded && all_wait_for_strings_found)
        break;
    }

    // Block until the next accessibility notification in any frame.
    VLOG(1) << "Waiting until the next accessibility event";
    AccessibilityNotificationWaiter accessibility_waiter(
        web_contents, ui::kAXModeComplete, ax::mojom::Event::kNone);
    accessibility_waiter.WaitForNotification();
  }

  for (WebContents* inner_contents : web_contents->GetInnerWebContents()) {
    WaitForAXTreeLoaded(static_cast<WebContentsImpl*>(inner_contents),
                        no_load_expected, std::vector<std::string>());
  }
}

BrowserAccessibility* DumpAccessibilityTestBase::FindNode(
    const std::string& name,
    BrowserAccessibility* search_root) {
  if (!search_root)
    search_root = GetManager()->GetRoot();

  CHECK(search_root);
  BrowserAccessibility* node = FindNodeInSubtree(*search_root, name);
  return node;
}

BrowserAccessibilityManager* DumpAccessibilityTestBase::GetManager() {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  return web_contents->GetRootBrowserAccessibilityManager();
}

BrowserAccessibility* DumpAccessibilityTestBase::FindNodeInSubtree(
    BrowserAccessibility& node,
    const std::string& name) {
  if (node.GetStringAttribute(ax::mojom::StringAttribute::kName) == name)
    return &node;

  for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
    BrowserAccessibility* result =
        FindNodeInSubtree(*node.PlatformGetChild(i), name);
    if (result)
      return result;
  }
  return nullptr;
}

}  // namespace content
