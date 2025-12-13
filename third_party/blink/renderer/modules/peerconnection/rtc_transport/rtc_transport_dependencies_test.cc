#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/rtc_transport_dependencies.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

class RtcTransportDependenciesTest : public ::testing::Test {
 private:
  test::TaskEnvironment task_environment_;
};

TEST_F(RtcTransportDependenciesTest, InitializationAndDestruction) {
  V8TestingScope scope;
  base::RunLoop run_loop;
  RtcTransportDependencies::GetInitialized(
      *scope.GetExecutionContext(),
      base::BindLambdaForTesting([&](RtcTransportDependencies* deps) {
        EXPECT_TRUE(deps);

        std::unique_ptr<P2PPortAllocator> port_allocator =
            deps->CreatePortAllocator();

        PostCrossThreadTask(
            *RtcTransportDependencies::NetworkTaskRunner(), FROM_HERE,
            CrossThreadBindOnce(
                [](std::unique_ptr<P2PPortAllocator> port_allocator,
                   CrossThreadOnceClosure loop_quit_closure) {
                  // Get the RtcTransportDependencies's members bound to the
                  // network thread by creating a port allocator session.
                  port_allocator->Initialize();
                  port_allocator->CreateSession("test", /*component=*/1,
                                                "ice_ufrag", "ice_password");
                  std::move(loop_quit_closure).Run();
                },
                std::move(port_allocator),
                CrossThreadOnceClosure(run_loop.QuitClosure())));
      }));
  run_loop.Run();
}

}  // namespace blink
