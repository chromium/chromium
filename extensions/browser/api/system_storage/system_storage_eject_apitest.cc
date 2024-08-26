// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SystemStorage eject API browser tests.

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "components/storage_monitor/test_storage_monitor.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/system_storage/storage_api_test_util.h"
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
  SystemStorageEjectApiTest() : monitor_(nullptr) {}

  SystemStorageEjectApiTest(const SystemStorageEjectApiTest&) = delete;
  SystemStorageEjectApiTest& operator=(const SystemStorageEjectApiTest&) =
      delete;

  ~SystemStorageEjectApiTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    monitor_ = TestStorageMonitor::CreateForBrowserTests();
    ShellApiTest::SetUpOnMainThread();
  }

  content::RenderFrameHost* GetMainFrame() {
    ExtensionTestMessageListener listener("loaded");
    const extensions::Extension* extension = LoadApp("system/storage_eject");

    // Wait for the extension to load completely so we can execute
    // the JavaScript function from test case.
    EXPECT_TRUE(listener.WaitUntilSatisfied());

    return extensions::ProcessManager::Get(browser_context())
        ->GetBackgroundHostForExtension(extension->id())
        ->main_frame_host();
  }

  void ExecuteCmdAndCheckReply(content::RenderFrameHost* frame,
                               const std::string& js_command,
                               const std::string& ok_message) {
    ExtensionTestMessageListener listener(ok_message);
    frame->ExecuteJavaScriptForTests(base::ASCIIToUTF16(js_command),
                                     base::NullCallback(),
                                     content::ISOLATED_WORLD_ID_GLOBAL);
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
  raw_ptr<TestStorageMonitor, DanglingUntriaged> monitor_;
};

IN_PROC_BROWSER_TEST_F(SystemStorageEjectApiTest, EjectTest) {
  // Stash the main frame to avoid calling GetMainFrame() again as it waits for
  // load.
  content::RenderFrameHost* main_frame = GetMainFrame();
  ExecuteCmdAndCheckReply(main_frame, "addAttachListener()", "add_attach_ok");

  // Attach / detach
  const std::string expect_attach_msg =
      base::StringPrintf("%s,%s", "attach_test_ok", kRemovableStorageData.name);
  ExtensionTestMessageListener attach_finished_listener(expect_attach_msg);
  Attach();
  EXPECT_TRUE(attach_finished_listener.WaitUntilSatisfied());

  ExecuteCmdAndCheckReply(main_frame, "ejectTest()", "eject_ok");
  EXPECT_EQ(kRemovableStorageData.device_id, monitor_->ejected_device());

  Detach();
}

IN_PROC_BROWSER_TEST_F(SystemStorageEjectApiTest, EjectBadDeviceTest) {
  ExecuteCmdAndCheckReply(GetMainFrame(), "ejectFailTest()",
                          "eject_no_such_device");

  EXPECT_EQ("", monitor_->ejected_device());
}
