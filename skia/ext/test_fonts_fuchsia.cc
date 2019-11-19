// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/test_fonts_fuchsia.h"

#include <fuchsia/fonts/cpp/fidl.h>
#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/interface_handle.h>
#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/default_context.h"
#include "base/fuchsia/file_utils.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "skia/ext/fontmgr_default.h"
#include "skia/ext/test_fonts.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/ports/SkFontMgr_fuchsia.h"

namespace skia {

fuchsia::fonts::ProviderSyncPtr RunTestProviderWithTestFonts(
    fidl::InterfaceHandle<fuchsia::sys::ComponentController>* controller_out) {
  // Start a fuchsia.fonts.Provider instance and configure it to load the test
  // fonts, which must be bundled in the calling process' package.
  fuchsia::sys::LaunchInfo launch_info;
  launch_info.url = "fuchsia-pkg://fuchsia.com/fonts#meta/fonts.cmx";
  launch_info.arguments.emplace(
      {"--no-default-fonts",
       "--font-manifest=/test_fonts/fuchsia_test_fonts_manifest.json"});
  launch_info.flat_namespace = fuchsia::sys::FlatNamespace::New();
  launch_info.flat_namespace->paths.push_back("/test_fonts");

  base::FilePath assets_path;
  if (!base::PathService::Get(base::DIR_ASSETS, &assets_path))
    LOG(FATAL) << "Can't get DIR_ASSETS";
  launch_info.flat_namespace->directories.push_back(
      base::fuchsia::OpenDirectory(assets_path.AppendASCII("test_fonts"))
          .TakeChannel());

  fidl::InterfaceHandle<fuchsia::io::Directory> font_provider_services_dir;
  launch_info.directory_request =
      font_provider_services_dir.NewRequest().TakeChannel();

  fuchsia::sys::LauncherSyncPtr launcher;
  base::fuchsia::ComponentContextForCurrentProcess()->svc()->Connect(
      launcher.NewRequest());
  launcher->CreateComponent(std::move(launch_info),
                            controller_out->NewRequest());

  sys::ServiceDirectory font_provider_services_client(
      std::move(font_provider_services_dir));

  fuchsia::fonts::ProviderSyncPtr provider;
  font_provider_services_client.Connect(provider.NewRequest());
  return provider;
}

void ConfigureTestFont() {
  // ComponentController for the font provider service started below. It's a
  // static field to keep the service running until the test process is
  // destroyed.
  static base::NoDestructor<
      fidl::InterfaceHandle<fuchsia::sys::ComponentController>>
      test_font_provider_controller;
  DCHECK(!*test_font_provider_controller);

  skia::OverrideDefaultSkFontMgr(SkFontMgr_New_Fuchsia(
      RunTestProviderWithTestFonts(test_font_provider_controller.get())));
}

}  // namespace skia
