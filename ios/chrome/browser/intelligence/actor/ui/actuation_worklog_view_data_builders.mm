// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data_builders.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Icon size for SF symbols in chips.
constexpr CGFloat kIconSize = 16.0;

// Returns header content with a status message above `task_title`.
ActuationHeaderContent* ContentWithStatus(int message_id,
                                          NSString* task_title) {
  return [[ActuationHeaderContent alloc]
      initWithTitle:l10n_util::GetNSString(message_id)
           subtitle:task_title];
}

// Returns the header content for the minimized display mode.
ActuationHeaderContent* MinimizedHeaderContent(NSString* task_title,
                                               actor::ActorTaskState task_state,
                                               NSString* current_step) {
  switch (task_state) {
    case actor::ActorTaskState::kInit:
    case actor::ActorTaskState::kActing:
    case actor::ActorTaskState::kReflecting:
    case actor::ActorTaskState::kPausedByActor:
    case actor::ActorTaskState::kPausedByUser:
      // TODO(crbug.com/571141885): Show a placeholder subtitle for the `kInit`
      // state, since at that point we don't have a task update yet.
      return [[ActuationHeaderContent alloc] initWithTitle:task_title
                                                  subtitle:current_step];
    case actor::ActorTaskState::kWaitingOnUser:
      return ContentWithStatus(IDS_IOS_ACTOR_HEADER_INPUT_NEEDED, task_title);
    case actor::ActorTaskState::kFinished:
      return ContentWithStatus(IDS_IOS_ACTOR_HEADER_TASK_DONE, task_title);
    // `kCancelled` is meant to only represent user cancellations. Other stop
    // reasons are expected to map to `kFinished` or `kFailed` once
    // crbug.com/532978481 is fixed.
    case actor::ActorTaskState::kCancelled:
      return ContentWithStatus(IDS_IOS_ACTOR_HEADER_TASK_STOPPED, task_title);
    case actor::ActorTaskState::kFailed:
      return ContentWithStatus(IDS_IOS_ACTOR_HEADER_TASK_FAILED, task_title);
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

ActuationHeaderContentVariants* ActuationHeaderContentVariantsForTask(
    NSString* task_title,
    actor::ActorTaskState task_state,
    NSString* current_step) {
  ActuationHeaderContent* default_content =
      [[ActuationHeaderContent alloc] initWithTitle:task_title subtitle:nil];
  return [[ActuationHeaderContentVariants alloc]
      initWithMinimizedContent:MinimizedHeaderContent(task_title, task_state,
                                                      current_step)
                compactContent:default_content
               expandedContent:default_content];
}
