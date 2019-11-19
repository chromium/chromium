// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "base/test/test_switches.h"
#include "google_apis/google_api_keys.h"
#include "mojo/core/embedder/embedder.h"
#include "net/base/escape.h"
#include "remoting/test/chromoting_test_driver_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace switches {
const char kAuthCodeSwitchName[] = "authcode";
const char kHelpSwitchName[] = "help";
const char kHostNameSwitchName[] = "hostname";
const char kHostJidSwitchName[] = "hostjid";
const char kLoggingLevelSwitchName[] = "verbosity";
const char kPinSwitchName[] = "pin";
const char kRefreshTokenPathSwitchName[] = "refresh-token-path";
const char kSingleProcessTestsSwitchName[] = "single-process-tests";
const char kShowHostListSwitchName[] = "show-host-list";
const char kTestEnvironmentSwitchName[] = "use-test-env";
const char kUserNameSwitchName[] = "username";
}

namespace {
const char kChromotingAuthScopeValues[] =
    "https://www.googleapis.com/auth/chromoting "
    "https://www.googleapis.com/auth/googletalk "
    "https://www.googleapis.com/auth/userinfo.email";

std::string GetAuthorizationCodeUri() {
  // Replace space characters with a '+' sign when formatting.
  bool use_plus = true;
  return base::StringPrintf(
      "https://accounts.google.com/o/oauth2/auth"
      "?scope=%s"
      "&redirect_uri=https://chromoting-oauth.talkgadget.google.com/"
      "talkgadget/oauth/chrome-remote-desktop/dev"
      "&response_type=code"
      "&client_id=%s"
      "&access_type=offline"
      "&approval_prompt=force",
      net::EscapeUrlEncodedData(kChromotingAuthScopeValues, use_plus).c_str(),
      net::EscapeUrlEncodedData(
          google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING),
          use_plus).c_str());
}

void PrintUsage() {
  printf("\n************************************\n");
  printf("*** Chromoting Test Driver Usage ***\n");
  printf("************************************\n");

  printf("\nUsage:\n");
  printf("  chromoting_test_driver --username=<example@gmail.com> [options]"
         " --hostname=<example hostname>\n");
  printf("\nRequired Parameters:\n");
  printf("  %s: Specifies which account to use when running tests\n",
         switches::kUserNameSwitchName);
  printf("  %s: Specifies which host to connect to when running tests\n",
         switches::kHostNameSwitchName);
  printf(
      "  %s: Retrieves and displays the connection status for all known "
      "hosts, no tests will be run\n",
      switches::kShowHostListSwitchName);
  printf("\nOptional Parameters:\n");
  printf("  %s: Exchanged for a refresh and access token for authentication\n",
         switches::kAuthCodeSwitchName);
  printf("  %s: Displays additional usage information\n",
         switches::kHelpSwitchName);
  printf("  %s: Path to a JSON file containing username/refresh_token KVPs\n",
         switches::kRefreshTokenPathSwitchName);
  printf("  %s: Specifies the optional logging level of the tool (0-3)."
         " [default: off]\n", switches::kLoggingLevelSwitchName);
  printf(
      "  %s: Specifies that the test environment APIs should be used."
      " [default: false]\n",
      switches::kTestEnvironmentSwitchName);
}

void PrintAuthCodeInfo() {
  printf("\n*******************************\n");
  printf("*** Auth Code Example Usage ***\n");
  printf("*******************************\n\n");

  printf("If this is the first time you are running the tool,\n");
  printf("you will need to provide an authorization code.\n");
  printf("This code will be exchanged for a long term refresh token which\n");
  printf("will be stored locally and used to acquire a short lived access\n");
  printf("token to connect to the remoting service apis and establish a\n");
  printf("remote host connection.\n\n");

  printf("Note: You may need to repeat this step if the stored refresh token");
  printf("\n      has been revoked or expired.\n");
  printf("      Passing in the same auth code twice will result in an error\n");

  printf("\nFollow these steps to produce an auth code:\n"
         " - Open the Authorization URL link shown below in your browser\n"
         " - Approve the requested permissions for the tool\n"
         " - Copy the 'code' value in the redirected URL\n"
         " - Run the tool and pass in copied auth code as a parameter\n");

  printf("\nAuthorization URL:\n");
  printf("%s\n", GetAuthorizationCodeUri().c_str());

  printf("\nRedirected URL Example:\n");
  printf("https://chromoting-oauth.talkgadget.google.com/talkgadget/oauth/"
         "chrome-remote-desktop/dev?code=4/AKtf...\n");

  printf("\nTool usage example with the newly created auth code:\n");
  printf("chromoting_test_driver --%s=example@gmail.com --%s=example_host_name"
         " --%s=4/AKtf...\n\n",
         switches::kUserNameSwitchName,
         switches::kHostNameSwitchName,
         switches::kAuthCodeSwitchName);
}

void PrintJsonFileInfo() {
  printf("\n****************************************\n");
  printf("*** Refresh Token File Example Usage ***\n");
  printf("****************************************\n\n");

  printf("In order to use this option, a valid JSON file must exist, be\n");
  printf("properly formatted, and contain a username/token KVP.\n");
  printf("Contents of example_file.json\n");
  printf("{\n");
  printf("  \"username1@fauxdomain.com\": \"1/3798Gsdf898shksdvfyi8sshad\",\n");
  printf("  \"username2@fauxdomain.com\": \"1/8974sdf87asdgadfgaerhfRsAa\",\n");
  printf("}\n\n");

  printf("\nTool usage example:\n");
  printf("chromoting_test_driver --%s=%s --%s=example_host_name"
         " --%s=./example_file.json\n\n",
         switches::kUserNameSwitchName, "username1@fauxdomain.com",
         switches::kHostNameSwitchName, switches::kRefreshTokenPathSwitchName);
}

}  // namespace

int main(int argc, char* argv[]) {
  base::TestSuite test_suite(argc, argv);
  base::FeatureList::InitializeInstance(std::string(), std::string());
  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);

  if (!base::CommandLine::InitializedForCurrentProcess()) {
    if (!base::CommandLine::Init(argc, argv)) {
      LOG(ERROR) << "Failed to initialize command line singleton.";
      return -1;
    }
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  DCHECK(command_line);

  // Do not retry if tests fails.
  command_line->AppendSwitchASCII(switches::kTestLauncherRetryLimit, "0");
  command_line->AppendSwitchASCII(
      switches::kIsolatedScriptTestLauncherRetryLimit, "0");

  // Different tests may require access to the same host if run in parallel.
  // To avoid shared resource contention, tests will be run one at a time.
  command_line->AppendSwitch(switches::kSingleProcessTestsSwitchName);

  // If the user passed in the help flag, then show the help info for this tool
  // and 'run' the tests which will print the gtest specific help and then exit.
  // NOTE: We do this check after updating the switches as otherwise the gtest
  //       help is written in parallel with our text and can appear interleaved.
  if (command_line->HasSwitch(switches::kHelpSwitchName)) {
    PrintUsage();
    PrintJsonFileInfo();
    PrintAuthCodeInfo();
#if defined(OS_IOS)
    return base::LaunchUnitTests(
        argc, argv,
        base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
#else
    return base::LaunchUnitTestsSerially(
        argc, argv,
        base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
#endif
  }

  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "ChromotingTestDriver");

  mojo::core::Init();

  // Update the logging verbosity level if user specified one.
  std::string verbosity_level(
      command_line->GetSwitchValueASCII(switches::kLoggingLevelSwitchName));
  if (!verbosity_level.empty()) {
    // Turn on logging for the test_driver and remoting components.
    // This switch is parsed during logging::InitLogging.
    command_line->AppendSwitchASCII("vmodule",
                                    "*/remoting/*=" + verbosity_level);
    logging::LoggingSettings logging_settings;
    logging::InitLogging(logging_settings);
  }

  remoting::test::ChromotingTestDriverEnvironment::EnvironmentOptions options;

  options.user_name =
      command_line->GetSwitchValueASCII(switches::kUserNameSwitchName);
  if (options.user_name.empty()) {
    LOG(ERROR) << "No username passed in, can't authenticate or run tests!";
    return -1;
  }
  VLOG(1) << "Running chromoting tests as: " << options.user_name;

  // Check to see if the user passed in a one time use auth_code for
  // refreshing their credentials.
  std::string auth_code =
      command_line->GetSwitchValueASCII(switches::kAuthCodeSwitchName);
  options.refresh_token_file_path =
     command_line->GetSwitchValuePath(switches::kRefreshTokenPathSwitchName);

  // The host name determines which host to initiate a session with from the
  // host list returned from the directory service.
  options.host_name =
      command_line->GetSwitchValueASCII(switches::kHostNameSwitchName);

  if (!options.host_name.empty()) {
    VLOG(1) << "host_name: '" << options.host_name << "'";
  } else if (command_line->HasSwitch(switches::kShowHostListSwitchName)) {
    options.host_name = "unspecified";
  } else {
    LOG(ERROR) << "No hostname passed in, connect to host requires hostname!";
    return -1;
  }

  options.host_jid =
      command_line->GetSwitchValueASCII(switches::kHostJidSwitchName);
  VLOG(1) << "host_jid: '" << options.host_jid << "'";

  options.pin = command_line->GetSwitchValueASCII(switches::kPinSwitchName);

  options.use_test_environment =
      command_line->HasSwitch(switches::kTestEnvironmentSwitchName);

  // Create and register our global test data object. It will handle
  // retrieving an access token or host list for the user. The GTest framework
  // will own the lifetime of this object once it is registered below.
  std::unique_ptr<remoting::test::ChromotingTestDriverEnvironment> shared_data(
      new remoting::test::ChromotingTestDriverEnvironment(options));

  if (!shared_data->Initialize(auth_code)) {
    VLOG(1) << "Failed to initialize ChromotingTestDriverEnvironment instance.";
    // If we failed to initialize our shared data object, then bail.
    return -1;
  }

  if (command_line->HasSwitch(switches::kShowHostListSwitchName)) {
    // When this flag is specified, we will show the host list and exit.
    shared_data->RefreshHostList();
    shared_data->DisplayHostList();
    return 0;
  }

  // This method is necessary as there are occasional propagation delays in the
  // backend and we don't want the test to fail because of that.
  if (!shared_data->WaitForHostOnline()) {
    VLOG(1) << "The expected host was not available for connections.";
    // Host is not online. No point running further tests.
    return -1;
  }

  if (options.pin.empty()) {
    LOG(WARNING) << "No PIN specified, tests may not run reliably.";
  }

  // Since we've successfully set up our shared_data object, we'll assign the
  // value to our global* and transfer ownership to the framework.
  remoting::test::g_chromoting_shared_data = shared_data.release();
  testing::AddGlobalTestEnvironment(remoting::test::g_chromoting_shared_data);

  // Running the tests serially will avoid clients from connecting to the same
  // host.
#if defined(OS_IOS)
  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
#else
  return base::LaunchUnitTestsSerially(
      argc, argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
#endif
}
