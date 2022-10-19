# Testing With Mojo

This document outlines some best practices and techniques for testing code which
internally uses a Mojo service. It assumes familiarity with the
[Mojo and Services] document.

## Example Code & Context

Suppose we have this Mojo interface:

```mojom
module example.mojom;

interface IncrementerService {
  Increment(int32 value) => (int32 new_value);
}
```

and this C++ class that uses it:

```c++
class Incrementer {
 public:
  Incrementer();

  void SetServiceForTesting(
      mojo::PendingRemote<mojom::IncrementerService> service);

  // The underlying service is async, so this method is too.
  void Increment(int32_t value,
                 IncrementCallback callback);

 private;
  mojo::Remote<mojom::IncrementerService> service_;
};

void Incrementer::SetServiceForTesting(
    mojo::PendingRemote<mojom::IncrementerService> service) {
  service_.Bind(std::move(service));
}

void Incrementer::Increment(int32_t value, IncrementCallback callback) {
  if (!service_)
    service_ = LaunchIncrementerService();
  service_->Increment(value, std::move(callback));
}
```

and we wish to swap a test fake in for the underlying IncrementerService, so we
can unit-test Incrementer. Specifically, we're trying to write this (silly) test:

```c++
// Test that Incrementer correctly handles when the IncrementerService fails to
// increment the value.
TEST(IncrementerTest, DetectsFailureToIncrement) {
  Incrementer incr;
  FakeIncrementerService service;
  incr.SetServiceForTest(service);

  // Incrementing is async, so we have to wait...
  base::RunLoop loop;
  int returned_value;
  incr.Increment(0,
    base::BindLambdaForTesting([&](int value) {
      returned_value = value;
      loop.Quit();
    }));
  loop.Run();

  EXPECT_EQ(0, returned_value);
}
```

## The Fake Service Itself

This part is fairly straightforward. Mojo generated a class called
mojom::IncrementerService, which is normally subclassed by
IncrementerServiceImpl (or whatever) in production; we can subclass it
ourselves:

```c++
class FakeIncrementerService : public mojom::IncrementerService {
 public:
  void Increment(int32_t value, IncrementCallback callback) override {
    // Does not actually increment, for test purposes!
    std::move(callback).Run(value);
  }
}
```

## Async Services

If we plug the FakeIncrementerService in in our test:

```c++
  mojo::Receiver<IncrementerService> receiver{&fake_service};
  incrementer->SetServiceForTest(receiver);
```

we can invoke it and wait for the response as we usually would:

```c++
  base::RunLoop loop;
  incrementer->Increment(1, base::BindLambdaForTesting(...));
  loop.Run();
```

... and all is well. However, we might reasonably want a more flexible
FakeIncrementerService, which allows for plugging different responses in as the
test progresses. In that case, we will actually need to wait twice: once for the
request to arrive at the FakeIncrementerService, and once for the response to be
delivered back to the Incrementer.

## Waiting For Requests

To do that, we can instead structure our fake service like this:

```c++
class FakeIncrementerService : public mojom::IncrementerService {
 public:
  void Increment(int32_t value, IncrementCallback callback) override {
    CHECK(!HasPendingRequest());
    last_value_ = value;
    last_callback_ = std::move(callback);
    if (wait_loop_)
      wait_loop_->Quit();
  }

  bool HasPendingRequest() const {
    return bool(last_callback_);
  }

  void WaitForRequest() {
    if (HasPendingRequest())
      return;
    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();
  }

  void AnswerRequest(int32_t value) {
    CHECK(HasPendingRequest());
    std::move(last_callback_).Run(value);
  }
};
```

That having been done, our test can now observe the state of the code under test
(in this case the Incrementer service) while the mojo request is pending, like
so:

```c++
  FakeIncrementerService service;
  mojo::Receiver<mojom::IncrementerService> receiver{&service};

  Incrementer incrementer;
  incrementer->SetServiceForTest(receiver);
  incrementer->Increment(1, base::BindLambdaForTesting(...));

  // This will do the right thing even if the Increment method later becomes
  // synchronous, and exercises the same async code paths as the production code
  // will.
  service.WaitForRequest();
  service.AnswerRequest(service.last_value() + 2);

  // The lambda passed in above will now asynchronously run somewhere here,
  // since the response is also delivered asynchronously by mojo.
```

## Test Ergonomics

The async-ness at both ends can create a good amount of boilerplate in test
code, which is unpleasant. This section gives some techniques for reducing it.

### Sync Wrappers

One can use the [synchronous runloop] pattern to make the mojo calls appear to
be synchronous *to the test bodies* while leaving them asynchronous in the
production code. Mojo actually generates test helpers for this already! We can
include `incrementer_service.mojom-test-utils.h` and then do:

```c++
int32_t Increment(Incrementer* incrementer, int32_t value) {
  mojom::IncrementerAsyncWaiter sync_incrementer(incrementer);
  return sync_incrementer.Increment(value);
}
```

Note that this only works if FakeIncrementerService does not need to be told
when to send a response (via AnswerRequest or similar) - if it does, this
pattern will deadlock!

To avoid that, the cleanest approach is to have the FakeIncrementerService
either contain a field with the next expected value, or a callback that produces
expected values on demand, so that your test code reads like:

```c++
  service.SetNextValue(2);
  EXPECT_EQ(Increment(incrementer, 1), 2);
```

or similar.

[Mojo and Services]: mojo_and_services.md
[synchronous runloop]: patterns/synchronous-runloop.md
