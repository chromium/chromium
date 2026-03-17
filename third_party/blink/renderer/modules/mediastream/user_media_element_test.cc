#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/html_user_media_element.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream_constraints.h"
#include "third_party/blink/renderer/modules/mediastream/user_media_element_constraints.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/v8_testing_scope.h"

namespace blink {

class UserMediaElementTest : public ::testing::Test {
 public:
  test::TaskEnvironment task_environment_;
};

TEST_F(UserMediaElementTest, SetConstraintsStoresValue) {
  V8TestingScope scope;
  auto* element =
      MakeGarbageCollected<HTMLUserMediaElement>(scope.GetDocument());
  MediaStreamConstraints* constraints = MediaStreamConstraints::Create();
  constraints->setVideo(true);

  UserMediaElementConstraints::setConstraints(*element, constraints);

  EXPECT_EQ(UserMediaElementConstraints::From(*element).GetConstraints(),
            constraints);
}

TEST_F(UserMediaElementTest, SetConstraintsOnlySetsOnce) {
  V8TestingScope scope;
  auto* element =
      MakeGarbageCollected<HTMLUserMediaElement>(scope.GetDocument());
  MediaStreamConstraints* constraints1 = MediaStreamConstraints::Create();
  constraints1->setVideo(true);

  UserMediaElementConstraints::setConstraints(*element, constraints1);
  EXPECT_EQ(UserMediaElementConstraints::From(*element).GetConstraints(),
            constraints1);

  MediaStreamConstraints* constraints2 = MediaStreamConstraints::Create();
  constraints2->setAudio(true);
  UserMediaElementConstraints::setConstraints(*element, constraints2);
  EXPECT_EQ(UserMediaElementConstraints::From(*element).GetConstraints(),
            constraints1);
}

}  // namespace blink
