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
  Incrementer incrementer;
  FakeIncrementerService service;
  // ... somehow use `service` as a test fake for `incrementer` ...

  incrementer.Increment(0, ...);

  // ... Get the result and compare it with 0 ...
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

We can plug the FakeIncrementerService into our test using:

```c++
  mojo::Receiver<IncrementerService> receiver{&fake_service};
  incrementer->SetServiceForTesting(receiver.BindNewPipeAndPassRemote());
```

we can invoke it and wait for the response as we usually would:

```c++
  base::test::TestFuture test_future;
  incrementer->Increment(0, test_future.GetCallback());
  int32_t result = test_future.Get();
  EXPECT_EQ(0, result);
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
    if (!signal_.IsReady()) {
      signal_->SetValue();
    }
  }

  bool HasPendingRequest() const {
    return bool(last_callback_);
  }

  void WaitForRequest() {
    if (HasPendingRequest()) {
      return;
    }
    signal_.Clear();
    signal_.Wait();
  }

  void AnswerRequest(int32_t value) {
    CHECK(HasPendingRequest());
    std::move(last_callback_).Run(value);
  }
 private:
  int32_t last_value_;
  IncrementCallback last_callback_;
  base::test::TestFuture signal_;
};
```

That having been done, our test can now observe the state of the code under test
(in this case the Incrementer service) while the mojo request is pending, like
so:

```c++
  FakeIncrementerService service;
  mojo::Receiver<mojom::IncrementerService> receiver{&service};

  Incrementer incrementer;
  incrementer->SetServiceForTesting(receiver.BindNewPipeAndPassRemote());
  incrementer->Increment(1, base::BindLambdaForTesting(...));

  // This will do the right thing even if the Increment method later becomes
  // synchronous, and exercises the same async code paths as the production code
  // will.
  service.WaitForRequest();
  service.AnswerRequest(service.last_value() + 2);

  // The lambda passed in above will now asynchronously run somewhere here,
  // since the response is also delivered asynchronously by mojo.
```

## Intercepting Messages to Bound Receivers

In some cases, particularly in browser tests, we may want to take an existing,
bound `mojo::Receiver` and intercept certain messages to it. This allows us to:
 - modify message parameters before the message is handled by the original
   implementation,
 - modify returned values by intercepting callbacks,
 - introduce failures, or
 - completely re-implement the message handling logic

To accomplish this, Mojo autogenerates an InterceptorForTesting class for each
interface that can be subclassed to perform the interception. Continuing with
the example above, we can include `incrementer_service.mojom-test-utils.h` and
then use the following to intercept and replace the number to be incremented:

```c++
class IncrementerServiceInterceptor
    : public mojom::IncrementerServiceInterceptorForTesting {
 public:
  // We'll assume RealIncrementerService implements the Mojo interface, owns the
  // the bound mojo::Receiver, and makes it available to use via a testing
  // method we added named `receiver_for_testing()`.
  IncrementerServiceInterceptor(RealIncrementerService* service,
                                int32_t value_to_inject)
      : service_(service),
        value_to_inject_(value_to_inject),
        swapped_impl_(service->receiver_for_testing(), this) {}

  ~IncrementerServiceInterceptor() override = default;

  mojom::IncrementerService* GetForwardingInterface()
      override {
    return service_;
  }

  void Increment(int32_t value,
                 IncrementCallback callback) override {
    GetForwardingInterface()->Increment(value_to_inject_, std::move(callback));
  }

 private:
  raw_ptr<RealIncrementerService> service_;
  int32_t value_to_inject_;
  mojo::test::ScopedSwapImplForTesting<
      mojo::Receiver<mojom::IncrementerService>>
      swapped_impl_;
};
```

## Ensuring Message Delivery

Both `mojo::Remote` and `mojo::Receiver` objects have a `FlushForTesting()`
method that can be used to ensure that queued messages and replies have been
sent to the other end of the message pipe, respectively. `mojo::Remote` objects
also have an asynchronous version of this method call `FlushAsyncForTesting()`
that accepts a `base::OnceCallback` that will be called upon completion. These
methods can be particularly helpful in tests where the `mojo::Remote` and
`mojo::Receiver` might be in separate processes.

[Mojo and Services]: mojo_and_services.md
