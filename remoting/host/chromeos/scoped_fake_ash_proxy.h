// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_SCOPED_FAKE_ASH_PROXY_H_
#define REMOTING_HOST_CHROMEOS_SCOPED_FAKE_ASH_PROXY_H_

#include <string>
#include <vector>

#include "mojo/public/cpp/bindings/receiver.h"
#include "remoting/host/chromeos/ash_proxy.h"

#include "base/test/test_future.h"

namespace remoting::test {

struct ScreenshotRequest {
  ScreenshotRequest(DisplayId display, AshProxy::ScreenshotCallback callback);
  ScreenshotRequest(ScreenshotRequest&&);
  ScreenshotRequest& operator=(ScreenshotRequest&&);
  ~ScreenshotRequest();

  DisplayId display;
  AshProxy::ScreenshotCallback callback;
};

// Simple basic implementation of |AshProxy|.
// Will automatically register itself as the global version in the constructor,
// and deregister in the destructor.
class ScopedFakeAshProxy : public AshProxy {
 public:
  static constexpr DisplayId kDefaultPrimaryDisplayId = 12345678901;

  ScopedFakeAshProxy();
  ScopedFakeAshProxy(const ScopedFakeAshProxy&) = delete;
  ScopedFakeAshProxy& operator=(const ScopedFakeAshProxy&) = delete;
  ~ScopedFakeAshProxy() override;

  display::Display& AddPrimaryDisplay(DisplayId id = kDefaultPrimaryDisplayId);

  display::Display& AddDisplayWithId(DisplayId id);

  // Create a display with the given specifications.
  // See display::ManagedDisplayInfo::CreateFromSpec for details of the
  // specification string.
  display::Display& AddDisplayFromSpecWithId(const std::string& spec,
                                             DisplayId id);

  void RemoveDisplay(DisplayId id);

  ScreenshotRequest WaitForScreenshotRequest();

  void ReplyWithScreenshot(const absl::optional<SkBitmap>& screenshot);

  // AshProxy implementation:
  DisplayId GetPrimaryDisplayId() const override;
  const std::vector<display::Display>& GetActiveDisplays() const override;
  const display::Display* GetDisplayForId(DisplayId display_id) const override;
  void TakeScreenshotOfDisplay(DisplayId display_id,
                               ScreenshotCallback callback) override;

  void CreateVideoCapturer(
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> video_capturer)
      override;

  viz::FrameSinkId GetFrameSinkId(DisplayId source_display_id) override;

  void SetVideoCapturerReceiver(
      mojo::Receiver<viz::mojom::FrameSinkVideoCapturer>* receiver);

 private:
  display::Display& AddDisplay(display::Display new_display);

  DisplayId primary_display_id_ = -1;
  std::vector<display::Display> displays_;
  mojo::Receiver<viz::mojom::FrameSinkVideoCapturer>* receiver_ = nullptr;

  base::test::TestFuture<ScreenshotRequest> screenshot_request_;
};

}  // namespace remoting::test

#endif  // REMOTING_HOST_CHROMEOS_SCOPED_FAKE_ASH_PROXY_H_
