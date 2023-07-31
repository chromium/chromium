// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/test/browser_test.h"
#include "content/test/ppapi/ppapi_test.h"
#include "ppapi/shared_impl/test_utils.h"

// This file lists tests for Pepper APIs (without NaCl) against content_shell.
// TODO(teravest): Move more tests here. http://crbug.com/371873

// See chrome/test/ppapi/ppapi_browsertests.cc for Pepper testing that's
// covered in browser_tests.

namespace content {
namespace {

// This macro finesses macro expansion to do what we want.
#define STRIP_PREFIXES(test_name) ppapi::StripTestPrefixes(#test_name)

#if defined(THREAD_SANITIZER)
#define DISABLE_IF_TSAN(test_name) DISABLED_##test_name
#else
#define DISABLE_IF_TSAN(test_name) test_name
#endif

#define TEST_PPAPI_IN_PROCESS(test_name) \
  IN_PROC_BROWSER_TEST_F(PPAPITest, test_name) { \
    RunTest(STRIP_PREFIXES(test_name)); \
  }

// OutOfProcessPPAPITest tests time out under ThreadSanitizer,
// see https://crbug.com/448323.
#define TEST_PPAPI_OUT_OF_PROCESS(test_name) \
  IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest, DISABLE_IF_TSAN(test_name)) { \
    RunTest(STRIP_PREFIXES(test_name)); \
  }

// Doesn't work in CrOS builds, http://crbug.com/619765
#if BUILDFLAG(IS_CHROMEOS_ASH)
#define MAYBE_BrowserFont DISABLED_BrowserFont
#else
#define MAYBE_BrowserFont BrowserFont
#endif
TEST_PPAPI_OUT_OF_PROCESS(MAYBE_BrowserFont)

TEST_PPAPI_IN_PROCESS(Buffer)
TEST_PPAPI_OUT_OF_PROCESS(Buffer)

TEST_PPAPI_IN_PROCESS(CharSet)
TEST_PPAPI_OUT_OF_PROCESS(CharSet)

TEST_PPAPI_IN_PROCESS(Console)
TEST_PPAPI_OUT_OF_PROCESS(Console)

TEST_PPAPI_IN_PROCESS(Core)
TEST_PPAPI_OUT_OF_PROCESS(Core)

TEST_PPAPI_IN_PROCESS(Crypto)
TEST_PPAPI_OUT_OF_PROCESS(Crypto)

TEST_PPAPI_IN_PROCESS(Graphics2D)
TEST_PPAPI_OUT_OF_PROCESS(Graphics2D)

TEST_PPAPI_IN_PROCESS(ImageData)
TEST_PPAPI_OUT_OF_PROCESS(ImageData)

// Fails on macOS; https://crbug.com/14531024
#if BUILDFLAG(IS_MAC)
#define MAYBE_InputEvent DISABLED_InputEvent
#else
#define MAYBE_InputEvent InputEvent
#endif
TEST_PPAPI_OUT_OF_PROCESS(MAYBE_InputEvent)

// "Instance" tests are really InstancePrivate tests. InstancePrivate is not
// supported in NaCl, so these tests are only run trusted.
// Also note that these tests are run separately on purpose (versus collapsed
// in to one IN_PROC_BROWSER_TEST_F macro), because some of them have leaks
// on purpose that will look like failures to tests that are run later.
TEST_PPAPI_IN_PROCESS(Instance_ExecuteScript)
TEST_PPAPI_OUT_OF_PROCESS(Instance_ExecuteScript)

IN_PROC_BROWSER_TEST_F(PPAPITest,
                       Instance_ExecuteScriptAtInstanceShutdown) {
  // In other tests, we use one call to RunTest so that the tests can all run
  // in one plugin instance. This saves time on loading the plugin (especially
  // for NaCl). Here, we actually want to destroy the Instance, to test whether
  // the destructor can run ExecJs successfully. That's why we have two
  // separate calls to RunTest; the second one forces a navigation which
  // destroys the instance from the prior RunTest.
  // See test_instance_deprecated.cc for more information.
  RunTest("Instance_SetupExecuteScriptAtInstanceShutdown");
  RunTest("Instance_ExecuteScriptAtInstanceShutdown");
}
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest,
    DISABLE_IF_TSAN(Instance_ExecuteScriptAtInstanceShutdown)) {
  // (See the comment for the in-process version of this test above)
  RunTest("Instance_SetupExecuteScriptAtInstanceShutdown");
  RunTest("Instance_ExecuteScriptAtInstanceShutdown");
}

TEST_PPAPI_IN_PROCESS(Instance_LeakedObjectDestructors)
TEST_PPAPI_OUT_OF_PROCESS(Instance_LeakedObjectDestructors)

// We run and reload the RecursiveObjects test to ensure that the
// InstanceObject (and others) are properly cleaned up after the first run.
IN_PROC_BROWSER_TEST_F(PPAPITest, Instance_RecursiveObjects) {
  RunTestAndReload("Instance_RecursiveObjects");
}
// TODO(dmichael): Make it work out-of-process (or at least see whether we
// care).
IN_PROC_BROWSER_TEST_F(OutOfProcessPPAPITest,
                       DISABLED_Instance_RecursiveObjects) {
  RunTestAndReload("Instance_RecursiveObjects");
}

// Flaky on all platforms (crbug.com/438729, crbug.com/800376)
TEST_PPAPI_OUT_OF_PROCESS(DISABLED_MediaStreamAudioTrack)

TEST_PPAPI_OUT_OF_PROCESS(MediaStreamVideoTrack)

TEST_PPAPI_IN_PROCESS(Memory)
TEST_PPAPI_OUT_OF_PROCESS(Memory)

TEST_PPAPI_OUT_OF_PROCESS(MessageHandler)

TEST_PPAPI_OUT_OF_PROCESS(MessageLoop_Basics)
TEST_PPAPI_OUT_OF_PROCESS(MessageLoop_Post)

TEST_PPAPI_OUT_OF_PROCESS(NetworkProxy)

// TODO(danakj): http://crbug.com/115286
TEST_PPAPI_IN_PROCESS(DISABLED_Scrollbar)
// http://crbug.com/89961
TEST_PPAPI_OUT_OF_PROCESS(DISABLED_Scrollbar)

TEST_PPAPI_IN_PROCESS(TraceEvent)
TEST_PPAPI_OUT_OF_PROCESS(TraceEvent)

TEST_PPAPI_IN_PROCESS(URLUtil)
TEST_PPAPI_OUT_OF_PROCESS(URLUtil)

TEST_PPAPI_IN_PROCESS(Var)
TEST_PPAPI_OUT_OF_PROCESS(Var)

// Flaky on mac, http://crbug.com/121107
#if BUILDFLAG(IS_MAC)
#define MAYBE_VarDeprecated DISABLED_VarDeprecated
#else
#define MAYBE_VarDeprecated VarDeprecated
#endif
TEST_PPAPI_IN_PROCESS(VarDeprecated)
TEST_PPAPI_OUT_OF_PROCESS(MAYBE_VarDeprecated)

TEST_PPAPI_IN_PROCESS(VarResource)
TEST_PPAPI_OUT_OF_PROCESS(VarResource)

// Flaky on Win, Linux and CrOS, http://crbug.com/602877
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_VideoDecoder DISABLED_VideoDecoder
#else
#define MAYBE_VideoDecoder VideoDecoder
#endif
TEST_PPAPI_OUT_OF_PROCESS(MAYBE_VideoDecoder)

TEST_PPAPI_IN_PROCESS(VideoDecoderDev)
TEST_PPAPI_OUT_OF_PROCESS(VideoDecoderDev)

TEST_PPAPI_OUT_OF_PROCESS(VideoEncoder)

}  // namespace
}  // namespace content
