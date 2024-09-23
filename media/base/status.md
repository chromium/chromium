# TypedStatus<T>

The purpose of TypedStatus is to provide a thin wrapper around return-value
enums that support causality tracking, data attachment, and general assistance
with debugging, without adding slowdowns due to returning large structs,
pointers, or more complicated types.

A use-every-feature example:
```c++
struct MyExampleStatusTraits {
  // [REQUIRED] Declare your enum
  enum class Codes : StatusCodeType {
    kSomething = 9090,
    kAnotherThing = 92,
    kAThirdThing = 458,
    kAFinalThing = 438,
  };

  // [REQUIRED] Declare your group name
  static constexpr StatusGroupType Group() { return "MyExampleStatus"; }

  // [OPTIONAL] Declare your "default" code. If this method is defined,
  // then the function OkStatus() can be used to return a status with this
  // code. Statuses created with this default code can not have any data,
  // causes, or a message attached.
  static constexpr Codes OkEnumValue() { return Codes::kSomething; }

  // [OPTIONAL] If |OnCreateFrom| is declared, then TypedStatus<T> can be
  // created with {T::Codes, SomeOtherType} or {T::Codes, string, SomeOtherType}
  // The pre-created TypedStatus is passed into this method for additional
  // manipulation.
  static void OnCreateFrom(TypedStatus<MyExampleStatusTraits>* impl,
                           const SomeOtherType& t) {
    impl->WithData("key", SomeOtherTypeToString(t));
  }

  // [OPTIONAL] If you'd like to be able to send your status to UKM, declare
  // this method in your traits. This allows you to pack any part of the
  // status internal data into a single ukm-ready uint32.
  static uint32_t PackExtraData(const internal::StatusData& data) {
    return 0;
  }
};

// Typically, you'd want to redefine your template instantiation, like this.
using MyExampleStatus = TypedStatus<MyExampleStatusTraits>;

```


## Using an existing `TypedStatus<T>`

All TypedStatus specializations have the following common API:
```c++
// The underlying code value.
T::Codes code() const;

// The underlying message.
std::string& message() const;

// Adds the current file & line number to the trace.
TypedStatus<T>&& AddHere() &&;

// Adds some named data to the status, such as a platform
// specific error value, ie: HRESULT. This data is for human consumption only
// in a developer setting, and can't be extracted from the TypedStatus
// normally. The code value should be sufficiently informative between sender
// and receiver of the TypedStatus.
template<typename D>
TypedStatus<T>&& WithData(const char *key, const D& value) &&;
template<typename D>
void WithData(const char *key, const D& value) &;

// Adds a "causal" status to this one.
// The type `R` will not be retained, and similarly with the data methods,
// `cause` will only be used for human consumption, and cannot be extracted
// under normal circumstances.
template<typename R>
TypedStatus<T>&& AddCause(TypedStatus<R>&& cause) &&;
template<typename R>
void AddCause(TypedStatus<R>&& cause) &;
```


## Quick usage guide

If you have an existing enum, and would like to wrap it:
```c++
enum class MyExampleEnum : StatusCodeType {
  kDefaultValue = 1,
  kThisIsAnExample = 2,
  kDontArgueInTheCommentSection = 3,
};
```

Define an |TypedStatusTraits|, picking a name for the group of codes:
(copying the descriptive comments is not suggested)

```c++
struct MyExampleStatusTraits {
  using Codes = MyExampleEnum;
  static constexpr StatusGroupType Group() { return "MyExampleStatus"; }
  static constexpr Codes OkEnumValue() { return Codes::kDefaultValue; }
}
```

Bind your typename:
```c++
using MyExampleStatus = media::TypedStatus<MyExampleStatusTraits>;
```

Use your new type:
```c++
MyExampleStatus Foo() {
  return MyExampleStatus::Codes::kThisIsAnExample;
}

int main() {
  auto result = Foo();
  switch(result.code()) {
    case MyExampleStatus::Codes::...:
      break;
    ...
  }
}
```

## Constructing a TypedStatus<T>
There are several ways to create a typed status, depending on what data you'd
like to encapsulate:

```
// To create an status with the default OK type, there's a helper function that
// creates any type you want, so long as it actually has a kOk value or
|OkEnumValue| implementation.
TypedStatus<MyType> ok = OkStatus();

// A status can be implicitly created from a code
TypedStatus<MyType> status = MyType::Codes::kMyCode;

// A status can be explicitly created from a code and message, or implicitly
// created from a brace initializer list of code and message
TypedStatus<MyType> status(MyType::Codes::kMyCode, "MyMessage");
TypedStatus<MyType> status = {MyType::Codes::kMyCode, "MyMessage"};

// If |MyType::OnCreateFrom<T>| is implemented, then a status can be created
// from a {code, T} pack, or a {code, message, T} pack:
TypedStatus<MyType> status = {MyType::Codes::kMyCode, 667};
TypedStatus<MyType> status = {MyType::Codes::kMyCode, "MyMessage", 667};

// A status can be created from packs of either {code, TypedStatus<Any>} or
// {code, message, TypedStatus<Any>} where TypedStatus<Any> will become the
// status that causes the return. Note that in this example,
// OtherType::Codes::kOther is itself being implicitly converted from a code
// to a TypedStatus<OtherType>.
TypedStatus<MyType> status = {MyType::Codes::kCode, OtherType::Codes::kOther};
TypedStatus<MyType> status = {MyType::Codes::kCode, "M", OtherType::Codes::kOther};
```



## TypedStatus<T>::Or<D>

For the common case where you'd like to return some constructed thing OR
an error type, we've also created `TypedStatus<T>::Or<D>`.

The `TypedStatus<T>::Or<D>` type can be constructed implicitly with either
a `TypedStatus<T>`, a `T`, or a `D`.

This type has methods:
```c++
bool has_value() const;

// Return the error, if we have one.
// Callers should ensure that this `!has_value()`.
TypedStatus<T> error() &&;

// Return the value, if we have one.
// Callers should ensure that this `has_value()`.
OtherType value() &&;

// It is invalid to call `code()` on an `Or<D>` type when
// has_value() is true and TypedStatusTraits<T>::OkEnumValue is nullopt.
T::Codes code();
```

Example usage:
```c++
MyExampleStatus::Or<std::unique_ptr<VideoDecoder>> CreateAndInitializeDecoder() {
  std::unique_ptr<VideoDecoder> decoder = decoder_factory_->GiveMeYourBestDecoder();
  auto init_status = decoder->Initialize(init_args_);
  // If the decoder initialized successfully, then just return it.
  if (init_status == InitStatusCodes::kOk)
    return std::move(decoder);
  // Otherwise, return a MediaExampleStatus caused by the init status.
  return MyExampleStatus(MyExampleEnum::kDontArgueInTheCommentSection).AddCause(
    std::move(init_status));
}

int main() {
  auto result = CreateAndInitializeDecoder();
  if (result.has_value())
    decoder_loop_->SetDecoder(std::move(result).value());
  else
    logger_->SendError(std::move(result).error());
}

```


## Testing
There are some helper matchers defined in test_helpers.h that can help convert
some of the trickier method expectations. For example, this:
```
EXPECT_CALL(object_, Foo(kExpectedCode));
```
becomes:
```
EXPECT_CALL(object_, Foo(HasStatusCode(kExpectedCode)));
```
The EXPECT_CALL macro won't test for overloaded operator== equality here, so
|HasStatusCode| is a matcher macro that allows checking if the expected status
has the matching error code.


## Additional setup for mojo

If you want to send a specialization of TypedStatus over mojo,
add the following to media_types.mojom:

```
struct MyExampleEnum {
  StatusBase? internal;
};
```

And add the following to media/mojo/mojom/BUILD.gn near the `StatusData` type
binding.

```
{
  mojom = "media.mojom.MyExampleEnum",
  cpp = "::media::MyExampleEnum"
},
```


## UKM & data-recording
TypedStatus is designed to be easily reported to UKM. A status is represented
by 16-bit hash of the group name, the 16-bit code, and 32 bits of extra data.
Any implementation of TypedStatus can define a |PackExtraData| method in the
traits struct which can operate on internal data and pack it into 32 bits.
For example, a TypedStatus which might often have wrapped HRESULTs might look
like this:
```c++
struct MyExampleStatusTraits {
  // If you do not have an existing enum, you can `enum class Codes { ... };`
  // here, instead of `using`.
  using Codes = MyExampleEnum;
  static constexpr StatusGroupType Group() { return "MyExampleStatus"; }
  static constexpr Codes OkEnumValue() { return Codes::kDefaultValue; }
  static uint32_t PackExtraData(const StatusData& info) {
    std::optional<int> hresult = info.data.GetIntValue("HRESULT");
    return static_cast<uint32_t>(hresult.has_value() ? *hresult : 0);
  }
}
```


## Design decisions
See go/typedstatus for design decisions.
