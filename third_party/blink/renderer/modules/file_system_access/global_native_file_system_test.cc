// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/global_native_file_system.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_directory_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_error.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_file_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_manager.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class MockNativeFileSystemManager
    : public mojom::blink::NativeFileSystemManager {
 public:
  MockNativeFileSystemManager(BrowserInterfaceBrokerProxy& broker,
                              base::OnceClosure reached_callback)
      : reached_callback_(std::move(reached_callback)), broker_(broker) {
    broker_.SetBinderForTesting(
        mojom::blink::NativeFileSystemManager::Name_,
        WTF::BindRepeating(
            &MockNativeFileSystemManager::BindNativeFileSystemManager,
            WTF::Unretained(this)));
  }
  MockNativeFileSystemManager(BrowserInterfaceBrokerProxy& broker)
      : broker_(broker) {
    broker_.SetBinderForTesting(
        mojom::blink::NativeFileSystemManager::Name_,
        WTF::BindRepeating(
            &MockNativeFileSystemManager::BindNativeFileSystemManager,
            WTF::Unretained(this)));
  }
  ~MockNativeFileSystemManager() override {
    broker_.SetBinderForTesting(mojom::blink::NativeFileSystemManager::Name_,
                                {});
  }

  using ChooseEntriesResponseCallback =
      base::OnceCallback<void(ChooseEntriesCallback callback)>;

  void SetQuitClosure(base::OnceClosure reached_callback) {
    reached_callback_ = std::move(reached_callback);
  }

  // Unused for these tests.
  void GetSandboxedFileSystem(
      GetSandboxedFileSystemCallback callback) override {}

  void ChooseEntries(
      mojom::ChooseFileSystemEntryType type,
      WTF::Vector<mojom::blink::ChooseFileSystemEntryAcceptsOptionPtr> accepts,
      bool include_accepts_all,
      ChooseEntriesCallback callback) override {
    if (choose_entries_response_callback_) {
      std::move(choose_entries_response_callback_).Run(std::move(callback));
    }

    if (reached_callback_)
      std::move(reached_callback_).Run();
  }

  void SetChooseEntriesResponse(ChooseEntriesResponseCallback callback) {
    choose_entries_response_callback_ = std::move(callback);
  }

  void GetFileHandleFromToken(
      mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken>,
      mojo::PendingReceiver<mojom::blink::NativeFileSystemFileHandle>)
      override {}

  void GetDirectoryHandleFromToken(
      mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken>,
      mojo::PendingReceiver<mojom::blink::NativeFileSystemDirectoryHandle>)
      override {}

  void GetEntryFromDragDropToken(
      mojo::PendingRemote<blink::mojom::blink::NativeFileSystemDragDropToken>
          token,
      GetEntryFromDragDropTokenCallback callback) override {}

 private:
  void BindNativeFileSystemManager(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(this,
                   mojo::PendingReceiver<mojom::blink::NativeFileSystemManager>(
                       std::move(handle)));
  }

  base::OnceClosure reached_callback_;
  ChooseEntriesResponseCallback choose_entries_response_callback_;
  mojo::ReceiverSet<mojom::blink::NativeFileSystemManager> receivers_;
  BrowserInterfaceBrokerProxy& broker_;
};

class GlobalNativeFileSystemTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    Navigate("http://localhost");
    GetDocument().GetSettings()->SetScriptEnabled(true);
  }

  void Navigate(const String& destinationUrl) {
    const KURL& url = KURL(NullURL(), destinationUrl);
    auto navigation_params =
        WebNavigationParams::CreateWithHTMLBuffer(SharedBuffer::Create(), url);
    GetDocument().GetFrame()->Loader().CommitNavigation(
        std::move(navigation_params), /*extra_data=*/nullptr);
    blink::test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
  }
};

TEST_F(GlobalNativeFileSystemTest, UserActivationRequiredOtherwiseDenied) {
  LocalFrame* frame = &GetFrame();
  EXPECT_FALSE(frame->HasStickyUserActivation());

  MockNativeFileSystemManager manager(frame->GetBrowserInterfaceBroker());
  manager.SetChooseEntriesResponse(WTF::Bind(
      [](MockNativeFileSystemManager::ChooseEntriesCallback callback) {
        FAIL();
      }));
  ClassicScript::CreateUnspecifiedScript(
      ScriptSourceCode("window.showOpenFilePicker();"))
      ->RunScript(GetFrame().DomWindow());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(frame->HasStickyUserActivation());
}

TEST_F(GlobalNativeFileSystemTest, UserActivationChooseEntriesSuccessful) {
  LocalFrame* frame = &GetFrame();
  EXPECT_FALSE(frame->HasStickyUserActivation());

  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(frame->HasStickyUserActivation());

  base::RunLoop manager_run_loop;
  MockNativeFileSystemManager manager(frame->GetBrowserInterfaceBroker(),
                                      manager_run_loop.QuitClosure());
  manager.SetChooseEntriesResponse(WTF::Bind(
      [](MockNativeFileSystemManager::ChooseEntriesCallback callback) {
        auto error = mojom::blink::NativeFileSystemError::New();
        error->status = mojom::blink::NativeFileSystemStatus::kOk;
        error->message = "";

        mojo::PendingRemote<mojom::blink::NativeFileSystemFileHandle>
            pending_remote;
        ignore_result(pending_remote.InitWithNewPipeAndPassReceiver());
        auto handle = mojom::blink::NativeFileSystemHandle::NewFile(
            std::move(pending_remote));
        auto entry = mojom::blink::NativeFileSystemEntry::New(std::move(handle),
                                                              "foo.txt");
        Vector<mojom::blink::NativeFileSystemEntryPtr> entries;
        entries.push_back(std::move(entry));

        std::move(callback).Run(std::move(error), std::move(entries));
      }));
  ClassicScript::CreateUnspecifiedScript(
      ScriptSourceCode("window.showOpenFilePicker();"))
      ->RunScript(GetFrame().DomWindow());
  manager_run_loop.Run();

  // Mock Manager finished sending data over the mojo pipe.
  // Clearing the user activation.
  frame->ClearUserActivation();
  EXPECT_FALSE(frame->HasStickyUserActivation());

  // Let blink-side receiver process the response and set the user activation
  // again.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(frame->HasStickyUserActivation());
}

TEST_F(GlobalNativeFileSystemTest, UserActivationChooseEntriesErrors) {
  LocalFrame* frame = &GetFrame();
  EXPECT_FALSE(frame->HasStickyUserActivation());

  using mojom::blink::NativeFileSystemStatus;

  NativeFileSystemStatus statuses[] = {
      NativeFileSystemStatus::kPermissionDenied,
      NativeFileSystemStatus::kInvalidState,
      NativeFileSystemStatus::kInvalidArgument,
      NativeFileSystemStatus::kOperationFailed,
      // kOperationAborted is when the user cancels the file selection.
      NativeFileSystemStatus::kOperationAborted,
  };
  MockNativeFileSystemManager manager(frame->GetBrowserInterfaceBroker());

  for (const NativeFileSystemStatus& status : statuses) {
    LocalFrame::NotifyUserActivation(
        frame, mojom::UserActivationNotificationType::kTest);
    EXPECT_TRUE(frame->HasStickyUserActivation());

    base::RunLoop manager_run_loop;
    manager.SetQuitClosure(manager_run_loop.QuitClosure());
    manager.SetChooseEntriesResponse(WTF::Bind(
        [](mojom::blink::NativeFileSystemStatus status,
           MockNativeFileSystemManager::ChooseEntriesCallback callback) {
          auto error = mojom::blink::NativeFileSystemError::New();
          error->status = status;
          error->message = "";
          Vector<mojom::blink::NativeFileSystemEntryPtr> entries;

          std::move(callback).Run(std::move(error), std::move(entries));
        },
        status));
    ClassicScript::CreateUnspecifiedScript(
        ScriptSourceCode("window.showOpenFilePicker();"))
        ->RunScript(GetFrame().DomWindow());
    manager_run_loop.Run();

    // Mock Manager finished sending data over the mojo pipe.
    // Clearing the user activation.
    frame->ClearUserActivation();
    EXPECT_FALSE(frame->HasStickyUserActivation());

    base::RunLoop().RunUntilIdle();
    EXPECT_FALSE(frame->HasStickyUserActivation());
  }
}

}  // namespace blink
