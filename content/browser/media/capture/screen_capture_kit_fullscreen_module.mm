// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/screen_capture_kit_fullscreen_module.h"

#include <array>

#include "base/metrics/histogram_functions.h"
#include "base/task/bind_post_task.h"
#import "base/task/single_thread_task_runner.h"
#include "content/public/common/content_features.h"

namespace content {
namespace {

BASE_FEATURE(kScreenCaptureKitFullscreenModule,
             "ScreenCaptureKitFullscreenModule",
             base::FEATURE_ENABLED_BY_DEFAULT);

static NSString* const kApplicationNameKeynote = @"Keynote";
static NSString* const kApplicationNameLibreOffice = @"LibreOffice";
static NSString* const kApplicationNamePowerPoint = @"Microsoft PowerPoint";
static NSString* const kApplicationNameOpenOffice = @"OpenOffice";

static NSString* const kEditorWindowNameOpenOffice = @" OpenOffice Impress";

bool IsPowerPointSlideShow(NSString* window_title) {
  // Localized strings of the title name that identifies the PowerPoint slide
  // show. This is needed in order to distinguish between the slide show window
  // and the presenter's view.
  static NSArray<NSString*>* kPowerPointSlideShowTitles = @[
    @"PowerPoint-Bildschirmpräsentation",
    @"Προβολή παρουσίασης PowerPoint",
    @"PowerPoint スライド ショー",
    @"PowerPoint Slide Show",
    @"PowerPoint 幻灯片放映",
    @"Presentación de PowerPoint",
    @"PowerPoint-slideshow",
    @"Presentazione di PowerPoint",
    @"Prezentácia programu PowerPoint",
    @"Apresentação do PowerPoint",
    @"PowerPoint-bildspel",
    @"Prezentace v aplikaci PowerPoint",
    @"PowerPoint 슬라이드 쇼",
    @"PowerPoint-lysbildefremvisning",
    @"PowerPoint-vetítés",
    @"PowerPoint Slayt Gösterisi",
    @"Pokaz slajdów programu PowerPoint",
    @"PowerPoint 投影片放映",
    @"Демонстрация PowerPoint",
    @"Diaporama PowerPoint",
    @"PowerPoint-diaesitys",
    @"Peragaan Slide PowerPoint",
    @"PowerPoint-diavoorstelling",
    @"การนำเสนอสไลด์ PowerPoint",
    @"Apresentação de slides do PowerPoint",
    @"הצגת שקופיות של PowerPoint",
    @"عرض شرائح في PowerPoint"
  ];

  for (NSString* pp_slide_title in kPowerPointSlideShowTitles) {
    if ([window_title hasPrefix:pp_slide_title]) {
      return true;
    }
  }
  return false;
}

bool IsOpenOfficeImpressWindow(NSString* window_title) {
  return [window_title hasSuffix:kEditorWindowNameOpenOffice];
}

bool API_AVAILABLE(macos(12.3))
    IsWindowFullscreen(SCWindow* window, NSArray<SCDisplay*>* displays) {
  for (SCDisplay* display : displays) {
    if (CGRectEqualToRect(window.frame, display.frame)) {
      return true;
    }
  }
  return false;
}

void API_AVAILABLE(macos(12.3))
    LogModeToUma(ScreenCaptureKitFullscreenModule::Mode mode) {
  base::UmaHistogramEnumeration("Media.ScreenCaptureKit.FullscreenModuleMode",
                                mode);
}

}  // namespace

std::unique_ptr<ScreenCaptureKitFullscreenModule>
MaybeCreateScreenCaptureKitFullscreenModule(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
    ScreenCaptureKitResetStreamInterface& reset_stream_interface,
    SCWindow* original_window) {
  if (base::FeatureList::IsEnabled(kScreenCaptureKitFullscreenModule)) {
    // Check if we should enable the fullscreen module for this window and what
    // mode to use.
    if ([kApplicationNamePowerPoint
            isEqualToString:original_window.owningApplication
                                .applicationName]) {
      return std::make_unique<ScreenCaptureKitFullscreenModule>(
          device_task_runner, reset_stream_interface, original_window.windowID,
          original_window.owningApplication.processID,
          ScreenCaptureKitFullscreenModule::Mode::kPowerPoint);
    }
    if ([kApplicationNameKeynote
            isEqualToString:original_window.owningApplication
                                .applicationName]) {
      return std::make_unique<ScreenCaptureKitFullscreenModule>(
          device_task_runner, reset_stream_interface, original_window.windowID,
          original_window.owningApplication.processID,
          ScreenCaptureKitFullscreenModule::Mode::kKeynote);
    }
    if ([kApplicationNameOpenOffice
            isEqualToString:original_window.owningApplication
                                .applicationName] &&
        IsOpenOfficeImpressWindow(original_window.title)) {
      return std::make_unique<ScreenCaptureKitFullscreenModule>(
          device_task_runner, reset_stream_interface, original_window.windowID,
          original_window.owningApplication.processID,
          ScreenCaptureKitFullscreenModule::Mode::kOpenOffice);
    }
    if ([kApplicationNameLibreOffice
            isEqualToString:original_window.owningApplication
                                .applicationName]) {
      // TODO(crbug.com/40233195): Implement support for LibreOffice.
      LogModeToUma(ScreenCaptureKitFullscreenModule::Mode::kLibreOffice);
      return nullptr;
    }
  }
  LogModeToUma(ScreenCaptureKitFullscreenModule::Mode::kUnsupported);
  return nullptr;
}

ScreenCaptureKitFullscreenModule::ScreenCaptureKitFullscreenModule(
    scoped_refptr<base::SingleThreadTaskRunner> device_task_runner,
    ScreenCaptureKitResetStreamInterface& reset_stream_interface,
    CGWindowID original_window_id,
    pid_t original_window_pid,
    Mode mode)
    : device_task_runner_(device_task_runner),
      reset_stream_interface_(reset_stream_interface),
      original_window_id_(original_window_id),
      original_window_pid_(original_window_pid),
      mode_(mode) {
  CHECK_NE(mode, Mode::kUnsupported);
  LogModeToUma(mode);
}

ScreenCaptureKitFullscreenModule::~ScreenCaptureKitFullscreenModule() = default;

void ScreenCaptureKitFullscreenModule::Start() {
  // Create a timer to periodically check if a new fullscreen window has been
  // created. The delay is set to 800 ms to give a response time that is less
  // than 1 second. Reducing the delay would increase the responsiveness at the
  // cost of a higher CPU load.
  timer_.Start(
      FROM_HERE, base::Milliseconds(800), this,
      &ScreenCaptureKitFullscreenModule::CheckForFullscreenPresentation);
}

void ScreenCaptureKitFullscreenModule::Reset() {
  timer_.Stop();
  fullscreen_mode_active_ = false;
  fullscreen_window_id_ = 0;
}

void ScreenCaptureKitFullscreenModule::CheckForFullscreenPresentation() {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  auto content_callback = base::BindPostTask(
      device_task_runner_,
      base::BindRepeating(&ScreenCaptureKitFullscreenModule::
                              OnFullscreenShareableContentCreated,
                          weak_factory_.GetWeakPtr()));

  if (get_shareable_content_for_test_) {
    get_shareable_content_for_test_.Run(content_callback);
  } else {
    auto handler = ^(SCShareableContent* content, NSError* error) {
      content_callback.Run(content);
    };
    [SCShareableContent getShareableContentExcludingDesktopWindows:true
                                               onScreenWindowsOnly:false
                                                 completionHandler:handler];
  }
}

void ScreenCaptureKitFullscreenModule::OnFullscreenShareableContentCreated(
    SCShareableContent* content) {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  if (!content || !timer_.IsRunning()) {
    return;
  }
  SCWindow* editor_window = nullptr;
  int number_of_impress_editor_windows = 0;
  for (SCWindow* window in content.windows) {
    if (window.windowID == original_window_id_) {
      editor_window = window;
    }
    if (mode_ == Mode::kOpenOffice &&
        window.owningApplication.processID == original_window_pid_ &&
        window.isOnScreen && !IsWindowFullscreen(window, [content displays]) &&
        IsOpenOfficeImpressWindow(window.title)) {
      ++number_of_impress_editor_windows;
    }
  }

  if (fullscreen_mode_active_) {
    // Verify that the fullscreen window is on screen. Reset to the original
    // window otherwise.
    for (SCWindow* window : [content windows]) {
      if (window.windowID == fullscreen_window_id_) {
        if (!window.isOnScreen) {
          fullscreen_mode_active_ = false;
          reset_stream_interface_->ResetStreamTo(editor_window);
        }
        break;
      }
    }
  } else {
    SCWindow* fullscreen_window =
        editor_window ? GetFullscreenWindow(content, editor_window,
                                            number_of_impress_editor_windows)
                      : nullptr;
    if (fullscreen_window) {
      // Fullscreen window detected, update stream to capture this window
      // instead.
      fullscreen_mode_active_ = true;
      fullscreen_window_id_ = fullscreen_window.windowID;
      reset_stream_interface_->ResetStreamTo(fullscreen_window);
    }
  }
}

SCWindow* ScreenCaptureKitFullscreenModule::GetFullscreenWindow(
    SCShareableContent* content,
    SCWindow* editor_window,
    int number_of_impress_editor_windows) const {
  DCHECK(device_task_runner_->RunsTasksInCurrentSequence());
  SCWindow* fullscreen_window = nullptr;
  int fullscreenWindowLayer = 0;
  for (SCWindow* window in content.windows) {
    // Only check windows that belong to the same application as the original
    // window.
    if (window.owningApplication.processID == original_window_pid_ &&
        window.windowID != original_window_id_ && window.onScreen &&
        [window.owningApplication.applicationName
            isEqualToString:editor_window.owningApplication.applicationName] &&
        IsWindowFullscreen(window, content.displays)) {
      switch (mode_) {
        case Mode::kPowerPoint:
          if ([window.title containsString:editor_window.title]) {
            if (IsPowerPointSlideShow(window.title)) {
              fullscreen_window = window;
            }
          }
          break;
        case Mode::kOpenOffice:
          // Since the window title is empty, we cannot make a certain match to
          // determine what presentation is fullscreen if there are more than
          // one Impress editor window open.
          if (window.title.length == 0 &&
              number_of_impress_editor_windows == 1) {
            fullscreen_window = window;
          }
          break;
        case Mode::kKeynote:
          // For Keynote we must select the window with the highest layer.
          if ([window.title isEqualToString:editor_window.title] &&
              !editor_window.onScreen &&
              window.windowLayer > fullscreenWindowLayer) {
            fullscreen_window = window;
            fullscreenWindowLayer = window.windowLayer;
          }
          break;
        case Mode::kLibreOffice:
        // TODO(crbug.com/40233195): Implement support for LibreOffice.
        case Mode::kUnsupported:
          NOTREACHED_IN_MIGRATION();
      }
    }
  }
  return fullscreen_window;
}

}  // namespace content
