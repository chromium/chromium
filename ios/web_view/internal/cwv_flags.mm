// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_flags_internal.h"

#include <memory>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_state.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "ios/web_view/internal/app/application_context.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

// Never skip an entry when getting feature entries.
bool SkipConditionalFeatureEntry(const flags_ui::FeatureEntry& entry) {
  return false;
}

const char kUseSyncSandboxFlagName[] = "use-sync-sandbox";
const char kUseWalletSandboxFlagName[] = "use-wallet-sandbox";
const char kUseWalletSandboxFlagNameEnabled[] = "use-wallet-sandbox@1";
const char kUseWalletSandboxFlagNameDisabled[] = "use-wallet-sandbox@2";

// |visible_name| and |visible_description| are not defined because
// //ios/web_view exposes these flags view CWVFlags public API instead of
// through an about:flags page.
const flags_ui::FeatureEntry kFeatureEntries[] = {
    // Controls if sync connects to the sandbox server instead of production.
    {kUseSyncSandboxFlagName, /*visible_name=*/"", /*visible_description=*/"",
     flags_ui::kOsIos,
     SINGLE_VALUE_TYPE_AND_VALUE(
         switches::kSyncServiceURL,
         "https://chrome-sync.sandbox.google.com/chrome-sync/alpha")},
    // Controls if wallet connects to the sandbox server instead of production.
    {kUseWalletSandboxFlagName, /*visible_name=*/"", /*visible_description=*/"",
     flags_ui::kOsIos,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         autofill::switches::kWalletServiceUseSandbox,
         "1",
         autofill::switches::kWalletServiceUseSandbox,
         "0")},
};

}  // namespace ios_web_view

@implementation CWVFlags {
  PrefService* _prefService;
  std::unique_ptr<flags_ui::PrefServiceFlagsStorage> _flagsStorage;
  std::unique_ptr<flags_ui::FlagsState> _flagsState;
}

+ (instancetype)sharedInstance {
  static CWVFlags* flags;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    flags = [[CWVFlags alloc]
        initWithPrefService:ios_web_view::ApplicationContext::GetInstance()
                                ->GetLocalState()];
  });
  return flags;
}

- (instancetype)initWithPrefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _prefService = prefService;
    _flagsStorage =
        std::make_unique<flags_ui::PrefServiceFlagsStorage>(_prefService);
    _flagsState = std::make_unique<flags_ui::FlagsState>(
        ios_web_view::kFeatureEntries, nullptr);
  }
  return self;
}

#pragma mark - Public

- (void)setUsesSyncAndWalletSandbox:(BOOL)usesSyncAndWalletSandbox {
  _flagsState->SetFeatureEntryEnabled(_flagsStorage.get(),
                                      ios_web_view::kUseSyncSandboxFlagName,
                                      usesSyncAndWalletSandbox);

  _flagsState->SetFeatureEntryEnabled(
      _flagsStorage.get(),
      usesSyncAndWalletSandbox
          ? ios_web_view::kUseWalletSandboxFlagNameEnabled
          : ios_web_view::kUseWalletSandboxFlagNameDisabled,
      true);

  _flagsStorage->CommitPendingWrites();
}

- (BOOL)usesSyncAndWalletSandbox {
  BOOL usesSyncSandbox = NO;
  BOOL usesWalletSandbox = NO;

  base::ListValue supportedFeatures;
  base::ListValue unsupportedFeatures;

  _flagsState->GetFlagFeatureEntries(
      _flagsStorage.get(), flags_ui::kGeneralAccessFlagsOnly,
      &supportedFeatures, &unsupportedFeatures,
      base::BindRepeating(&ios_web_view::SkipConditionalFeatureEntry));
  for (size_t i = 0; i < supportedFeatures.GetSize(); i++) {
    base::DictionaryValue* featureEntry;
    if (!supportedFeatures.GetDictionary(i, &featureEntry)) {
      NOTREACHED();
    }
    std::string internalName;
    if (!featureEntry->GetString("internal_name", &internalName)) {
      NOTREACHED();
    }
    if (internalName == ios_web_view::kUseSyncSandboxFlagName) {
      bool enabled;
      if (!featureEntry->GetBoolean("enabled", &enabled)) {
        NOTREACHED();
      }
      usesSyncSandbox = enabled;
    } else if (internalName == ios_web_view::kUseWalletSandboxFlagName) {
      base::ListValue* options;
      if (!featureEntry->GetList("options", &options)) {
        NOTREACHED();
      }
      for (size_t j = 0; j < options->GetSize(); j++) {
        base::DictionaryValue* option;
        if (!options->GetDictionary(j, &option)) {
          NOTREACHED();
        }
        std::string internalName;
        if (!option->GetString("internal_name", &internalName)) {
          NOTREACHED();
        }
        if (internalName == ios_web_view::kUseWalletSandboxFlagNameEnabled) {
          bool selected;
          if (!option->GetBoolean("selected", &selected)) {
            NOTREACHED();
          }
          usesWalletSandbox = selected;
        }
      }
    }
  }
  return usesSyncSandbox && usesWalletSandbox;
}

#pragma mark - Internal

- (void)convertFlagsToCommandLineSwitches {
  base::CommandLine* commandLine = base::CommandLine::ForCurrentProcess();
  _flagsState->ConvertFlagsToSwitches(
      _flagsStorage.get(), commandLine, flags_ui::kAddSentinels,
      switches::kEnableFeatures, switches::kDisableFeatures);
}

@end
