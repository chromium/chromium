// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SystemStorage eject API browser tests.

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/system_storage/storage_api_test_util.h"
#include "extensions/browser/api/system_storage/storage_info_provider.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

using extensions::test::TestStorageUnitInfo;
using extensions::test::kRemovableStorageData;
using storage_monitor::StorageMonitor;
using storage_monitor::TestStorageMonitor;

}  // namespace

class SystemStorageEjectApiTest : public extensions::ShellApiTest {
 public:
  SystemStorageEjectApiTest() : monitor_(NULL) {}
  ~SystemStorageEjectApiTest() override {}

 protected:
  void SetUpOnMainThread() override {
    monitor_ = TestStorageMonitor::CreateForBrowserTests();
    ShellApiTest::SetUpOnMainThread();
  }

  content::RenderViewHost* GetHost() {
    ExtensionTestMessageListener listener("loaded", false);
    const extensions::Extension* extension = LoadApp("system/storage_eject");

    // Wait for the extension to load completely so we can execute
    // the JavaScript function from test case.
    EXPECT_TRUE(listener.WaitUntilSatisfied());

    return extensions::ProcessManager::Get(browser_context())
        ->GetBackgroundHostForExtension(extension->id())
        ->render_view_host();
  }

  void ExecuteCmdAndCheckReply(content::RenderViewHost* host,
                               const std::string& js_command,
                               const std::string& ok_message) {
    ExtensionTestMessageListener listener(ok_message, false);
    host->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(js_command), base::NullCallback());
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  void Attach() {
    DCHECK(StorageMonitor::GetInstance()->IsInitialized());
    StorageMonitor::GetInstance()->receiver()->ProcessAttach(
        extensions::test::BuildStorageInfoFromTestStorageUnitInfo(
            kRemovableStorageData));
    content::RunAllPendingInMessageLoop();
  }

  void Detach() {
    DCHECK(StorageMonitor::GetInstance()->IsInitialized());
    StorageMonitor::GetInstance()->receiver()->ProcessDetach(
        kRemovableStorageData.device_id);
    content::RunAllPendingInMessageLoop();
  }

 protected:
  TestStorageMonitor* monitor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemStorageEjectApiTest);
};

IN_PROC_BROWSER_TEST_F(SystemStorageEjectApiTest, EjectTest) {
  content::RenderViewHost* host = GetHost();
  ExecuteCmdAndCheckReply(host, "addAttachListener()", "add_attach_ok");

  // Attach / detach
  const std::string expect_attach_msg =
      base::StringPrintf("%s,%s", "attach_test_ok", kRemovableStorageData.name);
  ExtensionTestMessageListener attach_finished_listener(expect_attach_msg,
                                                        false /* no reply */);
  Attach();
  EXPECT_TRUE(attach_finished_listener.WaitUntilSatisfied());

  ExecuteCmdAndCheckReply(host, "ejectTest()", "eject_ok");
  EXPECT_EQ(kRemovableStorageData.device_id, monitor_->ejected_device());

  Detach();
}

IN_PROC_BROWSER_TEST_F(SystemStorageEjectApiTest, EjectBadDeviceTest) {
  ExecuteCmdAndCheckReply(GetHost(), "ejectFailTest()", "eject_no_such_device");

  EXPECT_EQ("", monitor_->ejected_device());
}
