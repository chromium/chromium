// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_flags_internal.h"

#import <memory>
#import <optional>

#import "base/base_switches.h"
#import "base/command_line.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "base/values.h"
#import "components/autofill/core/common/autofill_switches.h"
#import "components/flags_ui/feature_entry.h"
#import "components/flags_ui/feature_entry_macros.h"
#import "components/flags_ui/flags_state.h"
#import "components/flags_ui/flags_storage.h"
#import "components/flags_ui/flags_ui_switches.h"
#import "components/flags_ui/pref_service_flags_storage.h"
#import "components/prefs/pref_service.h"
#import "components/sync/base/command_line_switches.h"
#import "ios/web_view/internal/app/application_context.h"

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
         syncer::kSyncServiceURL,
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

  base::Value::List supportedFeatures;
  base::Value::List unsupportedFeatures;

  _flagsState->GetFlagFeatureEntries(
      _flagsStorage.get(), flags_ui::kGeneralAccessFlagsOnly, supportedFeatures,
      unsupportedFeatures,
      base::BindRepeating(&ios_web_view::SkipConditionalFeatureEntry));

  for (const base::Value& supportedFeature : supportedFeatures) {
    const base::Value::Dict* supportedFeatureDict =
        supportedFeature.GetIfDict();
    DCHECK(supportedFeatureDict);

    const std::string* featureName =
        supportedFeatureDict->FindString("internal_name");
    DCHECK(featureName);

    if (*featureName == ios_web_view::kUseSyncSandboxFlagName) {
      std::optional<bool> maybeEnabled =
          supportedFeatureDict->FindBool("enabled");
      DCHECK(maybeEnabled.has_value());
      usesSyncSandbox = *maybeEnabled;
    } else if (*featureName == ios_web_view::kUseWalletSandboxFlagName) {
      const base::Value::List* options =
          supportedFeatureDict->FindList("options");
      DCHECK(options);

      for (const base::Value& option : *options) {
        const base::Value::Dict* optionDict = option.GetIfDict();
        DCHECK(optionDict);

        const std::string* optionName = optionDict->FindString("internal_name");
        DCHECK(optionName);

        if (*optionName == ios_web_view::kUseWalletSandboxFlagNameEnabled) {
          std::optional<bool> maybeSelected = optionDict->FindBool("selected");
          DCHECK(maybeSelected.has_value());
          usesWalletSandbox = *maybeSelected;
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
