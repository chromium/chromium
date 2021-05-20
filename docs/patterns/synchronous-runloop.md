# The Synchronous RunLoop Pattern

The synchronous RunLoop pattern involves creating a new RunLoop, setting up a
specified quit condition for it, then calling Run() on it to block the current
thread until that quit condition is reached.

## Use this pattern when:

You need to **block the current thread** until an event happens, and you have a
way to get notified of that event, via a callback or observer interface or
similar. A couple of common scenarios might be:

* Waiting for an asynchronous event (like a network request) to complete
* Waiting for an animation to finish
* Waiting for a page to have loaded
* Waiting for some call that requires a thread hop to complete

The fact that this blocks a thread means it is **almost never appropriate
outside test code**.

## Don't use this pattern when:

* You don't really need the entire thread to wait
* You don't have and can't add a way to get notified when the event happens
* You're waiting for a timer to fire - for that, [TaskEnvironment] is likely a
  better fit.

## Alternatives / see also:

* [TaskEnvironment]
* Restructuring your code to not require blocking a thread

## How this pattern works:

This pattern relies on two important facts about [base::RunLoop]:

1. `base::RunLoop::Quit()` is idempotent - once a RunLoop enters the quit
   state, quitting it again does nothing
2. Once a RunLoop is in the quit state, calling `base::RunLoop::Run()` on it is
   a no-op

That means that if your code does this:

```c++
base::RunLoop loop;
maybe-asynchronously { loop.Quit(); }
loop.Run();
LOG(INFO) << "Hello!";
```

then regardless of whether the maybe-asynchronous `loop.Quit()` is executed
before or after `loop.Run()`,  the "Hello!" message will never be printed before
both `loop.Run()` and `loop.Quit()` have happened. If the `Quit` happens
before the `Run`, the `Run` will be a no-op; if the `Quit` happens after the
`Run` has started, the `Run` will exit after the `Quit`.

## How to use this pattern in Chromium:

If the asynchronous thing in question takes a completion callback:

```c++
base::RunLoop run_loop;
Reply reply;
DoThingAndReply(
    base::BindLambdaForTesting([&](const Reply& r) {
        reply = r;
        run_loop.Quit();
    }));
run_loop.Run();
```

or perhaps even just:

```c++
base::RunLoop run_loop;
DoThing(run_loop.QuitClosure());
run_loop.Run();
```

If there exists a GizmoObserver interface with an OnThingDone event:

```c++
class TestGizmoObserver : public GizmoObserver {
 public:
  TestGizmoObserver(base::RunLoop* loop, Gizmo* thing)
      : GizmoObserver(thing), loop_(loop) {}

  // GizmoObserver:
  void OnThingStarted(Gizmo* observed_gizmo) override { ... }
  void OnThingProgressed(Gizmo* observed_gizmo) override { ... }
  void OnThingDone(Gizmo* observed_gizmo) override {
    loop_->Quit();
  }
};

base::RunLoop run_loop;
TestGizmoObserver observer(&run_loop, gizmo);
gizmo->StartDoingThing();
run_loop.Run();
```

This is sometimes wrapped up into a helper class that internally constructs the
RunLoop like so, if all you need to do is wait for the event but don't care
about observing any intermediate states too:

```c++
class ThingDoneWaiter : public GizmoObserver {
 public:
  ThingDoneWaiter(Gizmo* thing) : GizmoObserver(thing) {}

  void Wait() {
    run_loop_.Run();
  }

  // GizmoObserver:
  void OnThingDone(Gizmo* observed_gizmo) {
    run_loop_.Quit();
  }

 private:
  RunLoop run_loop_;
};

ThingDoneWaiter waiter(gizmo);
gizmo->StartDoingThing();
waiter.Wait();
```

## Events vs States

It's important to differentiate between waiting on an *event* (such as a
notification or callback being fired) vs waiting for a *state* (such as a
property on a given object).

When waiting for events, it is crucial that the observer is constructed in time
to see the event (see also [waiting too late](#starting-to-wait-for-an-event-too-late)).
States, on the other hand, can be queried beforehand in the body of a
Wait()-style function.

The following is an example of a Waiter helper class that waits for a state, as
opposed to an event:

```c++
class GizmoReadyWaiter : public GizmoObserver {
 public:
  GizmoReadyObserver(Gizmo* gizmo)
      : gizmo_(gizmo) {}
  ~GizmoReadyObserver() override = default;

  void WaitForGizmoReady() {
    if (!gizmo_->ready()) {
      gizmo_observation_.Observe(gizmo_);
      run_loop_.Run();
    }
  }

  // GizmoObserver:
  void OnGizmoReady(Gizmo* observed_gizmo) {
    run_loop_.Quit();
  }

 private:
  RunLoop run_loop_;
  Gizmo* gizmo_;
  base::ScopedObservation<Gizmo, GizmoObserver> gizmo_observation_{this};
};
```

## Sharp edges

### Starting to wait for an event too late

A common mis-use of this pattern is like so:

```c++
gizmo->StartDoingThing();
base::RunLoop run_loop;
TestGizmoObserver observer(&run_loop, gizmo);
run_loop.Run();
```

This looks tempting because it seems that you can write a helper function:

```c++
void TerribleHorribleNoGoodVeryBadWaitForThing(Gizmo* gizmo) {
  base::RunLoop run_loop;
  TestGizmoObserver observer(&run_loop, gizmo);
  run_loop.Run();
}
```

and then your test code can simply read:

```c++
gizmo->StartDoingThing();
TerribleHorribleNoGoodVeryBadWaitForThing(gizmo);
```

However, this is a recipe for a flaky test: if `gizmo->StartDoingThing()`
*completes* and would deliver the `OnThingDone` callback before your
`TestGizmoObserver` is ever constructed, the `TestGizmoObserver` will never
receive `OnThingDone`, and then your `run_loop.Run()` will run forever,
frustrating a future tree sheriff (and then probably you, shortly afterward).
This is especially dangerous when `gizmo->StartDoingThing()` involves a thread
hop or network request, because these can unpredictably complete before or after
your observer gets constructed. To be safe, always begin observing the event
*before* running the code that will eventually cause the event!

If you still really want a helper function, perhaps you just want to inline the
start:

```c++
void NiceFriendlyDoThingAndWait(Gizmo* gizmo) {
  base::RunLoop run_loop;
  TestGizmoObserver observer(&run_loop, gizmo);
  gizmo->StartDoingThing();
  run_loop.Run();
}
```

with the test code being:

```c++
NiceFriendlyDoThingAndWait(gizmo);
```

Note that this is not an issue when waiting on a *state*, since the observer can
query to see if that state is already the current state.

### Guessing RunLoop cycles

Sometimes, there's no easy way to observe completion of an event. In that case,
if the code under test looks like this:

```c++
void StartDoingThing() { PostTask(&StepOne); }
void StepOne() { PostTask(&StepTwo); }
void StepTwo() { /* done! */ }
```

it can be tempting to do:

```c++
gizmo->StartDoingThing();
base::RunLoop().RunUntilIdle();
/* now it's done! */
```

However, doing this is adding dependencies to your test code on the exact async
behavior of the production code - for example, the production code may depend on
work happening on another TaskRunner, which this won't successfully wait for.
This will make your test brittle and flaky.

Instead of doing this, it's vastly better to add a way (even if it's just via a
[test API]) to observe the event you're interested in.

### Not managing lifetimes

As with most patterns, lifetimes can be an issue with this pattern when using
observers. If you are waiting on a given event to happen, and the object that's
being observed instead goes out of scope, the test may hang.
Similar badness can happen if the Waiter isn't properly removed as an observer,
which could lead to Use-After-Frees.

There are two good mitigation practices here.

#### Keep Waiter-style helper classes as narrowly scoped as possible.
Consider something like
```c++
TEST_F(GizmoTest, WaitForGizmo) {
  GizmoWaiter waiter;
  Gizmo gizmo;
  gizmo.Initialize();
  waiter.WaitForGizmoReady();
  ASSERT_TRUE(gizmo.ready());
}
```

This looks safe, but may not be. If GizmoObserver removes itself as an observer
from Gizmo in its destructor, this will result in a Use-After-Free during the
test tear down.  Instead, scope the GizmoWaiter more narrowly:
```c++
TEST_F(GizmoTest, WaitForGizmo) {
  Gizmo gizmo;
  {
    GizmoWaiter waiter;
    gizmo.Initialize();
    waiter.WaitForGizmoReady();
  }
  ASSERT_TRUE(gizmo.ready());
}
```

Since the GizmoWaiter is now narrowly-scoped, it will be destroyed when it is
no longer needed, and avoid Use-After-Free concerns.

#### If in doubt, handle the destruction case appropriately
If you need to potentially handle the case where the object being observed is
destroyed while a waiter is still active, you can handle the destruction case
gracefully.


```c++
class GizmoReadyWaiter : public GizmoObserver {
 public:
  GizmoReadyObserver(Gizmo* gizmo)
      : gizmo_(gizmo) {}
  ~GizmoReadyObserver() override = default;

  void WaitForGizmoReady() {
    ASSERT_TRUE(gizmo_)
        << "Trying to call Wait() after the Gizmo was destroyed!";
    if (!gizmo_->ready()) {
      gizmo_observation_.Observe(gizmo_);
      run_loop_.Run();
    }
  }

  // GizmoObserver:
  void OnGizmoReady(Gizmo* observed_gizmo) {
    gizmo_observation_.Reset();
    run_loop_.Quit();
  }
  void OnGizmoDestroying(Gizmo* observed_gizmo) {
    DCHECK_EQ(gizmo_, observed_gizmo);
    gizmo_ = nullptr;
    // Remove the observer now, to avoid a UAF in the destructor.
    gizmo_observation_.Reset();
    // Bail out so we don't time out in the test waiting for a ready state
    // that will never come.
    run_loop_.Quit();
    // Was this a possible expected outcome? If not, consider:
    // ADD_FAILURE() << "The Gizmo was destroyed before it was ready!";
  }

 private:
  RunLoop run_loop_;
  Gizmo* gizmo_;
  base::ScopedObservation<Gizmo, GizmoObserver> gizmo_observation_{this};
};
```

[base::RunLoop]: ../../base/run_loop.h
[TaskEnvironment]: ../threading_and_tasks_testing.md
[test API]: testapi.md
