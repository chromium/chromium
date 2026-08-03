// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_tab_helper.h"

#import <string>

#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/enterprise/data_controls/core/browser/features.h"
#import "components/enterprise/data_controls/core/browser/prefs.h"
#import "components/enterprise/data_controls/core/browser/rule.h"
#import "components/policy/core/common/policy_types.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/enterprise/common/util.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_service_factory.h"
#import "ios/chrome/browser/enterprise/connectors/connectors_util.h"
#import "ios/chrome/browser/enterprise/data_controls/model/data_controls_metrics.h"
#import "ios/chrome/browser/enterprise/data_controls/utils/ios_clipboard_context.h"
#import "ios/chrome/browser/enterprise/enterprise_dialog/model/warning_dialog.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/snackbar/snackbar_message.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/components/enterprise/analysis/features.h"
#import "ios/web/public/web_state.h"
#import "ui/base/clipboard/clipboard_format_type.h"
#import "ui/base/clipboard/clipboard_metadata.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace data_controls {

DataControlsTabHelper::DataControlsTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  scoped_observation_.Observe(DataControlsPasteboardManager::GetInstance());
}

DataControlsTabHelper::~DataControlsTabHelper() = default;

void DataControlsTabHelper::ShouldAllowCopy(
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/444224082): Include size and format type for copy
  // operations.
  ui::ClipboardMetadata metadata;
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  const GURL& source_url = web_state_->GetLastCommittedURL();
  CopyPolicyVerdicts verdicts =
      IsCopyAllowedByPolicy(source_url, metadata, profile);

  std::string domain = GetManagementDomain(profile);
  NSString* snackbar_title =
      domain.empty()
          ? l10n_util::GetNSString(IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION)
          : l10n_util::GetNSStringF(IDS_DATA_CONTROLS_BLOCKED_LABEL_WITH_DOMAIN,
                                    base::UTF8ToUTF16(domain));

  switch (verdicts.copy_action_verdict.level()) {
    case Rule::Level::kWarn:
      ShowWarningDialog(
          enterprise::DialogType::kClipboardCopyWarn,
          GetManagementDomain(profile),
          base::BindOnce(&DataControlsTabHelper::FinishCopy,
                         weak_factory_.GetWeakPtr(), source_url,
                         profile->AsWeakPtr(), metadata, std::move(verdicts),
                         std::move(callback)));
      break;
    case Rule::Level::kBlock:
      ShowRestrictSnackbar(snackbar_title);
      [[fallthrough]];
    case Rule::Level::kReport:
    case Rule::Level::kAllow:
    case Rule::Level::kNotSet:
      FinishCopy(source_url, profile->AsWeakPtr(), metadata,
                 std::move(verdicts), std::move(callback),
                 /*bypassed=*/false);
      break;
  }
}

void DataControlsTabHelper::ShouldAllowPaste(
    base::OnceCallback<void(bool)> callback) {
  // TODO(crbug.com/444224082): Include size and format type for paste
  // operations.
  ui::ClipboardMetadata metadata;
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  const GURL& destination_url = web_state_->GetLastCommittedURL();

  DataControlsPasteboardManager* pasteboard_manager =
      DataControlsPasteboardManager::GetInstance();
  PasteboardSource source =
      pasteboard_manager->GetCurrentPasteboardItemsSource();

  PastePolicyVerdict policy_verdict =
      IsPasteAllowedByPolicy(source.source_url, destination_url, metadata,
                             source.source_profile, profile);
  std::string domain = policy_verdict.dialog_triggered_by_source
                           ? GetManagementDomain(source.source_profile)
                           : GetManagementDomain(profile);
  base::WeakPtr<ProfileIOS> weakSourceProfile =
      source.source_profile ? source.source_profile->AsWeakPtr()
                            : base::WeakPtr<ProfileIOS>{};

  NSString* snackbar_title =
      domain.empty()
          ? l10n_util::GetNSString(IDS_POLICY_ACTION_BLOCKED_BY_ORGANIZATION)
          : l10n_util::GetNSStringF(IDS_DATA_CONTROLS_BLOCKED_LABEL_WITH_DOMAIN,
                                    base::UTF8ToUTF16(domain));

  switch (policy_verdict.verdict.level()) {
    case Rule::Level::kWarn:
      paste_event_state_ = PasteEventState::kDisplayingWarningDialog;
      ShowWarningDialog(
          enterprise::DialogType::kClipboardPasteWarn, domain,
          base::BindOnce(
              &DataControlsTabHelper::PasteIfAllowedByDataControls,
              weak_factory_.GetWeakPtr(), destination_url, source.source_url,
              profile->AsWeakPtr(), weakSourceProfile, metadata,
              std::move(policy_verdict.verdict), std::move(callback)));
      break;
    case Rule::Level::kBlock:
      ShowRestrictSnackbar(snackbar_title);
      [[fallthrough]];
    case Rule::Level::kReport:
    case Rule::Level::kAllow:
    case Rule::Level::kNotSet:
      PasteIfAllowedByDataControls(destination_url, source.source_url,
                                   profile->AsWeakPtr(), weakSourceProfile,
                                   metadata, std::move(policy_verdict.verdict),
                                   std::move(callback),
                                   /*bypassed=*/false);
      break;
  }
}

void DataControlsTabHelper::PasteIfAllowedByDataControls(
    const GURL& destination_url,
    const GURL& source_url,
    base::WeakPtr<ProfileIOS> destination_profile,
    base::WeakPtr<ProfileIOS> source_profile,
    const ui::ClipboardMetadata& metadata,
    Verdict verdict,
    base::OnceCallback<void(bool)> callback,
    bool bypassed) {
  // Record the verdict level to the Paste histogram.
  base::UmaHistogramEnumeration(
      kIOSWebStateDataControlsClipboardPasteVerdictHistogram, verdict.level());

  if (verdict.level() > Rule::Level::kNotSet && destination_profile.get()) {
    MaybeReportDataControlsPaste(
        source_url, destination_url, source_profile.get(),
        destination_profile.get(), metadata, verdict, bypassed);
  }

  // The user may have navigated away from the page into which the paste
  // operation was initiated. If the URL has changed, we should block the paste
  // operation as the original destination is no longer available.
  if (destination_url != web_state_->GetLastCommittedURL()) {
    FinishPaste(std::move(callback), /*verdict_or_scan_success=*/false,
                /*analysis_warn_bypassed=*/false);
    return;
  }

  bool allowed = verdict.level() != Rule::Level::kBlock;
  if (verdict.level() == Rule::Level::kWarn) {
    allowed = bypassed;

    // Record whether user ignores the warning and decides to paste anyway.
    base::UmaHistogramBoolean(
        kIOSWebStateDataControlsClipboardPasteClipboardWarningBypassedHistogram,
        bypassed);
  }

  // Block the paste if it is not allowed or if the destination_profile is
  // destroyed.
  if (!allowed || !destination_profile) {
    FinishPaste(std::move(callback), /*verdict_or_scan_success=*/false,
                /*analysis_warn_bypassed=*/false);
    return;
  }

  // Get the connector service for both source profile and destination profile.
  // Only when neither profile has Bulk Data Entry enabled, we allow the paste.
  // Otherwise, run the Pasted Content Analysis.
  enterprise_connectors::ConnectorsService* source_profile_service = nullptr;
  if (source_profile) {
    source_profile_service =
        enterprise_connectors::ConnectorsServiceFactory::GetForProfile(
            source_profile.get());
  }

  // No need to check destination profile here because paste will be blocked if
  // it is destroyed.
  enterprise_connectors::ConnectorsService* destination_profile_service =
      enterprise_connectors::ConnectorsServiceFactory::GetForProfile(
          destination_profile.get());

  bool source_profile_connector_enabled =
      (source_profile_service &&
       enterprise_connectors::IsBulkDataEntryConnectorEnabled(
           source_profile_service));
  bool destination_profile_connector_enabled =
      (destination_profile_service &&
       enterprise_connectors::IsBulkDataEntryConnectorEnabled(
           destination_profile_service));

  if (!source_profile_connector_enabled &&
      !destination_profile_connector_enabled) {
    FinishPaste(std::move(callback), /*verdict_or_scan_success=*/allowed,
                /*analysis_warn_bypassed=*/false);
    return;
  }

  // Prioritize using the destination profile's enterprise connector if it is
  // enabled.
  base::WeakPtr<ProfileIOS> profile = destination_profile_connector_enabled
                                          ? destination_profile
                                          : source_profile;

  enterprise_connectors::ContentMetaData::CopiedTextSource copy_source =
      IOSClipboardContext::GetCopiedTextSource(source_url, source_profile.get(),
                                               destination_profile.get());

  DataControlsPasteboardManager::GetInstance()->GetPasteboardTextAndImage(
      base::BindOnce(&DataControlsTabHelper::RunPastedContentAnalysis,
                     weak_factory_.GetWeakPtr(), destination_url, profile,
                     std::move(copy_source), std::move(callback)));
}

void DataControlsTabHelper::PasteIfAllowedByContentAnalysis(
    base::OnceCallback<void(bool)> callback,
    enterprise_connectors::RequestHandlerResult result) {
  using enterprise_connectors::RequestHandlerResultActionLevel;
  RequestHandlerResultActionLevel action_level = ResultToActionLevel(result);

  // Always call `FinishPaste` if the paste is allowed because the pasteboard
  // items might need to be restored before pasting.
  switch (action_level) {
    case RequestHandlerResultActionLevel::kNotScan:
    case RequestHandlerResultActionLevel::kAudit:
      FinishPaste(std::move(callback), /*verdict_or_scan_success=*/true,
                  /*analysis_warn_bypassed=*/false);
      break;
    case RequestHandlerResultActionLevel::kWarn:
      paste_event_state_ = PasteEventState::kDisplayingWarningDialog;
      ShowWarningDialog(
          enterprise::DialogType::kPastedContentWarn, std::string(),
          base::BindOnce(&DataControlsTabHelper::FinishPaste,
                         weak_factory_.GetWeakPtr(), std::move(callback),
                         /*verdict_or_scan_success=*/false));
      break;
    case RequestHandlerResultActionLevel::kBlock:
      ShowRestrictSnackbar(l10n_util::GetNSString(
          IDS_ENTERPRISE_CONTENT_ANALYSIS_PASTE_BLOCKED_MESSAGE));
      FinishPaste(std::move(callback), /*verdict_or_scan_success=*/false,
                  /*analysis_warn_bypassed=*/false);
      break;
  }
}

void DataControlsTabHelper::RunPastedContentAnalysis(
    const GURL& destination_url,
    base::WeakPtr<ProfileIOS> profile,
    enterprise_connectors::ContentMetaData::CopiedTextSource copied_source,
    base::OnceCallback<void(bool)> callback,
    std::optional<PasteboardContentDLP> pasteboard_content) {
  // This method should be guarded by `IsBulkDataEntryConnectorEnabled` check in
  // the `PasteIfAllowedByDataControls` method.
  CHECK(base::FeatureList::IsEnabled(
      enterprise_connectors::kEnableBulkDataEntryConnectorIOS));

  // If the `pasteboard_content` does not have a value, the pasteboard
  // text/image size exceeds 100MB and we directly block the paste without
  // scanning.
  if (!pasteboard_content.has_value()) {
    enterprise_connectors::RequestHandlerResult result;
    result.final_result =
        enterprise_connectors::FinalContentAnalysisResult::FAILURE;
    PasteIfAllowedByContentAnalysis(std::move(callback), std::move(result));
    return;
  }

  // Silently block the paste event without showing the snackbar since it is
  // no longer valid if:
  // 1. The profile with the pasted content analysis turned on is destroyed,
  // and we cannot get the connevtor service, we should block the paste to
  // prevent user trying to bypass the rules.
  // 2. The user navigates away from the page while we were trying to get
  // the pasteboard content.
  if (!profile || destination_url != web_state_->GetLastCommittedURL()) {
    FinishPaste(std::move(callback), /*verdict_or_scan_success=*/false,
                /*analysis_warn_bypassed=*/false);
    return;
  }

  // TODO(crbug.com/537767156): Create a PasteboardContentHandlerIOS and use it
  // to upload the content for scanning and pass the result to
  // `PasteIfAllowedByContentAnalysis` as a callback.
  enterprise_connectors::RequestHandlerResult result;
  result.final_result =
      enterprise_connectors::FinalContentAnalysisResult::SUCCESS;
  PasteIfAllowedByContentAnalysis(std::move(callback), std::move(result));
}

void DataControlsTabHelper::ShouldAllowCut(
    base::OnceCallback<void(bool)> callback) {
  // "Cut" is treated the same way as "copy".
  ShouldAllowCopy(std::move(callback));
}

bool DataControlsTabHelper::ShouldAllowShare() {
  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  const GURL& source_url = web_state_->GetLastCommittedURL();

  Verdict verdict = IsShareAllowedByPolicy(source_url, profile);

  return verdict.level() != Rule::Level::kBlock;
}

bool DataControlsTabHelper::IsSearchWithFeatureEnabled() {
  return base::FeatureList::IsEnabled(data_controls::kDataControlsSearchWith);
}

bool DataControlsTabHelper::IsSearchWithAllowed() {
  if (!IsSearchWithFeatureEnabled()) {
    return true;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  const GURL& source_url = web_state_->GetLastCommittedURL();

  Verdict verdict = IsSearchWithAllowedByPolicy(source_url, profile);

  return verdict.level() != Rule::Level::kBlock;
}

void DataControlsTabHelper::ShouldAllowSearchWith(
    size_t text_length,
    base::OnceCallback<void(bool)> callback) {
  if (!IsSearchWithFeatureEnabled()) {
    std::move(callback).Run(true);
    return;
  }

  ProfileIOS* profile =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
  const GURL& source_url = web_state_->GetLastCommittedURL();

  Verdict verdict = IsSearchWithAllowedByPolicy(source_url, profile);

  base::UmaHistogramEnumeration(
      kIOSWebStateDataControlsSearchWithVerdictHistogram, verdict.level());

  ui::ClipboardMetadata metadata{
      .size = text_length * sizeof(std::u16string::value_type),
      .format_type = ui::ClipboardFormatType::PlainTextType()};

  switch (verdict.level()) {
    case Rule::Level::kBlock:
      // The menu item should have been blocked synchronously.
      std::move(callback).Run(false);
      break;
    case Rule::Level::kWarn:
      ShowWarningDialog(
          enterprise::DialogType::kClipboardActionWarn,
          GetManagementDomain(profile),
          base::BindOnce(&DataControlsTabHelper::FinishSearchWith,
                         weak_factory_.GetWeakPtr(), source_url,
                         profile->AsWeakPtr(), metadata, std::move(verdict),
                         std::move(callback)));
      break;
    case Rule::Level::kReport:
      MaybeReportDataControlsCopy(source_url, profile, metadata, verdict,
                                  /*bypassed=*/false);
      [[fallthrough]];
    case Rule::Level::kAllow:
    case Rule::Level::kNotSet:
      std::move(callback).Run(true);
      break;
  }
}

void DataControlsTabHelper::SetEnterpriseCommandsHandler(
    id<EnterpriseCommands> handler) {
  enterprise_handler_ = handler;
}

void DataControlsTabHelper::SetSnackbarHandler(
    id<SnackbarCommands> snackbar_handler) {
  snackbar_handler_ = snackbar_handler;
}
void DataControlsTabHelper::DidFinishClipboardRead() {
  DataControlsPasteboardManager::GetInstance()
      ->RestorePlaceholderToGeneralPasteboardIfNeeded();
}

void DataControlsTabHelper::FinishCopy(const GURL& source_url,
                                       base::WeakPtr<ProfileIOS> source_profile,
                                       const ui::ClipboardMetadata& metadata,
                                       CopyPolicyVerdicts verdicts,
                                       base::OnceCallback<void(bool)> callback,
                                       bool bypassed) {
  if (verdicts.copy_action_verdict.level() > Rule::Level::kNotSet &&
      source_profile.get()) {
    MaybeReportDataControlsCopy(source_url, source_profile.get(), metadata,
                                verdicts.copy_action_verdict, bypassed);
  }

  // The user may have navigated away from the page from which the copy
  // operation was initiated. If the URL has changed, we should block the copy
  // operation as the original content is no longer available.
  if (source_url != web_state_->GetLastCommittedURL()) {
    std::move(callback).Run(false);
    return;
  }

  Verdict verdict = std::move(verdicts.copy_action_verdict);

  // Record the verdict level to the Copy histogram.
  base::UmaHistogramEnumeration(
      kIOSWebStateDataControlsClipboardCopyVerdictHistogram, verdict.level());

  bool allowed = verdict.level() != Rule::Level::kBlock;
  if (verdict.level() == Rule::Level::kWarn) {
    allowed = bypassed;

    // Record whether user ignores the warning and decides to copy anyway.
    base::UmaHistogramBoolean(
        kIOSWebStateDataControlsClipboardCopyClipboardWarningBypassedHistogram,
        bypassed);
  }

  if (allowed) {
    ProfileIOS* profile =
        ProfileIOS::FromBrowserState(web_state_->GetBrowserState());
    auto* pasteboard_manager = DataControlsPasteboardManager::GetInstance();
    pasteboard_manager->SetNextPasteboardItemsSource(
        source_url, profile, verdicts.copy_to_os_clipbord);
  }

  std::move(callback).Run(allowed);
}

void DataControlsTabHelper::FinishSearchWith(
    const GURL& source_url,
    base::WeakPtr<ProfileIOS> source_profile,
    const ui::ClipboardMetadata& metadata,
    Verdict verdict,
    base::OnceCallback<void(bool)> callback,
    bool bypassed) {
  if (source_profile) {
    MaybeReportDataControlsCopy(source_url, source_profile.get(), metadata,
                                std::move(verdict), bypassed);
  }
  std::move(callback).Run(bypassed);
}

void DataControlsTabHelper::FinishShare(const GURL& source_url,
                                        Verdict verdict,
                                        base::OnceCallback<void(bool)> callback,
                                        bool bypassed) {
  // The user may have navigated away from the page from which the share
  // operation was initiated. If the URL has changed, we should block the share
  // operation as the original content is no longer available.
  if (source_url != web_state_->GetLastCommittedURL()) {
    std::move(callback).Run(false);
    return;
  }

  bool allowed = verdict.level() != Rule::Level::kBlock;
  if (verdict.level() == Rule::Level::kWarn) {
    allowed = bypassed;
  }

  std::move(callback).Run(allowed);
}

void DataControlsTabHelper::FinishPaste(base::OnceCallback<void(bool)> callback,
                                        bool verdict_or_scan_success,
                                        bool analysis_warn_bypassed) {
  bool allowed = analysis_warn_bypassed || verdict_or_scan_success;

  if (allowed) {
    DataControlsPasteboardManager::GetInstance()
        ->RestoreItemsToGeneralPasteboardIfNeeded(
            base::BindOnce(std::move(callback), allowed));
  } else {
    std::move(callback).Run(false);
  }
  paste_event_state_ = PasteEventState::kIdle;
}

void DataControlsTabHelper::ShowWarningDialog(
    enterprise::DialogType dialog_type,
    std::string_view org_domain,
    base::OnceCallback<void(bool)> on_bypassed_callback) {
  if (enterprise_handler_) {
    [enterprise_handler_
        showEnterpriseWarningDialog:dialog_type
                 organizationDomain:org_domain
                           callback:std::move(on_bypassed_callback)];
  } else {
    if (on_bypassed_callback) {
      std::move(on_bypassed_callback).Run(false);
    }
  }
}

void DataControlsTabHelper::ShowRestrictSnackbar(NSString* title) {
  SnackbarMessage* message = [[SnackbarMessage alloc] initWithTitle:title];
  [snackbar_handler_ showSnackbarMessageAfterDismissingKeyboard:message];
}

std::string DataControlsTabHelper::GetManagementDomain(ProfileIOS* profile) {
  if (!profile) {
    return std::string();
  }

  policy::PolicyScope scope = static_cast<policy::PolicyScope>(
      profile->GetPrefs()->GetInteger(kDataControlsRulesScopePref));
  return enterprise::GetManagementDomain(
      scope, IdentityManagerFactory::GetForProfile(profile));
}

void DataControlsTabHelper::OnPasteboardContentChanged() {
  switch (paste_event_state_) {
    case PasteEventState::kIdle:
      break;
    case PasteEventState::kDisplayingWarningDialog:
      [enterprise_handler_ dismissEnterpriseWarningDialog];
      paste_event_state_ = PasteEventState::kIdle;
      break;
  }
}

}  // namespace data_controls
