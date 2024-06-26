// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_file_chooser_host.h"

#include <stdint.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/pepper/mock_renderer_ppapi_host.h"
#include "content/renderer/render_frame_impl.h"
#include "content/test/test_content_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/resource_message_test_sink.h"
#include "ppapi/shared_impl/file_ref_create_info.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/test_globals.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"

namespace content {

using blink::mojom::FileChooser;
using blink::mojom::FileChooserFileInfo;
using blink::mojom::FileChooserFileInfoPtr;
using blink::mojom::FileChooserParams;
using blink::mojom::FileChooserParamsPtr;

namespace {

class MockFileChooser : public FileChooser {
 public:
  MockFileChooser(const blink::BrowserInterfaceBrokerProxy* broker,
                  base::OnceClosure reached_callback)
      : reached_callback_(std::move(reached_callback)) {
    broker->SetBinderForTesting(
        FileChooser::Name_,
        base::BindRepeating(&MockFileChooser::BindFileChooserReceiver,
                            base::Unretained(this)));
    broker_ = broker;
  }

  ~MockFileChooser() override {
    broker_->SetBinderForTesting(FileChooser::Name_, {});
  }

  const FileChooserParams& params() const {
    DCHECK(params_);
    return *params_;
  }

  void ResponseOnOpenFileChooser(std::vector<FileChooserFileInfoPtr> files) {
    DCHECK(callback_);
    DCHECK(params_);
    std::move(callback_).Run(blink::mojom::FileChooserResult::New(
        std::move(files), base::FilePath()));
    receivers_.FlushForTesting();
  }

 private:
  void BindFileChooserReceiver(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(this, mojo::PendingReceiver<FileChooser>(std::move(handle)));
  }

  void OpenFileChooser(FileChooserParamsPtr params,
                       OpenFileChooserCallback callback) override {
    callback_ = std::move(callback);
    params_ = std::move(params);
    std::move(reached_callback_).Run();
  }

  void EnumerateChosenDirectory(
      const base::FilePath& directory_path,
      EnumerateChosenDirectoryCallback callback) override {}

  raw_ptr<const blink::BrowserInterfaceBrokerProxy> broker_;
  mojo::ReceiverSet<FileChooser> receivers_;
  OpenFileChooserCallback callback_;
  FileChooserParamsPtr params_;
  base::OnceClosure reached_callback_;
};

class PepperFileChooserHostTest : public RenderViewTest {
 public:
  PepperFileChooserHostTest() : pp_instance_(123456) {}

  void SetUp() override {
    SetContentClient(&client_);
    RenderViewTest::SetUp();

    globals_.GetResourceTracker()->DidCreateInstance(pp_instance_);
    mock_file_chooser_ = std::make_unique<MockFileChooser>(
        &GetMainRenderFrame()->GetBrowserInterfaceBroker(),
        run_loop_.QuitClosure());
  }
  void TearDown() override {
    mock_file_chooser_.reset();
    globals_.GetResourceTracker()->DidDeleteInstance(pp_instance_);

    RenderViewTest::TearDown();
  }

  PP_Instance pp_instance() const { return pp_instance_; }
  MockFileChooser* mock_file_chooser() const {
    return mock_file_chooser_.get();
  }

 protected:
  base::RunLoop run_loop_;

 private:
  PP_Instance pp_instance_;

  // Disables locking for the duration of the test.
  ppapi::ProxyLock::LockingDisablerForTest disable_locking_;
  ppapi::TestGlobals globals_;
  TestContentClient client_;

  std::unique_ptr<MockFileChooser> mock_file_chooser_;
};

}  // namespace

TEST_F(PepperFileChooserHostTest, Show) {
  PP_Resource pp_resource = 123;

  MockRendererPpapiHost host(GetMainRenderFrame(), pp_instance());
  PepperFileChooserHost chooser(&host, pp_instance(), pp_resource);

  // Say there's a user gesture.
  host.set_has_user_gesture(true);

  std::vector<std::string> accept;
  accept.push_back("text/plain");
  PpapiHostMsg_FileChooser_Show show_msg(false, false, std::string(), accept);

  ppapi::proxy::ResourceMessageCallParams call_params(pp_resource, 0);
  ppapi::host::HostMessageContext context(call_params);
  int32_t result = chooser.OnResourceMessageReceived(show_msg, &context);
  EXPECT_EQ(PP_OK_COMPLETIONPENDING, result);

  run_loop_.Run();
  // The render view should have sent a chooser request to the browser

  // Basic validation of request.
  auto& params = mock_file_chooser()->params();
  EXPECT_EQ(FileChooserParams::Mode::kOpen, params.mode);
  ASSERT_EQ(1u, params.accept_types.size());
  EXPECT_EQ(accept[0], base::UTF16ToUTF8(params.accept_types[0]));

  // Send a chooser reply to the render view. Note our reply path has to have a
  // path separator so we include both a Unix and a Windows one.
  std::vector<blink::mojom::FileChooserFileInfoPtr> selected_info_vector;
  std::u16string display_name = u"Hello, world";
  selected_info_vector.push_back(
      blink::mojom::FileChooserFileInfo::NewNativeFile(
          blink::mojom::NativeFileInfo::New(
              base::FilePath(FILE_PATH_LITERAL("myp\\ath/foo")),
              display_name)));
  mock_file_chooser()->ResponseOnOpenFileChooser(
      std::move(selected_info_vector));

  // This should have sent the Pepper reply to our test sink.
  ppapi::proxy::ResourceMessageReplyParams reply_params;
  IPC::Message reply_msg;
  ASSERT_TRUE(host.sink().GetFirstResourceReplyMatching(
      PpapiPluginMsg_FileChooser_ShowReply::ID, &reply_params, &reply_msg));

  // Basic validation of reply.
  EXPECT_EQ(call_params.sequence(), reply_params.sequence());
  EXPECT_EQ(PP_OK, reply_params.result());
  PpapiPluginMsg_FileChooser_ShowReply::Schema::Param reply_msg_param;
  ASSERT_TRUE(
      PpapiPluginMsg_FileChooser_ShowReply::Read(&reply_msg, &reply_msg_param));
  const std::vector<ppapi::FileRefCreateInfo>& chooser_results =
      std::get<0>(reply_msg_param);
  ASSERT_EQ(1u, chooser_results.size());
  EXPECT_EQ(base::UTF16ToUTF8(display_name), chooser_results[0].display_name);
}

TEST_F(PepperFileChooserHostTest, NoUserGesture) {
  PP_Resource pp_resource = 123;

  MockRendererPpapiHost host(GetMainRenderFrame(), pp_instance());
  PepperFileChooserHost chooser(&host, pp_instance(), pp_resource);

  // Say there's no user gesture.
  host.set_has_user_gesture(false);

  std::vector<std::string> accept;
  accept.push_back("text/plain");
  PpapiHostMsg_FileChooser_Show show_msg(false, false, std::string(), accept);

  ppapi::proxy::ResourceMessageCallParams call_params(pp_resource, 0);
  ppapi::host::HostMessageContext context(call_params);
  int32_t result = chooser.OnResourceMessageReceived(show_msg, &context);
  EXPECT_EQ(PP_ERROR_NO_USER_GESTURE, result);
}

}  // namespace content
