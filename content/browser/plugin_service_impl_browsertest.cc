// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_service_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "ppapi/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_PPAPI)
#include "content/browser/ppapi_plugin_process_host.h"
#endif  // BUILDFLAG(ENABLE_PPAPI)

namespace content {

namespace {

#if BUILDFLAG(ENABLE_PPAPI)
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
  raw_ptr<base::RunLoop> run_loop_ = nullptr;
};
#endif  // BUILDFLAG(ENABLE_PPAPI)

}  // anonymous namespace

class PluginServiceImplBrowserTest : public ContentBrowserTest {
 public:
  PluginServiceImplBrowserTest()
      : plugin_path_(FILE_PATH_LITERAL("internal-nonesuch")),
        profile_dir_(FILE_PATH_LITERAL("/fake/user/foo/dir")) {}

  ~PluginServiceImplBrowserTest() override = default;

  void RegisterFakePlugin() {
    WebPluginInfo fake_info;
    fake_info.name = u"fake_plugin";
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

#if BUILDFLAG(ENABLE_PPAPI)
  void OpenChannelToFakePlugin(const std::optional<url::Origin>& origin,
                               TestPluginClient* client) {
    base::RunLoop run_loop;
    client->SetRunLoop(&run_loop);

    PluginServiceImpl* service = PluginServiceImpl::GetInstance();
    service->OpenChannelToPpapiPlugin(
        /*render_process_id=*/0, plugin_path_, profile_dir_, origin, client);
    client->WaitForQuit();
    client->SetRunLoop(nullptr);
  }
#endif  // BUILDFLAG(ENABLE_PPAPI)

  base::FilePath plugin_path_;
  base::FilePath profile_dir_;
};

IN_PROC_BROWSER_TEST_F(PluginServiceImplBrowserTest, GetPluginInfoByPath) {
  RegisterFakePlugin();

  PluginServiceImpl* service = PluginServiceImpl::GetInstance();

  WebPluginInfo plugin_info;
  ASSERT_TRUE(service->GetPluginInfoByPath(plugin_path_, &plugin_info));

  EXPECT_EQ(plugin_path_, plugin_info.path);
}

#if BUILDFLAG(ENABLE_PPAPI)
IN_PROC_BROWSER_TEST_F(PluginServiceImplBrowserTest, OriginLock) {
  RegisterFakePlugin();

  url::Origin origin1 = url::Origin::Create(GURL("http://google.com/"));
  url::Origin origin2 = url::Origin::Create(GURL("http://youtube.com/"));

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
  OpenChannelToFakePlugin(std::nullopt, &client3a);
  EXPECT_NE(base::kNullProcessId, client3a.plugin_pid());

  TestPluginClient client3b;
  OpenChannelToFakePlugin(std::nullopt, &client3b);
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

  static constexpr char kFakeURLTemplate[] = "https://foo.fake%d.com/";
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
  OpenChannelToFakePlugin(std::nullopt, &client);
  EXPECT_NE(base::kNullProcessId, client.plugin_pid());

  // And re-using existing processes is always possible.
  for (int i = 0; i < 4; ++i) {
    std::string url = base::StringPrintf(kFakeURLTemplate, i);
    url::Origin origin = url::Origin::Create(GURL(url));
    OpenChannelToFakePlugin(origin, &client);
    EXPECT_NE(base::kNullProcessId, client.plugin_pid());
  }
}
#endif  // BUILDFLAG(ENABLE_PPAPI)

}  // namespace content
