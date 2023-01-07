// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_printing.h"

#include "ppapi/cpp/dev/printing_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/tests/testing_instance.h"

namespace {
bool g_callback_triggered;
int32_t g_callback_result;
PP_PrintSettings_Dev g_print_settings;
}  // namespace

REGISTER_TEST_CASE(Printing);

class TestPrinting_Dev : public pp::Printing_Dev {
 public:
  explicit TestPrinting_Dev(pp::Instance* instance) :
  pp::Printing_Dev(instance) {}
  virtual ~TestPrinting_Dev() {}
  virtual uint32_t QuerySupportedPrintOutputFormats() { return 0; }
  virtual int32_t PrintBegin(
      const PP_PrintSettings_Dev& print_settings) { return 0; }
  virtual pp::Resource PrintPages(
      const PP_PrintPageNumberRange_Dev* page_ranges,
  uint32_t page_range_count) {
    return pp::Resource();
  }
  virtual void PrintEnd() {}
  virtual bool IsPrintScalingDisabled() { return false; }
};

TestPrinting::TestPrinting(TestingInstance* instance)
    : TestCase(instance),
      nested_event_(instance->pp_instance()) {
  callback_factory_.Initialize(this);
}

void TestPrinting::RunTests(const std::string& filter) {
  RUN_TEST(GetDefaultPrintSettings, filter);
}

std::string TestPrinting::TestGetDefaultPrintSettings() {
  g_callback_triggered = false;
  TestPrinting_Dev test_printing(instance_);
  pp::CompletionCallbackWithOutput<PP_PrintSettings_Dev> cb =
      callback_factory_.NewCallbackWithOutput(&TestPrinting::Callback);
  test_printing.GetDefaultPrintSettings(cb);
  nested_event_.Wait();

  ASSERT_EQ(PP_OK, g_callback_result);
  ASSERT_TRUE(g_callback_triggered);

  // Sanity check the |printable_area|, |content_area| and |paper_size| members.
  // It is possible these values are outside these ranges but it shouldn't
  // happen in practice and probably means there is an error in computing
  // the default print settings. These values are in points.
  ASSERT_TRUE(g_print_settings.printable_area.point.x < 200);
  ASSERT_TRUE(g_print_settings.printable_area.point.y < 200);
  ASSERT_TRUE(g_print_settings.printable_area.size.width < 2000);
  ASSERT_TRUE(g_print_settings.printable_area.size.height < 2000);

  ASSERT_TRUE(g_print_settings.content_area.point.x < 200);
  ASSERT_TRUE(g_print_settings.content_area.point.y < 200);
  ASSERT_TRUE(g_print_settings.content_area.size.width < 2000);
  ASSERT_TRUE(g_print_settings.content_area.size.height< 2000);

  ASSERT_TRUE(g_print_settings.paper_size.width < 2000);
  ASSERT_TRUE(g_print_settings.paper_size.height < 2000);

  PASS();
}

void TestPrinting::Callback(int32_t result,
                            PP_PrintSettings_Dev& print_settings) {
  g_callback_triggered = true;
  g_callback_result = result;
  g_print_settings = print_settings;
  nested_event_.Signal();
}
