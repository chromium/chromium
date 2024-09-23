// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/file_system_access/global_file_system_access.h"

#include <tuple>

#include "base/memory/raw_ref.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_file_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class MockFileSystemAccessManager
    : public mojom::blink::FileSystemAccessManager {
 public:
  MockFileSystemAccessManager(const BrowserInterfaceBrokerProxy& broker,
                              base::OnceClosure reached_callback)
      : reached_callback_(std::move(reached_callback)), broker_(broker) {
    broker_->SetBinderForTesting(
        mojom::blink::FileSystemAccessManager::Name_,
        WTF::BindRepeating(
            &MockFileSystemAccessManager::BindFileSystemAccessManager,
            WTF::Unretained(this)));
  }
  explicit MockFileSystemAccessManager(
      const BrowserInterfaceBrokerProxy& broker)
      : broker_(broker) {
    broker_->SetBinderForTesting(
        mojom::blink::FileSystemAccessManager::Name_,
        WTF::BindRepeating(
            &MockFileSystemAccessManager::BindFileSystemAccessManager,
            WTF::Unretained(this)));
  }
  ~MockFileSystemAccessManager() override {
    broker_->SetBinderForTesting(mojom::blink::FileSystemAccessManager::Name_,
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

  void GetSandboxedFileSystemForDevtools(
      const Vector<String>& directory_path_components,
      GetSandboxedFileSystemCallback callback) override {}

  void ChooseEntries(mojom::blink::FilePickerOptionsPtr options,
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
      mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken>,
      mojo::PendingReceiver<mojom::blink::FileSystemAccessFileHandle>)
      override {}

  void GetDirectoryHandleFromToken(
      mojo::PendingRemote<mojom::blink::FileSystemAccessTransferToken>,
      mojo::PendingReceiver<mojom::blink::FileSystemAccessDirectoryHandle>)
      override {}

  void GetEntryFromDataTransferToken(
      mojo::PendingRemote<
          blink::mojom::blink::FileSystemAccessDataTransferToken> token,
      GetEntryFromDataTransferTokenCallback callback) override {}

  void BindObserverHost(
      mojo::PendingReceiver<blink::mojom::blink::FileSystemAccessObserverHost>
          observer_host) override {}

 private:
  void BindFileSystemAccessManager(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(this,
                   mojo::PendingReceiver<mojom::blink::FileSystemAccessManager>(
                       std::move(handle)));
  }

  base::OnceClosure reached_callback_;
  ChooseEntriesResponseCallback choose_entries_response_callback_;
  mojo::ReceiverSet<mojom::blink::FileSystemAccessManager> receivers_;
  const raw_ref<const BrowserInterfaceBrokerProxy> broker_;
};

class GlobalFileSystemAccessTest : public PageTestBase {
 public:
  void SetUp() override {
    PageTestBase::SetUp();
    Navigate("http://localhost");
    GetDocument().GetSettings()->SetScriptEnabled(true);
  }

  void Navigate(const String& destinationUrl) {
    const KURL& url = KURL(NullURL(), destinationUrl);
    auto navigation_params =
        WebNavigationParams::CreateWithEmptyHTMLForTesting(url);
    GetDocument().GetFrame()->Loader().CommitNavigation(
        std::move(navigation_params), /*extra_data=*/nullptr);
    blink::test::RunPendingTasks();
    ASSERT_EQ(url.GetString(), GetDocument().Url().GetString());
  }
};

TEST_F(GlobalFileSystemAccessTest, UserActivationRequiredOtherwiseDenied) {
  LocalFrame* frame = &GetFrame();
  EXPECT_FALSE(frame->HasStickyUserActivation());

  MockFileSystemAccessManager manager(frame->GetBrowserInterfaceBroker());
  manager.SetChooseEntriesResponse(WTF::BindOnce(
      [](MockFileSystemAccessManager::ChooseEntriesCallback callback) {
        FAIL();
      }));
  ClassicScript::CreateUnspecifiedScript("window.showOpenFilePicker();")
      ->RunScript(GetFrame().DomWindow());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(frame->HasStickyUserActivation());
}

TEST_F(GlobalFileSystemAccessTest, UserActivationChooseEntriesSuccessful) {
  LocalFrame* frame = &GetFrame();
  EXPECT_FALSE(frame->HasStickyUserActivation());

  LocalFrame::NotifyUserActivation(
      frame, mojom::UserActivationNotificationType::kTest);
  EXPECT_TRUE(frame->HasStickyUserActivation());

  base::RunLoop manager_run_loop;
  MockFileSystemAccessManager manager(frame->GetBrowserInterfaceBroker(),
                                      manager_run_loop.QuitClosure());
  manager.SetChooseEntriesResponse(WTF::BindOnce(
      [](MockFileSystemAccessManager::ChooseEntriesCallback callback) {
        auto error = mojom::blink::FileSystemAccessError::New();
        error->status = mojom::blink::FileSystemAccessStatus::kOk;
        error->message = "";

        mojo::PendingRemote<mojom::blink::FileSystemAccessFileHandle>
            pending_remote;
        std::ignore = pending_remote.InitWithNewPipeAndPassReceiver();
        auto handle = mojom::blink::FileSystemAccessHandle::NewFile(
            std::move(pending_remote));
        auto entry = mojom::blink::FileSystemAccessEntry::New(std::move(handle),
                                                              "foo.txt");
        Vector<mojom::blink::FileSystemAccessEntryPtr> entries;
        entries.push_back(std::move(entry));

        std::move(callback).Run(std::move(error), std::move(entries));
      }));
  ClassicScript::CreateUnspecifiedScript("window.showOpenFilePicker();")
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

TEST_F(GlobalFileSystemAccessTest, UserActivationChooseEntriesErrors) {
  LocalFrame* frame = &GetFrame();
  EXPECT_FALSE(frame->HasStickyUserActivation());

  using mojom::blink::FileSystemAccessStatus;

  FileSystemAccessStatus statuses[] = {
      FileSystemAccessStatus::kPermissionDenied,
      FileSystemAccessStatus::kInvalidState,
      FileSystemAccessStatus::kInvalidArgument,
      FileSystemAccessStatus::kOperationFailed,
      // kOperationAborted is when the user cancels the file selection.
      FileSystemAccessStatus::kOperationAborted,
  };
  MockFileSystemAccessManager manager(frame->GetBrowserInterfaceBroker());

  for (const FileSystemAccessStatus& status : statuses) {
    LocalFrame::NotifyUserActivation(
        frame, mojom::UserActivationNotificationType::kTest);
    EXPECT_TRUE(frame->HasStickyUserActivation());

    base::RunLoop manager_run_loop;
    manager.SetQuitClosure(manager_run_loop.QuitClosure());
    manager.SetChooseEntriesResponse(WTF::BindOnce(
        [](mojom::blink::FileSystemAccessStatus status,
           MockFileSystemAccessManager::ChooseEntriesCallback callback) {
          auto error = mojom::blink::FileSystemAccessError::New();
          error->status = status;
          error->message = "";
          Vector<mojom::blink::FileSystemAccessEntryPtr> entries;

          std::move(callback).Run(std::move(error), std::move(entries));
        },
        status));
    ClassicScript::CreateUnspecifiedScript("window.showOpenFilePicker();")
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
