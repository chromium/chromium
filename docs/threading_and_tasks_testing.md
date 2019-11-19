# Testing Components Which Post Tasks

[TOC]

## Overview

So you've read  the [Threading and Tasks] documentation, surveyed the associated
[Threading and Tasks FAQ] and have implemented a state-of-the-art component. Now
you want to test it :). This document will explain how to write matching
state-of-the-art tests.

## Task Environments

In order to **unit test** a component which post tasks, you'll need to bring up
the task environment in the scope of your test (or test fixture). It will need
to outlive the majority of other members to ensure they have access to the task
system throughout their lifetime. There are a rare exceptions, like
`base::test::ScopedFeatureList`, that need to outlive the task environment. For
browser tests, see the [Browser tests](#browser-tests) section below.

Task environments come in various forms but share the same fundamental
characteristics:
 * There can be only one per test (if your base fixture already provides one:
   see [Base Fixture managed
   TaskEnvironment](#base-fixture-managed-taskenvironment) for the correct
   paradigm to supplement it).
 * Tasks cannot be posted outside the lifetime of a task environment.
 * Posted tasks will be run or be destroyed before the end of
   ~TaskEnvironment().
 * They all derive from `base::test::TaskEnvironment` and support its
   [`ValidTraits`] and sometimes more.
  * See usage example in [task_environment.h].
  * For example, a key characteristic is that its [TimeSource
    trait](#timesource-trait) can be used to mock time to ease testing of timers,
    timeouts, etc.

The `TaskEnvironment` member is typically exposed in the protected section of
the test fixture to allow tests to drive it directly (there's no need to expose
public Run\*() methods that merely forward to the private member).

### base::test::SingleThreadTaskEnvironment

Your component uses `base::ThreadTaskRunnerHandle::Get()` or
`base::SequencedTaskRunnerHandle::Get()` to post tasks to the thread it was
created on? You'll need at least a `base::test::SingleThreadTaskEnvironment` in
order for these APIs to be functional and `base::RunLoop` to run the posted
tasks.

Typically this will look something like this:

foo.h
```c++
class Foo {
 public:
  Foo() : owning_sequence_(base::SequencedTaskRunnerHandle::Get()) {}

  DoSomethingAndReply(base::OnceClosure reply) {
    DCHECK(owning_sequence_->RunsTasksInCurrentSequence());
    something_was_done_ = true;
    owning_sequence_->PostTask(on_done);
  }

  bool something_was_done() const { return something_was_done_; }

 private:
  bool something_was_done_ = false;
  scoped_refptr<base::SequencedTaskRunner> owning_sequence_;
};
```

foo_unittest.cc
```c++
TEST(FooTest, DoSomething) {
  base::test::SingleThreadTaskEnvironment task_environment;

  Foo foo;
  RunLoop run_loop;
  foo.DoSomethingAndReply(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(foo.something_was_done());
}
```

Note that `RunLoop().RunUntilIdle()` could be used instead of a `QuitClosure()`
above but [best
practices](https://developers.google.com/web/updates/2019/04/chromium-chronicle-1)
favor QuitClosure() over RunUntilIdle() as the latter can lead to flaky tests.

### Full fledged base::test::TaskEnvironment

If your components depends on `base::ThreadPool` (that's a good thing!), you'll
need a full `base::test::TaskEnvironment`. Don't be afraid to use a full
`TaskEnvironment` when appropriate: think of "SingleThread" as being a
readability term like "const", it documents that ThreadPool isn't used when it's
not but you shouldn't be afraid to lift it.

Task runners are still obtained by the product code through
[base/task/post_task.h] without necessitating a test-only task runner injection
seam :).

Typical use case:

foo_service.h
```c++
class FooService {
 public:
  FooService()
      : backend_task_runner_(
            base::CreateSequencedTaskRunnerWithTraits(
                base::MayBlock(), base::ThreadPool(),
                base::TaskPriority::BEST_EFFORT))),
        backend_(new FooBackend,
                 base::OnTaskRunnerDeleter(backend_task_runner_)) {}

  // Flushes state to disk async and replies.
  FlushAndReply(base::OnceClosure on_done) {
    DCHECK(owning_sequence_->RunsTasksInCurrentSequence());
    backend_task_runner_->PostTaskAndReply(
        base::BindOnce(&FooBackend::Flush, Unretained(backend_.get()),
        std::move(on_done));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // See https://youtu.be/m6Kz6pMaIxc?t=882 for memory management best
  // practices.
  std::unique_ptr<FooBackend, base::OnTaskRunnerDeleter> backend_;
};
```

foo_service_unittest.cc
```c++
TEST(FooServiceTest, FlushAndReply) {
  base::test::TaskEnvironment task_environment;

  FooService foo_service;
  RunLoop run_loop;
  foo_service.FlushAndReply(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(VerifyFooStateOnDisk());
}
```

### content::BrowserTaskEnvironment

This is the same thing as `base::test::TaskEnvironment` with the addition of
`content::BrowserThread` support. You need this if-and-only-if the code under
test is using `BrowserThread::UI` or `BrowserThread::IO`. For determinism, both
BrowserThreads will share the main thread and can be driven by RunLoop. By
default the main thread will use `MainThreadType::UI` but you can override this
via the [MainThreadType trait](#mainthreadtype-trait) to ask for an IO pump.

`BrowserTaskEnvironment::REAL_IO_THREAD` can be also used as a construction
trait for rare instances that desire distinct physical BrowserThreads.

### web::WebTaskEnvironment

This is the //ios equivalent of `content::BrowserTaskEnvironment` to simulate
`web::WebThread`.

## Task Environment Traits and Abilities

### Driving the Task Environment

All task environments support the following methods to run tasks:
 * `base::RunLoop:Run()`: run the main thread until the `QuitClosure()` is
   invoked (note: other threads also run in parallel by default).
 * `base::RunLoop::RunUntilIdle()`: run the main thread until it is idle. This
   is typically not what you want in multi-threaded environments as it may
   resume before `ThreadPool` is idle.
 * `TaskEnvironment::RunUntilIdle()`: Runs everything the TaskEnvironment is
   aware of. This excludes system events and any threads outside of the main
   thread and ThreadPool. It should be used with care when such external factors
   can be involved.
 * `TaskEnvironment::FastForward*()`: More on this in the TimeSource section
   below.

### TimeSource trait

By default tests run under `TimeSource::SYSTEM_TIME` which means delays are
real-time and `base::Time::Now()` and `base::TimeTicks::Now()` return live
system times
([context](https://chromium-review.googlesource.com/c/chromium/src/+/1742616)).

Whenever testing code with delays, you should favor `TimeSource::MOCK_TIME` as a
trait. This makes it such that delayed tasks and `base::Time::Now()` +
`base::TimeTicks::Now()` use a mock clock.

Under this mode, the mock clock will start at the current system time but will
then only advance when explicitly requested by `TaskEnvironment::FastForward*()`
and `TaskEnvironment::AdvanceClock()` methods *or* when `RunLoop::Run()` is
running and all managed threads become idle (auto-advances to the soonest
delayed task, if any, amongst all managed threads).

`TaskEnvironment::FastForwardBy()` repeatedly runs existing immediately
executable tasks until idle and then advances the mock clock incrementally to
run the next delayed task within the time delta. It may advance time by more
than the requested amount if running the tasks causes nested
time-advancing-method calls.

This makes it possible to test code with flush intervals, repeating timers,
timeouts, etc. without any test-specific seams in the product code, e.g.:

foo_storage.h
```c++
class FooStorage {
 public:
  static constexpr base::TimeDelta::kFlushInterval =
      base::TimeDelta::FromSeconds(30);

  // Sets |key| to |value|. Flushed to disk on the next flush interval.
  void Set(base::StringPiece key, base::StringPiece value);
};
```

foo_unittest.cc
```c++
class FooStorageTest {
 public:
  FooStorageTest() = default;

  // Test helper that returns true if |key| is found in the on disk storage.
  bool FindKeyInOnDiskStorage(base::StringPiece key);

 protected:
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FooStorage foo_storage_;
};

TEST_F(FooStorageTest, Set) {
  foo_storage_.Set("mykey", "myvalue");
  EXPECT_FALSE(FindKeyInOnDiskStorage("mykey"));
  task_environment.FastForwardBy(FooStorage::kFlushInterval);
  EXPECT_TRUE(FindKeyInOnDiskStorage("mykey"));
}
```

In contrast, `TaskEnvironment::AdvanceClock()` simply advances the mock time by
the requested amount, and does not run tasks. This may be useful in
cases where `TaskEnvironment::FastForwardBy()` would result in a livelock. For
example, if one task is blocked on a `WaitableEvent` and there is a delayed
task that would signal the event (e.g., a timeout), then
`TaskEnvironment::FastForwardBy()` will never complete. In this case, you could
advance the clock enough that the delayed task becomes runnable, and then
`TaskEnvironment::RunUntilIdle()` would run the delayed task, signalling the
event.

```c++
TEST(FooTest, TimeoutExceeded)
{
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::WaitableEvent event;
  base::RunLoop run_loop;
  base::PostTaskAndReply(
      FROM_HERE, {base::ThreadPool(), base::MayBlock()},
      base::BindOnce(&BlocksOnEvent, base::Unretained(&event)),
      run_loop.QuitClosure());
  base::PostDelayedTask(
      FROM_HERE, {base::ThreadPool()},
      base::BindOnce(&WaitableEvent::Signal, base::Unretained(&event)),
      kTimeout);
  // Can't use task_environment.FastForwardBy() since BlocksOnEvent blocks
  // and the task pool will not become idle.
  // Instead, advance time until the timeout task becomes runnable.
  task_environment.AdvanceClock(kTimeout);
  // Now the timeout task is runable.
  task_environment.RunUntilIdle();
  // The reply task should already have been executed, but run the run_loop to
  // verify.
  run_loop.Run();
}
```

### MainThreadType trait

The average component only cares about running its tasks and
`MainThreadType::DEFAULT` is sufficient. Components that care to interact
asynchronously with the system will likely need a `MainThreadType::UI` to be
able to receive system events (e.g,. UI or clipboard events).

Some components will prefer a main thread that handles asynchronous IO events
and will use `MainThreadType::IO`. Such components are typically the ones living
on BrowserThread::IO and being tested with a `BrowserTaskEnvironment`
initialized with `MainThreadType::IO`.

Note: This is strictly about requesting a specific `MessagePumpType` for the
main thread. It has nothing to do with `BrowserThread::UI` or
`BrowserThread::IO` which are named threads in the //content/browser code.

### ThreadPoolExecutionMode trait

By default non-delayed tasks posted to `base::ThreadPool` may run at any point.
Tests that require more determinism can request
`ThreadPoolExecutionMode::QUEUED` to enforce that tasks posted to
`base::ThreadPool` only run when `TaskEnvironment::RunUntilIdle()` or
`TaskEnvironment::FastForward*()` are invoked. Note that `RunLoop::Run()` does
**not** unblock the ThreadPool in this mode and thus strictly runs only the main
thread.

When `ThreadPoolExecutionMode::QUEUED` is mixed with `TimeSource::MOCK_TIME`,
time will auto-advance to the soonest task *that is allowed to run* when
required (i.e. it will ignore delayed tasks in the thread pool while in
`RunLoop::Run()`). See
`TaskEnvironmentTest.MultiThreadedMockTimeAndThreadPoolQueuedMode` for an
example.

This trait is of course irrelevant under `SingleThreadTaskEnvironment`.

### ThreadingMode trait

Prefer an explicit `SingleThreadTaskEnvironment` over using
`ThreadingMode::MAIN_THREAD_ONLY`. The only reason to use
`ThreadingMode::MAIN_THREAD_ONLY` explicitly is if the parent class of your test
fixture manages the `TaskEnvironment` but takes `TaskEnvironmentTraits` to let
its subclasses customize it and you really need a `SingleThreadTaskEnvironment`.

## Base Fixture managed TaskEnvironment

In some cases it makes sense to have the base fixture of an entire section of
the codebase be managing the `TaskEnvironment` (e.g. [ViewsTestBase]). It's
useful if such base fixture exposes `TaskEnvironmentTraits` to their subclasses
so that individual tests within that domain can fine-tune their traits as
desired.

This typically looks like this (in this case `FooTestBase` opts to enforce
`MainThreadType::UI` and leaves other traits to be specified as desired):

```c++
// Constructs a FooTestBase with |traits| being forwarded to its
// TaskEnvironment. MainThreadType always defaults to UI and must not be
// specified.
template <typename... TaskEnvironmentTraits>
NOINLINE explicit FooTestBase(TaskEnvironmentTraits&&... traits)
    : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                        std::forward<TaskEnvironmentTraits>(traits)...) {}
```

Note, if you're not familiar with traits: TaskEnvironment traits use
[base/traits_bag.h] and will automatically complain at compile-time if an
enum-based trait is specified more than once (i.e. subclasses will not compile
if re-specifying `MainThreadType` in the above example).

## Browser tests

This is all nice and fancy for unit tests, but what about browser\_tests,
ui\_integration\_tests, etc? Tests that subclass `content::BrowserTestBase` bring
up the entire environment (tasks & more) by default.

The downside is that you don't have fine-grained control over it like you would
with all the `TaskEnvironment` methods.

The favored paradigm is `RunLoop::Run()` + `QuitClosure()`. The asynchronous
nature of Chromium code makes this the most reliable way to wait for an event.

There are fancy versions of this to perform common actions, e.g.
[content/public/test/browser_test_utils.h]
[content/public/test/content_browser_test_utils.h] which will let you navigate,
execute scripts, simulate UI interactions, etc.

But the idea is always the same :
 1) Instantiate `RunLoop run_loop;`
 2) Kick off some work and hand-off `run_loop.QuitClosure()`
 3) `run_loop.Run()` until the `QuitClosure()` is called.

### MOCK_TIME in browser tests

So you fell in love with `TimeSource::MOCK_TIME` but now you're in a browser
test... yeah, sorry about that...

The eventual goal is to make it possible to set up TaskEnvironmentTraits from
your test fixture just like you can override command-line, etc. but we're not
there yet...

In the mean time you can still use the old
`base::ScopedMockTimeMessageLoopTaskRunner` to mock delayed tasks on the main
thread (you're out of luck on other threads for now). And you can use
`base::subtle::ScopedTimeClockOverrides` if you want to override `Now()`.

You think that's a mess? Just think that it used to be this way in unit tests
too and you'll be happy again :).

## Old paradigms

Here are some paradigms you might see throughout the code base and some insight
on what to do about them (hint: copying them is not one!). Migration help is
welcome [crbug.com/984323](https://crbug.com/984323)!

### base::TestMockTimeTaskRunner

This is the ancestor of `SingleThreadTaskEnvironment` + `TimeSource::MOCK_TIME`.
It's sort of equivalent but prefer task environments for consistency.

The only case where `base::TestMockTimeTaskRunner` is still the only option is
when writing regression tests that simulate a specific task execution order
across multiple sequences. To do so, use two `base::TestMockTimeTaskRunner` and
have the racing components post their tasks to separate ones. You can then
explicitly run tasks posted to each one from the main test thread in a way that
deterministically exercises the race resolution under test. This only applies to
task execution order races, data races still require parallel execution and this
is the main reason `TaskEnvironment` doesn't multiplex the `ThreadPool` tasks
onto the main thread (i.e. exercise data races, especially in the scope of
TSAN).

### base::TestSimpleTaskRunner

Prefer using `SingleThreadTaskEnvironment` over `base::TestSimpleTaskRunner`.
`TestSimpleTaskRunner` isn't as "simple" as it seems specifically because it
runs tasks in a surprising order (delays aren't respected and nesting doesn't
behave as usual). Should you prefer to flush all tasks regardless of delays,
`TimeSource::MOCK_TIME` and `TaskEnvironment::FastForwardUntilNoTasksRemain()`
have you covered.

### base::NullTaskRunner

Prefer `SingleThreadTaskEnvironment` or `TaskEnvironment` with
`ThreadPoolExecutionMode::QUEUED` over `base::NullTaskRunner`. A
`NullTaskRunner` might seem appealing, but not posting tasks is under-testing
the potential side-effects of the code under tests. All tests should be okay if
tasks born from their actions are run or deleted at a later point.

### base::ScopedMockTimeMessageLoopTaskRunner

This is the ancestor of `base::TestMockTimeTaskRunner` which is itself mostly
deprecated. As mentioned above in the [TimeSource trait](#timesource-trait)
section: This should never be used anymore except to mock time when there's
already a task system in place, e.g. in browser\_tests.

### SetTaskRunnerForTesting() and SetTickClockForTesting()

Prior to `TaskEnvironment::TimeSource::MOCK_TIME`, many components had
`SetClockForTesting()` in their product code. And before modern [Threading and
Tasks], some components had SetTaskRunnerForTesting(). Neither of these
test-only seams are required anymore now that task environments can mock those
from under-the-hood. Cleanup in favor of modern TaskEnvironment paradigms is
always appreciated ([crbug.com/984323](https://crbug.com/984323)).

### Other helper task runners

Different parts of the codebase have their own helper task runners. Please
migrate away from them or document them above. Ultimately the goal is for
`TaskEnvironment` and its subclasses to rule them all and to have a consistent
task testing API surface once and for all.

It is still okay for specific areas to have a base fixture that configures a
default `TaskEnvironment` appropriate for that area and use the
`TaskEnvironmentTraits` paradigm outlined in the [Base Fixture managed
TaskEnvironment](#base-fixture-managed-taskenvironment) section above to let
individual tests provide additional traits.

[Threading and Tasks]: threading_and_tasks.md
[Threading and Tasks FAQ]: threading_and_tasks_faq.md
[`ValidTraits`]: https://cs.chromium.org/chromium/src/base/test/task_environment.h?type=cs&q=ValidTraits&sq=package:chromium&g=0
[task_environment.h]: https://cs.chromium.org/chromium/src/base/test/task_environment.h
[base/task/post_task.h]: https://cs.chromium.org/chromium/src/base/task/post_task.h
[ViewsTestBase]: https://cs.chromium.org/chromium/src/ui/views/test/views_test_base.h
[base/traits_bag.h]: https://cs.chromium.org/chromium/src/base/traits_bag.h
[content/public/test/browser_test_utils.h]: https://cs.chromium.org/chromium/src/content/public/test/browser_test_utils.h
[content/public/test/content_browser_test_utils.h]: https://cs.chromium.org/chromium/src/content/public/test/content_browser_test_utils.h
[content/public/test/test_utils.h]: https://cs.chromium.org/chromium/src/content/public/test/test_utils.h
