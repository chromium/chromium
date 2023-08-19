#include "third_party/blink/renderer/modules/mediastream/media_stream_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "ui/display/screen_info.h"
#include "ui/display/screen_infos.h"

namespace blink {
namespace {

class FakeChromeClient : public RenderingTestChromeClient {
 public:
  const display::ScreenInfos& GetScreenInfos(LocalFrame&) const override {
    return screen_infos_;
  }
  void AddScreenInfo(display::ScreenInfo info) {
    screen_infos_.screen_infos.push_back(info);
  }

 private:
  display::ScreenInfos screen_infos_ = display::ScreenInfos();
};

class ScreenSizeTest : public RenderingTest {
 public:
  FakeChromeClient& GetChromeClient() const override { return *client_; }

 protected:
  Persistent<FakeChromeClient> client_ =
      MakeGarbageCollected<FakeChromeClient>();
};

TEST_F(ScreenSizeTest, Basic) {
  display::ScreenInfo screen;
  screen.rect = gfx::Rect(1920, 1200);
  client_->AddScreenInfo(screen);
  EXPECT_EQ(MediaStreamUtils::GetScreenSize(&GetFrame()),
            gfx::Size(1920, 1200));
}

TEST_F(ScreenSizeTest, MultiScreen) {
  display::ScreenInfo screen_1;
  display::ScreenInfo screen_2;
  screen_1.rect = gfx::Rect(1920, 1080);
  client_->AddScreenInfo(screen_1);
  screen_2.rect = gfx::Rect(1440, 2560);
  client_->AddScreenInfo(screen_2);
  EXPECT_EQ(MediaStreamUtils::GetScreenSize(&GetFrame()),
            gfx::Size(1920, 2560));
}

}  // namespace
}  // namespace blink
