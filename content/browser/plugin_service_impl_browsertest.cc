// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_service_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/content_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

constexpr char kURL1[] = "http://google.com/";
constexpr char kURL2[] = "http://youtube.com/";

class TestPluginClient : public PpapiPluginProcessHost::PluginClient {
 public:
  void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                           int* renderer_id) override {}
  void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                            base::ProcessId plugin_pid,
                            int plugin_child_id) override {
    plugin_pid_ = plugin_pid;
    run_loop_->Quit();
  }
  bool Incognito() override { return false; }

  base::ProcessId plugin_pid() const { return plugin_pid_; }
  void SetRunLoop(base::RunLoop* run_loop) { run_loop_ = run_loop; }
  void WaitForQuit() { run_loop_->Run(); }

 private:
  base::ProcessId plugin_pid_ = 0;
  base::RunLoop* run_loop_ = nullptr;
};

}  // anonymous namespace

class PluginServiceImplBrowserTest : public ContentBrowserTest {
 public:
  PluginServiceImplBrowserTest()
      : plugin_path_(FILE_PATH_LITERAL("internal-nonesuch")),
        profile_dir_(FILE_PATH_LITERAL("/fake/user/foo/dir")) {}

  ~PluginServiceImplBrowserTest() override = default;

  void RegisterFakePlugin() {
    WebPluginInfo fake_info;
    fake_info.name = base::ASCIIToUTF16("fake_plugin");
    fake_info.path = plugin_path_;
    fake_info.type = WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS;

    PluginServiceImpl* service = PluginServiceImpl::GetInstance();
    service->RegisterInternalPlugin(fake_info, true);
    service->Init();

    // Force plugins to load and wait for completion.
    base::RunLoop run_loop;
    service->GetPlugins(base::BindOnce(
        [](base::OnceClosure callback,
           const std::vector<WebPluginInfo>& ignore) {
          std::move(callback).Run();
        },
        run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OpenChannelToFakePlugin(const base::Optional<url::Origin>& origin,
                               TestPluginClient* client) {
    base::RunLoop run_loop;
    client->SetRunLoop(&run_loop);

    PluginServiceImpl* service = PluginServiceImpl::GetInstance();
    base::PostTask(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&PluginServiceImpl::OpenChannelToPpapiPlugin,
                       base::Unretained(service), 0, plugin_path_, profile_dir_,
                       origin, base::Unretained(client)));
    client->WaitForQuit();
    client->SetRunLoop(nullptr);
  }

  base::FilePath plugin_path_;
  base::FilePath profile_dir_;
};

IN_PROC_BROWSER_TEST_F(PluginServiceImplBrowserTest, OriginLock) {
  RegisterFakePlugin();

  url::Origin origin1 = url::Origin::Create(GURL(kURL1));
  url::Origin origin2 = url::Origin::Create(GURL(kURL2));

  TestPluginClient client1;
  OpenChannelToFakePlugin(origin1, &client1);
  EXPECT_NE(base::kNullProcessId, client1.plugin_pid());

  TestPluginClient client2a;
  OpenChannelToFakePlugin(origin2, &client2a);
  EXPECT_NE(base::kNullProcessId, client2a.plugin_pid());

  TestPluginClient client2b;
  OpenChannelToFakePlugin(origin2, &client2b);
  EXPECT_NE(base::kNullProcessId, client2b.plugin_pid());

  // Actual test: how plugins got lumped into two pids.
  EXPECT_NE(client1.plugin_pid(), client2a.plugin_pid());
  EXPECT_NE(client1.plugin_pid(), client2b.plugin_pid());
  EXPECT_EQ(client2a.plugin_pid(), client2b.plugin_pid());

  // Empty origins all go to same pid.
  TestPluginClient client3a;
  OpenChannelToFakePlugin(base::nullopt, &client3a);
  EXPECT_NE(base::kNullProcessId, client3a.plugin_pid());

  TestPluginClient client3b;
  OpenChannelToFakePlugin(base::nullopt, &client3b);
  EXPECT_NE(base::kNullProcessId, client3b.plugin_pid());

  // Actual test: how empty origins got lumped into pids.
  EXPECT_NE(client1.plugin_pid(), client3a.plugin_pid());
  EXPECT_NE(client1.plugin_pid(), client3b.plugin_pid());
  EXPECT_NE(client2a.plugin_pid(), client3a.plugin_pid());
  EXPECT_NE(client2a.plugin_pid(), client3b.plugin_pid());
  EXPECT_EQ(client3a.plugin_pid(), client3b.plugin_pid());
}

IN_PROC_BROWSER_TEST_F(PluginServiceImplBrowserTest, NoForkBombs) {
  RegisterFakePlugin();

  PluginServiceImpl* service = PluginServiceImpl::GetInstance();
  service->SetMaxPpapiProcessesPerProfileForTesting(4);

  const char* kFakeURLTemplate = "https://foo.fake%d.com/";
  TestPluginClient client;
  for (int i = 0; i < 4; ++i) {
    std::string url = base::StringPrintf(kFakeURLTemplate, i);
    url::Origin origin = url::Origin::Create(GURL(url));
    OpenChannelToFakePlugin(origin, &client);
    EXPECT_NE(base::kNullProcessId, client.plugin_pid());
  }

  // After a while we stop handing out processes per-origin.
  for (int i = 4; i < 8; ++i) {
    std::string url = base::StringPrintf(kFakeURLTemplate, i);
    url::Origin origin = url::Origin::Create(GURL(url));
    OpenChannelToFakePlugin(origin, &client);
    EXPECT_EQ(base::kNullProcessId, client.plugin_pid());
  }

  // But there's always room for the empty origin case.
  OpenChannelToFakePlugin(base::nullopt, &client);
  EXPECT_NE(base::kNullProcessId, client.plugin_pid());

  // And re-using existing processes is always possible.
  for (int i = 0; i < 4; ++i) {
    std::string url = base::StringPrintf(kFakeURLTemplate, i);
    url::Origin origin = url::Origin::Create(GURL(url));
    OpenChannelToFakePlugin(origin, &client);
    EXPECT_NE(base::kNullProcessId, client.plugin_pid());
  }
}

}  // namespace content
