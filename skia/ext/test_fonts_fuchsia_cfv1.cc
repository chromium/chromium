// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/test_fonts_fuchsia.h"

#include <fuchsia/fonts/cpp/fidl.h>
#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/sys/cpp/component_context.h>

#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "skia/ext/test_fonts.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace skia {

namespace {

// Runs the fonts component via fuchsia.sys.Launcher.
class TestFontsProvider {
 public:
  TestFontsProvider();
  TestFontsProvider(const TestFontsProvider&) = delete;
  TestFontsProvider& operator=(const TestFontsProvider&) = delete;
  ~TestFontsProvider();

  fuchsia::fonts::ProviderHandle GetProvider();

 private:
  fidl::InterfaceHandle<fuchsia::sys::ComponentController> controller_;
  absl::optional<sys::ServiceDirectory> services_client_;
};

TestFontsProvider::TestFontsProvider() {
  // Start a fuchsia.fonts.Provider instance and configure it to load the test
  // fonts, which must be bundled in the calling process' package.
  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url = "fuchsia-pkg://fuchsia.com/fonts#meta/fonts.cmx";
  // Note: the manifest name here matches the default used by the Fuchsia fonts
  // component so that the file can be found automagically by the modern (cfv2)
  // variant.
  launch_info.arguments.emplace(
      {"--font-manifest", "/test_fonts/all.font_manifest.json"});
  launch_info.flat_namespace = fuchsia::sys::FlatNamespace::New();
  launch_info.flat_namespace->paths.push_back("/test_fonts");

  base::FilePath assets_path;
  CHECK(base::PathService::Get(base::DIR_ASSETS, &assets_path))
      << "Can't get DIR_ASSETS";
  launch_info.flat_namespace->directories.push_back(
      base::OpenDirectoryHandle(assets_path.AppendASCII("test_fonts"))
          .TakeChannel());

  fidl::InterfaceHandle<fuchsia::io::Directory> font_provider_services_dir;
  launch_info.directory_request =
      font_provider_services_dir.NewRequest().TakeChannel();

  fuchsia::sys::LauncherSyncPtr launcher;
  auto status =
      base::ComponentContextForProcess()->svc()->Connect(launcher.NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "Connect to fuchsia.sys.Launcher";
  launcher->CreateComponent(std::move(launch_info), controller_.NewRequest());

  services_client_.emplace(std::move(font_provider_services_dir));
}

TestFontsProvider::~TestFontsProvider() = default;

fuchsia::fonts::ProviderHandle TestFontsProvider::GetProvider() {
  fuchsia::fonts::ProviderHandle font_provider;
  services_client_->Connect(font_provider.NewRequest());
  return font_provider;
}

}  // namespace

fuchsia::fonts::ProviderHandle GetTestFontsProvider() {
  static base::NoDestructor<TestFontsProvider> test_fonts_provider;
  return test_fonts_provider->GetProvider();
}

}  // namespace skia
