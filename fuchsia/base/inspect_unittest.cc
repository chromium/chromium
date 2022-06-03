// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/inspect.h"

#include <lib/fdio/directory.h>
#include <lib/inspect/cpp/hierarchy.h>
#include <lib/inspect/cpp/reader.h>
#include <lib/inspect/service/cpp/reader.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/inspect/cpp/component.h>

#include <cstdint>
#include <memory>

#include "base/fuchsia/mem_buffer_util.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/task_environment.h"
#include "components/version_info/version_info.h"
#include "fuchsia/base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cr_fuchsia {

namespace {

const char kVersion[] = "version";
const char kLastChange[] = "last_change_revision";

class InspectTest : public ::testing::Test {
 public:
  InspectTest() {
    fidl::InterfaceHandle<fuchsia::io::Directory> incoming_directory;
    auto incoming_services =
        std::make_shared<sys::ServiceDirectory>(std::move(incoming_directory));
    context_ = std::make_unique<sys::ComponentContext>(
        std::move(incoming_services),
        published_root_directory_.NewRequest().TakeChannel());
    inspector_ = std::make_unique<sys::ComponentInspector>(context_.get());
    base::RunLoop().RunUntilIdle();
  }

  InspectTest(const InspectTest&) = delete;
  InspectTest& operator=(const InspectTest&) = delete;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<sys::ComponentContext> context_;
  fidl::InterfaceHandle<fuchsia::io::Directory> published_root_directory_;
  std::unique_ptr<sys::ComponentInspector> inspector_;
};

}  // namespace

TEST_F(InspectTest, PublishVersionInfoToInspect) {
  cr_fuchsia::PublishVersionInfoToInspect(inspector_.get());
  fidl::InterfaceHandle<fuchsia::io::Directory> directory;
  zx_status_t status = fdio_service_connect_at(
      published_root_directory_.channel().get(), "diagnostics",
      directory.NewRequest().TakeChannel().release());
  ASSERT_EQ(ZX_OK, status);
  std::unique_ptr<sys::ServiceDirectory> diagnostics =
      std::make_unique<sys::ServiceDirectory>(std::move(directory));

  // Access the inspect::Tree where the data is served. |tree| is in the
  // directory created for the test, not the diagnostics directory for the test
  // component.
  fuchsia::inspect::TreePtr tree;
  diagnostics->Connect(tree.NewRequest());
  fuchsia::inspect::TreeContent content;
  base::RunLoop run_loop;
  tree->GetContent([&content, &run_loop](fuchsia::inspect::TreeContent c) {
    content = std::move(c);
    run_loop.Quit();
  });
  run_loop.Run();

  // Parse the data as an inspect::Hierarchy.
  ASSERT_TRUE(content.has_buffer());
  std::string buffer_data =
      base::StringFromMemBuffer(content.buffer()).value_or(std::string());
  inspect::Hierarchy hierarchy =
      inspect::ReadFromBuffer(cr_fuchsia::StringToBytes(buffer_data))
          .take_value();

  auto* property =
      hierarchy.node().get_property<inspect::StringPropertyValue>(kVersion);
  EXPECT_EQ(property->value(), version_info::GetVersionNumber());
  property =
      hierarchy.node().get_property<inspect::StringPropertyValue>(kLastChange);
  EXPECT_EQ(property->value(), version_info::GetLastChange());
}

}  // namespace cr_fuchsia
