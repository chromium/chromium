// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_mouse_cursor.h"

#include "ppapi/cpp/image_data.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(MouseCursor);

TestMouseCursor::TestMouseCursor(TestingInstance* instance)
    : TestCase(instance) {
}

bool TestMouseCursor::Init() {
  mouse_cursor_interface_ = static_cast<const PPB_MouseCursor*>(
      pp::Module::Get()->GetBrowserInterface(PPB_MOUSECURSOR_INTERFACE));
  return !!mouse_cursor_interface_;
}

void TestMouseCursor::RunTests(const std::string& filter) {
  RUN_TEST(Type, filter);
  RUN_TEST(Custom, filter);
  RUN_TEST(Point, filter);
}

std::string TestMouseCursor::TestType() {
  ASSERT_TRUE(PP_ToBool(mouse_cursor_interface_->SetCursor(
      instance_->pp_instance(), PP_MOUSECURSOR_TYPE_POINTER, 0, NULL)));
  ASSERT_FALSE(PP_ToBool(mouse_cursor_interface_->SetCursor(
      instance_->pp_instance(), static_cast<PP_MouseCursor_Type>(-2),
      0, NULL)));
  PASS();
}

std::string TestMouseCursor::TestCustom() {
  // First test a valid image.
  pp::ImageData valid_image(instance_,
                            pp::ImageData::GetNativeImageDataFormat(),
                            pp::Size(16, 16), true);
  PP_Point point = { 0, 0 };
  ASSERT_TRUE(PP_ToBool(mouse_cursor_interface_->SetCursor(
      instance_->pp_instance(), PP_MOUSECURSOR_TYPE_CUSTOM,
      valid_image.pp_resource(), &point)));

  // 0 image resource ID.
  ASSERT_FALSE(PP_ToBool(mouse_cursor_interface_->SetCursor(
      instance_->pp_instance(), PP_MOUSECURSOR_TYPE_CUSTOM, 0, NULL)));

  // Image specified for predefined type.
  ASSERT_FALSE(PP_ToBool(mouse_cursor_interface_->SetCursor(
      instance_->pp_instance(), PP_MOUSECURSOR_TYPE_POINTER,
      valid_image.pp_resource(), &point)));

  // A too-big image.
  pp::ImageData big_image(instance_, pp::ImageData::GetNativeImageDataFormat(),
                            pp::Size(65, 12), true);
  ASSERT_FALSE(PP_ToBool(mouse_cursor_interface_->SetCursor(
      instance_->pp_instance(), PP_MOUSECURSOR_TYPE_CUSTOM,
      big_image.pp_resource(), &point)));

  PASS();
}

std::string TestMouseCursor::TestPoint() {
  pp::ImageData valid_image(instance_,
                            pp::ImageData::GetNativeImageDataFormat(),
                            pp::Size(16, 16), true);
  PP_Point point = { -1, 0 };
  ASSERT_FALSE(PP_ToBool(mouse_cursor_interface_->SetCursor(
      instance_->pp_instance(), PP_MOUSECURSOR_TYPE_CUSTOM,
      valid_image.pp_resource(), &point)));

  point.x = 67;
  point.y = 5;
  ASSERT_FALSE(PP_ToBool(mouse_cursor_interface_->SetCursor(
      instance_->pp_instance(), PP_MOUSECURSOR_TYPE_CUSTOM,
      valid_image.pp_resource(), &point)));
  PASS();
}
