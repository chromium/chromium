// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_SCOPED_FAKE_ASH_DISPLAY_UTIL_H_
#define REMOTING_HOST_CHROMEOS_SCOPED_FAKE_ASH_DISPLAY_UTIL_H_

#include <string>
#include <vector>

#include "remoting/host/chromeos/ash_display_util.h"

#include "base/test/test_future.h"

namespace remoting {
namespace test {

struct ScreenshotRequest {
  ScreenshotRequest(DisplayId display,
                    AshDisplayUtil::ScreenshotCallback callback);
  ScreenshotRequest(ScreenshotRequest&&);
  ScreenshotRequest& operator=(ScreenshotRequest&&);
  ~ScreenshotRequest();

  DisplayId display;
  AshDisplayUtil::ScreenshotCallback callback;
};

// Simple basic implementation of |AshDisplayUtil|.
// Will automatically register itself as the global version in the constructor,
// and deregister in the destructor.
class ScopedFakeAshDisplayUtil : public AshDisplayUtil {
 public:
  static constexpr DisplayId kDefaultPrimaryDisplayId = 12345678901;

  ScopedFakeAshDisplayUtil();
  ScopedFakeAshDisplayUtil(const ScopedFakeAshDisplayUtil&) = delete;
  ScopedFakeAshDisplayUtil& operator=(const ScopedFakeAshDisplayUtil&) = delete;
  ~ScopedFakeAshDisplayUtil() override;

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

  // AshDisplayUtil implementation:
  DisplayId GetPrimaryDisplayId() const override;
  const std::vector<display::Display>& GetActiveDisplays() const override;
  const display::Display* GetDisplayForId(DisplayId display_id) const override;
  void TakeScreenshotOfDisplay(DisplayId display_id,
                               ScreenshotCallback callback) override;

 private:
  display::Display& AddDisplay(display::Display new_display);

  DisplayId primary_display_id_ = -1;
  std::vector<display::Display> displays_;

  base::test::TestFuture<ScreenshotRequest> screenshot_request_;
};

}  // namespace test
}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_SCOPED_FAKE_ASH_DISPLAY_UTIL_H_
