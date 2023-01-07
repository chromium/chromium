// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_view.h"

#include <stddef.h>

#include <sstream>

#include "ppapi/c/pp_time.h"
#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(View);

TestView::TestView(TestingInstance* instance)
    : TestCase(instance),
      post_quit_on_view_changed_(false) {
}

void TestView::DidChangeView(const pp::View& view) {
  last_view_ = view;
  page_visibility_log_.push_back(view.IsPageVisible());

  if (post_quit_on_view_changed_) {
    post_quit_on_view_changed_ = false;
    testing_interface_->QuitMessageLoop(instance_->pp_instance());
  }
}

bool TestView::Init() {
  return CheckTestingInterface();
}

void TestView::RunTests(const std::string& filter) {
  RUN_TEST(CreatedVisible, filter);
  RUN_TEST(CreatedInvisible, filter);
  RUN_TEST(PageHideShow, filter);
  RUN_TEST(SizeChange, filter);
  RUN_TEST(ClipChange, filter);
  RUN_TEST(ScrollOffsetChange, filter);
}

bool TestView::WaitUntilViewChanged() {
  size_t old_page_visibility_change_count = page_visibility_log_.size();

  // Run a nested run loop. It will exit either on ViewChanged or if the
  // timeout happens.
  post_quit_on_view_changed_ = true;
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  post_quit_on_view_changed_ = false;

  // We know we got a view changed event if something was appended to the log.
  return page_visibility_log_.size() > old_page_visibility_change_count;
}

void TestView::QuitMessageLoop(int32_t result) {
  testing_interface_->QuitMessageLoop(instance_->pp_instance());
}

std::string TestView::TestCreatedVisible() {
  ASSERT_FALSE(page_visibility_log_.empty());
  ASSERT_TRUE(page_visibility_log_[0]);
  PASS();
}

std::string TestView::TestCreatedInvisible() {
  ASSERT_FALSE(page_visibility_log_.empty());

  if (page_visibility_log_[0]) {
    // Add more error message since this test has some extra requirements.
    instance_->AppendError("Initial page is set to visible. NOTE: "
        "This test must be run in a background tab. "
        "Either run in the UI test which does this, or you can middle-click "
        "on the test link to run manually.");
  }
  ASSERT_FALSE(page_visibility_log_[0]);
  PASS();
}

std::string TestView::TestPageHideShow() {
  // Initial state should be visible.
  ASSERT_FALSE(page_visibility_log_.empty());
  ASSERT_TRUE(page_visibility_log_[0]);

  // Now that we're alive, tell the test knows it can change our visibility.
  instance_->ReportProgress("TestPageHideShow:Created");

  // Wait until we get a hide event, being careful to handle spurious
  // notifications of ViewChanged.
  while (WaitUntilViewChanged() && page_visibility_log_.back()) {
  }
  if (page_visibility_log_.back()) {
    // Didn't get a view changed event that changed visibility (though there
    // may have been some that didn't change visibility).
    // Add more error message since this test has some extra requirements.
    return "Didn't receive a hide event in timeout. NOTE: "
        "This test requires tab visibility to change and won't pass if you "
        "just run it in a browser. Normally the UI test should handle "
        "this. You can also run manually by waiting 2 secs, creating a new "
        "tab, waiting 2 more secs, and closing the new tab.";
  }

  // Tell the test so it can show us again.
  instance_->ReportProgress("TestPageHideShow:Hidden");

  // Wait until we get a show event.
  while (WaitUntilViewChanged() && !page_visibility_log_.back()) {
  }
  ASSERT_TRUE(page_visibility_log_.back());

  PASS();
}

std::string TestView::TestSizeChange() {
  pp::Rect original_rect = last_view_.GetRect();

  pp::Rect desired_rect = original_rect;
  desired_rect.set_width(original_rect.width() + 10);
  desired_rect.set_height(original_rect.height() + 12);

  std::ostringstream script_stream;
  script_stream << "var plugin = document.getElementById('plugin');";
  script_stream << "plugin.setAttribute('width', "
                << desired_rect.width() << ");";
  script_stream << "plugin.setAttribute('height', "
                << desired_rect.height() << ");";

  instance_->EvalScript(script_stream.str());

  while (WaitUntilViewChanged() && last_view_.GetRect() != desired_rect) {
  }
  ASSERT_TRUE(last_view_.GetRect() == desired_rect);

  PASS();
}

std::string TestView::TestClipChange() {
  pp::Rect original_rect = last_view_.GetRect();

  // Original clip should be the full frame.
  pp::Rect original_clip = last_view_.GetClipRect();
  ASSERT_TRUE(original_clip.x() == 1);
  ASSERT_TRUE(original_clip.y() == 1);
  ASSERT_TRUE(original_clip.width() == original_rect.width());
  ASSERT_TRUE(original_clip.height() == original_rect.height());

  int clip_amount = original_rect.height() / 2;

  // It might be nice to set the position to be absolute and set the location,
  // but this will cause WebKit to actually tear down the plugin and recreate
  // it. So instead we add a big div to cause the document to be scrollable,
  // and scroll it down.
  std::ostringstream script_stream;
  script_stream
      << "var big = document.createElement('div');"
      << "big.setAttribute('style', 'position:absolute; left:100px; "
                                    "top:0px; width:1px; height:5000px;');"
      << "document.body.appendChild(big);"
      << "window.scrollBy(0, " << original_rect.y() + clip_amount << ");";

  instance_->EvalScript(script_stream.str());

  pp::Rect desired_clip = original_clip;
  desired_clip.set_y(clip_amount + original_clip.y());
  desired_clip.set_height(
    desired_clip.height() - desired_clip.y() +  original_clip.y());

  while (WaitUntilViewChanged() && last_view_.GetClipRect() != desired_clip) {
  }
  ASSERT_TRUE(last_view_.GetClipRect() == desired_clip);
  PASS();
}

std::string TestView::TestScrollOffsetChange() {
  instance_->EvalScript("document.body.style.width = '5000px';"
                        "document.body.style.height = '5000px';");
  instance_->EvalScript("window.scrollTo(5, 1);");

  while (WaitUntilViewChanged() &&
         last_view_.GetScrollOffset() != pp::Point(5, 1)) {
  }
  ASSERT_EQ(pp::Point(5, 1), last_view_.GetScrollOffset());

  instance_->EvalScript("window.scrollTo(0, 0);");

  while (WaitUntilViewChanged() &&
         last_view_.GetScrollOffset() != pp::Point(0, 0)) {
  }
  ASSERT_EQ(pp::Point(0, 0), last_view_.GetScrollOffset());

  PASS();
}
