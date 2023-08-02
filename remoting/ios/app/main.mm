// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "remoting/ios/app/app_delegate.h"

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "remoting/client/in_memory_log_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

int main(int argc, char* argv[]) {
  // This class is designed to fulfill the dependents needs when it goes out of
  // scope and gets destructed.
  base::AtExitManager exitManager;

  // Publicize the CommandLine.
  base::CommandLine::Init(argc, argv);

  // Required to find the ICU data file, used by some file_util routines.
  base::i18n::InitializeICU();

  remoting::InMemoryLogHandler::Register();

#ifdef DEBUG
  // Set min log level for debug builds.  For some reason this has to be
  // negative.
  logging::SetMinLogLevel(-1);
#endif

  l10n_util::OverrideLocaleWithCocoaLocale();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "" /* Overridden by cocal locale */, NULL,
      ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);

  @autoreleasepool {
    return UIApplicationMain(
        argc, argv, nil, NSStringFromClass([AppDelegate class]));
  }
}
