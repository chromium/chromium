// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/ppapi/ppapi_test.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/ppapi_test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/base/media_switches.h"
#include "net/base/filename_util.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "third_party/blink/public/common/switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/cras_audio_client.h"
#endif

namespace content {

PPAPITestMessageHandler::PPAPITestMessageHandler() {
}

TestMessageHandler::MessageResponse PPAPITestMessageHandler::HandleMessage(
    const std::string& json) {
  std::string trimmed;
  base::TrimString(json, "\"", &trimmed);
  if (trimmed == "...")
    return CONTINUE;
  message_ = trimmed;
  return DONE;
}

void PPAPITestMessageHandler::Reset() {
  TestMessageHandler::Reset();
  message_.clear();
}

PPAPITestBase::PPAPITestBase() { }

void PPAPITestBase::SetUpCommandLine(base::CommandLine* command_line) {
  // Some stuff is hung off of the testing interface which is not enabled
  // by default.
  command_line->AppendSwitch(switches::kEnablePepperTesting);

  // Smooth scrolling confuses the scrollbar test.
  command_line->AppendSwitch(switches::kDisableSmoothScrolling);

  // Allow manual garbage collection.
  command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                  "--expose_gc");

  command_line->AppendSwitch(switches::kUseFakeUIForMediaStream);

  command_line->AppendSwitchASCII(
      switches::kAutoplayPolicy,
      switches::autoplay::kNoUserGestureRequiredPolicy);

  // This doesn't test what we ship, but is needed by some tests (like
  // TestMessageHandler::TestExceptions, fileSystem-related tests) and given
  // PPAPI deprecation it doesn't seem worth fixing the tests now.
  command_line->AppendSwitch(switches::kAllowFileAccessFromFiles);

  // TODO(crbug.com/40166667): Remove once NaCl code can be deleted.
  command_line->AppendSwitchASCII(blink::switches::kBlinkSettings,
                                  "allowNonEmptyNavigatorPlugins=true");
}

GURL PPAPITestBase::GetTestFileUrl(const std::string& test_case) {
  base::FilePath test_path;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;

    EXPECT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_path));
    test_path = test_path.Append(FILE_PATH_LITERAL("ppapi"));
    test_path = test_path.Append(FILE_PATH_LITERAL("tests"));
    test_path = test_path.Append(FILE_PATH_LITERAL("test_case.html"));

    // Sanity check the file name.
    EXPECT_TRUE(base::PathExists(test_path));
  }

  GURL test_url = net::FilePathToFileURL(test_path);

  GURL::Replacements replacements;
  std::string query = BuildQuery(std::string(), test_case);
  replacements.SetQueryStr(query);
  return test_url.ReplaceComponents(replacements);
}

void PPAPITestBase::RunTest(const std::string& test_case) {
  GURL url = GetTestFileUrl(test_case);
  RunTestURL(url);
}

void PPAPITestBase::RunTestAndReload(const std::string& test_case) {
  GURL url = GetTestFileUrl(test_case);
  RunTestURL(url);
  // If that passed, we simply run the test again, which navigates again.
  RunTestURL(url);
}

void PPAPITestBase::RunTestURL(const GURL& test_url) {
  // See comment above TestingInstance in ppapi/test/testing_instance.h.
  // Basically it sends messages using the DOM automation controller. The
  // value of "..." means it's still working and we should continue to wait,
  // any other value indicates completion (in this case it will start with
  // "PASS" or "FAIL"). This keeps us from timing out on waits for long tests.
  PPAPITestMessageHandler handler;
  JavascriptTestObserver observer(shell()->web_contents(), &handler);
  shell()->LoadURL(test_url);
  shell()->web_contents()->Focus();

  ASSERT_TRUE(observer.Run()) << handler.error_message();
  EXPECT_STREQ("PASS", handler.message().c_str());
}

PPAPITest::PPAPITest() : in_process_(true) {
}

void PPAPITest::SetUpCommandLine(base::CommandLine* command_line) {
  PPAPITestBase::SetUpCommandLine(command_line);
  ASSERT_TRUE(ppapi::RegisterTestPlugin(command_line));

  if (in_process_)
    command_line->AppendSwitch(switches::kPpapiInProcess);
}

std::string PPAPITest::BuildQuery(const std::string& base,
                                  const std::string& test_case){
  return base::StringPrintf("%stestcase=%s", base.c_str(), test_case.c_str());
}

OutOfProcessPPAPITest::OutOfProcessPPAPITest() {
  in_process_ = false;
}

void OutOfProcessPPAPITest::SetUp() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::CrasAudioClient::InitializeFake();
  ash::CrasAudioHandler::InitializeForTesting();
#endif
  ContentBrowserTest::SetUp();
}

void OutOfProcessPPAPITest::TearDown() {
  ContentBrowserTest::TearDown();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::CrasAudioHandler::Shutdown();
  ash::CrasAudioClient::Shutdown();
#endif
}

}  // namespace content
