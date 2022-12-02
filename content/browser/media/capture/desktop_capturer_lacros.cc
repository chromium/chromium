// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capturer_lacros.h"

#include "base/feature_list.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/common/content_features.h"
#include "media/capture/capture_switches.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"

namespace content {
namespace {

// An SkBitmap backed subclass of DesktopFrame. This enables the webrtc system
// to retain the SkBitmap buffer without having to copy the pixels out until
// they are needed (e.g., for encoding).
class DesktopFrameSkia : public webrtc::DesktopFrame {
 public:
  explicit DesktopFrameSkia(const SkBitmap& bitmap)
      : webrtc::DesktopFrame(
            webrtc::DesktopSize(bitmap.width(), bitmap.height()),
            bitmap.rowBytes(),
            static_cast<uint8_t*>(bitmap.getPixels()),
            nullptr),
        bitmap_(bitmap) {}
  ~DesktopFrameSkia() override = default;

 private:
  DesktopFrameSkia(const DesktopFrameSkia&) = delete;
  DesktopFrameSkia& operator=(const DesktopFrameSkia&) = delete;

  SkBitmap bitmap_;
};

}  // namespace

DesktopCapturerLacros::DesktopCapturerLacros(
    CaptureType capture_type,
    const webrtc::DesktopCaptureOptions& options)
    : capture_type_(capture_type),
      options_(options),
      is_aura_capture_enabled_(
          base::FeatureList::IsEnabled(features::kLacrosAuraCapture)) {
  // Allow this class to be constructed on any sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);

  if (is_aura_capture_enabled_) {
    InitializeWidgetMap();
    aura::Env::GetInstance()->AddObserver(this);
  }
}

DesktopCapturerLacros::~DesktopCapturerLacros() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_aura_capture_enabled_) {
    aura::Env::GetInstance()->RemoveObserver(this);
  }
}

bool DesktopCapturerLacros::GetSourceList(SourceList* result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<crosapi::mojom::SnapshotSourcePtr> sources;

  {
    mojo::SyncCallRestrictions::ScopedAllowSyncCall allow_sync_call;
    snapshot_capturer_->ListSources(&sources);
  }

  for (const auto& source : sources) {
    Source s;
    s.id = source->id;
    s.title = source->title;
    s.display_id = source->display_id;

    if (is_aura_capture_enabled_ && source->window_unique_id) {
      // Use the AcceleratedWidget's value as the in process identifier, since
      // that is unique to the process. Since we aren't called on the UI thread,
      // we cannot call |DesktopMediaID::RegisterNativeWindow()| directly.
      base::AutoLock lock(widget_map_lock_);
      if (auto it = widget_map_.find(source->window_unique_id.value());
          it != widget_map_.end()) {
        s.in_process_id = static_cast<content::DesktopMediaID::Id>(it->second);
      }
    }
    result->push_back(s);
  }
  return true;
}

bool DesktopCapturerLacros::SelectSource(SourceId id) {
#if DCHECK_IS_ON()
  // OK to modify on any thread prior to calling Start.
  DCHECK(!callback_ || sequence_checker_.CalledOnValidSequence());
#endif

  selected_source_ = id;
  return true;
}

bool DesktopCapturerLacros::FocusOnSelectedSource() {
  return true;
}

void DesktopCapturerLacros::Start(Callback* callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callback_ = callback;

  // The lacros service exists at all times except during early start-up and
  // late shut-down. This class should never be used in those two times.
  auto* lacros_service = chromeos::LacrosService::Get();
  DCHECK(lacros_service);

  // The remote connection to the screen manager. Because we do not live on the
  // main thread, we cannot just query for this via |LacrosService::GetRemote|,
  // which is thread affine. However, since we only need it to get the Screen
  // Capturer, we don't need to keep the remote to it around.
  mojo::Remote<crosapi::mojom::ScreenManager> screen_manager;

  lacros_service->BindScreenManagerReceiver(
      screen_manager.BindNewPipeAndPassReceiver());

  // Lacros can assume that Ash is at least M88.
  int version =
      lacros_service->GetInterfaceVersion(crosapi::mojom::ScreenManager::Uuid_);
  CHECK_GE(version, 1);

  if (capture_type_ == kScreen) {
    screen_manager->GetScreenCapturer(
        snapshot_capturer_.BindNewPipeAndPassReceiver());
  } else {
    screen_manager->GetWindowCapturer(
        snapshot_capturer_.BindNewPipeAndPassReceiver());
  }
}

void DesktopCapturerLacros::CaptureFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  DCHECK(!capturing_frame_);
  capturing_frame_ = true;
#endif

  snapshot_capturer_->TakeSnapshot(
      selected_source_, base::BindOnce(&DesktopCapturerLacros::DidTakeSnapshot,
                                       weak_factory_.GetWeakPtr()));
}

bool DesktopCapturerLacros::IsOccluded(const webrtc::DesktopVector& pos) {
  return false;
}

void DesktopCapturerLacros::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {}

void DesktopCapturerLacros::SetExcludedWindow(webrtc::WindowId window) {}

void DesktopCapturerLacros::DidTakeSnapshot(bool success,
                                            const SkBitmap& snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

#if DCHECK_IS_ON()
  capturing_frame_ = false;
#endif

  if (!success) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT,
                               std::unique_ptr<webrtc::DesktopFrame>());
    return;
  }

  callback_->OnCaptureResult(Result::SUCCESS,
                             std::make_unique<DesktopFrameSkia>(snapshot));
}

void DesktopCapturerLacros::InitializeWidgetMap() {
  // On desktop aura there is one WindowTreeHost per top-level window. Thus, by
  // iterating through the set of window_tree_hosts, we're essentially iterating
  // through the list of known windows.
  for (auto* host : aura::Env::GetInstance()->window_tree_hosts()) {
    OnHostInitialized(host);
  }
}

void DesktopCapturerLacros::OnHostInitialized(aura::WindowTreeHost* host) {
  DCHECK(host);
  base::AutoLock lock(widget_map_lock_);
  widget_map_.emplace(host->GetUniqueId(), host->GetAcceleratedWidget());
}

void DesktopCapturerLacros::OnHostDestroyed(aura::WindowTreeHost* host) {
  DCHECK(host);

  // Unfortunately, by the time we get notified that the host is destroyed,
  // the platform window has already been destroyed, and so we cannot call
  // |WindowTreeHost::GetUniqueId| to remove our widget from the map here.
  // As this class is currently only created when the picker is open, we expect
  // this notification to be fairly rare, so we'll do this less efficient
  // find/erase rather than either maintaining a separate/reverse map or making
  // our making the far more common "Find the AcceleratedWidget for the
  // corresponding window id from Ash-chrome" use-case slower.
  base::AutoLock lock(widget_map_lock_);
  auto it = base::ranges::find(widget_map_, host->GetAcceleratedWidget(),
                               [](auto it) { return it.second; });

  if (it != widget_map_.end())
    widget_map_.erase(it);
}

}  // namespace content
