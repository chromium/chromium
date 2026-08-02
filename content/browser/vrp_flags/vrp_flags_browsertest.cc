// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/vrp_flags/vrp_flags.h"

#include <set>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "components/vrp_flags/vrp_flags_impl.h"
#include "content/browser/vrp_flags/vrp_flags_factory_impl.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "sandbox/policy/switches.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace content {

class VrpFlagsBrowserTest : public ContentBrowserTest {
 public:
  VrpFlagsBrowserTest() {
    vrp_flags::VrpFlagsImpl::GetInstance()->SetForTesting(true);
  }
  ~VrpFlagsBrowserTest() override {
    vrp_flags::VrpFlagsImpl::GetInstance()->SetForTesting(false);
  }

  void SetUp() override {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            sandbox::policy::switches::kNoSandbox)) {
      GTEST_SKIP() << "Skipping test because --no-sandbox is present.";
    }
    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(vrp_flags::switches::kVrpFlags);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        "MAP victim.test " +
            net::HostPortPair::FromURL(embedded_test_server()->base_url())
                .ToString() +
            ",EXCLUDE localhost");
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (request.relative_url == "/poc.html") {
            auto response =
                std::make_unique<net::test_server::BasicHttpResponse>();
            response->set_code(net::HTTP_OK);
            response->set_content_type("text/html");
            response->set_content(
                "<html><body>Victim<script>"
                "window.addEventListener('message', (e) => {"
                "  if (e.data === 'ping') e.source.postMessage('pong', '*');"
                "});"
                "</script></body></html>");
            return response;
          }
          return nullptr;
        }));
    embedded_test_server()->StartAcceptingConnections();
  }

  mojo::Remote<vrp_flags::mojom::VrpFlags> GetRemote() {
    VrpFlagsFactoryImpl::Bind(nullptr, factory_.BindNewPipeAndPassReceiver());

    mojo::Remote<vrp_flags::mojom::VrpFlags> remote;
    factory_->BindBrowserVrpFlags(remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  mojo::Remote<vrp_flags::mojom::VrpFlags> GetGpuRemote() {
    VrpFlagsFactoryImpl::Bind(nullptr, factory_.BindNewPipeAndPassReceiver());

    mojo::Remote<vrp_flags::mojom::VrpFlags> remote;
    factory_->BindGpuVrpFlags(remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  mojo::Remote<vrp_flags::mojom::VrpFlags> GetNetworkRemote() {
    VrpFlagsFactoryImpl::Bind(nullptr, factory_.BindNewPipeAndPassReceiver());

    mojo::Remote<vrp_flags::mojom::VrpFlags> remote;
    factory_->BindNetworkVrpFlags(remote.BindNewPipeAndPassReceiver());
    return remote;
  }

 protected:
  mojo::Remote<vrp_flags::mojom::VrpFlagsFactory> factory_;
};

IN_PROC_BROWSER_TEST_F(VrpFlagsBrowserTest, GetWriteLocations) {
  mojo::Remote<vrp_flags::mojom::VrpFlags> remote = GetRemote();
  base::RunLoop run_loop;
  remote->GetWriteLocations(base::BindLambdaForTesting(
      [&](const std::vector<uint64_t>& locations, uint64_t value) {
        EXPECT_EQ(locations.size(), 5u);
        std::set<uint64_t> unique_locations;
        for (uint64_t location : locations) {
          EXPECT_NE(location, 0u);
          unique_locations.insert(location);
        }
        EXPECT_EQ(unique_locations.size(), 5u);
        EXPECT_NE(value, 0u);
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(VrpFlagsBrowserTest, WriteAttempted) {
  mojo::Remote<vrp_flags::mojom::VrpFlags> remote = GetRemote();

  uint64_t location = 0;
  uint64_t value = 0;
  {
    base::RunLoop run_loop;
    remote->GetWriteLocations(base::BindLambdaForTesting(
        [&](const std::vector<uint64_t>& locations, uint64_t v) {
          location = locations[0];
          value = v;
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  // SAFETY - simulates an arbitrary write for testing.
  UNSAFE_BUFFERS({
    uint64_t* ptr = reinterpret_cast<uint64_t*>(location);
    *ptr = value;
  });

  {
    base::RunLoop run_loop;
    remote->WriteAttempted(location,
                           base::BindLambdaForTesting([&](bool success) {
                             EXPECT_TRUE(success);
                             run_loop.Quit();
                           }));
    run_loop.Run();
  }

  // SAFETY - simulates an arbitrary write for testing - write the wrong value.
  UNSAFE_BUFFERS({
    uint64_t* ptr = reinterpret_cast<uint64_t*>(location);
    *ptr = value + 1;
  });

  {
    base::RunLoop run_loop;
    remote->WriteAttempted(location,
                           base::BindLambdaForTesting([&](bool success) {
                             EXPECT_FALSE(success);
                             run_loop.Quit();
                           }));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(VrpFlagsBrowserTest, ReadAttempted) {
  mojo::Remote<vrp_flags::mojom::VrpFlags> remote = GetRemote();

  base::UnguessableToken prefix;
  {
    base::RunLoop run_loop;
    remote->GetReadPrefix(
        base::BindLambdaForTesting([&](const base::UnguessableToken& p) {
          prefix = p;
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  EXPECT_FALSE(prefix.is_empty());

  // Use a prearranged value for testing.
  base::UnguessableToken known_flag =
      base::UnguessableToken::CreateForTesting(0x45671234, 0x45671234);
  vrp_flags::VrpFlagsImpl::GetInstance()->SetReadValueForTesting(known_flag);

  {
    base::RunLoop run_loop;
    remote->ReadAttempted(known_flag,
                          base::BindLambdaForTesting([&](bool success) {
                            EXPECT_TRUE(success);
                            run_loop.Quit();
                          }));
    run_loop.Run();
  }

  // Check that wrong flag still fails.
  {
    base::RunLoop run_loop;
    remote->ReadAttempted(base::UnguessableToken::Create(),
                          base::BindLambdaForTesting([&](bool success) {
                            EXPECT_FALSE(success);
                            run_loop.Quit();
                          }));
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(VrpFlagsBrowserTest, ReadAttemptedZeroIsh) {
  mojo::Remote<vrp_flags::mojom::VrpFlags> remote = GetRemote();

  // Ensure allocated.
  {
    base::RunLoop run_loop;
    remote->GetReadPrefix(base::BindLambdaForTesting(
        [&](const base::UnguessableToken& p) { run_loop.Quit(); }));
    run_loop.Run();
  }

  base::RunLoop run_loop;
  // Impossible to do a zero test as the mojom rejects a 0,0 token in any case.
  remote->ReadAttempted(base::UnguessableToken::CreateForTesting(0, 1),
                        base::BindLambdaForTesting([&](bool success) {
                          EXPECT_FALSE(success);
                          run_loop.Quit();
                        }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(VrpFlagsBrowserTest, WriteAttemptedZero) {
  mojo::Remote<vrp_flags::mojom::VrpFlags> remote = GetRemote();

  // Ensure allocated.
  {
    base::RunLoop run_loop;
    remote->GetWriteLocations(
        base::BindLambdaForTesting([&](const std::vector<uint64_t>& locations,
                                       uint64_t value) { run_loop.Quit(); }));
    run_loop.Run();
  }

  base::RunLoop run_loop;
  remote->WriteAttempted(0, base::BindLambdaForTesting([&](bool success) {
                           EXPECT_FALSE(success);
                           run_loop.Quit();
                         }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(VrpFlagsBrowserTest, BindGpuVrpFlags) {
  mojo::Remote<vrp_flags::mojom::VrpFlags> remote = GetGpuRemote();
  base::RunLoop run_loop;
  remote->GetWriteLocations(base::BindLambdaForTesting(
      [&](const std::vector<uint64_t>& locations, uint64_t value) {
        EXPECT_EQ(locations.size(), 5u);
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(VrpFlagsBrowserTest, BindNetworkVrpFlags) {
  mojo::Remote<vrp_flags::mojom::VrpFlags> remote = GetNetworkRemote();
  base::RunLoop run_loop;
  remote->GetWriteLocations(base::BindLambdaForTesting(
      [&](const std::vector<uint64_t>& locations, uint64_t value) {
        EXPECT_EQ(locations.size(), 5u);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Verifies spawning a victim renderer in a foreground tab and binding its
// VrpFlags.
IN_PROC_BROWSER_TEST_F(VrpFlagsBrowserTest,
                       StartRendererForVrpFlags_SpawnForegroundTab) {
  // Navigate main browser window to test page and bind
  // VrpFlagsFactoryImpl for the main frame to emulate the attacker render using
  // MojoJS to communicate with VrpFlagsFactory.
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/poc.html")));
  VrpFlagsFactoryImpl::Bind(shell()->web_contents()->GetPrimaryMainFrame(),
                            factory_.BindNewPipeAndPassReceiver());

  // StartRendererForVrpFlags returns the port used for the victim page.
  mojo::Remote<vrp_flags::mojom::VrpFlags> victim_remote;
  uint16_t port = 0;
  {
    base::RunLoop run_loop;
    factory_->StartRendererForVrpFlags(
        vrp_flags::mojom::VictimDisposition::kSpawnForegroundTab,
        victim_remote.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting([&](uint16_t assigned_port) {
          port = assigned_port;
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  EXPECT_NE(port, 0u);

  // Verify a new browser window/tab was created and confirm host matching
  // assigned port.
  ASSERT_EQ(2u, Shell::windows().size());
  WebContents* victim_contents = Shell::windows()[1]->web_contents();
  EXPECT_TRUE(WaitForLoadStop(victim_contents));
  EXPECT_EQ(EvalJs(victim_contents, "window.location.host").ExtractString(),
            base::StringPrintf("victim.test:%u", port));

  // Invoke GetWriteLocations in the victim renderer remote and verify 5
  // write locations are returned.
  base::RunLoop run_loop;
  victim_remote->GetWriteLocations(base::BindLambdaForTesting(
      [&](const std::vector<uint64_t>& locations, uint64_t value) {
        EXPECT_EQ(locations.size(), 5u);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Verifies spawning a victim renderer in a background tab and binding its
// VrpFlags.
IN_PROC_BROWSER_TEST_F(VrpFlagsBrowserTest,
                       StartRendererForVrpFlags_SpawnBackgroundTab) {
  // Navigate main browser window to test page and bind
  // VrpFlagsFactoryImpl for the main frame.
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/poc.html")));
  VrpFlagsFactoryImpl::Bind(shell()->web_contents()->GetPrimaryMainFrame(),
                            factory_.BindNewPipeAndPassReceiver());

  // Call StartRendererForVrpFlags with kSpawnBackgroundTab disposition
  // and get assigned port.
  mojo::Remote<vrp_flags::mojom::VrpFlags> victim_remote;
  uint16_t port = 0;
  {
    base::RunLoop run_loop;
    factory_->StartRendererForVrpFlags(
        vrp_flags::mojom::VictimDisposition::kSpawnBackgroundTab,
        victim_remote.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting([&](uint16_t assigned_port) {
          port = assigned_port;
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  EXPECT_NE(port, 0u);

  // Verify a new background tab/window was created and confirm host matching
  // assigned port.
  ASSERT_EQ(2u, Shell::windows().size());
  WebContents* victim_contents = Shell::windows()[1]->web_contents();
  EXPECT_TRUE(WaitForLoadStop(victim_contents));
  EXPECT_EQ(EvalJs(victim_contents, "window.location.host").ExtractString(),
            base::StringPrintf("victim.test:%u", port));

  // Invoke GetWriteLocations in the victim renderer remote and verify 5
  // write locations are returned.
  base::RunLoop run_loop;
  victim_remote->GetWriteLocations(base::BindLambdaForTesting(
      [&](const std::vector<uint64_t>& locations, uint64_t value) {
        EXPECT_EQ(locations.size(), 5u);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Verifies Site Isolation guarantees when spawning a victim renderer tab.
IN_PROC_BROWSER_TEST_F(VrpFlagsBrowserTest,
                       StartRendererForVrpFlags_SiteIsolation) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/poc.html")));
  VrpFlagsFactoryImpl::Bind(shell()->web_contents()->GetPrimaryMainFrame(),
                            factory_.BindNewPipeAndPassReceiver());

  // Request spawning a victim renderer in a foreground tab.
  mojo::Remote<vrp_flags::mojom::VrpFlags> victim_remote;
  uint16_t port = 0;
  {
    base::RunLoop run_loop;
    factory_->StartRendererForVrpFlags(
        vrp_flags::mojom::VictimDisposition::kSpawnForegroundTab,
        victim_remote.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting([&](uint16_t assigned_port) {
          port = assigned_port;
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  EXPECT_NE(port, 0u);

  // Verify that the VrpFlags Mojo interface is correctly bound across process
  // boundaries.
  base::RunLoop run_loop;
  victim_remote->GetWriteLocations(base::BindLambdaForTesting(
      [&](const std::vector<uint64_t>& locations, uint64_t value) {
        EXPECT_EQ(locations.size(), 5u);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Ensure that the attacker main frame and the newly spawned victim tab
  // (navigated to victim.test:<port>) are hosted in completely separate
  // RenderProcessHost instances.
  ASSERT_GE(Shell::windows().size(), 2u);
  RenderProcessHost* attacker_process =
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess();
  RenderProcessHost* victim_process =
      Shell::windows()[1]->web_contents()->GetPrimaryMainFrame()->GetProcess();
  EXPECT_NE(attacker_process, victim_process);
}

// Verifies manual victim tab spawning initiated via JavaScript window.open().
IN_PROC_BROWSER_TEST_F(VrpFlagsBrowserTest,
                       StartRendererForVrpFlags_WindowOpen) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/poc.html")));
  VrpFlagsFactoryImpl::Bind(shell()->web_contents()->GetPrimaryMainFrame(),
                            factory_.BindNewPipeAndPassReceiver());

  // Register a victim receiver with kManualSpawn.
  mojo::Remote<vrp_flags::mojom::VrpFlags> victim_remote;
  uint16_t port = 0;
  {
    base::RunLoop run_loop;
    factory_->StartRendererForVrpFlags(
        vrp_flags::mojom::VictimDisposition::kManualSpawn,
        victim_remote.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting([&](uint16_t assigned_port) {
          port = assigned_port;
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  EXPECT_NE(port, 0u);

  // Attacker JS opens victim popup via window.open targeting
  // victim.test:<port>, exercising VrpNavigationThrottle interception for
  // popup window creations.
  EXPECT_TRUE(ExecJs(
      shell(),
      base::StringPrintf(
          "window.w = window.open('http://victim.test:%u/poc.html');", port)));

  ASSERT_EQ(2u, Shell::windows().size());
  WebContents* popup_contents = Shell::windows()[1]->web_contents();
  EXPECT_TRUE(WaitForLoadStop(popup_contents));
  EXPECT_EQ(EvalJs(popup_contents, "window.location.host").ExtractString(),
            base::StringPrintf("victim.test:%u", port));

  // Verify cross-origin postMessage communication between attacker window and
  // popup.
  EXPECT_EQ(
      EvalJs(shell(),
             "new Promise((resolve) => {"
             "  window.addEventListener('message', (e) => resolve(e.data), "
             "{once: true});"
             "  window.w.postMessage('ping', '*');"
             "});")
          .ExtractString(),
      "pong");

  // Confirm VrpFlags Mojo IPC functionality with the popup's renderer.
  base::RunLoop run_loop;
  victim_remote->GetWriteLocations(base::BindLambdaForTesting(
      [&](const std::vector<uint64_t>& locations, uint64_t value) {
        EXPECT_EQ(locations.size(), 5u);
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Verifies manual victim renderer spawning initiated via JavaScript iframe
// injection.
IN_PROC_BROWSER_TEST_F(VrpFlagsBrowserTest, StartRendererForVrpFlags_Iframe) {
  ASSERT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/poc.html")));
  VrpFlagsFactoryImpl::Bind(shell()->web_contents()->GetPrimaryMainFrame(),
                            factory_.BindNewPipeAndPassReceiver());

  // Register a victim receiver with kManualSpawn.
  mojo::Remote<vrp_flags::mojom::VrpFlags> victim_remote;
  uint16_t port = 0;
  {
    base::RunLoop run_loop;
    factory_->StartRendererForVrpFlags(
        vrp_flags::mojom::VictimDisposition::kManualSpawn,
        victim_remote.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting([&](uint16_t assigned_port) {
          port = assigned_port;
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  EXPECT_NE(port, 0u);

  // Attacker JS injects an iframe targeting victim.test:<port>, exercising
  // VrpNavigationThrottle interception for out-of-process subframe (OOPIF)
  // navigations.
  EXPECT_TRUE(ExecJs(
      shell(),
      base::StringPrintf("const iframe = document.createElement('iframe');"
                         "iframe.id = 'victim_frame';"
                         "iframe.src = 'http://victim.test:%u/poc.html';"
                         "document.body.appendChild(iframe);",
                         port)));

  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Test cross-frame postMessage communication between parent frame and
  // iframe.
  EXPECT_EQ(
      EvalJs(shell(),
             "new Promise((resolve) => {"
             "  window.addEventListener('message', (e) => resolve(e.data), "
             "{once: true});"
             "  "
             "document.getElementById('victim_frame').contentWindow."
             "postMessage('ping', '*');"
             "});")
          .ExtractString(),
      "pong");

  // Verify VrpFlags Mojo interface binding to the subframe renderer.
  base::RunLoop run_loop;
  victim_remote->GetWriteLocations(base::BindLambdaForTesting(
      [&](const std::vector<uint64_t>& locations, uint64_t value) {
        EXPECT_EQ(locations.size(), 5u);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace content
