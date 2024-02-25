// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/safety_check/model/ios_chrome_safety_check_manager_constants.h"

const std::string NameForSafetyCheckState(
    UpdateChromeSafetyCheckState check_state) {
  switch (check_state) {
    case UpdateChromeSafetyCheckState::kDefault:
      return "UpdateChromeSafetyCheckState::kDefault";
    case UpdateChromeSafetyCheckState::kUpToDate:
      return "UpdateChromeSafetyCheckState::kUpToDate";
    case UpdateChromeSafetyCheckState::kOutOfDate:
      return "UpdateChromeSafetyCheckState::kOutOfDate";
    case UpdateChromeSafetyCheckState::kManaged:
      return "UpdateChromeSafetyCheckState::kManaged";
    case UpdateChromeSafetyCheckState::kRunning:
      return "UpdateChromeSafetyCheckState::kRunning";
    case UpdateChromeSafetyCheckState::kOmahaError:
      return "UpdateChromeSafetyCheckState::kOmahaError";
    case UpdateChromeSafetyCheckState::kNetError:
      return "UpdateChromeSafetyCheckState::kNetError";
    case UpdateChromeSafetyCheckState::kChannel:
      return "UpdateChromeSafetyCheckState::kChannel";
  }
}

const std::string NameForSafetyCheckState(
    PasswordSafetyCheckState check_state) {
  switch (check_state) {
    case PasswordSafetyCheckState::kDefault:
      return "PasswordSafetyCheckState::kDefault";
    case PasswordSafetyCheckState::kSafe:
      return "PasswordSafetyCheckState::kSafe";
    case PasswordSafetyCheckState::kUnmutedCompromisedPasswords:
      return "PasswordSafetyCheckState::kUnmutedCompromisedPasswords";
    case PasswordSafetyCheckState::kReusedPasswords:
      return "PasswordSafetyCheckState::kReusedPasswords";
    case PasswordSafetyCheckState::kWeakPasswords:
      return "PasswordSafetyCheckState::kWeakPasswords";
    case PasswordSafetyCheckState::kDismissedWarnings:
      return "PasswordSafetyCheckState::kDismissedWarnings";
    case PasswordSafetyCheckState::kRunning:
      return "PasswordSafetyCheckState::kRunning";
    case PasswordSafetyCheckState::kDisabled:
      return "PasswordSafetyCheckState::kDisabled";
    case PasswordSafetyCheckState::kError:
      return "PasswordSafetyCheckState::kError";
    case PasswordSafetyCheckState::kSignedOut:
      return "PasswordSafetyCheckState::kSignedOut";
  }
}

const std::string NameForSafetyCheckState(
    SafeBrowsingSafetyCheckState check_state) {
  switch (check_state) {
    case SafeBrowsingSafetyCheckState::kDefault:
      return "SafeBrowsingSafetyCheckState::kDefault";
    case SafeBrowsingSafetyCheckState::kManaged:
      return "SafeBrowsingSafetyCheckState::kManaged";
    case SafeBrowsingSafetyCheckState::kRunning:
      return "SafeBrowsingSafetyCheckState::kRunning";
    case SafeBrowsingSafetyCheckState::kSafe:
      return "SafeBrowsingSafetyCheckState::kSafe";
    case SafeBrowsingSafetyCheckState::kUnsafe:
      return "SafeBrowsingSafetyCheckState::kUnsafe";
  }
}

// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
std::optional<UpdateChromeSafetyCheckState> UpdateChromeSafetyCheckStateForName(
    const std::string& check_state) {
  if (check_state == "UpdateChromeSafetyCheckState::kDefault") {
    return UpdateChromeSafetyCheckState::kDefault;
  }

  if (check_state == "UpdateChromeSafetyCheckState::kUpToDate") {
    return UpdateChromeSafetyCheckState::kUpToDate;
  }

  if (check_state == "UpdateChromeSafetyCheckState::kOutOfDate") {
    return UpdateChromeSafetyCheckState::kOutOfDate;
  }

  if (check_state == "UpdateChromeSafetyCheckState::kManaged") {
    return UpdateChromeSafetyCheckState::kManaged;
  }

  if (check_state == "UpdateChromeSafetyCheckState::kRunning") {
    return UpdateChromeSafetyCheckState::kRunning;
  }

  if (check_state == "UpdateChromeSafetyCheckState::kOmahaError") {
    return UpdateChromeSafetyCheckState::kOmahaError;
  }

  if (check_state == "UpdateChromeSafetyCheckState::kNetError") {
    return UpdateChromeSafetyCheckState::kNetError;
  }

  if (check_state == "UpdateChromeSafetyCheckState::kChannel") {
    return UpdateChromeSafetyCheckState::kChannel;
  }

  return std::nullopt;
}

// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
std::optional<PasswordSafetyCheckState> PasswordSafetyCheckStateForName(
    const std::string& check_state) {
  if (check_state == "PasswordSafetyCheckState::kDefault") {
    return PasswordSafetyCheckState::kDefault;
  }

  if (check_state == "PasswordSafetyCheckState::kSafe") {
    return PasswordSafetyCheckState::kSafe;
  }

  if (check_state == "PasswordSafetyCheckState::kUnmutedCompromisedPasswords") {
    return PasswordSafetyCheckState::kUnmutedCompromisedPasswords;
  }

  if (check_state == "PasswordSafetyCheckState::kReusedPasswords") {
    return PasswordSafetyCheckState::kReusedPasswords;
  }

  if (check_state == "PasswordSafetyCheckState::kWeakPasswords") {
    return PasswordSafetyCheckState::kWeakPasswords;
  }

  if (check_state == "PasswordSafetyCheckState::kDismissedWarnings") {
    return PasswordSafetyCheckState::kDismissedWarnings;
  }

  if (check_state == "PasswordSafetyCheckState::kRunning") {
    return PasswordSafetyCheckState::kRunning;
  }

  if (check_state == "PasswordSafetyCheckState::kDisabled") {
    return PasswordSafetyCheckState::kDisabled;
  }

  if (check_state == "PasswordSafetyCheckState::kError") {
    return PasswordSafetyCheckState::kError;
  }

  if (check_state == "PasswordSafetyCheckState::kSignedOut") {
    return PasswordSafetyCheckState::kSignedOut;
  }

  return std::nullopt;
}

// WARNING - PLEASE READ: Sadly, we cannot switch over strings in C++, so be
// very careful when updating this method to ensure all enums are accounted for.
std::optional<SafeBrowsingSafetyCheckState> SafeBrowsingSafetyCheckStateForName(
    const std::string& check_state) {
  if (check_state == "SafeBrowsingSafetyCheckState::kDefault") {
    return SafeBrowsingSafetyCheckState::kDefault;
  }

  if (check_state == "SafeBrowsingSafetyCheckState::kManaged") {
    return SafeBrowsingSafetyCheckState::kManaged;
  }

  if (check_state == "SafeBrowsingSafetyCheckState::kRunning") {
    return SafeBrowsingSafetyCheckState::kRunning;
  }

  if (check_state == "SafeBrowsingSafetyCheckState::kSafe") {
    return SafeBrowsingSafetyCheckState::kSafe;
  }

  if (check_state == "SafeBrowsingSafetyCheckState::kUnsafe") {
    return SafeBrowsingSafetyCheckState::kUnsafe;
  }

  return std::nullopt;
}
