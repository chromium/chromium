// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/crash_report/breadcrumbs/application_breadcrumbs_logger.h"

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/application_breadcrumbs_not_user_action.inc"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_manager.h"
#import "ios/chrome/browser/crash_report/crash_report_helper.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kBreadcrumbOrientation[] = "Orientation";

ApplicationBreadcrumbsLogger::ApplicationBreadcrumbsLogger(
    breadcrumbs::BreadcrumbManager* breadcrumb_manager)
    : breadcrumb_manager_(breadcrumb_manager),
      user_action_callback_(
          base::BindRepeating(&ApplicationBreadcrumbsLogger::OnUserAction,
                              base::Unretained(this))),
      memory_pressure_listener_(std::make_unique<base::MemoryPressureListener>(
          FROM_HERE,
          base::BindRepeating(&ApplicationBreadcrumbsLogger::OnMemoryPressure,
                              base::Unretained(this)))) {
  base::AddActionCallback(user_action_callback_);

  breakpad::MonitorBreadcrumbManager(breadcrumb_manager_);
  breadcrumb_manager_->AddEvent("Startup");

  orientation_observer_ = [NSNotificationCenter.defaultCenter
      addObserverForName:UIDeviceOrientationDidChangeNotification
                  object:nil
                   queue:nil
              usingBlock:^(NSNotification*) {
                if (UIDevice.currentDevice.orientation == last_orientation_)
                  return;
                last_orientation_ = UIDevice.currentDevice.orientation;

                std::string event(kBreadcrumbOrientation);
                switch (UIDevice.currentDevice.orientation) {
                  case UIDeviceOrientationUnknown:
                    event += " #unknown";
                    break;
                  case UIDeviceOrientationPortrait:
                    event += " #portrait";
                    break;
                  case UIDeviceOrientationPortraitUpsideDown:
                    event += " #portrait-upside-down";
                    break;
                  case UIDeviceOrientationLandscapeLeft:
                    event += " #landscape-left";
                    break;
                  case UIDeviceOrientationLandscapeRight:
                    event += " #landscape-right";
                    break;
                  case UIDeviceOrientationFaceUp:
                    event += " #face-up";
                    break;
                  case UIDeviceOrientationFaceDown:
                    event += " #face-down";
                    break;
                }
                breadcrumb_manager_->AddEvent(event);
              }];
}

ApplicationBreadcrumbsLogger::~ApplicationBreadcrumbsLogger() {
  [NSNotificationCenter.defaultCenter removeObserver:orientation_observer_];
  breadcrumb_manager_->AddEvent("Shutdown");
  base::RemoveActionCallback(user_action_callback_);
  breakpad::StopMonitoringBreadcrumbManager(breadcrumb_manager_);
  if (persistent_storage_manager_) {
    persistent_storage_manager_->StopMonitoringBreadcrumbManager(
        breadcrumb_manager_);
  }
}

void ApplicationBreadcrumbsLogger::SetPersistentStorageManager(
    std::unique_ptr<BreadcrumbPersistentStorageManager>
        persistent_storage_manager) {
  if (persistent_storage_manager_) {
    persistent_storage_manager_->StopMonitoringBreadcrumbManager(
        breadcrumb_manager_);
  }

  persistent_storage_manager_ = std::move(persistent_storage_manager);
  persistent_storage_manager_->MonitorBreadcrumbManager(breadcrumb_manager_);
}

BreadcrumbPersistentStorageManager*
ApplicationBreadcrumbsLogger::GetPersistentStorageManager() const {
  return persistent_storage_manager_.get();
}

void ApplicationBreadcrumbsLogger::OnUserAction(const std::string& action,
                                                base::TimeTicks action_time) {
  // Filter out unwanted actions.
  if (action.find("InProductHelp.") == 0) {
    // InProductHelp actions are very noisy.
    return;
  }

  if (!IsUserTriggeredAction(action)) {
    // These actions are not useful for breadcrumbs.
    return;
  }

  breadcrumb_manager_->AddEvent(action.c_str());
}

void ApplicationBreadcrumbsLogger::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  std::string pressure_string = "";
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      pressure_string = "None";
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      pressure_string = "Moderate";
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      pressure_string = "Critical";
      break;
  }

  std::string event =
      base::StringPrintf("Memory Pressure: %s", pressure_string.c_str());
  breadcrumb_manager_->AddEvent(event);
}

bool ApplicationBreadcrumbsLogger::IsUserTriggeredAction(
    const std::string& action) {
  // The variable kNotUserTriggeredActions is a sorted array of
  // strings generated by generate_not_user_triggered_actions.py.
  // It is defined in application_breadcrumbs_not_user_action.inc.
  return !std::binary_search(std::begin(kNotUserTriggeredActions),
                             std::end(kNotUserTriggeredActions), action);
}
