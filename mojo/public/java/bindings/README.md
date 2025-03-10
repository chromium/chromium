# Mojo Java Bindings API
This document is a subset of the [Mojo documentation](/mojo/README.md).

[TOC]

## Overview

The Mojo Java Bindings API leverages the [Java System API](/mojo/public/java/system/README.md)
to provide a more natural set of primitives for communicating over Mojo message
pipes. Combined with generated code from the [Mojom IDL and bindings generator](/mojo/public/tools/bindings/README.md),
users can easily connect interface clients and implementations across arbitrary
intra- and inter-process boundaries.

This document provides a brief guide to Java Bindings API usage with example
code snippets. Some foundational knowledge of Mojo and mojom interface
definitions is assumed. Familiarity with the [C++ bindings](/mojo/public/cpp/bindings/README.md)
may be useful for comparison but isn't strictly required. For detailed API
references you can consult the class definitions in [this directory](https://cs.chromium.org/chromium/src/mojo/public/java/bindings/src/org/chromium/mojo/bindings/).

# Should I or should I not use the Java bindings?

Java bindings typically only make sense on the browser side and for Android
devices where there is some service that's unique to Android. As such, Java
bindings are typically rarely used, but they are used sometimes. When
implementing a cross-platform feature or service, it's probably more appropriate
to use C++ Mojo bindings and (if necessary) plumb the platform-specific code
implemented in Java via JNI.

C++ bindings have more features than the Java bindings. In simple cases, Java
bindings provide enough functionality to do everything by themselves. However,
for complex usage, it may be necessary to instead use the C++ bindings and plumb
data through JNI. See the section on notable lacking features below.

However, working within Java can avoid boilerplate and complex memory management
between C++ and Java.

If your service doesn't really need to be implemented in Java (because it
doesn't depend on or benefit from any Java-exclusive APIs), you should probably
just write the whole thing in C++.

Implementing Mojo interfaces in Java is subject to the same Chromium IPC
[practices](/docs/security/mojo.md#Security) and [review](/docs/security/ipc-reviews.md)
processes. However, most reviewers are used to reviewing C++ code.

# Notable lacking features

This is not an exhaustive list.

## AssociatedInterfaces

These are not supported.

## ReportBadMessage

There is no way for Java to report a bad message, which is the standard practice
when the renderer sends a message that indicates it's misbehaving (and perhaps
compromised).

The best you can do if you receive a bad message is to just ignore the
request. However, try to close any Proxy objects or InterfaceRequests that might
not otherwise have needed closing.

If the IPC call usually expects a response (via a callback), but the callback
has not been called and the callback gets garbage collected, this will lead to
the Mojo connection being disconnected. The involvement of the garbage collector
makes this somewhat non-deterministic, so this behavior should not be relied
upon.

## Missing type conversions (StructTraits)

There are a few non-primitive but common standard library-like mojom types. The
C++ bindings allow the definition of custom conversions between C++ and mojom
types via StructTraits. The auto-generated Java interfaces don't implement these
custom conversions as they don't support StructTraits, so you don't get the
obvious conversions between these mojom and Java types. This isn't necessarily a
deal-breaker though, as you can still implement the conversions yourself (with
care).

An example is `String16`, representing a UTF-16 string. For `String16`, C++ can
convert transparently to/from `std::u16string`. However, in Java, this merely
produces something which instead resembles the underlying mojom struct - which
just contains a `short[]` array.

## Synchronous calls

You cannot perform synchronous method calls _from_ Java. However, Java can still
receive sync calls (as `[Sync]` annotations only affect the behavior on the
caller side). In general, you shouldn't need to make synchronous calls as they
have many [disadvantages](/mojo/public/cpp/bindings/README.md#think-carefully-before-you-decide-to-use-sync-calls).

# Key differences compared to C++ bindings

The Java bindings' APIs don't particularly use the terms "receiver" and
"remote". Instead, the terms "Interface" and "Proxy" might be used. However,
this document will often use a mixture of these terms, as "pending", "remotes",
and "receivers" are terms used in mojom.

In C++, you'll typically have a `pending_receiver<InterfaceType>` which you then
bind to an implementation of `InterfaceType`. In Java, this is an
`InterfaceRequest<InterfaceType>`, which can be bound to your implementation of
`InterfaceType`.

In C++, you typically have to hold on to the receiver for as long as you want it
alive, or deliberately use a "self-owned" receiver that stays alive as long as
the other side of the IPC connection needs it. In Java, Mojo will keep the
underlying IPC connection and your interface implementation (receiver) alive as
long as the other end stays alive. In practice, this behaves somewhat like
self-owned receivers.

In C++, there is generally a greater distinction between remotes and
pending_remotes. In Java, you will typically just deal with an
`InterfaceType.Proxy` object, which can be used as either a remote or
pending_remote interchangeably.

# Object and connection lifetimes

When implementing an IPC interface, you'll also need to implement a `close()`
method and an `onConnectionError()` method. The `close()` method will
automatically be called whenever the IPC disconnects (either gracefully or due
to an error). The `onConnectionError()` method will only be called (in addition
to `close()`) when the connection ends due to an error.

A Mojo connection that's bound to an implementation object will keep that object
alive. The connection is maintained by Mojo so long as the other (remote) end is
alive (bound or unbound). Note that calling the `close()` method on your
interface implementation yourself won't stop calls from coming in! In java, the
receiver generally isn't able to unilaterally end a connection cleanly.

A Proxy object will keep any connection it's bound to alive until you call
`close()` on it.

No longer needed Proxy objects and InterfaceRequests which are to be discarded
without binding should be closed. **Failing to do so can lead to resource leaks,
produce warnings, or lead to non-deterministic crashes**. Failing to close Proxy
objects may produce warnings in log output such as
`java.lang.IllegalStateException: Warning: Router objects should be explicitly closed when no longer required otherwise you may leak handles.`.
Failing to close InterfaceRequests may produce `Handle was not closed.`
warnings.

# Types

## Primitive types

| mojom type | non-nullable type | nullable type | Comment |
|------------|-------------------|---------------|---------|
| `bool` | `boolean` | `Boolean` |  |
| `int8`, `uint8` | `byte` | `Byte` | Signed and unsigned[^1] |
| `int16`, `uint16` | `short` | `Short` | Signed and unsigned[^1] |
| `int32`, `uint32` | `int` | `Integer` | Signed and unsigned[^1] |
| `int64`, `uint64` | `long` | `Long` | Signed and unsigned[^1] |
| `string` | `String` | `String` | Character encoding[^2] |
| `array<T>` | `T[]` | `T[]` |  |
| `array<T, N>` | `T[]` | `T[]` |  |
| `map<S, T>` | `Map<S, T>` | `Map<S, T>` |  |
| `handle` | `Handle` | `Handle` |  |
| `handle<message_pipe>` | `MessagePipeHandle` | `MessagePipeHandle` |  |
| `handle<shared_buffer>` | `SharedBufferHandle` | `SharedBufferHandle` |  |
| `handle<data_pipe_producer>` | `DataPipe.ProducerHandle` | `DataPipe.ProducerHandle` |  |
| `handle<data_pipe_consumer>` | `DataPipe.ConsumerHandle` | `DataPipe.ConsumerHandle` |  |
| `pending_remote<T>` | `T` or `T.Proxy` | `T` or `T.Proxy` | Automatic conversions[^3] |
| `pending_receiver<T>` | `InterfaceRequest<T>` | `InterfaceRequest<T>` |  |
| `pending_associated_remote<T>` | - | - | Unsupported[^4] |
| `pending_associated_receiver<T>` | - | - | Unsupported[^4] |


[^1]: Both signed and unsigned integers use the same Java types. Java has
    well-defined overflow and underflow behavior. If you need to represent the
    larger half of an unsigned number space, use the negative number space of
    the Java integer types. For example, a uint32 with value 0xFFFFFFFF, is
    represented by a -1 int in Java; 0xFFFFFFFE is -2; etc.

[^2]: Mojom strings are UTF-8 encoded, whilst Java strings are UTF-16
    encoded. The Java Mojo bindings will automatically translate between the two
    encodings for you. However, you should bear in mind that certain invalid
    Unicode strings (for example, unpaired surrogates) can result in lossy
    conversions. (If you're dealing with binary data, you should not be using
    strings anyway.)

[^3]: Passing a `pending_remote<InterfaceType>` over IPC to Java will cause it
    to be automatically converted to an `InterfaceType.Proxy` before it is
    passed to any of your interface implementations. Proxy objects can also be
    passed over IPC as pending_remotes. Java's Mojo bindings do not really make
    a distinction between pending and non-pending remotes as C++ does and will
    just automatically bind, unbind, and rebind them as needed when crossing IPC
    boundaries. See the section on passing interfaces through IPC for details.

[^4]: Associated interfaces are not supported in Java. Attempting to use them
    will cause an `AssociatedInterfaceNotSupportedException` or
    `AssociatedInterfaceRequestNotSupportedException`.

## Enums

```
enum CoffeeType {
  LATTE,
  ESPRESSO,
  CAPPUCCINO,
};
```

```java
// Non-nullable enum
@CoffeeType.EnumType long coffeeType = CoffeeType.LATTE;
// Nullable enum
@CoffeeType.EnumType Long coffeeType = null;
```

Enums are just integers with an annotation.

The constants for the different variants of an enum are available directly under
the enum's generated Java class and use SCREAMING_SNAKE_CASE. (Note that this is
unlike the tags for unions.)

## Structs

```
struct BrewCoffeeRequest {
  CoffeeType coffee_type;
  uint64 beans;
  double litres_of_water;
  double litres_of_milk;
  double kilos_of_sugar;
  string? customer_name;
  // Set to null to automatically allocate some cups
  uint64? cups;
};
```

```java
// Create an instance. You should not make any assumptions about the default
// state as it may be invalid.
BrewCoffeeRequest request = new BrewCoffeeRequest();
// Make sure to set all the fields. Each data member is public without setters
// or getters.
request.coffeeType = CoffeeType.LATTE;
request.beans = 30;
request.litresOfWater = 0.448;
request.litresOfMilk = 0.05;
request.kilosOfSugar = 0.002;
request.customerName = null;
request.cups = new Long(2);
```

As there are no getters or setters, there are no nullness checks when reading or
writing to the data members of a struct. The members of a newly constructed
struct object which are specified as non-nullable in the mojom may in fact be
null-initialized and need to be filled out before the struct is sent across
IPC. Serialization and deserialization when being sent or received across a Mojo
channel will both perform nullness checks. If you receive a struct over IPC, it
is guaranteed to comply with the nullability specified in the mojom file.

## Unions

```
union BrewCoffeeResponse {
  uint64 cups_of_coffee;
  string error_message;
};
```

```java
// Create an instance. You should not make any assumptions about the default
// value as it may be invalid.
BrewCoffeeResponse response = new BrewCoffeeResponse();
// Set it to the cups_of_coffee variant with a well-defined value.
response.setCupsOfCoffee(2);
// Alternatively, set it to the error_message variant.
response.setErrorMessage("I am a teapot");

// Reading a union
switch (response.which()) {
    case BrewCoffeeResponse.Tag.CupsOfCoffee:
        Log.i(TAG, "We got %d cups of coffee.\n", response.getCupsOfCoffee());
        break;
    case BrewCoffeeResponse.Tag.ErrorMessage:
        Log.e(TAG, "Could not brew coffee: %s\n.", response.getErrorMessage());
        break;
    default:
        // Your implementation is probably out of sync with the mojom.
        throw new IllegalArgumentException("Unknown tag");
}
```

The content of a newly constructed union object may be null-initialized, even if
the variant is specified as non-nullable in the mojom. Serialization and
deserialization when being sent or received across a Mojo channel will generally
both perform nullness checks. **(crbug.com/337849882: Invalid union data
received over IPC may deserialize into a union with a default-constructed
state. This may result in null content for a non-nullable variant.)**

When using a getter, the autogenerated mojo code will assert that the union has
the correct variant/tag. (This is [roughly equivalent to a DCHECK](/styleguide/java/java.md#asserts).)

Java Unions internally hold the data for separate variants under separate member
variables. These members are not cleared/nullified when other variants are set
and therefore may continue to hold references to any nested data under them.

The constants for the different tags/variants of a union are available under a
static Tag class under the union's generated Java class and use
PascalCase. (Note that this is unlike the variants for enums.)

# Interfaces

Consider the following interface used as an example in the following sections:

```
interface CoffeeMachine {
  const double kUsCupInLitres = 0.236588;
  const double kUsLegalCupInLitres = 0.24;
  const double kImperialCupInLitres = 0.284131;

  EnterLowPowerMode();
  PerformCleaningCycle() => ();
  BrewCoffee(BrewCoffeeRequest request) => (BrewCoffeeResponse response);
};
```

## Making IPC calls

```java
// Constant names are converted to Java style
Log.i(TAG, "There are %f US cups in 1 litre",
        1.0 / CoffeeMachine.US_CUP_IN_LITRES);

// coffeeMachine is of type CoffeeMachine.Proxy (which is itself an
// implementation of CoffeeMachine).

// Calling a method with no input, output, or even a completion callback.
coffeeMachine.enterLowPowerMode();

// Calling a method with no input or output, but that has a response callback
// for completion.
CoffeeMachine.PerformCleaningCycle_Response callback =
        new CoffeeMachine.PerformCleaningCycle_Response() {
    @Override
    public void call() {
        Log.i(TAG, "The cleaning cycle has finished");
    }
};
coffeeMachine.performCleaningCycle(callback);

// Calling a method with an input and an output.
CoffeeMachine.BrewCoffee_Response callback =
        new CoffeeMachine.BrewCoffee_Response() {
    @Override
    public void call(BrewCoffeeResponse response) {
        // Handle the result of the brew coffee request here.
    }
};
coffeeMachine.brewCoffee(brewCoffeeRequest, callback);
```

Calling these methods is asynchronous and will not block the caller.

Note that calling a method with legal inputs will not itself ever fail with an
exception if there is an IPC problem. Instead, you should set a
`ConnectionErrorHandler` on your Proxy objects if you wish to detect errors.

```java
// coffeeMachine is of type CoffeeMachine.Proxy
ConnectionErrorHandler connectionErrorHandler = new ConnectionErrorHandler() {
    @Override
    public void onConnectionError(MojoException e) {
        // Handle the error.
    }
};
coffeeMachine.getProxyHandler().setErrorHandler(connectionErrorHandler);
```

Once you are done with your Proxy object, assuming you haven't transferred it
across IPC, make sure to close it.

```java
coffeeMachine.close();
```

Holding onto an unclosed Proxy will generally keep an IPC connection alive
(unless the other side unilaterally closes it). If you forget to close it, you
may leak resources (on both sides of the IPC!), get warnings, or trigger
non-deterministic crashes.

## Receiving IPC calls

```java
class CoffeeMachineImpl implements CoffeeMachine {
    @Override
    public void onConnectionError(MojoException e) {
        // Handle an error here. Note that close() will also be called.
    }

    @Override
    public void close() {
        // Closed - either gracefully or through an error.
    }

    @Override
    public void enterLowPowerMode() {
        Log.i(TAG, "Zzz...");
    }

    @Override
    public void performCleaningCycle(
            CoffeeMachine.PerformCleaningCycle_Response callback) {
        Log.i(TAG, "Cleaned!");
        callback.call();
    }

    @Override
    public void brewCoffee(
            BrewCoffeeRequest request,
            CoffeeMachine.BrewCoffee_Response callback) {
        CoffeeMachine.BrewCoffeeResponse response =
                new CoffeeMachine.BrewCoffeeResponse();
        response.setErrorMessage("I am a teapot");
        callback.call(response);
    }
}
```

When implementing a receiver for your interface, you will need to override the
`onConnectionError(MojoException)` and `close()` methods in addition to your
mojom-defined methods.

Whilst it's technically possible for a single implementation object to be bound
as the handler for multiple channels or receivers, you should avoid this as the
`onConnectionError(MojoException)` and `close()` methods do not provide any
explicit information about which channel their calls relate to. It's also
conceptually confusing if you continue to use a `Closeable` that may have been
closed.

Note that a Mojo connection that's bound to an implementation object will keep
that implementation object alive. However, your implementation object has no
inherent influence over the lifetime of the connection (after all, you're just
implementing an interface).

# Registering, mapping, binding, and passing interfaces

## Working with interface brokers

Interface brokers are how the renderer process usually obtains access to an IPC
interface. In Java, this will typically bind the receiver implementations for
you.

See a few examples from [code search](https://source.chromium.org/search?q=language:java%20class:InterfaceRegistrar&sq=).
There are a few examples for general registrars for Android WebView and Chrome
on Android. You will likely be able to re-use one of these for your purposes.

Adding your implementation to the binder mapping happens in the normal C++
`*_bindings.cc` or `*_binders.cc` files, which are subject to IPC security
reviews. The mapping code will route over to the Java implementation. An
`InterfaceRegistrar`, `InterfaceRegistry`, and a factory for your implementation
class is typically used to perform the binding of an implementation to an
interface that's obtained via the browser interface broker.

Much as how different layers or components use different `*_bindings.cc` and
`*_binders.cc` files to expose your IPC implementation via interface brokers,
there are also different interface registrars. Sometimes, their
approaches/implementations can be a little inconsistent. Choosing which
interface registrar to use, or creating an entirely new one, is dependent on the
use case and is outside of the scope of this document. We will only briefly look
at adding a hypothetical blink-defined "CoffeeMachine" interface to Android
WebView as an example.

`//android_webview/browser/aw_content_browser_client_receiver_bindings.cc`:

```c++
template <typename Interface>
void ForwardToJavaFrame(content::RenderFrameHost* render_frame_host,
                        mojo::PendingReceiver<Interface> receiver) {
  render_frame_host->GetJavaInterfaces()->GetInterface(std::move(receiver));
}

void BindCoffeeMachineReceiver(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::CoffeeMachine> receiver) {
  // Optional: if you wanted to perform any pre-binding checks, such as to
  // make sure that the origin is secure, etc, now is the time to do it:
  const url::Origin& origin = render_frame_host->GetLastCommittedOrigin();
  if (!network::IsOriginPotentiallyTrustworthy(origin)) {
    mojo::ReportBadMessage(
        "Attempted to access CoffeeMachine from non-trustworthy origin.");
    return;
  };

  ForwardToJavaFrame<blink::mojom::CoffeeMachine>(
      render_frame_host, std::move(receiver));
}

void AwContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<blink::mojom::CoffeeMachine>(
      base::BindRepeating(&BindCoffeeMachineReceiver));
  // ... Mappings for other interfaces
}
```

`//android_webview/java/src/org/chromium/android_webview/AwInterfaceRegistrar.java`:

```java
class AwInterfaceRegistrar {
    @CalledByNative
    private static void registerMojoInterfaces() {
        InterfaceRegistrar.Registry.addRenderFrameHostRegistrar(
                new AndroidWebviewRenderFrameHostInterfaceRegistrar());
    }

    private static class AndroidWebviewRenderFrameHostInterfaceRegistrar
            implements InterfaceRegistrar<RenderFrameHost> {
        @Override
        public void registerInterfaces(
                InterfaceRegistry registry,
                RenderFrameHost renderFrameHost) {
            // A made up example:
            registry.addInterface(
                    CoffeeMachineImpl.MANAGER,
                    new CoffeeMachineImplFactory(renderFrameHost));
            // ... Calls for other interfaces.
        }
    }
}
```

`CoffeeMachineImplFactory.java`:

```java
public class CoffeeMachineImplFactory implements InterfaceFactory {
    private final RenderFrameHost mRenderFrameHost;

    public CoffeeMachineFactory(RenderFrameHost renderFrameHost) {
        mRenderFrameHost = renderFrameHost;
    }

    @Override
    public CoffeeMachineImpl createImpl() {
        return new CoffeeMachineImpl(mRenderFrameHost);
    }
}
```

However, if you're passing pending_receivers around as part of the IPC calls of
your interfaces, you will need to do the binding yourself, as described in the
next sections.





## Passing interfaces through IPC calls (pending_receivers and pending_remotes)

Consider the following interfaces used as an example in the following sections:

```
interface CoffeeMachine {
  BrewCoffee(BrewCoffeeRequest request) => (BrewCoffeeResponse response);
  // ...
};

interface Employee {
  ProvideCoffeeAccess(pending_remote<CoffeeMachine> coffee_machine);
};

interface Kitchen {
  RequestCoffeeAccess(pending_receiver<CoffeeMachine> coffee_machine);
};
```

### Receiving a pending_receiver and binding to it

There is a bit of a terminology mismatch in Java compared to C++/mojom where
Java uses some older phrasing. Notably, pending_receivers are called
"InterfaceRequests" in Java (this actually used to be the old term used in other
bindings like C++). Once received on the Java side, these can then be bound to
an implementation.

```java
class KitchenImpl implements Kitchen {
    // ...
    @Override
    public void requestCoffeeAccess(
            InterfaceRequest<CoffeeMachine> interfaceRequest) {
        CoffeeMachine coffeeMachine = new CoffeeMachineImpl();
        CoffeeMachine.MANAGER.bind(coffeeMachine, interfaceRequest);
    }
}
```

If you do not intend to bind an implementation to an InterfaceRequest and
instead wish to just discard it, you should `close()` it.

### Receiving a pending_remote and using it

Pending remotes are automatically bound and converted to Proxy objects when
received.

```java
class EmployeeImpl implements Employee {
    // ...
    @Override
    public void provideCoffeeAccess(CoffeeMachine coffeeMachine) {
        // We can immediately use the remote/proxy
        coffeeMachine.brewCoffee(mMyUsualPlease, mDrinkCoffeeCallback);
        coffeeMachine.close();

        // Or, we could pass the proxy onto somewhere else, and it will be
        // unbound from the proxy and converted from a remote back into a
        // pending remote.
        mColleague.provideCoffeeAccess(coffeeMachine);
        // Do not close() if passed elsewhere.
    }
}
```

If you are finished with the remote/proxy (and have not passed it on elsewhere),
you should `close()` it.

### Creating and sending pending_receivers and pending_remotes

You can create a connected pair of an `InterfaceType.Proxy` and an
`InterfaceRequest<InterfaceType>`. You can then either use these yourself or
send them over IPC.

```java
// import org.chromium.mojo.system.Pair;
// import org.chromium.mojo.system.impl.CoreImpl;

Pair<CoffeeMachine.Proxy, InterfaceRequest<CoffeeMachine>> pair =
        CoffeeMachine.MANAGER.getInterfaceRequest(CoreImpl.getInstance());

// Both the proxy and the interface request can used by us or be sent over IPC.
// Note that calls can be queued via the proxy/remote even before the
// pending_receiver/interface request is bound to an implementation.
employee.provideCoffeeAccess(pair.first);
kitchen.requestCoffeeAccess(pair.second);
```

There is a shortcut if you want to provide the other side of the IPC with a
remote to an implementation you own. Instead of creating a pair of a Proxy and
an InterfaceRequest, you can directly supply your implementation object instead
of a proxy and Mojo will create, bind, and send a pending remote for you:

```java
CoffeeMachine coffeeMachine = new CoffeeMachineImpl()
employee.provideCoffeeAccess(coffeeMachine);
```

Note that whether you send a pending_remote one way or send a pending_receiver
the other way can have an effect on ownership, lifetimes, and performance. It
may be better to supply a pending_receiver as an argument to a method call than
to wait for a pending_remote from the response callback - especially if you want
to queue up messages before all binding has taken place.

# Threading

When receiving IPC calls, Mojo will invoke your interface implementation's
methods on the thread/sequence on which the implementation was bound. This also
extends to the `close` and `onConnectionError` methods. (This is similar to how
things work in C++.)

You should generally only use the Mojo APIs from within a valid sequence, though
purely manipulating mojom-generated Java data types (structs, enums, unions) is
OK. Various Mojo method calls will crash if invoked from outside of a valid
sequence.

Note that ordinary Proxy objects are not thread safe and should generally only
be used from the thread on which they are created. However, you can create a
thread safe wrapper around your proxy object using
`InterfaceType.MANAGER.buildThreadSafeProxy(originalProxy)`, which will forward
calls to the thread on which the thread-safe proxy wrapper was created. You must
therefore build the thread-safe proxy from the thread which owns the original
proxy.
