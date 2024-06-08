// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/web_test/browser/web_test_browser_main_platform_support.h"

#include <windows.h>

#include <stddef.h>

#include <iostream>
#include <list>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/dwrite_font_proxy_impl_win.h"
#include "content/shell/common/shell_switches.h"
#include "ui/gfx/win/direct_write.h"

namespace content {

namespace {

void SetupFonts() {
  // Load Ahem font. Ahem.ttf is copied to the build directory by
  // //third_party/test_fonts .
  base::FilePath base_path;
  base::PathService::Get(base::DIR_MODULE, &base_path);
  base::FilePath font_path =
      base_path.Append(FILE_PATH_LITERAL("/test_fonts/Ahem.ttf"));

  DWriteFontProxyImpl::SideLoadFontForTesting(font_path);
}

}  // namespace

bool WebTestBrowserCheckLayoutSystemDeps() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisableSystemFontCheck)) {
    return true;
  }

  std::list<std::string> errors;

  // This metric will be 17 when font size is "Normal".
  // The size of drop-down menus depends on it.
  if (::GetSystemMetrics(SM_CXVSCROLL) != 17)
    errors.push_back("Must use normal size fonts (96 dpi).");

  NONCLIENTMETRICS metrics = {0};
  metrics.cbSize = sizeof(NONCLIENTMETRICS);
  bool success = !!::SystemParametersInfo(SPI_GETNONCLIENTMETRICS,
                                          metrics.cbSize, &metrics, 0);
  PCHECK(success);
  LOGFONTW* system_fonts[] = {&metrics.lfStatusFont, &metrics.lfMenuFont,
                              &metrics.lfSmCaptionFont};
  const wchar_t required_font[] = L"Segoe UI";
  int required_font_size = -12;
  for (size_t i = 0; i < std::size(system_fonts); ++i) {
    if (system_fonts[i]->lfHeight != required_font_size ||
        wcscmp(required_font, system_fonts[i]->lfFaceName)) {
      errors.push_back(
          "Must use either the Aero or Basic theme. Or change display language "
          "to English.");
      break;
    }
  }

  for (const auto& error : errors)
    std::cerr << error << "\n";
  return errors.empty();
}

void WebTestBrowserPlatformInitialize() {
  SetupFonts();
}

}  // namespace content
