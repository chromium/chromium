// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/ash_display_util.h"

#include "ash/shell.h"
#include "base/no_destructor.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "remoting/base/constants.h"
#include "ui/compositor/layer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace remoting {

namespace {

absl::optional<SkBitmap> ToSkBitmap(
    std::unique_ptr<viz::CopyOutputResult> result) {
  if (result->IsEmpty())
    return absl::nullopt;

  auto scoped_bitmap = result->ScopedAccessSkBitmap();
  return scoped_bitmap.GetOutScopedBitmap();
}

class DefaultAshDisplayUtil : public AshDisplayUtil {
 public:
  DefaultAshDisplayUtil() = default;
  DefaultAshDisplayUtil(const DefaultAshDisplayUtil&) = delete;
  DefaultAshDisplayUtil& operator=(const DefaultAshDisplayUtil&) = delete;
  ~DefaultAshDisplayUtil() override = default;

  // AshDisplayUtil implementation:
  DisplayId GetPrimaryDisplayId() const override {
    if (!screen())
      return display::kDefaultDisplayId;

    return screen()->GetPrimaryDisplay().id();
  }

  const std::vector<display::Display>& GetActiveDisplays() const override {
    if (!display_manager())
      return empty_display_list_;

    return display_manager()->active_display_list();
  }

  const display::Display* GetDisplayForId(DisplayId display_id) const override {
    if (!display_manager())
      return nullptr;

    if (!display_manager()->IsActiveDisplayId(display_id))
      return nullptr;

    return &display_manager()->GetDisplayForId(display_id);
  }

  void TakeScreenshotOfDisplay(DisplayId display_id,
                               ScreenshotCallback callback) override {
    aura::Window* root_window = GetRootWindowForId(display_id);
    if (!root_window) {
      std::move(callback).Run(absl::nullopt);
      return;
    }

    auto request = std::make_unique<viz::CopyOutputRequest>(
        viz::CopyOutputRequest::ResultFormat::RGBA,
        viz::CopyOutputRequest::ResultDestination::kSystemMemory,
        base::BindOnce(&ToSkBitmap).Then(std::move(callback)));

    request->set_area(gfx::Rect(root_window->bounds().size()));
    root_window->layer()->RequestCopyOfOutput(std::move(request));
  }

 private:
  const display::Screen* screen() const { return display::Screen::GetScreen(); }
  // We can not return a const pointer, as the ash shell has no const getter for
  // the display manager :/
  ash::Shell* shell() const { return ash::Shell::Get(); }
  const display::DisplayManager* display_manager() const {
    if (!shell())
      return nullptr;
    return shell()->display_manager();
  }
  aura::Window* GetRootWindowForId(DisplayId id) {
    if (!shell())
      return nullptr;

    return shell()->GetRootWindowForDisplayId(id);
  }

  const std::vector<display::Display> empty_display_list_;
};

AshDisplayUtil* g_instance_for_testing_ = nullptr;

}  // namespace

// static
AshDisplayUtil& AshDisplayUtil::Get() {
  static base::NoDestructor<DefaultAshDisplayUtil> instance_;

  if (g_instance_for_testing_)
    return *g_instance_for_testing_;

  return *instance_;
}

// static
void AshDisplayUtil::SetInstanceForTesting(AshDisplayUtil* instance) {
  if (instance)
    DCHECK(!g_instance_for_testing_);
  g_instance_for_testing_ = instance;
}

AshDisplayUtil::~AshDisplayUtil() = default;

}  // namespace remoting
