// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/vrp_flags/vrp_flags.h"

#include <set>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/run_loop.h"
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
#include "sandbox/policy/switches.h"
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
  }

  mojo::Remote<vrp_flags::mojom::VrpFlags> GetRemote() {
    VrpFlagsFactoryImpl::Bind(factory_.BindNewPipeAndPassReceiver());

    mojo::Remote<vrp_flags::mojom::VrpFlags> remote;
    factory_->BindBrowserVrpFlags(remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  mojo::Remote<vrp_flags::mojom::VrpFlags> GetGpuRemote() {
    VrpFlagsFactoryImpl::Bind(factory_.BindNewPipeAndPassReceiver());

    mojo::Remote<vrp_flags::mojom::VrpFlags> remote;
    factory_->BindGpuVrpFlags(remote.BindNewPipeAndPassReceiver());
    return remote;
  }

  mojo::Remote<vrp_flags::mojom::VrpFlags> GetNetworkRemote() {
    VrpFlagsFactoryImpl::Bind(factory_.BindNewPipeAndPassReceiver());

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

}  // namespace content
