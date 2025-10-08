#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport_dependencies.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

class RtcTransportDependenciesTest : public ::testing::Test {
 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(RtcTransportDependenciesTest, Initialization) {
  V8TestingScope scope;
  base::RunLoop run_loop;
  RtcTransportDependencies::GetInitialized(
      *scope.GetExecutionContext(),
      base::BindLambdaForTesting([&](RtcTransportDependencies* deps) {
        EXPECT_TRUE(deps);

        EXPECT_NE(deps->CreatePortAllocator(), nullptr);

        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace blink
