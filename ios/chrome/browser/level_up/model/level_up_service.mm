// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/level_up_service.h"

#import "base/functional/bind.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "base/values.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/scoped_user_pref_update.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/buildflags.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"

LevelUpService::LevelUpService(PrefService* pref_service)
    : pref_service_(pref_service) {
  if (!IsLevelUpEnabled()) {
    return;
  }
  PopulateTasks();
  LoadPrefs();
}

LevelUpService::~LevelUpService() = default;

bool LevelUpService::IsUIEnabled() const {
  return is_ui_enabled_;
}

void LevelUpService::SetUIEnabled(bool ui_enabled) {
  if (is_ui_enabled_ == ui_enabled) {
    return;
  }
  is_ui_enabled_ = ui_enabled;
  pref_service_->SetBoolean(prefs::kLevelUpUIEnabled, is_ui_enabled_);
}

int LevelUpService::GetCurrentLevel() const {
  return current_level_;
}

void LevelUpService::MarkTaskCompleted(TaskType task_type) {
  std::string storage_id = TaskTypeToString(task_type);
  if (storage_id == TaskTypeToString(TaskType::kUnknown)) {
    return;
  }

  if (completed_tasks_.insert(storage_id).second) {
    // Update prefs.
    ScopedListPrefUpdate update(pref_service_, prefs::kLevelUpCompletedTasks);
    update->Append(storage_id);

    // Recalculate level.
    int new_level = 0;
    bool all_tasks_completed = true;
    for (const auto& [type, info] : tasks_) {
      if (!completed_tasks_.contains(TaskTypeToString(type))) {
        all_tasks_completed = false;
        break;
      }
    }
    if (all_tasks_completed) {
      new_level = 1;
    }

    if (new_level > current_level_) {
      current_level_ = new_level;
      pref_service_->SetInteger(prefs::kLevelUpHighestLevel, current_level_);
    }
  }
}

bool LevelUpService::IsTaskCompleted(TaskType task_type) const {
  std::string storage_id = TaskTypeToString(task_type);
  return completed_tasks_.contains(storage_id);
}

const TaskInfo* LevelUpService::GetTaskInfo(TaskType task_type) const {
  auto it = tasks_.find(task_type);
  if (it != tasks_.end()) {
    return &it->second;
  }
  return nullptr;
}

const std::map<TaskType, TaskInfo>& LevelUpService::GetTasks() const {
  return tasks_;
}

void LevelUpService::PopulateTasks() {
  auto no_op_nav = base::BindRepeating([]() {
    // TODO(crbug.com/513245990): Implement navigation in a separate CL.
  });

#if BUILDFLAG(IOS_USE_BRANDED_ASSETS)
  NSString* geminiIcon = kGeminiBrandedLogoSymbol;
  bool geminiIsCustom = true;
#else
  NSString* geminiIcon = kGeminiNonBrandedLogoSymbol;
  bool geminiIsCustom = false;
#endif

  tasks_.insert(std::make_pair(
      TaskType::kTabGroups,
      TaskInfo(TaskType::kTabGroups, /*title_id=*/0, /*task_description_id=*/0,
               base::SysNSStringToUTF8(kTabsSymbol),
               /*is_custom_symbol=*/false, LevelUpTaskCategory::kProductivity,
               /*trigger_action=*/"MobileTabGroupUserCreatedNewGroup",
               no_op_nav)));

  tasks_.insert(std::make_pair(
      TaskType::kAutofill,
      TaskInfo(TaskType::kAutofill, /*title_id=*/0,
               /*task_description_id=*/0,
               base::SysNSStringToUTF8(kPasswordManagerSymbol),
               /*is_custom_symbol=*/true, LevelUpTaskCategory::kProductivity,
               /*trigger_action=*/"", no_op_nav)));

  tasks_.insert(std::make_pair(
      TaskType::kPinTabs,
      TaskInfo(TaskType::kPinTabs, /*title_id=*/0, /*task_description_id=*/0,
               base::SysNSStringToUTF8(kPinSymbol),
               /*is_custom_symbol=*/false, LevelUpTaskCategory::kProductivity,
               /*trigger_action=*/"MobileTabPinned", no_op_nav)));

  tasks_.insert(std::make_pair(
      TaskType::kGemini,
      TaskInfo(TaskType::kGemini, /*title_id=*/0, /*task_description_id=*/0,
               base::SysNSStringToUTF8(geminiIcon),
               /*is_custom_symbol=*/geminiIsCustom,
               LevelUpTaskCategory::kProductivity,
               /*trigger_action=*/"", no_op_nav)));

  tasks_.insert(std::make_pair(
      TaskType::kPaymentMethods,
      TaskInfo(TaskType::kPaymentMethods, /*title_id=*/0,
               /*task_description_id=*/0,
               base::SysNSStringToUTF8(kCreditCardSymbol),
               /*is_custom_symbol=*/false, LevelUpTaskCategory::kProductivity,
               /*trigger_action=*/"AutofillCreditCardsViewed", no_op_nav)));

  tasks_.insert(std::make_pair(
      TaskType::kQuickDelete,
      TaskInfo(TaskType::kQuickDelete, /*title_id=*/0,
               /*task_description_id=*/0, base::SysNSStringToUTF8(kTrashSymbol),
               /*is_custom_symbol=*/false, LevelUpTaskCategory::kSafety,
               /*trigger_action=*/"", no_op_nav)));

  tasks_.insert(std::make_pair(
      TaskType::kSafeBrowsing,
      TaskInfo(TaskType::kSafeBrowsing, /*title_id=*/0,
               /*task_description_id=*/0,
               base::SysNSStringToUTF8(kShieldSymbol),
               /*is_custom_symbol=*/false, LevelUpTaskCategory::kSafety,
               /*trigger_action=*/"MobilePrivacySafeBrowsingSettingsClose",
               no_op_nav)));

  tasks_.insert(std::make_pair(
      TaskType::kIncognito,
      TaskInfo(TaskType::kIncognito, /*title_id=*/0, /*task_description_id=*/0,
               base::SysNSStringToUTF8(kIncognitoSymbol),
               /*is_custom_symbol=*/true, LevelUpTaskCategory::kSafety,
               /*trigger_action=*/"", no_op_nav)));

  tasks_.insert(std::make_pair(
      TaskType::kPasswordCheckup,
      TaskInfo(TaskType::kPasswordCheckup, /*title_id=*/0,
               /*task_description_id=*/0,
               base::SysNSStringToUTF8(kPasswordManagerSymbol),
               /*is_custom_symbol=*/true, LevelUpTaskCategory::kSafety,
               /*trigger_action=*/"MobilePasswordCheckupSettingsClose",
               no_op_nav)));

  tasks_.insert(std::make_pair(
      TaskType::kLensSearch,
      TaskInfo(TaskType::kLensSearch, /*title_id=*/0, /*task_description_id=*/0,
               base::SysNSStringToUTF8(kCameraLensSymbol),
               /*is_custom_symbol=*/true, LevelUpTaskCategory::kSearch,
               /*trigger_action=*/"", no_op_nav)));

  tasks_.insert(std::make_pair(
      TaskType::kAISearch,
      TaskInfo(TaskType::kAISearch, /*title_id=*/0, /*task_description_id=*/0,
               base::SysNSStringToUTF8(kMagnifyingglassSparkSymbol),
               /*is_custom_symbol=*/true, LevelUpTaskCategory::kSearch,
               /*trigger_action=*/"MobileNTPMIAEntryPointTapped", no_op_nav)));

  tasks_.insert(std::make_pair(
      TaskType::kCameraSearch,
      TaskInfo(TaskType::kCameraSearch, /*title_id=*/0,
               /*task_description_id=*/0,
               base::SysNSStringToUTF8(kCameraSymbol),
               /*is_custom_symbol=*/true, LevelUpTaskCategory::kSearch,
               /*trigger_action=*/"", no_op_nav)));
}

void LevelUpService::LoadPrefs() {
  is_ui_enabled_ = pref_service_->GetBoolean(prefs::kLevelUpUIEnabled);
  current_level_ = pref_service_->GetInteger(prefs::kLevelUpHighestLevel);

  const base::ListValue& list =
      pref_service_->GetList(prefs::kLevelUpCompletedTasks);
  for (const auto& value : list) {
    if (value.is_string()) {
      completed_tasks_.insert(value.GetString());
    }
  }
}

void LevelUpService::Shutdown() {
  // TODO(crbug.com/513246860): Implement if needed.
}

// static
void LevelUpService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kLevelUpCompletedTasks,
                             user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kLevelUpHighestLevel, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kLevelUpUIEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}
