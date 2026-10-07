// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/fake_screen_capture_picker_mac.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <unistd.h>

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "content/browser/media/capture/screen_capture_kit_device_mac.h"
#include "content/public/browser/desktop_media_id.h"
#include "media/capture/video/video_capture_device.h"

using Source = webrtc::DesktopCapturer::Source;

namespace content {
namespace {
SCContentFilter* GetContentFilterAndSource(
    SCShareableContent* content,
    DesktopMediaID::Type type,
    pid_t browser_pid,
    const std::optional<std::string>& browser_window_title,
    Source* out_source) API_AVAILABLE(macos(14.4)) {
  SCWindow* largest_browser_window = nil;
  for (SCWindow* window in content.windows) {
    if (window.owningApplication.processID != browser_pid ||
        window.windowLayer != 0) {
      continue;
    }

    // Filter by title if a browser window title is set.
    if (browser_window_title) {
      std::string window_title = base::SysNSStringToUTF8(window.title);
      if (window_title.find(*browser_window_title) == std::string::npos) {
        continue;
      }
    }

    if (!largest_browser_window ||
        window.frame.size.width * window.frame.size.height >
            largest_browser_window.frame.size.width *
                largest_browser_window.frame.size.height) {
      largest_browser_window = window;
    }
  }

  if (type == DesktopMediaID::Type::TYPE_SCREEN) {
    SCRunningApplication* browser_app = nil;
    if (content.displays.count > 0) {
      for (SCRunningApplication* app in content.applications) {
        if (app.processID == browser_pid) {
          browser_app = app;
          break;
        }
      }
    }
    if (browser_app) {
      SCDisplay* display = content.displays[0];
      if (largest_browser_window) {
        CGFloat max_overlap = 0;
        for (SCDisplay* d in content.displays) {
          CGRect intersection =
              CGRectIntersection(d.frame, largest_browser_window.frame);
          CGFloat overlap =
              CGRectGetWidth(intersection) * CGRectGetHeight(intersection);
          if (overlap > max_overlap) {
            max_overlap = overlap;
            display = d;
          }
        }
      }
      VLOG(1) << "FakeScreenCapturePickerMac::Open (block), "
                 "selecting display: "
              << display.displayID;
      out_source->id = display.displayID;
      return [[SCContentFilter alloc] initWithDisplay:display
                                includingApplications:@[ browser_app ]
                                     exceptingWindows:@[]];
    }
    LOG(ERROR) << "No displays found to share.";
    return nil;
  }

  if (largest_browser_window) {
    VLOG(1) << "FakeScreenCapturePickerMac::Open (block), "
               "selecting window: "
            << largest_browser_window.windowID << " title: "
            << base::SysNSStringToUTF8(largest_browser_window.title);
    out_source->id = largest_browser_window.windowID;
    return [[SCContentFilter alloc]
        initWithDesktopIndependentWindow:largest_browser_window];
  }
  LOG(ERROR) << "No windows found to share.";
  return nil;
}

class API_AVAILABLE(macos(14.4)) FakeScreenCapturePickerMac
    : public NativeScreenCapturePicker {
 public:
  FakeScreenCapturePickerMac(std::optional<std::string> browser_window_title,
                             base::OnceClosure stop_callback)
      : browser_window_title_(std::move(browser_window_title)),
        stop_callback_(std::move(stop_callback)) {}
  ~FakeScreenCapturePickerMac() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void Open(DesktopMediaID::Type type,
            base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
            base::OnceCallback<void(Source)> picker_callback,
            base::OnceClosure cancel_callback,
            base::OnceClosure error_callback,
            base::OnceCallback<void(DesktopMediaID::Id)> stop_audio_callback)
      override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(type == DesktopMediaID::Type::TYPE_SCREEN ||
          type == DesktopMediaID::Type::TYPE_WINDOW);
    VLOG(1) << "FakeScreenCapturePickerMac::Open, type: " << type;
    if (base::mac::MacOSVersion() < 14'04'00) {
      if (error_callback) {
        std::move(error_callback).Run();
      }
      return;
    }

    __block base::OnceCallback<void(SCShareableContent*, NSError*)>
        on_shareable_content =
            base::BindPostTaskToCurrentDefault(base::BindOnce(
                &FakeScreenCapturePickerMac::OnShareableContent,
                weak_ptr_factory_.GetWeakPtr(), type,
                std::move(created_callback), std::move(picker_callback),
                std::move(error_callback)));
    [SCShareableContent
        getCurrentProcessShareableContentWithCompletionHandler:^(
            SCShareableContent* content, NSError* error) {
          std::move(on_shareable_content).Run(content, error);
        }];
  }

  void Close(DesktopMediaID device_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << "FakeScreenCapturePickerMac::Close, device_id: "
            << device_id.ToString();
  }

  void GetApplicationAudioCaptureId(
      DesktopMediaID::Id session_id,
      GetApplicationAudioCaptureIdCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(callback).Run(std::nullopt);
  }

  std::unique_ptr<media::VideoCaptureDevice> CreateDevice(
      const DesktopMediaID& source) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    VLOG(1) << "FakeScreenCapturePickerMac::CreateDevice, source: "
            << source.ToString();

    if (base::mac::MacOSVersion() < 14'04'00) {
      return nullptr;
    }

    if (!filter_) {
      LOG(ERROR) << "Failed to create content filter.";
      return nullptr;
    }

    VLOG(1) << "FakeScreenCapturePickerMac::CreateDevice, creating device for "
            << source.ToString();
    return CreateScreenCaptureKitDeviceMac(
        source, /*is_native_picker=*/true, filter_, base::DoNothing(),
        /*pip_screen_capture_coordinator_proxy=*/nullptr,
        std::move(stop_callback_));
  }

  base::WeakPtr<NativeScreenCapturePicker> GetWeakPtr() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnShareableContent(
      DesktopMediaID::Type type,
      base::OnceCallback<void(DesktopMediaID::Id)> created_callback,
      base::OnceCallback<void(Source)> picker_callback,
      base::OnceClosure error_callback,
      SCShareableContent* content,
      NSError* error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (error || !content) {
      if (error) {
        LOG(ERROR) << "Error getting shareable content: "
                   << base::SysNSStringToUTF8([error localizedDescription]);
      }
      if (error_callback) {
        std::move(error_callback).Run();
      }
      return;
    }

    Source source;
    SCContentFilter* new_filter = GetContentFilterAndSource(
        content, type, getpid(), browser_window_title_, &source);
    if (!new_filter) {
      if (error_callback) {
        std::move(error_callback).Run();
      }
      return;
    }

    filter_ = new_filter;
    if (created_callback) {
      std::move(created_callback).Run(source.id);
    }
    if (picker_callback) {
      std::move(picker_callback).Run(source);
    }
  }

  std::optional<std::string> browser_window_title_;
  SCContentFilter* __strong filter_ = nil;
  base::OnceClosure stop_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<FakeScreenCapturePickerMac> weak_ptr_factory_{this};
};

}  // namespace

std::unique_ptr<NativeScreenCapturePicker> CreateFakeScreenCapturePickerMac(
    std::optional<std::string> browser_window_title,
    base::OnceClosure stop_callback) {
  if (@available(macOS 14.4, *)) {
    return std::make_unique<FakeScreenCapturePickerMac>(
        std::move(browser_window_title), std::move(stop_callback));
  }
  return nullptr;
}

}  // namespace content
