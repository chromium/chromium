# TypedStatus<T>

The purpose of TypedStatus is to provide a thin wrapper around return-value
enums that support causality tracking, data attachment, and general assistance
with debugging, without adding slowdowns due to returning large structs,
pointers, or more complicated types.

TypedStatus<T> should be specialized with a traits struct that defines:

  Codes - enum (usually enum class) that would be the return type, if we weren't
          using TypedStatus.
  static constexpr StatusGroupType Group() { return "NameOfStatus"; }
  static constexpr absl::optional<Codes> DefaultEnumValue() {
    return Codes::kCodeThatShouldBeSuperOptimizedEGSuccess;
    // Can return nullopt to optimize none of them.  No idea why you'd do that.
  }

Typically one would:

  struct MyStatusTraits { ... };
  using MyStatus = TypedStatus<MyStatusTraits>;

## Using an existing `TypedStatus<T>`

The current canonical TypedStatus is called `Status` for historical reasons,
though that will soon change.

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
// and reciever of the TypedStatus.
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
(copying the desciptive comments is not suggested)

```c++
struct MyExampleStatusTraits {
  // If you do not have an existing enum, you can `enum class Codes { ... };`
  // here, instead of `using`.
  using Codes = MyExampleEnum;
  static constexpr StatusGroupType Group() { return "MyExampleStatus"; }
  static constexpr absl::optional<Codes> { return Codes::kDefaultValue; }
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

For the common case where you'd like to return some constructed thing OR
an error type, we've also created `TypedStatus<T>::Or<D>`.

The `TypedStatus<T>::Or<D>` type can be constructed implicitly with either
a `TypedStatus<T>`, a `T`, or a `D`.

This type has methods:
```c++
bool has_value() const;
bool has_error() const;

// Return the error, if we have one.
// Callers should ensure that this `has_error()`.
TypedStatus<T> error() &&;

// Return the value, if we have one.
// Callers should ensure that this `has_value()`.
OtherType value() &&;

// It is invalid to call `code()` on an `Or<D>` type when
// has_value() is true and TypedStatusTraits<T>::DefaultEnumValue is nullopt.
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



## Design decisions
See go/typedstatus for design decisions.
