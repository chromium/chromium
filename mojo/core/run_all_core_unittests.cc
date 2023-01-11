// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "mojo/core/mojo_core_unittest.h"
#include "mojo/public/c/system/core.h"
#include "mojo/public/cpp/system/dynamic_library_support.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

base::FilePath GetMojoCoreLibraryPath() {
#if BUILDFLAG(IS_FUCHSIA)
  return base::FilePath("libmojo_core.so");
#else  // BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN)
  const char kLibraryFilename[] = "mojo_core.dll";
#else
  const char kLibraryFilename[] = "libmojo_core.so";
#endif
  base::FilePath executable_dir =
      base::CommandLine::ForCurrentProcess()->GetProgram().DirName();
  if (executable_dir.IsAbsolute())
    return executable_dir.AppendASCII(kLibraryFilename);

  base::FilePath current_directory;
  CHECK(base::GetCurrentDirectory(&current_directory));
  return current_directory.Append(executable_dir).AppendASCII(kLibraryFilename);

#endif  // BUILDFLAG(IS_FUCHSIA)
}

int main(int argc, char** argv) {
  base::TestSuite test_suite(argc, argv);

  MojoInitializeFlags flags = MOJO_INITIALIZE_FLAG_NONE;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (!command_line.HasSwitch(switches::kTestChildProcess))
    flags |= MOJO_INITIALIZE_FLAG_AS_BROKER;

  absl::optional<base::FilePath> library_path;
  if (command_line.HasSwitch(switches::kMojoUseExplicitLibraryPath))
    library_path = GetMojoCoreLibraryPath();

  if (command_line.HasSwitch(switches::kMojoLoadBeforeInit)) {
    CHECK_EQ(MOJO_RESULT_OK, mojo::LoadCoreLibrary(library_path));
    CHECK_EQ(MOJO_RESULT_OK, mojo::InitializeCoreLibrary(flags));
  } else {
    CHECK_EQ(MOJO_RESULT_OK,
             mojo::LoadAndInitializeCoreLibrary(library_path, flags));
  }

  int result = base::LaunchUnitTests(
      argc, argv,
      base::BindOnce(&base::TestSuite::Run, base::Unretained(&test_suite)));

  CHECK_EQ(MOJO_RESULT_OK, MojoShutdown(nullptr));
  return result;
}
