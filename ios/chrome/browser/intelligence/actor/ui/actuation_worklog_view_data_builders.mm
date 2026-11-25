// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data_builders.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/ui/intelligence_header_item.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Point size of the SF symbols in chips and labeled items.
constexpr CGFloat kIconSize = 16.0;

// Point size of the header button symbols.
constexpr CGFloat kHeaderButtonPointSize = 14.0;

// Accessibility label of the header pause button.
// TODO(crbug.com/573515833): Localize the string.
NSString* const kPauseButtonTitle = @"Pause";

// Returns the header button pausing the task.
IntelligenceHeaderItem* PauseHeaderItem() {
  // The mocks use the stop symbol for the pause button.
  return [IntelligenceHeaderItem
           itemWithIdentifier:kActuationHeaderPauseItemIdentifier
                         icon:SymbolWithPointSize(SymbolStopFill,
                                                  kHeaderButtonPointSize)
                        title:kPauseButtonTitle
      accessibilityIdentifier:
          kActuationHeaderPauseButtonAccessibilityIdentifier];
}

// Returns the primary header button for `task_state`: a pausable task shows
// pause, and any other task shows stop.
IntelligenceHeaderItem* PrimaryHeaderItem(actor::ActorTaskState task_state) {
  switch (task_state) {
    case actor::ActorTaskState::kInit:
    case actor::ActorTaskState::kActing:
    case actor::ActorTaskState::kReflecting:
    case actor::ActorTaskState::kWaitingOnUser:
      return PauseHeaderItem();
    case actor::ActorTaskState::kPausedByActor:
    case actor::ActorTaskState::kPausedByUser:
    case actor::ActorTaskState::kCancelled:
    case actor::ActorTaskState::kFinished:
    case actor::ActorTaskState::kFailed:
      return ActuationHeaderCloseItem();
  }
}

// Returns header content with a status message above `task_title`.
ActuationHeaderContent* ContentWithStatus(
    int message_id,
    NSString* task_title,
    IntelligenceHeaderItem* primary_item) {
  return [[ActuationHeaderContent alloc]
      initWithTitle:l10n_util::GetNSString(message_id)
           subtitle:task_title
        primaryItem:primary_item];
}

// Returns the header content for the minimized display mode.
ActuationHeaderContent* MinimizedHeaderContent(
    NSString* task_title,
    actor::ActorTaskState task_state,
    NSString* current_step,
    IntelligenceHeaderItem* primary_item) {
  switch (task_state) {
    case actor::ActorTaskState::kInit:
    case actor::ActorTaskState::kActing:
    case actor::ActorTaskState::kReflecting:
    case actor::ActorTaskState::kPausedByActor:
    case actor::ActorTaskState::kPausedByUser:
      // TODO(crbug.com/571141885): Show a placeholder subtitle for the `kInit`
      // state, since at that point we don't have a task update yet.
      return [[ActuationHeaderContent alloc] initWithTitle:task_title
                                                  subtitle:current_step
                                               primaryItem:primary_item];
    case actor::ActorTaskState::kWaitingOnUser:
      return ContentWithStatus(IDS_IOS_ACTOR_HEADER_INPUT_NEEDED, task_title,
                               primary_item);
    case actor::ActorTaskState::kFinished:
      return ContentWithStatus(IDS_IOS_ACTOR_HEADER_TASK_DONE, task_title,
                               primary_item);
    // `kCancelled` is meant to only represent user cancellations. Other stop
    // reasons are expected to map to `kFinished` or `kFailed` once
    // crbug.com/532978481 is fixed.
    case actor::ActorTaskState::kCancelled:
      return ContentWithStatus(IDS_IOS_ACTOR_HEADER_TASK_STOPPED, task_title,
                               primary_item);
    case actor::ActorTaskState::kFailed:
      return ContentWithStatus(IDS_IOS_ACTOR_HEADER_TASK_FAILED, task_title,
                               primary_item);
  }
}

}  // namespace

ActuationWorklogChip* ActuationWorklogChipForToolType(
    std::optional<actor::ToolType> tool_type) {
  if (!tool_type) {
    return nil;
  }
  switch (*tool_type) {
    case actor::ToolType::kClick:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_CLICKING)
                  icon:SymbolWithPointSize(SymbolCursorArrowRays, kIconSize)];
    case actor::ToolType::kType:
    case actor::ToolType::kAttemptFormFilling:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(IDS_IOS_ACTOR_WORKLOG_CHIP_TYPING)
                  icon:SymbolWithPointSize(SymbolKeyboard, kIconSize)];
    case actor::ToolType::kScroll:
    case actor::ToolType::kScrollTo:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_SCROLLING)
                  icon:SymbolWithPointSize(SymbolCursorArrowMotionLines,
                                           kIconSize)];
    case actor::ToolType::kAttemptLogin:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_FILLING_PASSWORD)
                  icon:SymbolWithPointSize(SymbolKey, kIconSize)];
    case actor::ToolType::kNavigate:
    case actor::ToolType::kBack:
    case actor::ToolType::kForward:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_NAVIGATING)
                  icon:SymbolWithPointSize(SymbolGlobe, kIconSize)];
    case actor::ToolType::kWait:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_WAITING)
                  icon:SymbolWithPointSize(SymbolHourglass, kIconSize)];
    case actor::ToolType::kWaitZeroDuration:
    case actor::ToolType::kUnknown:
      return nil;
    case actor::ToolType::kSelect:
    case actor::ToolType::kDragAndRelease:
    case actor::ToolType::kCloseTab:
    case actor::ToolType::kCreateTab:
    case actor::ToolType::kActivateTab:
      return [[ActuationWorklogChip alloc]
          initWithText:l10n_util::GetNSString(
                           IDS_IOS_ACTOR_WORKLOG_CHIP_DEFAULT)
                  icon:SymbolWithPointSize(SymbolCursorArrow, kIconSize)];
  }
}

IntelligenceHeaderItem* ActuationHeaderCloseItem() {
  return [IntelligenceHeaderItem
           itemWithIdentifier:kActuationHeaderCloseItemIdentifier
                         icon:SymbolWithPointSize(SymbolXMark,
                                                  kHeaderButtonPointSize)
                        title:l10n_util::GetNSString(IDS_CLOSE)
      accessibilityIdentifier:
          kActuationHeaderCloseButtonAccessibilityIdentifier];
}

ActuationHeaderContentVariants* ActuationHeaderContentVariantsForTask(
    NSString* task_title,
    actor::ActorTaskState task_state,
    NSString* current_step) {
  IntelligenceHeaderItem* primary_item = PrimaryHeaderItem(task_state);
  ActuationHeaderContent* default_content =
      [[ActuationHeaderContent alloc] initWithTitle:task_title
                                           subtitle:nil
                                        primaryItem:primary_item];
  return [[ActuationHeaderContentVariants alloc]
      initWithMinimizedContent:MinimizedHeaderContent(task_title, task_state,
                                                      current_step,
                                                      primary_item)
                compactContent:default_content
               expandedContent:default_content];
}

ActuationWorklogItem* ActuationWorklogItemForUserResponse(NSString* response) {
  CHECK(response.length);
  return [ActuationWorklogItem
      labeledItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_ACTOR_WORKLOG_USER_RESPONDED_TITLE)
                  subtitle:l10n_util::GetNSStringF(
                               IDS_IOS_ACTOR_WORKLOG_USER_RESPONSE,
                               base::SysNSStringToUTF16(response))
                      icon:SymbolWithPointSize(SymbolEllipsisMessage,
                                               kIconSize)];
}

ActuationWorklogItem* ActuationWorklogItemForTaskContinues() {
  return [ActuationWorklogItem
      labeledItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_ACTOR_WORKLOG_TASK_CONTINUES)
                  subtitle:nil
                      icon:SymbolWithPointSize(SymbolPlayFill, kIconSize)];
}

ActuationWorklogItem* ActuationWorklogItemForTaskEnded() {
  return [ActuationWorklogItem
      labeledItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_ACTOR_WORKLOG_TASK_ENDED)
                  subtitle:nil
                      icon:SymbolWithPointSize(SymbolStopFill, kIconSize)];
}
