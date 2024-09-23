// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/remote.h"

#include <stdint.h>

#include <optional>
#include <tuple>
#include <utility>

#include "base/barrier_closure.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "mojo/core/test/mojo_test_base.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/bindings/shared_associated_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/bindings/tests/remote_unittest.test-mojom.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "mojo/public/cpp/system/wait.h"
#include "mojo/public/interfaces/bindings/tests/math_calculator.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom.h"
#include "mojo/public/interfaces/bindings/tests/scoping.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace remote_unittest {
namespace {

class MathCalculatorImpl : public math::Calculator {
 public:
  explicit MathCalculatorImpl(PendingReceiver<math::Calculator> receiver)
      : total_(0.0), receiver_(this, std::move(receiver)) {}
  ~MathCalculatorImpl() override = default;

  void Clear(ClearCallback callback) override {
    total_ = 0.0;
    std::move(callback).Run(total_);
  }

  void Add(double value, AddCallback callback) override {
    total_ += value;
    std::move(callback).Run(total_);
  }

  void Multiply(double value, MultiplyCallback callback) override {
    total_ *= value;
    std::move(callback).Run(total_);
  }

  Receiver<math::Calculator>& receiver() { return receiver_; }

  double total() const { return total_; }

 private:
  double total_;
  Receiver<math::Calculator> receiver_;
};

class MathCalculatorUI {
 public:
  explicit MathCalculatorUI(PendingRemote<math::Calculator> calculator)
      : calculator_(std::move(calculator)), output_(0.0) {}

  bool is_connected() const { return calculator_.is_connected(); }
  void set_disconnect_handler(base::OnceClosure closure) {
    calculator_.set_disconnect_handler(std::move(closure));
  }

  void Add(double value, base::OnceClosure closure) {
    calculator_->Add(
        value, base::BindOnce(&MathCalculatorUI::Output, base::Unretained(this),
                              std::move(closure)));
  }

  void Multiply(double value, base::OnceClosure closure) {
    calculator_->Multiply(
        value, base::BindOnce(&MathCalculatorUI::Output, base::Unretained(this),
                              std::move(closure)));
  }

  double GetOutput() const { return output_; }

  Remote<math::Calculator>& remote() { return calculator_; }

 private:
  void Output(base::OnceClosure closure, double output) {
    output_ = output;
    if (closure)
      std::move(closure).Run();
  }

  Remote<math::Calculator> calculator_;
  double output_;
};

class SelfDestructingMathCalculatorUI {
 public:
  explicit SelfDestructingMathCalculatorUI(
      PendingRemote<math::Calculator> calculator)
      : calculator_(std::move(calculator)), nesting_level_(0) {
    ++num_instances_;
  }

  void BeginTest(bool nested, base::OnceClosure closure) {
    nesting_level_ = nested ? 2 : 1;
    calculator_->Add(
        1.0, base::BindOnce(&SelfDestructingMathCalculatorUI::Output,
                            base::Unretained(this), std::move(closure)));
  }

  static int num_instances() { return num_instances_; }

  void Output(base::OnceClosure closure, double value) {
    if (--nesting_level_ > 0) {
      // Add some more and wait for re-entrant call to Output!
      calculator_->Add(
          1.0, base::BindOnce(&SelfDestructingMathCalculatorUI::Output,
                              base::Unretained(this), std::move(closure)));
    } else {
      std::move(closure).Run();
      delete this;
    }
  }

 private:
  ~SelfDestructingMathCalculatorUI() { --num_instances_; }

  Remote<math::Calculator> calculator_;
  int nesting_level_;
  static int num_instances_;
};

// static
int SelfDestructingMathCalculatorUI::num_instances_ = 0;

class ReentrantServiceImpl : public sample::Service {
 public:
  ~ReentrantServiceImpl() override = default;

  explicit ReentrantServiceImpl(PendingReceiver<sample::Service> receiver)
      : call_depth_(0),
        max_call_depth_(0),
        receiver_(this, std::move(receiver)) {}

  int max_call_depth() { return max_call_depth_; }

  void Frobinate(sample::FooPtr foo,
                 sample::Service::BazOptions baz,
                 PendingRemote<sample::Port> port,
                 sample::Service::FrobinateCallback callback) override {
    max_call_depth_ = std::max(++call_depth_, max_call_depth_);
    if (call_depth_ == 1) {
      EXPECT_TRUE(receiver_.WaitForIncomingCall());
    }
    call_depth_--;
    std::move(callback).Run(5);
  }

  void GetPort(PendingReceiver<sample::Port> receiver) override {}

 private:
  int call_depth_;
  int max_call_depth_;
  Receiver<sample::Service> receiver_;
};

class IntegerAccessorImpl : public sample::IntegerAccessor {
 public:
  IntegerAccessorImpl() : integer_(0) {}
  ~IntegerAccessorImpl() override = default;

  int64_t integer() const { return integer_; }

  void set_closure(base::OnceClosure closure) { closure_ = std::move(closure); }

 private:
  // sample::IntegerAccessor implementation.
  void GetInteger(GetIntegerCallback callback) override {
    std::move(callback).Run(integer_, sample::Enum::VALUE);
  }
  void SetInteger(int64_t data, sample::Enum type) override {
    integer_ = data;
    if (closure_)
      std::move(closure_).Run();
  }

  int64_t integer_;
  base::OnceClosure closure_;
};

class RemoteTest : public BindingsTestBase {
 public:
  RemoteTest() = default;
  ~RemoteTest() override { base::RunLoop().RunUntilIdle(); }

  void PumpMessages() { base::RunLoop().RunUntilIdle(); }
};

TEST_P(RemoteTest, IsBound) {
  Remote<math::Calculator> calc;
  EXPECT_FALSE(calc);
  MathCalculatorImpl calc_impl(calc.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(calc);
}

class EndToEndRemoteTest : public RemoteTest {
 public:
  void RunTest(const scoped_refptr<base::SequencedTaskRunner> runner) {
    base::RunLoop run_loop;
    done_closure_ = run_loop.QuitClosure();
    done_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
    runner->PostTask(FROM_HERE, base::BindOnce(&EndToEndRemoteTest::RunTestImpl,
                                               base::Unretained(this)));
    run_loop.Run();
  }

 private:
  void RunTestImpl() {
    PendingRemote<math::Calculator> calc;
    calc_impl_ = std::make_unique<MathCalculatorImpl>(
        calc.InitWithNewPipeAndPassReceiver());
    calculator_ui_ = std::make_unique<MathCalculatorUI>(std::move(calc));
    calculator_ui_->Add(2.0, base::BindOnce(&EndToEndRemoteTest::AddDone,
                                            base::Unretained(this)));
    calculator_ui_->Multiply(5.0,
                             base::BindOnce(&EndToEndRemoteTest::MultiplyDone,
                                            base::Unretained(this)));
    EXPECT_EQ(0.0, calculator_ui_->GetOutput());
  }

  void AddDone() { EXPECT_EQ(2.0, calculator_ui_->GetOutput()); }

  void MultiplyDone() {
    EXPECT_EQ(10.0, calculator_ui_->GetOutput());
    calculator_ui_.reset();
    calc_impl_.reset();
    done_runner_->PostTask(FROM_HERE, std::move(done_closure_));
  }

  base::OnceClosure done_closure_;
  scoped_refptr<base::SequencedTaskRunner> done_runner_;
  std::unique_ptr<MathCalculatorUI> calculator_ui_;
  std::unique_ptr<MathCalculatorImpl> calc_impl_;
};

TEST_P(EndToEndRemoteTest, EndToEnd) {
  RunTest(base::SingleThreadTaskRunner::GetCurrentDefault());
}

TEST_P(EndToEndRemoteTest, EndToEndOnSequence) {
  RunTest(base::ThreadPool::CreateSequencedTaskRunner({}));
}

TEST_P(RemoteTest, Movable) {
  Remote<math::Calculator> a;
  Remote<math::Calculator> b;
  MathCalculatorImpl calc_impl(b.BindNewPipeAndPassReceiver());

  EXPECT_TRUE(!a);
  EXPECT_FALSE(!b);

  a = std::move(b);

  EXPECT_FALSE(!a);
  EXPECT_TRUE(!b);
}

TEST_P(RemoteTest, Resettable) {
  Remote<math::Calculator> a;

  EXPECT_FALSE(a);

  MessagePipe pipe;

  // Save this so we can test it later.
  Handle handle = pipe.handle0.get();

  a.Bind(PendingRemote<math::Calculator>(std::move(pipe.handle0), 0u));

  EXPECT_TRUE(a);

  a.reset();

  EXPECT_FALSE(a);

  // Test that handle was closed by waiting for its peer to signal.
  Wait(pipe.handle1.get(), MOJO_HANDLE_SIGNAL_PEER_CLOSED);
}

TEST_P(RemoteTest, InvalidPendingRemotes) {
  PendingRemote<math::Calculator> invalid_remote;
  EXPECT_FALSE(invalid_remote);

  // A "null" remote is just a generic helper for an uninitialized
  // PendingRemote. Verify that it's equivalent to above.
  PendingRemote<math::Calculator> null_remote{NullRemote()};
  EXPECT_FALSE(null_remote);
}

TEST_P(RemoteTest, IsConnected) {
  PendingRemote<math::Calculator> remote;
  MathCalculatorImpl calc_impl(remote.InitWithNewPipeAndPassReceiver());

  MathCalculatorUI calculator_ui(std::move(remote));

  base::RunLoop run_loop;
  calculator_ui.Add(2.0, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(2.0, calculator_ui.GetOutput());
  EXPECT_TRUE(calculator_ui.is_connected());

  calculator_ui.Multiply(5.0, base::OnceClosure());
  EXPECT_TRUE(calculator_ui.is_connected());

  calc_impl.receiver().reset();

  base::RunLoop run_loop2;
  calculator_ui.set_disconnect_handler(run_loop2.QuitClosure());

  // The state change isn't picked up locally yet.
  EXPECT_TRUE(calculator_ui.is_connected());

  run_loop2.Run();

  // OK, now we see the disconnection.
  EXPECT_FALSE(calculator_ui.is_connected());
}

TEST_P(RemoteTest, DisconnectCallback) {
  PendingRemote<math::Calculator> remote;
  MathCalculatorImpl calc_impl(remote.InitWithNewPipeAndPassReceiver());

  MathCalculatorUI calculator_ui(std::move(remote));

  bool connected = true;
  base::RunLoop run_loop;
  calculator_ui.remote().set_disconnect_handler(base::BindLambdaForTesting([&] {
    connected = false;
    run_loop.Quit();
  }));

  base::RunLoop run_loop2;
  calculator_ui.Add(2.0, run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_EQ(2.0, calculator_ui.GetOutput());
  EXPECT_TRUE(calculator_ui.is_connected());

  calculator_ui.Multiply(5.0, base::OnceClosure());
  EXPECT_TRUE(calculator_ui.is_connected());

  calc_impl.receiver().reset();

  // The state change isn't picked up locally yet.
  EXPECT_TRUE(calculator_ui.is_connected());

  run_loop.Run();

  // OK, now we see the disconnection.
  EXPECT_FALSE(calculator_ui.is_connected());

  // We should have also been able to observe the error through the error
  // handler.
  EXPECT_FALSE(connected);
}

TEST_P(RemoteTest, DestroyRemoteOnMethodResponse) {
  PendingRemote<math::Calculator> remote;
  MathCalculatorImpl calc_impl(remote.InitWithNewPipeAndPassReceiver());

  EXPECT_EQ(0, SelfDestructingMathCalculatorUI::num_instances());

  SelfDestructingMathCalculatorUI* impl =
      new SelfDestructingMathCalculatorUI(std::move(remote));
  base::RunLoop run_loop;
  impl->BeginTest(false, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(0, SelfDestructingMathCalculatorUI::num_instances());
}

TEST_P(RemoteTest, NestedDestroyRemoteOnMethodResponse) {
  PendingRemote<math::Calculator> remote;
  MathCalculatorImpl calc_impl(remote.InitWithNewPipeAndPassReceiver());

  EXPECT_EQ(0, SelfDestructingMathCalculatorUI::num_instances());

  SelfDestructingMathCalculatorUI* impl =
      new SelfDestructingMathCalculatorUI(std::move(remote));
  base::RunLoop run_loop;
  impl->BeginTest(true, run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(0, SelfDestructingMathCalculatorUI::num_instances());
}

TEST_P(RemoteTest, ReentrantWaitForIncomingCall) {
  Remote<sample::Service> remote;
  ReentrantServiceImpl impl(remote.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop, run_loop2;
  remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
                    base::BindLambdaForTesting([&](int) { run_loop.Quit(); }));
  remote->Frobinate(nullptr, sample::Service::BazOptions::REGULAR, NullRemote(),
                    base::BindLambdaForTesting([&](int) { run_loop2.Quit(); }));

  run_loop.Run();
  run_loop2.Run();

  EXPECT_EQ(2, impl.max_call_depth());
}

TEST_P(RemoteTest, QueryVersion) {
  IntegerAccessorImpl impl;
  Remote<sample::IntegerAccessor> remote;
  Receiver<sample::IntegerAccessor> receiver(
      &impl, remote.BindNewPipeAndPassReceiver());

  EXPECT_EQ(0u, remote.version());

  base::RunLoop run_loop;
  remote.QueryVersion(base::BindLambdaForTesting([&](uint32_t version) {
    EXPECT_EQ(3u, version);
    run_loop.Quit();
  }));
  run_loop.Run();

  EXPECT_EQ(3u, remote.version());
}

TEST_P(RemoteTest, RequireVersion) {
  IntegerAccessorImpl impl;
  Remote<sample::IntegerAccessor> remote;
  Receiver<sample::IntegerAccessor> receiver(
      &impl, remote.BindNewPipeAndPassReceiver());

  EXPECT_EQ(0u, remote.version());

  remote.RequireVersion(1u);
  EXPECT_EQ(1u, remote.version());
  base::RunLoop run_loop;
  impl.set_closure(run_loop.QuitClosure());
  remote->SetInteger(123, sample::Enum::VALUE);
  run_loop.Run();
  EXPECT_TRUE(remote.is_connected());
  EXPECT_EQ(123, impl.integer());

  remote.RequireVersion(3u);
  EXPECT_EQ(3u, remote.version());
  base::RunLoop run_loop2;
  impl.set_closure(run_loop2.QuitClosure());
  remote->SetInteger(456, sample::Enum::VALUE);
  run_loop2.Run();
  EXPECT_TRUE(remote.is_connected());
  EXPECT_EQ(456, impl.integer());

  // Require a version that is not supported by the impl side.
  remote.RequireVersion(4u);
  // This value is set to the input of RequireVersion() synchronously.
  EXPECT_EQ(4u, remote.version());
  base::RunLoop run_loop3;
  remote.set_disconnect_handler(run_loop3.QuitClosure());
  remote->SetInteger(789, sample::Enum::VALUE);
  run_loop3.Run();
  EXPECT_FALSE(remote.is_connected());
  // The call to SetInteger() after RequireVersion(4u) is ignored.
  EXPECT_EQ(456, impl.integer());
}

class StrongMathCalculatorImpl : public math::Calculator {
 public:
  StrongMathCalculatorImpl(bool* destroyed) : destroyed_(destroyed) {}
  ~StrongMathCalculatorImpl() override { *destroyed_ = true; }

  // math::Calculator implementation.
  void Clear(ClearCallback callback) override {
    std::move(callback).Run(total_);
  }

  void Add(double value, AddCallback callback) override {
    total_ += value;
    std::move(callback).Run(total_);
  }

  void Multiply(double value, MultiplyCallback callback) override {
    total_ *= value;
    std::move(callback).Run(total_);
  }

 private:
  double total_ = 0.0;
  raw_ptr<bool> destroyed_;
};

TEST(StrongConnectorTest, Math) {
  base::test::SingleThreadTaskEnvironment task_environment;

  bool disconnected = false;
  bool destroyed = false;
  PendingRemote<math::Calculator> calc;
  base::RunLoop run_loop;

  UniqueReceiverSet<math::Calculator> receivers;
  receivers.Add(std::make_unique<StrongMathCalculatorImpl>(&destroyed),
                calc.InitWithNewPipeAndPassReceiver());
  receivers.set_disconnect_handler(base::BindLambdaForTesting([&] {
    disconnected = true;
    run_loop.Quit();
  }));

  {
    MathCalculatorUI calculator_ui(std::move(calc));

    base::RunLoop run_loop2, run_loop3;
    calculator_ui.Add(2.0, run_loop2.QuitClosure());
    calculator_ui.Multiply(5.0, run_loop3.QuitClosure());
    run_loop2.Run();
    run_loop3.Run();

    EXPECT_EQ(10.0, calculator_ui.GetOutput());
    EXPECT_FALSE(disconnected);
    EXPECT_FALSE(destroyed);
  }

  // Destroying calculator_ui should close the pipe and signal disconnection on
  // the receiving end, which will in turn destroy the impl since it's in a
  // UniqueReceiverSet.
  run_loop.Run();
  EXPECT_TRUE(disconnected);
  EXPECT_TRUE(destroyed);
}

class WeakMathCalculatorImpl : public math::Calculator {
 public:
  WeakMathCalculatorImpl(PendingReceiver<math::Calculator> receiver,
                         bool* disconnected,
                         bool* destroyed,
                         base::OnceClosure closure)
      : destroyed_(destroyed), receiver_(this, std::move(receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        [](bool* disconnected, base::OnceClosure closure) {
          *disconnected = true;
          std::move(closure).Run();
        },
        disconnected, std::move(closure)));
  }
  ~WeakMathCalculatorImpl() override { *destroyed_ = true; }

  void Clear(ClearCallback callback) override {
    std::move(callback).Run(total_);
  }

  void Add(double value, AddCallback callback) override {
    total_ += value;
    std::move(callback).Run(total_);
  }

  void Multiply(double value, MultiplyCallback callback) override {
    total_ *= value;
    std::move(callback).Run(total_);
  }

 private:
  double total_ = 0.0;
  raw_ptr<bool> destroyed_;
  base::OnceClosure closure_;

  Receiver<math::Calculator> receiver_;
};

TEST(WeakConnectorTest, Math) {
  base::test::SingleThreadTaskEnvironment task_environment;

  bool disconnected = false;
  bool destroyed = false;
  MessagePipe pipe;
  base::RunLoop run_loop;
  WeakMathCalculatorImpl impl(
      PendingReceiver<math::Calculator>(std::move(pipe.handle0)), &disconnected,
      &destroyed, run_loop.QuitClosure());

  {
    MathCalculatorUI calculator_ui(
        PendingRemote<math::Calculator>(std::move(pipe.handle1), 0u));

    base::RunLoop run_loop2, run_loop3;
    calculator_ui.Add(2.0, run_loop2.QuitClosure());
    calculator_ui.Multiply(5.0, run_loop3.QuitClosure());
    run_loop2.Run();
    run_loop3.Run();

    EXPECT_EQ(10.0, calculator_ui.GetOutput());
    EXPECT_FALSE(disconnected);
    EXPECT_FALSE(destroyed);
  }

  run_loop.Run();
  EXPECT_TRUE(disconnected);
  EXPECT_FALSE(destroyed);
}

class CImpl : public C {
 public:
  CImpl(bool* d_called, base::OnceClosure closure)
      : d_called_(d_called), closure_(std::move(closure)) {}
  ~CImpl() override = default;

  void Bind(PendingReceiver<C> receiver) {
    receiver_.Bind(std::move(receiver));
  }

 private:
  void D() override {
    *d_called_ = true;
    std::move(closure_).Run();
  }

  Receiver<C> receiver_{this};
  raw_ptr<bool> d_called_;
  base::OnceClosure closure_;
};

class BImpl : public B {
 public:
  BImpl(bool* d_called, base::OnceClosure closure)
      : c_(d_called, std::move(closure)) {}
  ~BImpl() override = default;

  void Bind(PendingReceiver<B> receiver) {
    receiver_.Bind(std::move(receiver));
  }

 private:
  void GetC(PendingReceiver<C> receiver) override {
    c_.Bind(std::move(receiver));
  }

  Receiver<B> receiver_{this};
  CImpl c_;
};

class AImpl : public A {
 public:
  AImpl(PendingReceiver<A> receiver, base::OnceClosure closure)
      : d_called_(false),
        receiver_(this, std::move(receiver)),
        b_(&d_called_, std::move(closure)) {}
  ~AImpl() override = default;

  bool d_called() const { return d_called_; }

 private:
  void GetB(PendingReceiver<B> receiver) override {
    b_.Bind(std::move(receiver));
  }

  bool d_called_;
  Receiver<A> receiver_;
  BImpl b_;
};

TEST_P(RemoteTest, Scoping) {
  Remote<A> a;
  base::RunLoop run_loop;
  AImpl a_impl(a.BindNewPipeAndPassReceiver(), run_loop.QuitClosure());

  EXPECT_FALSE(a_impl.d_called());

  {
    Remote<B> b;
    a->GetB(b.BindNewPipeAndPassReceiver());
    Remote<C> c;
    b->GetC(c.BindNewPipeAndPassReceiver());
    c->D();
  }

  // While B & C have fallen out of scope, the receiving endpoints will continue
  // to operate, and any messages sent prior to destruction will be delivered.
  EXPECT_FALSE(a_impl.d_called());
  run_loop.Run();
  EXPECT_TRUE(a_impl.d_called());
}

class PingTestImpl : public sample::PingTest {
 public:
  explicit PingTestImpl(PendingReceiver<sample::PingTest> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~PingTestImpl() override = default;

 private:
  // sample::PingTest:
  void Ping(PingCallback callback) override { std::move(callback).Run(); }

  Receiver<sample::PingTest> receiver_;
};

// Tests that FuseProxy does what it's supposed to do.
TEST_P(RemoteTest, Fusion) {
  PendingRemote<sample::PingTest> pending_remote;
  PingTestImpl impl(pending_remote.InitWithNewPipeAndPassReceiver());

  // Create another PingTest pipe and fuse it to the one hanging off |impl|.
  Remote<sample::PingTest> remote;
  EXPECT_TRUE(FusePipes(remote.BindNewPipeAndPassReceiver(),
                        std::move(pending_remote)));

  // Ping!
  bool called = false;
  base::RunLoop loop;
  remote->Ping(base::BindLambdaForTesting([&] {
    called = true;
    loop.Quit();
  }));
  loop.Run();
  EXPECT_TRUE(called);
}

void Fail() {
  FAIL() << "Unexpected connection error";
}

TEST_P(RemoteTest, FlushForTesting) {
  PendingRemote<math::Calculator> remote;
  MathCalculatorImpl calc_impl(remote.InitWithNewPipeAndPassReceiver());

  MathCalculatorUI calculator_ui(std::move(remote));
  calculator_ui.remote().set_disconnect_handler(base::BindOnce(&Fail));

  calculator_ui.Add(2.0, base::DoNothing());
  calculator_ui.remote().FlushForTesting();
  EXPECT_EQ(2.0, calculator_ui.GetOutput());

  calculator_ui.Multiply(5.0, base::DoNothing());
  calculator_ui.remote().FlushForTesting();

  EXPECT_EQ(10.0, calculator_ui.GetOutput());
}

TEST_P(RemoteTest, FlushAsyncForTesting) {
  PendingRemote<math::Calculator> remote;
  MathCalculatorImpl calc_impl(remote.InitWithNewPipeAndPassReceiver());

  MathCalculatorUI calculator_ui(std::move(remote));
  calculator_ui.remote().set_disconnect_handler(base::BindOnce(&Fail));

  calculator_ui.Add(2.0, base::DoNothing());
  base::RunLoop run_loop;
  calculator_ui.remote().FlushAsyncForTesting(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(2.0, calculator_ui.GetOutput());

  calculator_ui.Multiply(5.0, base::DoNothing());
  base::RunLoop run_loop2;
  calculator_ui.remote().FlushAsyncForTesting(run_loop2.QuitClosure());
  run_loop2.Run();

  EXPECT_EQ(10.0, calculator_ui.GetOutput());
}

TEST_P(RemoteTest, FlushForTestingWithClosedPeer) {
  Remote<math::Calculator> calc;
  std::ignore = calc.BindNewPipeAndPassReceiver();
  bool called = false;
  calc.set_disconnect_handler(
      base::BindLambdaForTesting([&] { called = true; }));
  calc.FlushForTesting();
  EXPECT_TRUE(called);
  calc.FlushForTesting();
}

TEST_P(RemoteTest, FlushAsyncForTestingWithClosedPeer) {
  Remote<math::Calculator> calc;
  std::ignore = calc.BindNewPipeAndPassReceiver();
  bool called = false;
  calc.set_disconnect_handler(
      base::BindLambdaForTesting([&] { called = true; }));
  base::RunLoop run_loop;
  calc.FlushAsyncForTesting(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(called);

  base::RunLoop run_loop2;
  calc.FlushAsyncForTesting(run_loop2.QuitClosure());
  run_loop2.Run();
}

TEST_P(RemoteTest, DisconnectWithReason) {
  Remote<math::Calculator> calc;
  MathCalculatorImpl calc_impl(calc.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  calc.set_disconnect_with_reason_handler(base::BindLambdaForTesting(
      [&](uint32_t custom_reason, const std::string& description) {
        EXPECT_EQ(42u, custom_reason);
        EXPECT_EQ("hey", description);
        run_loop.Quit();
      }));

  calc_impl.receiver().ResetWithReason(42u, "hey");

  run_loop.Run();
}

TEST_P(RemoteTest, PendingReceiverResetWithReason) {
  Remote<math::Calculator> calc;
  auto pending_receiver = calc.BindNewPipeAndPassReceiver();

  base::RunLoop run_loop;
  calc.set_disconnect_with_reason_handler(base::BindLambdaForTesting(
      [&](uint32_t custom_reason, const std::string& description) {
        EXPECT_EQ(88u, custom_reason);
        EXPECT_EQ("greetings", description);
        run_loop.Quit();
      }));

  pending_receiver.ResetWithReason(88u, "greetings");

  run_loop.Run();
}

TEST_P(RemoteTest, CallbackIsPassedRemote) {
  Remote<sample::PingTest> remote;
  auto pending_receiver = remote.BindNewPipeAndPassReceiver();

  base::RunLoop run_loop;

  // Make a call with the proxy's lifetime bound to the response callback.
  sample::PingTest* raw_proxy = remote.get();
  remote.set_disconnect_handler(run_loop.QuitClosure());
  raw_proxy->Ping(
      base::BindOnce([](Remote<sample::PingTest>) {}, std::move(remote)));

  // Signal disconnection on |remote|. This will ultimately lead to the proxy's
  // response callbacks being destroyed, which will in turn lead to the proxy
  // being destroyed. This should not crash.
  pending_receiver.reset();
  run_loop.Run();
}

TEST_P(RemoteTest, DisconnectHandlerOwnsRemote) {
  Remote<sample::PingTest>* remote = new Remote<sample::PingTest>;
  auto pending_receiver = remote->BindNewPipeAndPassReceiver();

  base::RunLoop run_loop;

  // Make a call with |remote|'s lifetime bound to the disconnect handler.
  remote->set_disconnect_handler(base::BindOnce(
      [](base::OnceClosure quit, Remote<sample::PingTest>* owned_remote) {
        owned_remote->reset();
        std::move(quit).Run();
      },
      run_loop.QuitClosure(), base::Owned(remote)));

  // Signal disconnection on |remote|. In the disconnect handler |remote| is
  // reset. This shouldn't immediately destroy the callback (and |remote| that
  // it owns), before the callback is completed.
  pending_receiver.reset();
  run_loop.Run();
}

TEST_P(RemoteTest, SharedRemote) {
  PendingRemote<math::Calculator> pending_remote;
  MathCalculatorImpl calc_impl(pending_remote.InitWithNewPipeAndPassReceiver());
  SharedRemote<math::Calculator> shared_remote(std::move(pending_remote));

  base::RunLoop run_loop;
  base::OnceClosure quit_closure = run_loop.QuitClosure();

  // Send a message on |thread_safe_remote| from a different sequence.
  auto main_task_runner = base::SequencedTaskRunner::GetCurrentDefault();
  auto sender_task_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  sender_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        shared_remote->Add(
            123, base::BindLambdaForTesting([&](double result) {
              EXPECT_EQ(123, result);
              // Validate the callback is invoked on the calling sequence.
              EXPECT_TRUE(sender_task_runner->RunsTasksInCurrentSequence());
              main_task_runner->PostTask(FROM_HERE, std::move(quit_closure));
            }));
      }));

  run_loop.Run();
}

TEST_P(RemoteTest, SharedRemoteWithTaskRunner) {
  const scoped_refptr<base::SequencedTaskRunner> other_thread_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});

  PendingRemote<math::Calculator> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();

  // Create a ThreadSafeRemote that we'll bind from a different thread.
  SharedRemote<math::Calculator> shared_remote(std::move(remote),
                                               other_thread_task_runner);
  ASSERT_TRUE(shared_remote);

  MathCalculatorImpl* math_calc_impl = nullptr;
  {
    base::RunLoop run_loop;
    auto quit_closure = run_loop.QuitClosure();
    other_thread_task_runner->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&] {
          math_calc_impl = new MathCalculatorImpl(std::move(receiver));
          std::move(quit_closure).Run();
        }));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    shared_remote->Add(123, base::BindLambdaForTesting([&](double result) {
                         EXPECT_EQ(123, result);
                         run_loop.Quit();
                       }));
    run_loop.Run();
  }

  other_thread_task_runner->DeleteSoon(FROM_HERE, math_calc_impl);

  // Reset the SharedRemote now so the background thread state tied to its
  // internal Remote can be deleted before the background thread itself is
  // cleaned up.
  shared_remote.reset();
}

class SequenceCheckerImpl : public mojom::SequenceChecker {
 public:
  SequenceCheckerImpl() = default;
  ~SequenceCheckerImpl() override = default;

  void set_quit_callback(base::OnceClosure callback) {
    quit_callback_ = std::move(callback);
  }

  // mojom::SequenceChecker:
  void Bind(
      PendingAssociatedReceiver<mojom::SequenceChecker> receiver) override {
    receivers_.Add(this, std::move(receiver));
  }

  void AddClient(
      PendingAssociatedRemote<mojom::SequenceCheckerClient> client) override {
    clients_.Add(std::move(client));
  }

  void Check(int32_t n) override {
    CHECK_EQ(next_expected_value_, n);
    ++next_expected_value_;
  }

  void GetNextExpectedValue(GetNextExpectedValueCallback callback) override {
    for (auto& client : clients_)
      client->OnNextExpectedValueQueried(next_expected_value_);
    std::move(callback).Run(next_expected_value_);
  }

  void Quit(QuitCallback callback) override {
    for (auto& client : clients_)
      client->OnQuit();

    // Destroys `this`, so we don't bother responding.
    DCHECK(quit_callback_);
    std::move(quit_callback_).Run();
  }

 private:
  int32_t next_expected_value_ = 0;
  AssociatedReceiverSet<mojom::SequenceChecker> receivers_;
  AssociatedRemoteSet<mojom::SequenceCheckerClient> clients_;
  base::OnceClosure quit_callback_;
};

class SequenceCheckerClientImpl : public mojom::SequenceCheckerClient {
 public:
  SequenceCheckerClientImpl() = default;
  ~SequenceCheckerClientImpl() override = default;

  PendingAssociatedRemote<mojom::SequenceCheckerClient> MakeRemote() {
    PendingAssociatedRemote<mojom::SequenceCheckerClient> remote;
    receivers_.Add(this, remote.InitWithNewEndpointAndPassReceiver());
    return remote;
  }

  // mojom::SequenceCheckerClient:
  void OnNextExpectedValueQueried(int32_t n) override {}
  void OnQuit() override {}

 private:
  AssociatedReceiverSet<mojom::SequenceCheckerClient> receivers_;
};

TEST_P(RemoteTest, SharedRemotePassAssociatedEndpointsEarly) {
  // Verifies that we can start passing associated endpoints over a SharedRemote
  // as soon as it's constructed, even if it's still scheduled to bind on a
  // background thread.
  const scoped_refptr<base::SequencedTaskRunner> other_thread_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});
  PendingRemote<mojom::SequenceChecker> remote;
  other_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](PendingReceiver<mojom::SequenceChecker> receiver) {
                       MakeSelfOwnedReceiver(
                           std::make_unique<SequenceCheckerImpl>(),
                           std::move(receiver));
                     },
                     remote.InitWithNewPipeAndPassReceiver()));

  SharedRemote<mojom::SequenceChecker> checker(std::move(remote),
                                               other_thread_task_runner);
  PendingAssociatedRemote<mojom::SequenceChecker> pending_associated_checker;
  checker->Bind(
      pending_associated_checker.InitWithNewEndpointAndPassReceiver());

  SharedAssociatedRemote<mojom::SequenceChecker> associated_checker =
      mojo::SharedAssociatedRemote<mojom::SequenceChecker>(
          std::move(pending_associated_checker), other_thread_task_runner);

  PendingAssociatedRemote<mojom::SequenceChecker>
      later_pending_associated_checker;
  auto later_associated_receiver =
      later_pending_associated_checker.InitWithNewEndpointAndPassReceiver();
  SharedAssociatedRemote<mojom::SequenceChecker> later_associated_checker =
      mojo::SharedAssociatedRemote<mojom::SequenceChecker>(
          std::move(later_pending_associated_checker),
          other_thread_task_runner);
  checker->Bind(std::move(later_associated_receiver));

  checker->Check(0);
  associated_checker->Check(1);
  later_associated_checker->Check(2);
  checker->Check(3);

  // Make sure the above Checks reach the impl before we pass the test.
  int32_t next_expected_value = 0;
  EXPECT_TRUE(checker->GetNextExpectedValue(&next_expected_value));
  EXPECT_EQ(4, next_expected_value);
}

TEST_P(RemoteTest, SharedRemoteEarlySyncCall) {
  // Verifies that sync calls made immediately after SharedRemote setup (with
  // off-thread binding) do not deadlock.
  const scoped_refptr<base::SequencedTaskRunner> other_thread_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});
  PendingRemote<mojom::SequenceChecker> remote;
  other_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](PendingReceiver<mojom::SequenceChecker> receiver) {
                       MakeSelfOwnedReceiver(
                           std::make_unique<SequenceCheckerImpl>(),
                           std::move(receiver));
                     },
                     remote.InitWithNewPipeAndPassReceiver()));
  SharedRemote<mojom::SequenceChecker> checker(std::move(remote),
                                               other_thread_task_runner);

  int32_t next_expected_value = -1;
  EXPECT_TRUE(checker->GetNextExpectedValue(&next_expected_value));
  EXPECT_EQ(0, next_expected_value);
}

TEST_P(RemoteTest, SharedRemoteSyncCallWithPendingEventOnSameThread) {
  // Verifies that a sync reply on a SharedRemote is properly handled even if
  // there's an another event (in this case, an async message to an associated
  // interface) ahead of it in the underlying router's task queue.
  const scoped_refptr<base::SequencedTaskRunner> other_thread_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});
  PendingRemote<mojom::SequenceChecker> remote;
  other_thread_task_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](PendingReceiver<mojom::SequenceChecker> receiver) {
                       MakeSelfOwnedReceiver(
                           std::make_unique<SequenceCheckerImpl>(),
                           std::move(receiver));
                     },
                     remote.InitWithNewPipeAndPassReceiver()));
  SharedRemote<mojom::SequenceChecker> checker(std::move(remote),
                                               other_thread_task_runner);

  SequenceCheckerClientImpl client;
  checker->AddClient(client.MakeRemote());

  int32_t next_expected_value = -1;
  EXPECT_TRUE(checker->GetNextExpectedValue(&next_expected_value));
  EXPECT_EQ(0, next_expected_value);
}

// Flaky on all platforms. https://crbug.com/1224768
TEST_P(RemoteTest, DISABLED_DisconnectDuringOffThreadSyncWaitWithUnprocessedTasks) {
  // Regression test for https://crbug.com/1223628.
  //
  // This tests a fairly obscure edge case where one or more message tasks is
  // queued and ready for dispatch to one or more endpoints on a pipe, but
  // another endpoint on one of the same sequences is blocking the thread on an
  // off-thread sync wait via a SharedAssociatedRemote proxy. We test that in
  // this scenario, disconnection of the underlying pipe will interrupt the sync
  // wait as expected.

  const scoped_refptr<base::SequencedTaskRunner> impl_sequence =
      base::ThreadPool::CreateSequencedTaskRunner({});
  const scoped_refptr<base::SequencedTaskRunner> associated_sequence =
      base::ThreadPool::CreateSequencedTaskRunner({});
  SharedRemote<mojom::SequenceChecker> remote;
  UniqueReceiverSet<mojom::SequenceChecker> checkers;
  base::RunLoop bind_loop;
  base::OnceClosure bind_done = bind_loop.QuitClosure();
  impl_sequence->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        // Make sure the impl clears `checkers` immediately when Quit() is
        // invoked so that it self-destructs.
        auto impl = std::make_unique<SequenceCheckerImpl>();
        impl->set_quit_callback(
            base::BindLambdaForTesting([&] { checkers.Clear(); }));
        checkers.Add(std::move(impl),
                     remote.BindNewPipeAndPassReceiver(impl_sequence));
        std::move(bind_done).Run();
      }));
  bind_loop.Run();

  // Bind an associated endpoint that sends messages from a different background
  // sequence.
  SharedAssociatedRemote<mojom::SequenceChecker> associated_remote;
  remote->Bind(
      associated_remote.BindNewEndpointAndPassReceiver(associated_sequence));

  // Add a new client, so that the impl can queue up an outgoing message before
  // disconnecting. We do this to ensure there's a non-error task in the local
  // endpoint's task queue before disconnection can be observed. The task won't
  // dispatch because this thread will be blocked on the sync wait below.
  SequenceCheckerClientImpl client;
  associated_remote->AddClient(client.MakeRemote());

  // Finally, do a sync call. This should still terminate as soon as the remote
  // is disconnected by the impl's quit callback set above, despite the fact
  // that there will be undispatched tasks queued on the local client endpoint.
  // The bug this test is covering would cause this wait to hang indefinitely.
  EXPECT_FALSE(associated_remote->Quit());
}

TEST_P(RemoteTest, SharedRemoteDisconnectCallback) {
  PendingRemote<math::Calculator> remote;
  MathCalculatorImpl calc_impl(remote.InitWithNewPipeAndPassReceiver());

  const scoped_refptr<base::SequencedTaskRunner> main_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});
  SharedRemote<math::Calculator> shared_remote(std::move(remote),
                                               main_task_runner);

  bool connected = true;
  base::RunLoop run_loop;
  // Register a callback to set_disconnect_handler. It should be called when the
  // pipe is disconnected.
  shared_remote.set_disconnect_handler(base::BindLambdaForTesting([&] {
                                         connected = false;
                                         run_loop.Quit();
                                       }),
                                       main_task_runner);

  base::RunLoop run_loop2;
  shared_remote->Add(123, base::BindLambdaForTesting([&](double result) {
                       EXPECT_EQ(123, result);
                       run_loop2.Quit();
                     }));
  run_loop2.Run();

  calc_impl.receiver().reset();
  run_loop.Run();

  // |connected| should be false after calling the disconnect callback.
  EXPECT_FALSE(connected);
}

constexpr int32_t kMagicNumber = 42;

class SharedRemoteSyncTestImpl : public mojom::SharedRemoteSyncTest {
 public:
  SharedRemoteSyncTestImpl() = default;
  ~SharedRemoteSyncTestImpl() override = default;

  // mojom::SharedRemoteSyncTest implementation:
  void Fetch(FetchCallback callback) override {
    // Post an async task to our current task runner to respond to this message.
    // Because the Remote and Receiver are bound to the same sequence, this will
    // only run if the Remote doesn't block the sequence on the sync call made
    // by the test below.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), kMagicNumber));
  }
};

TEST_P(RemoteTest, SharedRemoteSyncOnlyBlocksCallingSequence) {
  // Verifies that a sync call on a SharedRemote only blocks the calling
  // sequence, not the sequence to which the underlying Remote is bound.
  // See https://crbug.com/1016022.

  const scoped_refptr<base::SequencedTaskRunner> bound_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::WithBaseSyncPrimitives()});

  PendingRemote<mojom::SharedRemoteSyncTest> pending_remote;
  auto receiver = pending_remote.InitWithNewPipeAndPassReceiver();

  SharedRemote<mojom::SharedRemoteSyncTest> remote(std::move(pending_remote),
                                                   bound_task_runner);
  bound_task_runner->PostTask(
      FROM_HERE, base::BindOnce(
                     [](PendingReceiver<mojom::SharedRemoteSyncTest> receiver) {
                       MakeSelfOwnedReceiver(
                           std::make_unique<SharedRemoteSyncTestImpl>(),
                           std::move(receiver));
                     },
                     std::move(receiver)));

  int32_t value = 0;
  remote->Fetch(&value);
  EXPECT_EQ(kMagicNumber, value);

  remote.reset();

  // Resetting |remote| above will ultimately post a task to |bound_task_runner|
  // to signal a connection error and trigger the self-owned Receiver's
  // destruction. This ensures that the task will run, avoiding leaks.
  task_environment()->RunUntilIdle();
}

TEST_P(RemoteTest, SharedRemoteSyncCallsFromOffBoundConstructionSequence) {
  // Regression test for https://crbug.com/1102921. Verifies that when
  // bound to its construction sequence, a SharedRemote doesn't try blocking
  // that sequence when a sync call is made from another sequence.

  const scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::WithBaseSyncPrimitives()});

  // Ensure waiting on the main thread is not allowed so that blocking attempts
  // will break the test.
  base::DisallowBaseSyncPrimitives();

  PendingRemote<mojom::SharedRemoteSyncTest> pending_remote;
  SharedRemoteSyncTestImpl impl;
  Receiver<mojom::SharedRemoteSyncTest> receiver(
      &impl, pending_remote.InitWithNewPipeAndPassReceiver());

  int32_t value = 0;
  base::RunLoop loop;
  base::OnceClosure quit = loop.QuitClosure();
  SharedRemote<mojom::SharedRemoteSyncTest> remote(std::move(pending_remote));
  background_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([remote, &value, &quit] {
        EXPECT_TRUE(remote->Fetch(&value));
        EXPECT_EQ(kMagicNumber, value);
        std::move(quit).Run();
      }));

  loop.Run();

  // TaskEnvironment teardown wants to block the main thread.
  base::internal::ResetThreadRestrictionsForTesting();
}

TEST_P(RemoteTest, SharedRemoteSyncCallsFromBoundNonConstructionSequence) {
  // Regression test for https://crbug.com/1102921. Verifies that when
  // bound to some sequence other than that which constructed it, a SharedRemote
  // properly blocks when making sync calls from the bound sequence.

  const scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::WithBaseSyncPrimitives()});

  PendingRemote<mojom::SharedRemoteSyncTest> pending_remote;
  SharedRemoteSyncTestImpl impl;
  Receiver<mojom::SharedRemoteSyncTest> receiver(
      &impl, pending_remote.InitWithNewPipeAndPassReceiver());

  int32_t value = 0;
  base::RunLoop loop;
  base::OnceClosure quit = loop.QuitClosure();
  SharedRemote<mojom::SharedRemoteSyncTest> remote(std::move(pending_remote),
                                                   background_task_runner);
  background_task_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([remote, &value, &quit] {
        EXPECT_TRUE(remote->Fetch(&value));
        EXPECT_EQ(kMagicNumber, value);
        std::move(quit).Run();
      }));

  loop.Run();
}

TEST_P(RemoteTest, RemoteSet) {
  std::vector<std::optional<MathCalculatorImpl>> impls(4);

  PendingRemote<math::Calculator> remote0;
  PendingRemote<math::Calculator> remote1;
  PendingRemote<math::Calculator> remote2;
  PendingRemote<math::Calculator> remote3;
  impls[0].emplace(remote0.InitWithNewPipeAndPassReceiver());
  impls[1].emplace(remote1.InitWithNewPipeAndPassReceiver());
  impls[2].emplace(remote2.InitWithNewPipeAndPassReceiver());
  impls[3].emplace(remote3.InitWithNewPipeAndPassReceiver());

  RemoteSet<math::Calculator> remotes;
  auto id0 = remotes.Add(Remote<math::Calculator>(std::move(remote0)));
  auto id1 = remotes.Add(std::move(remote1));
  auto id2 = remotes.Add(std::move(remote2));
  auto id3 = remotes.Add(std::move(remote3));

  // Send a message to each and wait for a reply.
  {
    base::RunLoop loop;
    constexpr double kValue = 42.0;
    auto on_add = base::BarrierClosure(8, loop.QuitClosure());
    for (auto& remote : remotes) {
      remote->Add(kValue, base::BindLambdaForTesting([&](double total) {
                    EXPECT_EQ(kValue, total);
                    on_add.Run();
                  }));
    }

    // Use Get() to get a specified remote from RemoteSet.
    std::vector<mojo::RemoteSetElementId> ids = {id0, id1, id2, id3};
    for (auto& id : ids) {
      remotes.Get(id)->Add(kValue,
                           base::BindLambdaForTesting([&](double total) {
                             EXPECT_EQ(kValue * 2, total);
                             on_add.Run();
                           }));
    }
    loop.Run();

    EXPECT_EQ(kValue * 2, impls[0]->total());
    EXPECT_EQ(kValue * 2, impls[1]->total());
    EXPECT_EQ(kValue * 2, impls[2]->total());
    EXPECT_EQ(kValue * 2, impls[3]->total());
  }

  EXPECT_FALSE(remotes.empty());

  // Wipe out each of the impls and wait for a disconnect notification for each.

  {
    base::RunLoop loop;
    remotes.set_disconnect_handler(
        base::BindLambdaForTesting([&](RemoteSetElementId id) {
          EXPECT_EQ(id, id0);
          EXPECT_FALSE(remotes.Contains(id0));
          EXPECT_TRUE(remotes.Contains(id1));
          EXPECT_TRUE(remotes.Contains(id2));
          EXPECT_TRUE(remotes.Contains(id3));
          loop.Quit();
        }));
    impls[0].reset();
    loop.Run();
  }

  EXPECT_FALSE(remotes.empty());

  {
    base::RunLoop loop;
    remotes.set_disconnect_handler(
        base::BindLambdaForTesting([&](RemoteSetElementId id) {
          EXPECT_EQ(id, id2);
          EXPECT_FALSE(remotes.Contains(id0));
          EXPECT_TRUE(remotes.Contains(id1));
          EXPECT_FALSE(remotes.Contains(id2));
          EXPECT_TRUE(remotes.Contains(id3));
          loop.Quit();
        }));
    impls[2].reset();
    loop.Run();
  }

  EXPECT_FALSE(remotes.empty());

  {
    // Test that remote set disconnect_with_reason_handler can handle resets
    // without reason.
    base::RunLoop loop;
    remotes.set_disconnect_with_reason_handler(base::BindLambdaForTesting(
        [&](RemoteSetElementId id, uint32_t custom_reason_code,
            const std::string& description) {
          EXPECT_EQ(id, id1);
          EXPECT_EQ(custom_reason_code, static_cast<uint32_t>(0));
          EXPECT_EQ(description, "");
          EXPECT_FALSE(remotes.Contains(id0));
          EXPECT_FALSE(remotes.Contains(id1));
          EXPECT_FALSE(remotes.Contains(id2));
          EXPECT_TRUE(remotes.Contains(id3));
          loop.Quit();
        }));
    impls[1].reset();
    loop.Run();
  }

  EXPECT_FALSE(remotes.empty());

  {
    // Test that remote set disconnect_with_reason_handler can handle resets
    // with reason.
    base::RunLoop loop;
    remotes.set_disconnect_with_reason_handler(base::BindLambdaForTesting(
        [&](RemoteSetElementId id, uint32_t custom_reason_code,
            const std::string& description) {
          EXPECT_EQ(id, id3);
          EXPECT_EQ(custom_reason_code, static_cast<uint32_t>(10));
          EXPECT_EQ(description, "custom description");
          EXPECT_FALSE(remotes.Contains(id0));
          EXPECT_FALSE(remotes.Contains(id1));
          EXPECT_FALSE(remotes.Contains(id2));
          EXPECT_FALSE(remotes.Contains(id3));
          loop.Quit();
        }));
    impls[3]->receiver().ResetWithReason(10, "custom description");
    loop.Run();
  }

  EXPECT_TRUE(remotes.empty());
}

bool* dump_without_crashing_flag;
extern "C" void HandleDumpWithoutCrashing() {
  *dump_without_crashing_flag = true;
}

class LargeMessageTestImpl : public mojom::LargeMessageTest {
 public:
  explicit LargeMessageTestImpl(
      PendingReceiver<mojom::LargeMessageTest> receiver)
      : receiver_(this, std::move(receiver)) {}
  ~LargeMessageTestImpl() override = default;

  // mojom::LargeMessageTest implementation:
  void ProcessData(const std::vector<uint8_t>& data,
                   ProcessDataCallback callback) override {
    std::move(callback).Run(data.size());
  }

  void ProcessLotsOfData(const std::vector<uint8_t>& data,
                         ProcessLotsOfDataCallback callback) override {
    std::move(callback).Run(data.size());
  }

  void GetLotsOfData(uint64_t data_size,
                     GetLotsOfDataCallback callback) override {
    std::move(callback).Run(std::vector<uint8_t>(data_size));
  }

 private:
  Receiver<mojom::LargeMessageTest> receiver_;
};

// TODO(crbug.com/40226674): Flaky on Linux/ASAN, Mac, and Fuchsia bots.
TEST_P(RemoteTest, DISABLED_SendVeryLargeMessages) {
  Remote<mojom::LargeMessageTest> remote;
  LargeMessageTestImpl impl(remote.BindNewPipeAndPassReceiver());

  bool did_dump_without_crashing = false;
  dump_without_crashing_flag = &did_dump_without_crashing;
  base::debug::SetDumpWithoutCrashingFunction(&HandleDumpWithoutCrashing);

  // The test runner configures Mojo to cap message size at
  // `kMaxMessageSizeInTests`, so we test with data that's double that size.
  constexpr size_t kBigDataSize =
      core::test::MojoTestBase::kMaxMessageSizeInTests * 2;
  std::vector<uint8_t> lots_of_data(kBigDataSize);
  uint64_t data_size = 0;
  ASSERT_TRUE(remote->ProcessData(lots_of_data, &data_size));
  EXPECT_EQ(kBigDataSize, data_size);

  if (GetParam() == BindingsTestSerializationMode::kNeverSerialize) {
    // If the message is never serialized, there won't be a crash report even
    // without the [UnlimitedSize] attribute.
    EXPECT_FALSE(did_dump_without_crashing);
  } else {
    EXPECT_TRUE(did_dump_without_crashing);
  }

  did_dump_without_crashing = false;
  data_size = 0;
  ASSERT_TRUE(remote->ProcessLotsOfData(lots_of_data, &data_size));
  EXPECT_EQ(kBigDataSize, data_size);

  // Serialized or not, this message won't generate a crash report because it's
  // explicitly marked with [UnlimitedSize].
  EXPECT_FALSE(did_dump_without_crashing);

  // [UnlimitedSize] also allows replies to be large.
  did_dump_without_crashing = false;
  lots_of_data.clear();
  ASSERT_TRUE(remote->GetLotsOfData(kBigDataSize, &lots_of_data));
  EXPECT_EQ(kBigDataSize, lots_of_data.size());
  EXPECT_FALSE(did_dump_without_crashing);

  base::debug::SetDumpWithoutCrashingFunction(nullptr);
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(RemoteTest);
INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(EndToEndRemoteTest);

}  // namespace
}  // namespace remote_unittest
}  // namespace test
}  // namespace mojo
