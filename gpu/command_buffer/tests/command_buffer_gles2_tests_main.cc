// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_executor.h"
#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "gpu/gles2_conform_support/egl/test_support.h"  // NOLINT

// This file implements the main entry point for tests for command_buffer_gles2,
// the mode of command buffer where the code is compiled as a standalone dynamic
// library and exposed through EGL API.
namespace {

int RunHelper(base::TestSuite* testSuite) {
#if defined(USE_OZONE)
  base::SingleThreadTaskExecutor executor(base::MessagePumpType::UI);
#else
  base::SingleThreadTaskExecutor executor(base::MessagePumpType::IO);
#endif
  return testSuite->Run();
}

}  // namespace

int main(int argc, char** argv) {
// NOTE: we initialize globals through TestSuite constructor or through JNI
// library registration process on Android.

// However, when the system is compiled with component build,
// command_buffer_gles2 library and this test runner share the globals, such
// as AtExitManager and JVM references.  When command_buffer_gles2 is run with
// the test runner, the globals may be populated.  Any other app linking to
// command_buffer_gles2 of course can not provide the globals.

// When the system is compiled without component build, command_buffer_gles2
// gets its own globals, while the test runner gets its own. The runner
// initialize different global variables than the ones command_buffer_gles2
// library uses. For example, there should be a global AtExitManager for the
// test runner, and there should be a global AtExitManager that the library
// uses. Similarly, if the test runner would use JNI, it should have global
// reference to the JNI environent. If the command_buffer_gles2 library would
// use JNI, it should have its own global reference to the JNI that always
// remains null. The reference of the library should always stay null, since
// JNI is not part of command_buffer_gles2 exported API (EGL API), and thus
// there is no way for the client of the library to populate the JNI
// pointer. The client may not even be a Java app.

// We signify that the globals have been initialized when running
// the component build.
#if defined(COMPONENT_BUILD)
  g_command_buffer_gles_has_atexit_manager = true;
#endif

  base::TestSuite test_suite(argc, argv);
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool pool;
#endif
  testing::InitGoogleMock(&argc, argv);
  return base::LaunchUnitTestsSerially(
      argc, argv, base::BindOnce(&RunHelper, base::Unretained(&test_suite)));
}
