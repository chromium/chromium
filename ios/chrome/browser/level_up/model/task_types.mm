// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/model/task_types.h"

std::string TaskTypeToString(TaskType type) {
  switch (type) {
    case TaskType::kUnknown:
      return "Unknown";
    case TaskType::kTabGroups:
      return "TabGroups";
    case TaskType::kAutofill:
      return "Autofill";
    case TaskType::kPinTabs:
      return "PinTabs";
    case TaskType::kGemini:
      return "Gemini";
    case TaskType::kPaymentMethods:
      return "PaymentMethods";
    case TaskType::kQuickDelete:
      return "QuickDelete";
    case TaskType::kSafeBrowsing:
      return "SafeBrowsing";
    case TaskType::kIncognito:
      return "Incognito";
    case TaskType::kPasswordCheckup:
      return "PasswordCheckup";
    case TaskType::kLensSearch:
      return "LensSearch";
    case TaskType::kAISearch:
      return "AISearch";
    case TaskType::kCameraSearch:
      return "CameraSearch";
  }
}
