// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/utility_process_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_service.mojom.h"
#include "content/test/sandbox_status.test-mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "printing/buildflags/buildflags.h"
#include "sandbox/policy/linux/sandbox_linux.h"
#include "sandbox/policy/switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/assistant/buildflags.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

using sandbox::policy::SandboxLinux;
using sandbox::policy::SandboxType;

namespace {

std::vector<SandboxType> GetSandboxTypesToTest() {
  std::vector<SandboxType> types;
  // We need the standard sandbox config to run this test.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          sandbox::policy::switches::kNoSandbox)) {
    return types;
  }

  for (SandboxType t = SandboxType::kNoSandbox; t <= SandboxType::kMaxValue;
       t = static_cast<SandboxType>(static_cast<int>(t) + 1)) {
    // These sandbox types can't be spawned in a utility process.
    if (t == SandboxType::kRenderer || t == SandboxType::kGpu)
      continue;
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
    if (t == SandboxType::kZygoteIntermediateSandbox)
      continue;
#endif

    types.push_back(t);
  }
  return types;
}

}  // namespace

namespace content {

constexpr char kTestProcessName[] = "sandbox_test_process";

class UtilityProcessSandboxBrowserTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<SandboxType> {
 public:
  UtilityProcessSandboxBrowserTest() = default;
  ~UtilityProcessSandboxBrowserTest() override = default;

 protected:
  void RunUtilityProcess() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    base::RunLoop run_loop;
    done_closure_ =
        base::BindOnce(&UtilityProcessSandboxBrowserTest::DoneRunning,
                       base::Unretained(this), run_loop.QuitClosure());
    if (base::FeatureList::IsEnabled(features::kProcessHostOnUI)) {
      RunUtilityProcessOnProcessThread();
    } else {
      GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(&UtilityProcessSandboxBrowserTest::
                                        RunUtilityProcessOnProcessThread,
                                    base::Unretained(this)));
      run_loop.Run();
    }
  }

 private:
  void RunUtilityProcessOnProcessThread() {
    DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                            ? content::BrowserThread::UI
                            : content::BrowserThread::IO);
    UtilityProcessHost* host = new UtilityProcessHost();
    host->SetSandboxType(GetParam());
    host->SetName(u"SandboxTestProcess");
    host->SetMetricsName(kTestProcessName);
    EXPECT_TRUE(host->Start());

    host->GetChildProcess()->BindReceiver(
        service_.BindNewPipeAndPassReceiver());
    service_->GetSandboxStatus(base::BindOnce(
        &UtilityProcessSandboxBrowserTest::OnGotSandboxStatusOnProcessThread,
        base::Unretained(this)));
  }

  void OnGotSandboxStatusOnProcessThread(int32_t sandbox_status) {
    DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(features::kProcessHostOnUI)
                            ? content::BrowserThread::UI
                            : content::BrowserThread::IO);

    // Aside from kNoSandbox, every utility process launched explicitly with a
    // sandbox type should always end up with a sandbox.
    // kVideoCapture is equivalent to kNoSandbox on all platforms except
    // Fuchsia.
    switch (GetParam()) {
      case SandboxType::kNoSandbox:
      case SandboxType::kVideoCapture:
        EXPECT_EQ(sandbox_status, 0);
        break;

      case SandboxType::kCdm:
      case SandboxType::kPpapi:
      case SandboxType::kPrintCompositor:
      case SandboxType::kService:
      case SandboxType::kUtility: {
        constexpr int kExpectedFullSandboxFlags =
            SandboxLinux::kPIDNS | SandboxLinux::kNetNS |
            SandboxLinux::kSeccompBPF | SandboxLinux::kYama |
            SandboxLinux::kSeccompTSYNC | SandboxLinux::kUserNS;
        EXPECT_EQ(sandbox_status, kExpectedFullSandboxFlags);
        break;
      }

      case SandboxType::kAudio:
#if BUILDFLAG(IS_CHROMEOS_ASH)
      case SandboxType::kIme:
      case SandboxType::kTts:
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
      case SandboxType::kLibassistant:
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      case SandboxType::kNetwork:
#if BUILDFLAG(ENABLE_PRINTING)
      case SandboxType::kPrintBackend:
#endif
      case SandboxType::kSpeechRecognition: {
        constexpr int kExpectedPartialSandboxFlags =
            SandboxLinux::kSeccompBPF | SandboxLinux::kYama |
            SandboxLinux::kSeccompTSYNC;
        EXPECT_EQ(sandbox_status, kExpectedPartialSandboxFlags);
        break;
      }

      case SandboxType::kGpu:
      case SandboxType::kRenderer:
      case SandboxType::kZygoteIntermediateSandbox:
        NOTREACHED();
        break;
    }

    service_.reset();
    GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(done_closure_));
  }

  void DoneRunning(base::OnceClosure quit_closure) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    std::move(quit_closure).Run();
  }

  mojo::Remote<mojom::SandboxStatusService> service_;
  base::OnceClosure done_closure_;
};

IN_PROC_BROWSER_TEST_P(UtilityProcessSandboxBrowserTest, VerifySandboxType) {
  RunUtilityProcess();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UtilityProcessSandboxBrowserTest,
    testing::ValuesIn(GetSandboxTypesToTest()),
    [](const testing::TestParamInfo<
        UtilityProcessSandboxBrowserTest::ParamType>& info) {
      auto name = sandbox::policy::StringFromUtilitySandboxType(info.param);
      name[0] = base::ToUpperASCII(name[0]);
      return name;
    });

// In some configurations (e.g. Linux ASAN) GetSandboxTypesToTest() returns an
// empty list. Suppress runtime warnings about unparameterized tests. See
// https://crbug.com/1192206
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(UtilityProcessSandboxBrowserTest);

}  // namespace content
