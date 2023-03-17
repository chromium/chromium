// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_BROWSERTEST_BASE_H_
#define CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_BROWSERTEST_BASE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/cxx20_erase_list.h"
#include "base/files/file_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/platform/inspect/ax_api_type.h"
#include "ui/accessibility/platform/inspect/ax_inspect_scenario.h"
#include "ui/accessibility/platform/inspect/ax_inspect_test_helper.h"

namespace content {

class BrowserAccessibility;
class BrowserAccessibilityManager;

// Base class for an accessibility browsertest that takes an HTML file as
// input, loads it into a tab, dumps some accessibility data in text format,
// then compares that text to an expectation file in the same directory.
//
// The system was inspired by WebKit/Blink LayoutTests, but customized for
// testing accessibility in Chromium.
//
// See content/test/data/accessibility/readme.md for an overview.
class DumpAccessibilityTestBase
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<ui::AXApiType::Type> {
 public:
  DumpAccessibilityTestBase();
  ~DumpAccessibilityTestBase() override;

  void SignalRunTestOnMainThread(int) override;

  // Given a path to an HTML file relative to the test directory,
  // loads the HTML, loads the accessibility tree, calls Dump(), then
  // compares the output to the expected result and has the test succeed
  // or fail based on the diff.
  // Run with given AXMode.
  void RunTest(ui::AXMode mode,
               const base::FilePath file_path,
               const char* file_dir,
               const base::FilePath::StringType& expectations_qualifier =
                   FILE_PATH_LITERAL(""));

  // Given a path to an HTML file relative to the test directory,
  // loads the HTML, loads the accessibility tree, calls Dump(), then
  // compares the output to the expected result and has the test succeed
  // or fail based on the diff.
  // Run with default kAXModeComplete.
  void RunTest(const base::FilePath file_path,
               const char* file_dir,
               const base::FilePath::StringType& expectations_qualifier =
                   FILE_PATH_LITERAL(""));

  template <const char* type>
  void RunTypedTest(const base::FilePath::CharType* file_path,
                    ui::AXMode mode = ui::kAXModeComplete) {
    base::FilePath test_path = GetTestFilePath("accessibility", type);
    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(base::PathExists(test_path)) << test_path.LossyDisplayName();
    }
    base::FilePath test_file = test_path.Append(base::FilePath(file_path));

    std::string dir(std::string() + "accessibility/" + type);
    RunTest(mode, test_file, dir.c_str());
  }

  template <std::vector<ui::AXApiType::Type> TestPasses(),
            ui::AXApiType::TypeConstant type>
  static std::vector<ui::AXApiType::Type> TestPassesExcept() {
    std::vector<ui::AXApiType::Type> passes = TestPasses();
    base::Erase(passes, type);
    return passes;
  }

  template <ui::AXApiType::TypeConstant type>
  static std::vector<ui::AXApiType::Type> TreeTestPassesExcept() {
    return TestPassesExcept<ui::AXInspectTestHelper::TreeTestPasses, type>();
  }

  template <ui::AXApiType::TypeConstant type>
  static std::vector<ui::AXApiType::Type> EventTestPassesExcept() {
    return TestPassesExcept<ui::AXInspectTestHelper::EventTestPasses, type>();
  }

  static std::vector<ui::AXApiType::Type> TreeTestPassesExceptUIA() {
    return TreeTestPassesExcept<ui::AXApiType::kWinUIA>();
  }

  static std::vector<ui::AXApiType::Type> EventTestPassesExceptUIA() {
    return EventTestPassesExcept<ui::AXApiType::kWinUIA>();
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void SetUp() override;

  //
  // For subclasses to override:
  //

  // This is called by RunTest after the document has finished loading,
  // including the load complete accessibility event. The subclass should
  // dump whatever that specific test wants to dump, returning the result
  // as a sequence of strings.
  virtual std::vector<std::string> Dump(ui::AXMode mode) = 0;

  // Add the default filters that are applied to all tests.
  virtual std::vector<ui::AXPropertyFilter> DefaultFilters() const = 0;

  // This gets called if the diff didn't match; the test can print
  // additional useful info.
  virtual void OnDiffFailed() {}

  // Choose which feature flags to enable or disable.
  virtual void ChooseFeatures(
      std::vector<base::test::FeatureRef>* enabled_features,
      std::vector<base::test::FeatureRef>* disabled_features);

  //
  // Helpers
  //

  // Dump the accessibility tree with all provided filters into a string.
  std::string DumpTreeAsString() const;

  // Dump the whole accessibility tree, without applying any filters,
  // and return it as a string.
  std::string DumpUnfilteredAccessibilityTreeAsString();

  void RunTestForPlatform(ui::AXMode mode,
                          const base::FilePath file_path,
                          const char* file_dir,
                          const base::FilePath::StringType&
                              expectations_qualifier = FILE_PATH_LITERAL(""));

  // Retrieve the accessibility node that matches the accessibility name. There
  // is an optional search_root parameter that defaults to the document root if
  // not provided.
  BrowserAccessibility* FindNode(
      const std::string& name,
      BrowserAccessibility* search_root = nullptr) const;

  // Retrieve the browser accessibility manager object for the current web
  // contents.
  BrowserAccessibilityManager* GetManager() const;

  std::unique_ptr<ui::AXTreeFormatter> CreateFormatter() const;

  // Returns a list of captured events fired after the invoked action.
  using InvokeAction = base::OnceCallback<base::Value()>;
  std::pair<base::Value, std::vector<std::string>> CaptureEvents(
      InvokeAction invoke_action,
      ui::AXMode mode);

  // Test scenario loaded from the test file.
  ui::AXInspectScenario scenario_;

  // Whether we should enable accessibility after navigating to the page,
  // otherwise we enable it first.
  bool enable_accessibility_after_navigating_;

  base::test::ScopedFeatureList scoped_feature_list_;

  bool HasHtmlAttribute(BrowserAccessibility& node,
                        const char* attr,
                        const std::string& value) const;

  BrowserAccessibility* FindNodeByHTMLAttribute(const char* attr,
                                                const std::string& value) const;

 protected:
  ui::AXInspectTestHelper test_helper_;

  WebContentsImpl* GetWebContents() const;

  // Wait until all accessibility events and dirty objects have been processed
  // with the given AXMode.
  void WaitForEndOfTest(ui::AXMode mode) const;

  // Perform any requested default actions and wait until a notification is
  // received that each action is performed with the given AXMode.
  void PerformAndWaitForDefaultActions(ui::AXMode mode);

  // Support the @WAIT-FOR directive (node, tree tests only) with the given
  // AXMode.
  void WaitForExpectedText(ui::AXMode mode);

  // Wait for default action, expected text and then end of test signal with the
  // given AXMode.
  void WaitForFinalTreeContents(ui::AXMode mode);

  // Creates a new secure test server that can be used in place of the default
  // HTTP embedded_test_server defined in BrowserTestBase. The new test server
  // can then be retrieved using the same embedded_test_server() method used
  // to get the BrowserTestBase HTTP server.
  void UseHttpsTestServer();

  // This will return either the https test server or the
  // default one specified in BrowserTestBase, depending on if an https test
  // server was created by calling UseHttpsTestServer().
  net::EmbeddedTestServer* embedded_test_server() {
    return (https_test_server_) ? https_test_server_.get()
                                : BrowserTestBase::embedded_test_server();
  }

 private:
  BrowserAccessibility* FindNodeInSubtree(BrowserAccessibility& node,
                                          const std::string& name) const;

  BrowserAccessibility* FindNodeByHTMLAttributeInSubtree(
      BrowserAccessibility& node,
      const char* attr,
      const std::string& value) const;

  std::map<std::string, unsigned> CollectAllFrameUrls(
      const std::vector<std::string>& skip_urls);

  // Wait until all initial content is completely loaded, included within
  // subframes, objects and portals with given AXMode.
  void WaitForAllFramesLoaded(ui::AXMode mode);

  void OnEventRecorded(const std::string& event) const {
    VLOG(1) << "++ Platform event: " << event;
  }

  bool has_performed_default_actions_ = false;

  // Secure test server, isn't created by default. Needs to be
  // created using UseHttpsTestServer() and then called with
  // embedded_test_server().
  std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_DUMP_ACCESSIBILITY_BROWSERTEST_BASE_H_
