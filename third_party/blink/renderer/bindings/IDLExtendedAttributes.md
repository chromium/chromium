# Blink IDL Extended Attributes

[TOC]

## Introduction

The main interest in extended attributes are their _semantics_: Blink implements many more extended attributes than the Web IDL standard, to specify various behavior.

The authoritative list of allowed extended attributes and values is [bindings/scripts/validator/rules/supported_extended_attributes.py](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/validator/rules/supported_extended_attributes.py). This is complete but not necessarily precise (there may be unused extended attributes or values), since validation is run on build, but coverage isn't checked.

Syntactically, Blink IDL extended attributes differ from standard Web IDL extended attributes in a few ways:

* the value of a key=value pair can be a string literal, not just an identifier: `key="foo"` or `key=("foo","bar")`

Blink IDL also does not support certain recent features of the Web IDL grammar:

* Values that are not identifiers or strings are _not_ supported (the `Other` production): any non-identifier should simply be quoted (this could be changed to remove the need for quotes, but requires rather lengthy additions to the parser).

Semantically, only certain extended attributes allow lists. Similarly, only certain extended attributes allow string literals.

Extended attributes either take no value, take a required value, or take an optional value.

As a rule, we do _not_ add extended attributes to the IDL that are not supported by the compiler (and are thus nops). This is because it makes the IDL misleading: looking at the IDL, it looks like it should do something, but actually doesn't, which is opaque (it requires knowledge of compiler internals). Instead, please place a comment on the preceding line, with the desired extended attribute and a TODO referring to the relevant bug. For example (back when [Bug 358506](https://crbug.com/358506) was open):

```webidl
// TODO(crbug.com/358506): should be [MeasureAs=Foo] but [MeasureAs] not supported on constants.
const unsigned short bar;
```

## Naming

Extended attributes are named in UpperCamelCase, and are conventionally written as the name of the attribute within brackets, as `[ExampleExtendedAttribute]`, per [Web IDL typographic conventions](https://webidl.spec.whatwg.org/#conventions).

There are a few rules in naming extended attributes:

Names should be aligned with the Web IDL spec as much as possible.
Lastly, please do not confuse "_extended_ attributes", which go inside `[...]` and modify various IDL elements, and "attributes", which are of the form `attribute foo` and are interface members.

## Special cases

### Overloaded methods

Extended attributes mostly work normally on overloaded methods, affecting only that method (not other methods with the same name), but there are a few exceptions, due to all the methods sharing a single callback.

`[RuntimeEnabled]` works correctly on overloaded methods, both on individual overloads or (if specified on all method with that name) on the entire method ([Bug 339000](https://crbug.com/339000)).

*** note
While `[DeprecateAs]`, `[MeasureAs]` only affect callback for non-overloaded methods, the logging code is instead put in the method itself for overloaded methods, so these can be placed on the method to log in question.
***

### Special operations (methods)

Extended attributes on special operations (methods) are largely the same as those on methods generally, though many fewer are used.

Anonymous special operations default to being implemented by a function named `anonymousIndexedGetter` etc.

`[ImplementedAs]` can be used if there is an existing Blink function to use to implement the operation, but you do _not_ wish to expose a named operation. Otherwise you can simply put the special keyword in the line of the exposed normal operation.

For example:

```webidl
[ImplementedAs=item] getter DOMString (unsigned long index);  // Does not add item() to the interface: only an indexed property getter
```

or:

```webidl
getter DOMString item(unsigned long index);  // Use existing method item() to implement indexed property getter
```

`[ImplementedAs]` is also useful if you want an indexed property or named property to use an _attribute_. For example:

```webidl
attribute DOMString item;
[ImplementedAs=item] getter DOMString (unsigned long index);
[ImplementedAs=setItem] setter DOMString (unsigned long index);
```

There is one interface extended attribute that only affects special operations: `[LegacyOverrideBuiltIns]`.

The following extended attributes are used on special operations, as on methods generally: `[RaisesException]`.

### Partial interfaces

Extended attributes on partial interface members work as normal.

`[RuntimeEnabled]`, etc. are used to allow the entire partial interface to be selectively enabled or disabled, and function as if the extended attribute were applied to each _member_ (methods, attributes, and constants). Style-wise, if the entire partial interface should be enabled or disabled, these extended attributes should be used on the partial interface, not on each individual member; this clarifies intent and simplifies editing. However:

* If some members should not be disabled, this cannot be used on the partial interface; this is often the case for constants.
* If different members should be controlled by different flags, this must be specified individually.
* If a flag obviously applies to only one member of a single-member interface (i.e., it is named after that member), the extended attribute should be on the member.

The extended attribute `[ImplementedAs]` is mandatory. A partial
interface must have `[ImplementedAs]` extended attribute to specify the C++ class that includes the required static methods.
This may be a static-only class, or for cases where a single static method is a simple getter for an object, that object's
class may implement the required static method.

### interface mixins

Extended attributes on members of an interface mixin work as normal.

* `[ImplementedAs]` is only necessary for these legacy files: otherwise the class (C++) implementing (IDL) interface mixins does not need to be specified, as this is handled in Blink C++.

* `[RuntimeEnabled]` behaves as for partial interfaces.

### Inheritance

Extended attributes are generally not inherited: only extended attributes on the interface itself are consulted. However, there are a handful of extended attributes that are inherited (applying them to an ancestor interface applies them to the descendants). These are extended attributes that affect memory management, and currently consists of `[ActiveScriptWrappable]`.

## Standard Web IDL Extended Attributes

These are defined in the [ECMAScript-specific extended attributes](https://webidl.spec.whatwg.org/#es-extended-attributes) section of the [Web IDL spec](https://webidl.spec.whatwg.org/), and alter the binding behavior.

### [AllowShared]

Standard: [AllowShared](https://webidl.spec.whatwg.org/#AllowShared)

Summary: `[AllowShared]` indicates that a parameter, which must be an ArrayBufferView (or subtype of, e.g. typed arrays), is allowed to be backed by a SharedArrayBuffer. It also indicates that an ArrayBuffer parameter allows a SharedArrayBuffer to be passed.

Usage: `[AllowShared]` must be specified on a parameter to a method:

```webidl
interface Context {
    void bufferData1([AllowShared] ArrayBufferView buffer);
    void bufferData2([AllowShared] Float32Array buffer);
    void bufferData3([AllowShared] ArrayBuffer buffer);
}
```

A SharedArrayBuffer is a distinct type from an ArrayBuffer, but both types use ArrayBufferViews to view the data in the buffer. Most methods do not permit an ArrayBufferView that is backed by a SharedArrayBuffer, and will throw an exception. This attribute indicates that this method permits a shared ArrayBufferView.

When applied to an ArrayBuffer argument, the underlying C++ method called by the bindings receives a `DOMArrayBufferBase*` instead of `DOMArrayBuffer*`.

### [CEReactions]

Standard: [CEReactions](https://html.spec.whatwg.org/C/#cereactions)

Summary: `[CEReactions]` indicates that
[custom element reactions](https://html.spec.whatwg.org/C/#concept-custom-element-reaction)
are triggered for this method or attribute.

Usage: `[CEReactions]` takes no arguments.

Note that `blink::CEReactionsScope` must be constructed after `blink::ExceptionState`.

### [Clamp]

Standard: [Clamp](https://webidl.spec.whatwg.org/#Clamp)

Summary: `[Clamp]` indicates that when an ECMAScript Number is converted to the IDL type, out of range values will be clamped to the range of valid values, rather than using the operators that use a modulo operation (ToInt32, ToUint32, etc.).

Usage: The `[Clamp]` extended attribute MUST appear on an integer type.

```webidl
interface XXX {
    attribute [Clamp] unsigned short attributeName;
};
```

Annotated type with `[Clamp]` can be specified on extended attributes or methods arguments:

```webidl
interface GraphicsContext {
    void setColor(octet red, octet green, octet blue);
    void setColorClamped([Clamp] octet red, [Clamp] octet green, [Clamp] octet blue);
};
```

Calling the non-`[Clamp]` version of `setColor()` uses **ToUint8()** to coerce the Numbers to octets. Hence calling `context.setColor(-1, 255, 257)` is equivalent to calling `setColor(255, 255, 1)`.

Calling the `[Clamp]` version of `setColor()` uses **clampTo()** to coerce the Numbers to octets. Hence calling `context.setColor(-1, 255, 257)` is equivalent to calling `setColorClamped(0, 255, 255)`.

### [CrossOriginIsolated]

Standard: [CrossOriginIsolated](https://webidl.spec.whatwg.org/#CrossOriginIsolated)

Summary: Interfaces and interface members with a `CrossOriginIsolated` attribute are exposed only inside contexts whose [cross-origin isolated capability](https://html.spec.whatwg.org/multipage/webappapis.html#concept-settings-object-cross-origin-isolated-capability) is enabled.

Usage: The `[CrossOriginIsolated]` attribute may be specified on interfaces, attributes, and members:

```webidl
[CrossOriginIsolated]
interface HighResolutionTimer {
  DOMHighResTimeStamp getHighResolutionTime();
};
```

### [EnforceRange]

Standard: [EnforceRange](https://webidl.spec.whatwg.org/#EnforceRange)

Summary: `[EnforceRange]` indicates that when an ECMAScript Number is converted to the IDL type, out of range values will result in a TypeError exception being thrown.

Usage: The `[EnforceRange]` extended attribute MUST appear on an integer type.

```webidl
interface XXX {
    attribute [EnforceRange] unsigned short attributeName;
};
```

Annotated type with `[EnforceRange]` can be specified on extended attributes on methods arguments:

```webidl
interface GraphicsContext {
    void setColor(octet red, octet green, octet blue);
    void setColorEnforced([EnforceRange] octet red, [EnforceRange] octet green, [EnforceRange] octet blue);
};
```

Calling the non-`[EnforceRange]` version of `setColor()` uses **ToUint8()** to coerce the Numbers to octets. Hence calling `context.setColor(-1, 255, 257)` is equivalent to calling `setColor(255, 255, 1)`.

Calling the `[EnforceRange]` version of `setColorEnforced()` with an out of range value, such as -1, 256, or Infinity will result in a `TypeError` exception.

### [Exposed]

Standard: [Exposed](https://webidl.spec.whatwg.org/#Exposed)

Summary: Indicates on which global object or objects (e.g., Window, WorkerGlobalScope) the interface property is generated, i.e., in which global scope or scopes an interface exists. This is primarily of interest for the constructor, i.e., the [interface object Call method](https://webidl.spec.whatwg.org/#es-interface-call). If `[Exposed]` is not present or overridden by a standard extended attribute `[LegacyNoInterfaceObject]` (the value of the property on the global object corresponding to the interface is called the **interface object**), which results in no interface property being generated.

As with `[LegacyNoInterfaceObject]` does not affect generated code for the interface itself, only the code for the corresponding global object. A partial interface is generated at build time, containing an attribute for each interface property on that global object.

All non-callback interfaces without `[LegacyNoInterfaceObject]` have a corresponding interface property on the global object. Note that in the Web IDL spec, callback interfaces with constants also have interface properties, but in Blink callback interfaces only have methods (no constants or attributes), so this is not applicable. `[Exposed]` can be used with different values to indicate on which global object or objects the property should be generated. Valid values are:

* `Window`
* [Worker](https://www.whatwg.org/specs/web-apps/current-work/multipage/workers.html#the-workerglobalscope-common-interface)
* [SharedWorker](https://www.whatwg.org/specs/web-apps/current-work/multipage/workers.html#shared-workers-and-the-sharedworkerglobalscope-interface)
* [DedicatedWorker](https://www.whatwg.org/specs/web-apps/current-work/multipage/workers.html#dedicated-workers-and-the-dedicatedworkerglobalscope-interface)
* [ServiceWorker](https://rawgithub.com/slightlyoff/ServiceWorker/master/spec/service_worker/index.html#service-worker-global-scope)

For reference, see [ECMAScript 5.1: 15.1 The Global Object](http://www.ecma-international.org/ecma-262/5.1/#sec-15.1) ([annotated](http://es5.github.io/#x15.1)), [HTML: 10 Web workers](http://www.whatwg.org/specs/web-apps/current-work/multipage/workers.html), [Web Workers](http://dev.w3.org/html5/workers/), and [Service Workers](https://rawgithub.com/slightlyoff/ServiceWorker/master/spec/service_worker/index.html) specs.

It is possible to have the global constructor generated on several interfaces by listing them, e.g. `[Exposed=(Window,WorkerGlobalScope)]`.

Usage: `[Exposed]` can be specified on interfaces that do not have the `[LegacyNoInterfaceObject]` extended attribute.

```webidl
[
    Exposed=DedicatedWorker
] interface XXX {
    ...
};

[
    Exposed=(Window,Worker)
] interface YYY {
    ...
};
```

Exposed can also be specified with a method, attribute and constant.

As a Blink-specific extension, we allow `Exposed(Arguments)` form, such as `[Exposed(Window Feature1, DedicatedWorker Feature2)]`. You can use this form to vary the exposing global scope based on runtime enabled features. For example, `[Exposed(Window Feature1, Worker Feature2)]` exposes the qualified element to Window if "Feature1" is enabled and to Worker if "Feature2" is enabled.

### [Global]

Standard: [Global](https://webidl.spec.whatwg.org/#Global)

Summary: The `[Global]` extended attribute can be used to give a name to one or more global interfaces, which can then be referenced by the `[Exposed]` extended attribute.

This extended attribute must either take an identifier or take an identifier list.

The identifier argument or identifier list argument the `[Global]` extended attribute is declared with define the interface's global names

### [HTMLConstructor]

Standard: [HTMLConstructor](https://html.spec.whatwg.org/C/#html-element-constructors)

Summary: HTML Elements have special constructor behavior. Interface object of given interface with the `[HTMLConstructor]` attribute will have specific behavior when called.

Usage: Must take no arguments, and must not appear on anything other than an interface. It must appear once on an interface, and the interface cannot be annotated with `[Constructor]` or `[LegacyNoInterfaceObject]` extended attributes. It must not be used on a callback interface.

### [LegacyFactoryFunction]

Standard: [LegacyFactoryFunction](https://webidl.spec.whatwg.org/#LegacyFactoryFunction)

Summary: If you want to allow JavaScript to create a DOM object of XXX using a different name constructor (i.e. allow JavaScript to create an XXX object using "new YYY()", where YYY != XXX), you can use `[LegacyFactoryFunction]`.

Usage: The possible usage is `[LegacyFactoryFunction=YYY(...)]`. Just as with constructors. `[LegacyFactoryFunction]` can be specified on interfaces. The spec allows multiple legacy factory functions, but the Blink IDL compiler currently only supports at most one.

```webidl
[
    LegacyFactoryFunction=Audio(DOMString data),
] interface HTMLAudioElement {
    ...
};
```

The semantics are the same as constructors, except that the name changes: JavaScript can make a DOM object by `new Audio()` instead of by `new HTMLAudioElement()`.

Whether you should allow an interface to have a legacy factory function or not depends on the spec of each interface.

### [LegacyLenientSetter]

Standard: [LegacyLenientSetter](https://webidl.spec.whatwg.org/#LenientSetter)

Summary: `[LegacyLenientSetter]` indicates that a no-op setter will be generated for a readonly attribute’s accessor property. This results in erroneous assignments to the property in strict mode to be ignored rather than causing an exception to be thrown.

`[LegacyLenientSetter]` must take no arguments, and must not appear on anything other than a readonly regular attribute.

### [LegacyNoInterfaceObject]

Standard: [LegacyNoInterfaceObject](https://webidl.spec.whatwg.org/#NoInterfaceObject)

Summary: If the `[LegacyNoInterfaceObject]` extended attribute appears on an interface, it indicates that an interface object will not exist for the interface in the ECMAScript binding. See also the standard `[Exposed=xxx]` extended attribute; these two do _not_ change the generated code for the interface itself.

Note that every interface has a corresponding property on the ECMAScript global object, _except:_

* callback interfaces with no constants, and
* non-callback interface with the `[LegacyNoInterfaceObject]` extended attribute,

Usage: `[LegacyNoInterfaceObject]` can be specified on interfaces.

```webidl
[
    LegacyNoInterfaceObject
] interface XXX {
    ...
};
```

Note that `[LegacyNoInterfaceObject]` **MUST** be specified on testing interfaces, as follows:

```webidl
[
    LegacyNoInterfaceObject  // testing interfaces do not appear on global objects
] interface TestingInterfaceX {
    ...
};
```

### [LegacyNullToEmptyString]

Standard: [LegacyNullToEmptyString](https://webidl.spec.whatwg.org/#LegacyNullToEmptyString)

Summary: `[LegacyNullToEmptyString]` indicates that a JavaScript null is converted to `""` instead of `"null"`.

Usage: `[LegacyNullToEmptyString]` must be specified on a DOMString type.

```webidl
attribute [LegacyNullToEmptyString] DOMString str;
void func([LegacyNullToEmptyString] DOMString str);
```

Implementation: Given `[LegacyNullToEmptyString]`, a JavaScript null is converted to a Blink empty string, for which `String::IsEmpty()` returns true, but `String::IsNull()` return false.

### [LegacyOverrideBuiltIns]

Standard: [LegacyOverrideBuiltIns](https://webidl.spec.whatwg.org/#LegacyOverrideBuiltIns)

Summary: Affects named property operations, making named properties shadow built-in properties of the object.

### [LegacyUnenumerableNamedProperties]

Standard: [LegacyUnenumerableNamedProperties](https://webidl.spec.whatwg.org/#LegacyUnenumerableNamedProperties)

Summary: If an IDL interface [supports named properties](https://webidl.spec.whatwg.org/#dfn-support-named-properties), this extended attribute causes those properties not to be enumerable.

```webidl
[
    LegacyUnenumerableNamedProperties
] interface HTMLCollection {
    ...
    getter Element? namedItem(DOMString name);
}
```

In the example above, named properties in `HTMLCollection` instances (such as those returned by `document.getElementsByTagName()`) are not enumerable. In other words, `for-in` loops do not iterate over them, they are not listed by `Object.keys()` calls and the property descriptor returned by `Object.getPropertyDescriptor()` has its `enumerable` property set to `false`.

The `[LegacyUnenumerableNamedProperties]` extended attribute must be used **only** in interfaces that support named properties.

### [LegacyUnforgeable]

Standard: [LegacyUnforgeable](https://webidl.spec.whatwg.org/#Unforgeable)

Summary: Makes interface members unconfigurable and also controls where the member is defined.

Usage: Can be specified on interface methods or non-static interface attributes:

```webidl
[LegacyUnforgeable] void func();
[LegacyUnforgeable] attribute DOMString str;
```

By default, interface members are configurable (i.e. you can modify a property descriptor corresponding to the member and also you can delete the property). `[LegacyUnforgeable]` makes the member unconfiguable so that you cannot modify or delete the property corresponding to the member.

`[LegacyUnforgeable]` changes where the member is defined, too. By default, attribute getters/setters and methods are defined on a prototype chain. `[LegacyUnforgeable]` defines the member on the instance object instead of the prototype object.

### [LegacyWindowAlias]

Standard: [LegacyWindowAlias](https://webidl.spec.whatwg.org/#LegacyWindowAlias)

### [NewObject]

Standard: [NewObject](https://webidl.spec.whatwg.org/#NewObject)

Summary: Signals that a method that returns an object type always returns a new object or promise.

When a method returns an interface type, this extended attribute generates a test in debug mode to ensure that no wrapper object for the returned DOM object exists yet. Also see `[DoNotTestNewObject]`. When a method returns a Promise, this extended attribute currently does nothing.

### [PutForwards]

Standard: [PutForwards](https://webidl.spec.whatwg.org/#PutForwards)

Summary: Indicates that assigning to the attribute will have specific behavior. Namely, the assignment is “forwarded” to the attribute (specified by the extended attribute argument) on the object that is currently referenced by the attribute being assigned to.

Usage: Can be specified on `readonly` attributes:

```webidl
[PutForwards=href] readonly attribute Location location;
```

On setting the location attribute, the assignment will be forwarded to the Location.href attribute.

### [Replaceable]

Standard: [Replaceable](https://webidl.spec.whatwg.org/#Replaceable)

Summary: `[Replaceable]` controls if a given read only regular attribute is "replaceable" or not.

Usage: `[Replaceable]` can be specified on attributes:

```webidl
interface DOMWindow {
    [Replaceable] readonly attribute long screenX;
};
```

Intuitively, "replaceable" means that you can set a new value to the attribute without overwriting the original value. If you delete the new value, then the original value still remains.

Specifically, a writable attribute, without `[Replaceable]`, behaves as follows:

```js
window.screenX;  // Evaluates to 0
window.screenX = "foo";
window.screenX;  // Evaluates to "foo"
delete window.screenX;
window.screenX;  // Evaluates to undefined. 0 is lost.
```

A read only attribute, with `[Replaceable]`, behaves as follows:

```js
window.screenX;  // Evaluates to 0
window.screenX = "foo";
window.screenX;  // Evaluates to "foo"
delete window.screenX;
window.screenX;  // Evaluates to 0. 0 remains.
```

Whether `[Replaceable]` should be specified or not depends on the spec of each attribute.

### [SameObject]

Standard: [SameObject](https://webidl.spec.whatwg.org/#SameObject)

Summary: Signals that a `readonly` attribute that returns an object type always returns the same object.

This attribute has no effect on code generation and should simply be used in Blink IDL files if the specification uses it. If you want the binding layer to cache the resulting object, use `[SaveSameObject]`.

### [SecureContext]

Standard: [SecureContext](https://webidl.spec.whatwg.org/#SecureContext)

Summary: Interfaces and interface members with a `SecureContext` attribute are exposed only inside ["Secure Contexts"](https://w3c.github.io/webappsec-secure-contexts/).

```webidl
interface PointerEvent : MouseEvent {
    [SecureContext] sequence<PointerEvent> getCoalescedEvents();
};
```

### [Serializable]

Standard: [Serializable](https://html.spec.whatwg.org/C/#serializable)

Summary: Serializable objects support being serialized, and later deserialized, for persistence in storage APIs or for passing with `postMessage()`.

```webidl
[Serializable] interface Blob {
    ...
};
```

This attribute has no effect on code generation and should simply be used in Blink IDL files if the specification uses it. Code to perform the serialization/deserialization must be added to `V8ScriptValueSerializer` for types in `core/` or `V8ScriptValueDeserializerForModules` for types in `modules/`.

### [StringContext=TrustedHTML|TrustedScript|TrustedScriptURL]

Standard: [TrustedType](https://w3c.github.io/trusted-types/dist/spec/#!trustedtypes-extended-attribute)

Summary: Indicate that a DOMString for HTMLs and scripts or USVString for script URLs is to be supplemented with additional Trusted Types enforcement logic.

Usage: Must be specified on a DOMString or a USVString type.

```webidl
typedef [StringContext=TrustedHTML] DOMString TrustedString;
attribute TrustedString str;
void func(TrustedString str);
```

### [Transferable]

Standard: [Transferable](https://html.spec.whatwg.org/C/#transferable)

Summary: Transferable objects support being transferred across Realms with `postMessage()`.

```webidl
[Transferable] interface MessagePort {
    ...
};
```

This attribute has no effect on code generation and should simply be used in Blink IDL files if the specification uses it. Code to perform the transfer steps must be added to `V8ScriptValueSerializer` for types in `core/` or `V8ScriptValueDeserializerForModules` for types in `modules/`.

### [Unscopable]

Standard: [Unscopable](https://webidl.spec.whatwg.org/#Unscopable)

Summary: The interface member will not appear as a named property within `with` statements.

Usage: Can be specified on attributes or interfaces.

## Common Blink-specific IDL Extended Attributes

These extended attributes are widely used.

### [ActiveScriptWrappable]

Summary: `[ActiveScriptWrappable]` indicates that a given DOM object should be kept alive as long as the DOM object has pending activities.

Usage: `[ActiveScriptWrappable]` can be specified on interfaces, and **is inherited**:

```webidl
[
    ActiveScriptWrappable
] interface Foo {
    ...
};
```

If an interface X has `[ActiveScriptWrappable]` and an interface Y inherits the interface X, then the interface Y will also have `[ActiveScriptWrappable]`. For example

```webidl
[
    ActiveScriptWrappable
] interface Foo {};

interface Bar : Foo {};  // inherits [ActiveScriptWrappable] from Foo
```

If a given DOM object needs to be kept alive as long as the DOM object has pending activities, you need to specify `[ActiveScriptWrappable]`. For example, `[ActiveScriptWrappable]` can be used when the DOM object is expecting events to be raised in the future.

If you use `[ActiveScriptWrappable]`, the corresponding Blink class needs to inherit ActiveScriptWrappable and override hasPendingActivity(). For example, in case of XMLHttpRequest, core/xml/XMLHttpRequest.h would look like this:

```c++
class XMLHttpRequest : public ActiveScriptWrappable<XMLHttpRequest> {
  // Returns true if the object needs to be kept alive.
  bool HasPendingActivity() const override { return ...; }
}
```

### [Affects]

Summary: `[Affects=Nothing]` indicates that a function must not produce JS-observable side effects, while `[Affects=Everything]` indicates that a function may produce JS-observable side effects. Functions which are not considered free of JS-observable side effects will never be invoked by V8 with throwOnSideEffect.

Usage for attributes and operations: `[Affects=Nothing]` and `[Affects=Everything]` can be specified on an operation, or on an attribute to indicate that its getter callback is side effect free or side effecting:

```webidl
interface HTMLFoo {
    [Affects=Everything] attribute Bar bar;
    [Affects=Nothing] Bar baz();
    void removeItems();
};
```

When neither `[Affects=Nothing]` nor `[Affects=Everything]` is specified, the default for operations is `[Affects=Everything]`, while for attributes it's `[Affects=Nothing]`. Functions marked as side effect free are allowed to be nondeterministic, throw exceptions, force layout, and recalculate style, but must not set values, cache objects, or schedule execution that will be observable after the function completes. If a marked function calls into V8, it must properly handle cases when the V8 call returns an MaybeHandle.

All DOM constructors are assumed to side effects. However, an exception can be explicitly indicated when calling constructors using the V8 API method Function::NewInstanceWithSideEffectType().

There is not yet support for marking SymbolKeyedMethodConfigurations as side-effect free. This requires additional support in V8 to allow Intrinsics.

### [CallWith], [GetterCallWith], [SetterCallWith]

Summary: `[CallWith]` indicates that the bindings code calls the Blink implementation with additional information.

Each value changes the signature of the Blink methods by adding an additional parameter to the head of the parameter list, such as `ScriptState*` for `[CallWith=ScriptState]`.

`[GetterCallWith]` and `[SetterCallWith]` apply to attributes, and only affects the signature of the getter and setter, respectively.

NOTE: The `ExecutionContext` that you can get with [CallWith=ExecutionContext] or that you can extract from [CallWith=ScriptState] is the ExecutionContext associated with the V8 wrapper of the receiver object, which might be different from the ExecutionContext of the document tree to which the receiver object belongs.  See the following example.

```js
// V8 wrapper of |span| is associated with |windowA|.
span = windowA.document.createElement("span");
// |span| belongs to the document tree of |windowB|.
windowB.document.body.appendChild(span);
```

```c++
// Suppose [CallWith=ExecutionContext] void foo();
void HTMLSpanElement::foo(ExecutionContext* execution_context) {
  // The ExecutionContext associated with the creation context of the V8 wrapper
  // of the receiver object.
  execution_context;  // the ExecutionContext of |windowA|

  // Node::GetExecutionContext() returns the ExecutionContext of the document
  // tree to which |this| (the receiver object) belongs.
  GetExecutionContext();  // the ExecutionContext of |windowB|
}
```

#### [CallWith=ScriptState]

`[CallWith=ScriptState]` is used in a number of places for methods.
ScriptState holds all information about script execution.
You can retrieve Frame, ExcecutionContext, v8::Context, v8::Isolate etc
from ScriptState.

IDL example:

```webidl
interface Example {
    [CallWith=ScriptState] attribute DOMString str;
    [CallWith=ScriptState] DOMString func(boolean a, boolean b);
};
```

C++ Blink function signatures:

```c++
String Example::str(ScriptState* state);
String Example::func(ScriptState* state, bool a, bool b);
```

Be careful when you use `[CallWith=ScriptState]`.
You should not store the passed-in ScriptState on a DOM object.
This is because if the stored ScriptState is used by some method called by a different
world (note that the DOM object is shared among multiple worlds), it leaks the ScriptState
to the world. ScriptState must be carefully maintained in a way that doesn't leak
to another world.

#### [CallWith=ExecutionContext] _deprecated_

`[CallWith=ExecutionContext]` is a less convenient version of `[CallWith=ScriptState]`
because you can just retrieve ExecutionContext from ScriptState.
Use `[CallWith=ScriptState]` instead.

IDL example:

```webidl
interface Example {
    [CallWith=ExecutionContext] attribute DOMString str;
    [CallWith=ExecutionContext] DOMString func(boolean a, boolean b);
};
```

C++ Blink function signatures:

```c++
String Example::str(ExecutionContext* context);
String Example::func(ExecutionContext* context, bool a, bool b);
```

*** note
`[CallWith=...]` arguments are added at the _head_ of `XXX::Create(...)'s` arguments, and ` [RaisesException]`'s `ExceptionState` argument is added at the _tail_ of `XXX::Create(...)`'s arguments.
***

### [ContextEnabled]

Summary: `[ContextEnabled]` renders the generated interface bindings unavailable by default, but also generates code which allows individual script contexts opt into installing the bindings.

Usage: `[ContextEnabled=FeatureName]`. FeatureName is an arbitrary name used to identify the feature at runtime.

```webidl
[
    ContextEnabled=MojoJS
] interface Mojo { ... };
```

When applied to an interface, the generated code for the relevant global object will include a public `InstallFeatureName()` method which can be called to install the interface on the global object.

Note that `[ContextEnabled]` is not mututally exclusive to `[RuntimeEnabled]`, and a feature which may be enabled by either mechanism will be enabled if the appropriate `[RuntimeEnabled]` feature is enabled; _or_ if the appropriate `[ContextEnabled]` feature is enabled; _or_ if both are enabled.

### [DeprecateAs]

Summary: Measures usage of a deprecated feature via `UseCounter`, and notifies developers about deprecation via a devtools issue.

`[DeprecateAs]` can be considered an extended form of `[MeasureAs]`: it both measures the feature's usage via the same `UseCounter` mechanism, and also sends out an issue to devtools in order to inform developers that the code they've written will stop working at some point in the relatively near future.

Usage: `[DeprecateAs]` can be specified on methods, attributes, and constants.

```webidl
    [DeprecateAs=DeprecatedPrefixedAttribute] attribute Node prefixedAttribute;
    [DeprecateAs=DeprecatedPrefixedMethod] Node prefixedGetInterestingNode();
    [DeprecateAs=DeprecatedPrefixedConstant] const short DEPRECATED_PREFIXED_CONSTANT = 1;
```

For more documentation on deprecations, see [the documentation](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/third_party/blink/renderer/core/frame/deprecation/README.md).

### [HighEntropy]

Summary: Denotes an API that exposes data that folks on the internet find useful for fingerprinting.

Attributes and methods marked as `[HighEntropy]` are known to be practically useful for [identifying particular clients](https://dev.chromium.org/Home/chromium-security/client-identification-mechanisms) on the web today.
Both methods and attribute/constant getters annotated with this attribute are wired up to [`Dactyloscoper::Record`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/frame/dactyloscoper.h) for additional processing.

```webidl
[HighEntropy] attribute Node interestingAttribute;
[HighEntropy] Node getInterestingNode();
[HighEntropy] const INTERESTING_CONSTANT = 1;
```

Attributes and methods labeled with `[HighEntropy=Direct]` are simple surfaces which can be expressed as a sequence of bytes without any need for additional parsing logic.
For now, this label is only supported for attribute getters, although the `[HighEntropy]` label is supported more broadly. Note that `[HighEntropy=Direct]` must be accompanied by either `[Measure]` or `[MeasureAs]`.

```webidl
[HighEntropy=Direct, MeasureAs=SimpleNamedAttribute] attribute unsigned long simpleNamedAttribute;
```

### [ImplementedAs]

Summary: `[ImplementedAs]` specifies a method name in Blink, if the method name in an IDL file and the method name in Blink are different.

[ImplementedAs] can also be used for dictionary members.

`[ImplementedAs]` is _discouraged_. Please use only if absolutely necessary: rename Blink internal names to align with IDL.

Usage: The possible usage is `[ImplementedAs=XXX]`, where XXX is a method name in Blink. `[ImplementedAs]` can be specified on interfaces, methods and attributes.

```webidl
[
    ImplementedAs=DOMPath
] interface Path {
    [ImplementedAs=classAttribute] attribute int class;
    [ImplementedAs=deleteFunction] void delete();
};
```

Method names in Blink default to being the same as the name in an IDL file. In some cases this is not possible, e.g., `delete` is a C++ reserved word. In such cases, you can explicitly specify the method name in Blink by `[ImplementedAs]`. Generally the `[ImplementedAs]` name should be in lowerCamelCase. You should _not_ use `[ImplementedAs]` simply to avoid renaming Blink methods.

### [LogActivity]

Summary: logs activity, using V8PerContextData::activityLogger. Widely used. Interacts with `[PerWorldBindings]`, `[LogAllWorlds]`.

Usage:

* Valid values for attributes are: `GetterOnly`, `SetterOnly`, (no value)
* Valid values for methods are: (no value)

For methods all calls are logged, and by default for attributes all access (calls to getter or setter) are logged, but this can be restricted to just read (getter) or just write (setter).

### [Measure]

Summary: Measures usage of a specific feature via `UseCounter`.

In order to measure usage of specific features, Chrome submits anonymous statistics through the Histogram recording system for users who opt-in to sharing usage statistics. This extended attribute hooks up a specific feature to this measurement system.

Usage: `[Measure]` can be specified on interfaces, methods, attributes, and constants.

(_deprecated_) When specified on an interface usage of the constructor will be measured. This behavior could be changed in the future. Specify `[Measure]` on constructor operations instead.

The generated feature name must be added to `WebFeature` (in [blink/public/mojom/use_counter/metrics/web_feature.mojom](https://source.chromium.org/chromium/chromium/src/+/master:third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom)).

```webidl
[Measure] attribute Node interestingAttribute;
[Measure] Node getInterestingNode();
[Measure] const INTERESTING_CONSTANT = 1;
```

### [MeasureAs]

Summary: Like `[Measure]`, but the feature name is provided as the extended attribute value.
This is similar to the standard `[DeprecateAs]` extended attribute, but does not display a deprecation warning.

Usage: `[MeasureAs]` can be specified on interfaces, methods, attributes, and constants.

(_deprecated_) Specifying `[MeasureAs]` on interfaces is deprecated. Specify `[MeasureAs]` on constructor operations instead.

The value must match one of the enumeration values in `WebFeature` (in [blink/public/mojom/use_counter/metrics/web_feature.mojom](https://source.chromium.org/chromium/chromium/src/+/master:third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom)).

```webidl
[MeasureAs=AttributeWeAreInterestedIn] attribute Node interestingAttribute;
[MeasureAs=MethodsAreInterestingToo] Node getInterestingNode();
[MeasureAs=EvenSomeConstantsAreInteresting] const INTERESTING_CONSTANT = 1;
```

### [NotEnumerable]

Summary: Controls the enumerability of methods and attributes.

Usage: `[NotEnumerable]` can be specified on methods and attributes

```webidl
[NotEnumerable] attribute DOMString str;
[NotEnumerable] void foo();
```

`[NotEnumerable]` indicates that the method or attribute is not enumerable.

### [PassAsSpan]

Summary: Denotes that an argument should be passed as `base::span<const
uint8_t>`

Usage: `[PassAsSpan]` can only be used on `ArrayBuffer`-like operation arguments
(including `ArrayBufferView`s and typed arrays) that are read-only
and not retained by the implementation.

This extended attribute denotes that the implementation accepts this argument as
a span of bytes (`base::span<const uint8_t>`).  The memory referred by the span
is only valid for the duration of the bindings call. Passing array buffers as
spans is much faster compared to passing a union of several array-like types
(such as `BufferSource` union) both on the generated bindings side and on the
implementation side.

### [RaisesException]

Summary: Tells the code generator to append an `ExceptionState&` argument when calling the Blink implementation.

Implementations may use the methods on this parameter (e.g. `ExceptionState::ThrowDOMException`) to throw exceptions.

Usage: `[RaisesException]` can be specified on methods and attributes, `[RaisesException=Getter]` and `[RaisesException=Setter]` can be specified on attributes, and `[RaisesException=Constructor]` can be specified on interfaces where `[Constructor]` is also specified. On methods and attributes, the IDL looks like:

```webidl
interface XXX {
    [RaisesException] attribute long count;
    [RaisesException=Getter] attribute long count1;
    [RaisesException=Setter] attribute long count2;
    [RaisesException] void foo();
};
```

And the Blink implementations would look like:

```c++
long XXX::Count(ExceptionState& exception_state) {
  if (...) {
    exception_state.ThrowDOMException(TypeMismatchError, ...);
    return;
  }
  ...;
}

void XXX::SetCount(long value, ExceptionState& exception_state) {
  if (...) {
    exception_state.ThrowDOMException(TypeMismatchError, ...);
    return;
  }
  ...;
}

void XXX::foo(ExceptionState& exception_state) {
  if (...) {
    exception_state.ThrowDOMException(TypeMismatchError, ...);
    return;
  }
  ...;
};
```

If `[RaisesException=Constructor]` is specified on an interface and `[Constructor]` is also specified then an `ExceptionState&` argument is added when calling the `XXX::Create(...)` constructor callback.

```webidl
[
    Constructor(float x),
    RaisesException=Constructor,
]
interface XXX {
    ...
};
```

Blink needs to implement the following method as a constructor callback:

```c++
XXX* XXX::Create(float x, ExceptionState& exception_state) {
  ...;
  if (...) {
    exception_state.ThrowDOMException(TypeMismatchError, ...);
    return nullptr;
  }
  ...;
}
```

### [Reflect]

Specification: [The spec of Reflect](http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#reflect) - _defined in spec prose, not as an IDL extended attribute._

Summary: `[Reflect]` indicates that a given attribute should reflect the values of a corresponding content attribute.

Usage: The possible usage is `[Reflect]` or `[Reflect=X]`, where X is the name of a corresponding content attribute. `[Reflect]` can be specified on attributes:

```webidl
interface Element {
    [Reflect] attribute DOMString id;
    [Reflect=class] attribute DOMString className;
};
```

(Informally speaking,) a content attribute means an attribute on an HTML tag: `<div id="foo" class="fooClass"></div>`

Here `id` and `class` are content attributes.

If a given attribute in an IDL file is marked as `[Reflect]`, it indicates that the attribute getter returns the value of the corresponding content attribute and that the attribute setter sets the value of the corresponding content attribute. In the above example, `div.id` returns `"foo"`, and `div.id = "bar"` assigns `"bar"` to the `id` content attribute.

If the name of the corresponding content attribute is different from the attribute name in an IDL file, you can specify the content attribute name by `[Reflect=X]`. For example, in case of `[Reflect=class]`, if `div.className="barClass"` is evaluated, then `"barClass"` is assigned to the `class` content attribute.

Whether `[Reflect]` should be specified or not depends on the spec of each attribute.

### [ReflectEmpty]

Specification: [Enumerated attributes](http://www.whatwg.org/specs/web-apps/current-work/#enumerated-attribute) - _defined in spec prose, not as an IDL extended attribute._

Summary: `[ReflectEmpty]` gives the attribute keyword value to reflect when an attribute is present, but without a value; it supplements `[ReflectOnly]` and `[Reflect]`.

Usage: The possible usage is `[ReflectEmpty="value"]` in combination with `[ReflectOnly]`:

```webidl
interface HTMLMyElement {
    [Reflect, ReflectOnly=("for", "against"), ReflectEmpty="for"] attribute DOMString vote;
};
```

The `[ReflectEmpty]` extended attribute specifies the value that an IDL getter for the `vote` attribute should return when the content attribute is present, but without a value (e.g., return `"for"` when accessing the `vote` IDL attribute on `<my-element vote/>`.) Its (string) literal value must be one of the possible values that the `[ReflectOnly]` extended attribute lists.

`[ReflectEmpty]` should be used if the specification for the content attribute has an empty attribute value mapped to some attribute state. For HTML, this applies to [enumerated attributes](http://www.whatwg.org/specs/web-apps/current-work/#enumerated-attribute) only.

Non-empty string value specified by `[ReflectEmpty]` must be added to
`core/html/keywords.json5`.

When no value is specified by `[ReflectEmpty]`, the value will be IDL null if the attribute type is nullable, otherwise the empty string.

### [ReflectInvalid]

Specification: [Limited value attributes](http://www.whatwg.org/specs/web-apps/current-work/#limited-to-only-known-values) - _defined in spec prose, not as an IDL extended attribute._

Summary: `[ReflectInvalid]` gives the attribute keyword value to reflect when an attribute has an invalid/unknown value. It supplements `[ReflectOnly]` and `[Reflect]`.

Usage: The possible usage is `[ReflectInvalid="value"]` in combination with `[ReflectOnly]`:

```webidl
interface HTMLMyElement {
    [Reflect, ReflectOnly=("left", "right"), ReflectInvalid="left"] attribute DOMString direction;
};
```

The `[ReflectInvalid]` extended attribute specifies the value that an IDL getter for the `direction` attribute should return when the content attribute has an unknown value (e.g., return `"left"` when accessing the `direction` IDL attribute on `<my-element direction=dont-care />`.) Its (string) literal value must be one of the possible values that the `[ReflectOnly]` extended attribute lists.

`[ReflectInvalid]` should be used if the specification for the content attribute has an _invalid value state_ defined. For HTML, this applies to [enumerated attributes](http://www.whatwg.org/specs/web-apps/current-work/#enumerated-attribute) only.

Non-empty string value specified by `[ReflectInvalid]` must be added to
`core/html/keywords.json5`.

When no value is specified by `[ReflectInvalid]`, the value will be IDL null if the attribute type is nullable, otherwise the empty string.

### [ReflectMissing]

Specification: [Limited value attributes](http://www.whatwg.org/specs/web-apps/current-work/#limited-to-only-known-values) - _defined in spec prose, not as an IDL extended attribute._

Summary: `[ReflectMissing]` gives the attribute keyword value to reflect when an attribute isn't present. It supplements `[ReflectOnly]` and `[Reflect]`.

Usage: The possible usage is `[ReflectMissing="value"]` in combination with `[ReflectOnly]`:

```webidl
interface HTMLMyElement {
    [Reflect, ReflectOnly=("ltr", "rtl", "auto"), ReflectMissing="auto"] attribute DOMString preload;
};
```

The `[ReflectMissing]` extended attribute specifies the value that an IDL getter for the `direction` attribute should return when the content attribute is missing (e.g., return `"auto"` when accessing the `preload` IDL attribute on `<my-element>`.) Its (string) literal value must be one of the possible values that the `[ReflectOnly]` extended attribute lists.

`[ReflectMissing]` should be used if the specification for the content attribute has a _missing value state_ defined. For HTML, this applies to [enumerated attributes](http://www.whatwg.org/specs/web-apps/current-work/#enumerated-attribute) only.

Non-empty string value specified by `[ReflectMissing]` must be added to
`core/html/keywords.json5`.

When no value is specified by `[ReflectMissing]`, the value will be IDL null if the attribute type is nullable, otherwise the empty string.

### [ReflectOnly]

Specification: [Limited value attributes](http://www.whatwg.org/specs/web-apps/current-work/#limited-to-only-known-values) - _defined in spec prose, not as an IDL extended attribute._

Summary: `[ReflectOnly]` indicates that a reflected string attribute should be limited to a set of allowable values; it supplements `[Reflect]`.

Usage: The possible usages are `[ReflectOnly="value"]` and `[ReflectOnly=("A1",...,"An")]` where A1 (up to n) are the attribute values allowed. `[ReflectOnly]` is used in combination with `[Reflect]`:

```webidl
interface HTMLMyElement {
    [Reflect, ReflectOnly="on"] attribute DOMString toggle;
    [Reflect=q, ReflectOnly=("first", "second", "third", "fourth")] attribute DOMString quarter;
};
```

The ReflectOnly attribute limits the range of values that the attribute getter can return from its reflected attribute. If the content attribute has a value that is a case-insensitive match for one of `ReflectOnly`'s values, then it will be returned. To allow attribute values that use characters that go beyond what IDL identifiers may contain, string literals are used. This is a Blink syntactic extension to extended attributes.

If there is no match, the empty string will be returned. As required by the specification, no such checking is performed when the reflected IDL attribute is set.

`[ReflectOnly]` should be used if the specification for a reflected IDL attribute says it is _"limited to only known values"_.

Non-empty string values specified by `[ReflectOnly]` must be added to
`core/html/keywords.json5`.

### [RuntimeEnabled]

Summary: `[RuntimeEnabled]` wraps the generated code with `if (RuntimeEnabledFeatures::FeatureNameEnabled) { ...code... }`.

Usage: `[RuntimeEnabled=FeatureName]`. FeatureName must be included in [runtime\_enabled\_features.json5](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/platform/runtime_enabled_features.json5).

```webidl
[
    RuntimeEnabled=MediaSession
] interface MediaSession { ... };
```

Only when the feature is enabled at runtime (using a command line flag, for example, or when it is enabled only in certain platforms), the binding would be exposed to the web.

```webidl
// Overload can be replaced with optional if `[RuntimeEnabled]` is removed
foo(long x);
[RuntimeEnabled=FeatureName] foo(long x, long y);
```

For more information, see [RuntimeEnabledFeatures](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/platform/runtime_enabled_features.json5).

### [SaveSameObject]

Summary: Caches the resulting object and always returns the same object.

When specified, caches the resulting object and returns it in later calls so that the attribute always returns the same object. Must be accompanied with `[SameObject]`.

## Rare Blink-specific IDL Extended Attributes

These extended attributes are rarely used, generally only in one or two places, and may be candidates for deprecation and removal.

### [CachedAccessor]

Summary: Caches the accessor result in a private property (not directly accessible from JS). Improves accessor reads (getter) at the expense of extra memory and manual invalidation which should be trivial in most cases.


*** note
* The getter cannot have any side effects since calls to the getter will be replaced by a cheap property load.
* It uses a **push approach**, so updates must be _pushed_ every single time, it **DOES NOT** invalidate/update the cache automatically.
* Despite being cached, the getter can still be called on certain circumstances, consistency is a must.
* The cache **MUST** be initialized before using it. There's no default value like _undefined_ or a _hole_.
***


Usage: `[CachedAccessor]` takes no arguments, can be specified on attributes.

```webidl
interface HTMLFoo {
    [CachedAccessor] readonly attribute Bar bar;
};
```


Register the required property in V8PrivateProperty.h.
To update the cached value (e.g. for HTMLFoo.bar) proceed as follows:

```c++
V8PrivateProperty::GetHTMLFooBarCachedAccessor().Set(context, object, new_value);
```

### [CachedAttribute]

Summary: For performance optimization, `[CachedAttribute]` indicates that a wrapped object should be cached on a DOM object. Rarely used.

Usage: `[CachedAttribute]` can be specified on attributes, and takes a required value, generally called isXXXDirty (e.g. isValueDirty):

```webidl
interface HTMLFoo {
    [CachedAttribute=isKeyDirty] attribute DOMString key;
    [CachedAttribute=isValueDirty] attribute SerializedScriptValue serializedValue;
};
```

Without `[CachedAttribute]`, the key getter works in the following way:

1. HTMLFoo::key() is called in Blink.
2. The result of HTMLFoo::key() is passed to ToV8(), and is converted to a wrapped object.
3. The wrapped object is returned.

In case where HTMLFoo::key() or the operation to wrap the result is costly, you can cache the wrapped object onto the DOM object. With CachedAttribute, the key getter works in the following way:

1. If the wrapped object is cached, the cached wrapped object is returned. That's it.
2. Otherwise, `HTMLFoo::key()` is called in Blink.
3. The result of `HTMLFoo::key()` is passed to `ToV8()`, and is converted to a wrapped object.
4. The wrapped object is cached.
5. The wrapped object is returned.

`[CachedAttribute]` is particularly useful for serialized values, since deserialization can be costly. Without `[CachedAttribute]`, the serializedValue getter works in the following way:

1. `HTMLFoo::serializedValue()` is called in Blink.
2. The result of `HTMLFoo::serializedValue()` is deserialized.
3. The deserialized result is passed to `ToV8()`, and is converted to a wrapped object.
4. The wrapped object is returned.

In case where `HTMLFoo::serializedValue()`, the deserialization or the operation to wrap the result is costly, you can cache the wrapped object onto the DOM object. With `[CachedAttribute]`, the serializedValue getter works in the following way:

1. If the wrapped object is cached, the cached wrapped object is returned. That's it.
2. Otherwise, `HTMLFoo::serializedValue()` is called in Blink.
3. The result of `HTMLFoo::serializedValue()` is deserialized.
4. The deserialized result is passed to `toJS()` or `ToV8()`, and is converted to a wrapped object.
5. The wrapped object is cached.
6. The wrapped object is returned.

*** note
You should cache attributes if and only if it is really important for performance. Not only does caching increase the DOM object size, but also it increases the overhead of "cache-miss"ed getters. In addition, setters always need to invalidate the cache.
***

`[CachedAttribute]` takes a required parameter which the name of a method to call on the implementation object. The method should be const, take void and return bool. Before the cached attribute is used, the method will be called. If the method returns true the cached value is not used, which will result in the accessor being called again. This allows the implementation to both gain the performance benefit of caching (when the conversion to a script value can be done lazily) while allowing the value to be updated. The typical use pattern is:

```c++
// Called internally to update value
void Object::SetValue(Type data) {
  data_ = data;
  attribute_dirty_ = true;
}

// Called by generated binding code
bool Object::IsAttributeDirty() const {
  return attribute_dirty_;
}

// Called by generated binding code if no value cached or IsAttributeDirty() returns true
ScriptValue Object::attribute(ExecutionContext* context) {
  attribute_dirty_ = false;
  return ConvertDataToScriptValue(data_);
}
```

### [CheckSecurity]

Summary: Check whether a given access is allowed or not in terms of the
same-origin security policy.

Usage for attributes and methods: `[CheckSecurity=ReturnValue]` enables a
security check on that property. The security check verifies that the caller is
allowed to access the returned value. If access is denied, the return value will
be `undefined` and an exception will be raised. In practice, attribute uses are
all `[readonly]`, and method uses are all `[RaisesException]`.

```webidl
[CheckSecurity=ReturnValue] readonly attribute Document contentDocument;
[CheckSecurity=ReturnValue] SVGDocument getSVGDocument();
```

This is important because cross-origin access is not transitive. For example, if
`window` and `window.parent` are cross-origin, access to `window.parent` is
allowed, but access to `window.parent.document` is not.

### [ConvertibleToObject]

Summary:

Forces generation of code to convert native to script value for dictionaries and unions.
This is assumed for all types that appear as return values for methods (or arguments to
callback methods), but may need to be specified explicitly for cases where the conversion
happens internally in C++ code and is not specified in IDL.

Usage:
```webidl
[ConvertiableToObject] dictionary Foo {
    DOMString bar;
}

void frob([ConvertiableToObject] (Foo or USVString) param);
```

### [CrossOrigin]

Summary: Allows cross-origin access to an attribute or method. Used for
implementing [CrossOriginProperties] from the spec in location.idl and
window.idl.

Usage for methods:
```webidl
[CrossOrigin] void blur();
```

Note that setting this attribute on a method will disable [security
checks](#_CheckSecurity_i_m_a_), since this method can be invoked cross-origin.

Usage for attributes:
```webidl
[CrossOrigin] readonly attribute unsigned long length;
```
With no arguments, defaults to allowing cross-origin reads, but
not cross-origin writes.

```webidl
[CrossOrigin=Setter] attribute DOMString href;
```
With `Setter`, allows cross-origin writes, but not cross-origin reads. This is
used for the `Location.href` attribute: cross-origin writes to this attribute
are allowed, since it navigates the browsing context, but allowing cross-origin
reads would leak cross-origin information.

```webidl
[CrossOrigin=(Getter,Setter)] readonly attribute Location location;
```
With both `Getter` and `Setter`, allows both cross-origin reads and cross-origin
writes. This is used for the `Window.location` attribute.

### [HasAsyncIteratorReturnAlgorithm]

Summary: `[HasAsyncIteratorReturnAlgorithm]` indicates whether an [asynchronously iterable declaration](https://webidl.spec.whatwg.org/#dfn-async-iterable-declaration) has an [asynchronous iterator return algorithm](https://webidl.spec.whatwg.org/#asynchronous-iterator-return).

This tells the code generator to add a `return()` method to the async iterator. The Blink implementation must then provide the return algorithm by overriding `AsyncIterationSourceBase::AsyncIteratorReturn()`.

Usage: Applies only to `async iterable` declarations. See core/streams/readable_stream.idl for an example.

### [IsolatedContext]

Summary: Interfaces and interface members with a `IsolatedContext` extended attribute are exposed only inside isolated contexts.
This attribute is primarily intended for Isolated Web Apps (see [explainer](https://github.com/WICG/isolated-web-apps)) with an option for the embedder to include their own additional scenarios.

Note that it's likely for these requirements to shift over time: <https://crbug.com/1206150>.

Usage: The `[IsolatedContext]` extended attribute may be specified on interfaces, attributes, and operations:

```webidl
[IsolatedContext]
interface TCPSocket {
  ...
};
```

### [LegacyFactoryFunction_CallWith]

Summary: The same as `[CallWith]` but applied to the legacy factory functions (aka the named constructors).

### [LegacyFactoryFunction_RaisesException]

Summary: The same as `[RaisesException]` but applied to the legacy factory functions (aka the named constructors).

### [LegacyWindowAlias_Measure]

Summary: The same as `[Measure]` but applied to the property exposed as `[LegacyWindowAlias]`.

### [LegacyWindowAlias_MeasureAs]

Summary: The same as `[MeasureAs]` but applied to the property exposed as `[LegacyWindowAlias]`.

### [LegacyWindowAlias_RuntimeEnabled]

Summary: The same as `[RuntimeEnabled]` but applied to the property exposed as `[LegacyWindowAlias]`.

### [NoAllocDirectCall]

Summary: `[NoAllocDirectCall]` marks a given method as being usable with the fast API calls implemented in V8. They get their value conversions inlined in TurboFan, leading to overall better performance.

Usage: The method must adhere to the following requirements:

1. Doesn't trigger GC, i.e., doesn't allocate Blink or V8 objects;
2. Doesn't trigger JavaScript execution;
3. Has no side effect.

Those requirements lead to the specific inability to log warnings to the console, as logging uses `MakeGarbageCollected<ConsoleMessage>`. If logging needs to happen, the method marked with `[NoAllocDirectCall]` should expect a last parameter `bool* has_error`, in which it might store `true` to signal V8. V8 will in turn re-execute the "default" callback, giving the possibility of the exception/error to be reported. This mechanism also implies that the "fast" callback is idempotent up to the point of reporting the error.

If `[NoAllocDirectCall]` is applied to a method, then the corresponding implementation C++ class must **also** derive from the [`NoAllocDirectCallHost` class](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/platform/bindings/no_alloc_direct_call_host.h).

Calling `ThrowDOMException` would seemingly cause `MakeGarbageCollected<DOMException>` to occur, violating the requirement about potentially triggering garbage collection. However, `ThrowDOMException` from a `[NoAllocDirectCall]` method is actually safe in practice. When generating the bindings for a method which is marked as both `[NoAllocDirectCall]` and `[RaisesException]`, V8 will automatically use [`NoAllocDirectCallExceptionState`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/platform/bindings/no_alloc_direct_call_exception_state.h) instead of `ExceptionState`. This class will defer the allocation of the `DOMException` object via `PostDeferrableAction` until it is safe to allocate GC memory. The `WTF::String` inside of the `DOMException` is not a V8 object and does not participate in garbage collection, so its allocation is safe and doesn't violate the requirements of `[NoAllocDirectCall]`.

Note: the `[NoAllocDirectCall]` extended attribute can only be applied to methods, and not attributes. An attribute getter's V8 return value constitutes a V8 allocation, and setters likely allocate on the Blink side.

### [PerWorldBindings]

Summary: Generates faster bindings code by avoiding check for isMainWorld().

This optimization only makes sense for wrapper-types (i.e. types that have a corresponding IDL interface), as we don't need to check in which world we are for other types.

*** note
This optimization works by generating 2 separate code paths for the main world and for isolated worlds. As a consequence, using this extended attribute will increase binary size and we should refrain from overusing it.
***

### [URL]

Summary: `[URL]` indicates that a given DOMString represents a URL.

Usage: `[URL]` can be specified on DOMString attributes that have `[Reflect]` extended attribute specified only:

```webidl
[Reflect, URL] attribute DOMString url;
```

You need to specify `[URL]` if a given DOMString represents a URL, since getters of URL attributes need to be realized in a special routine in Blink, i.e. `Element::getURLAttribute(...)`. If you forgot to specify `[URL]`, then the attribute getter might cause a bug.

Only used in some HTML*ELement.idl files and one other place.

## Temporary Blink-specific IDL Extended Attributes

These extended attributes are _temporary_ and are only in use while some change is in progress. Unless you are involved with the change, you can generally ignore them, and should not use them.

### [InjectionMitigated]

Summary: Interfaces and interface members with an `[InjectionMitigated]` attribute are exposed only in contexts that enforce a [strict Content Security Policy](https://csp.withgoogle.com/docs/strict-csp.html) and [Trusted Types](https://w3c.github.io/trusted-types/dist/spec/).

This attribute implements the core idea behind the [Securer Contexts explainer](https://github.com/mikewest/securer-contexts), and may be renamed as it works its way through the standards process.

### [IsCodeLike]

This implements the TC39 "Dynamic Code Brand Checks" proposal. By attaching
the [IsCodeLike] attribute to a type, its instances will be treated as
"code like" objects, as detailed in the spec.

Standard: [TC39 Dynamic Code Brand Checks](https://github.com/tc39/proposal-dynamic-code-brand-checks)

## Discouraged Blink-specific IDL Extended Attributes

These extended attributes are _discouraged_ - they are not deprecated, but they should be avoided and removed if possible.

### [BufferSourceTypeNoSizeLimit]

Summary: The byte length of buffer source types is currently restricted to be under 2 GB (exactly speaking, it must be less than the max size of a direct mapped memory of PartitionAlloc, which is a little less than 2 GB).  This extended attribute removes this limitation.

Consult with the bindings team before you use this extended attribute.

### [IDLTypeImplementedAsV8Promise]

Summary: Indicates that an IDL `Promise` type should be implemented as `v8::Local<v8::Promise>` rather than the default `ScriptPromiseUntyped` type.

This is currently only used for the return types of `AsyncIteratorBase` methods. Consult with the bindings team before you use this extended attribute.

### [NodeWrapInOwnContext]

Summary: Forces a Node to be wrapped in its own context, rather than the receiver's context.

In most cases, return values are wrapped in the receiver context (i.e., the context of the interface whose operation/attribute/etc. is being called). The bindings assert that `[NodeWrapInOwnContext]` is only used for `Node`s. When used, we find the correct `ScriptState` with the `Node`'s `ExecutionContext` and the current `DOMWrapperWorld`, and if it exists, wrap the `Node` using that `ScriptState`. If that `ScriptState` is not available (usually because the `Node` is detached), we fall back to the receiver `ScriptState`.

`[NodeWrapInOwnContext]` is only necessary where a `Node` may be returned by an interface from a different context, *and* that interface does not use `[CheckSecurity=ReturnValue]` to enable cross-context security checks. The only interfaces that this applies to are ones that unnecessarily mix contexts (`NodeFilter`, `NodeIterator`, and `TreeWalker`), and new usage should not be introduced.

### [TargetOfExposed]

Summary: Interfaces specified with `[Global]` expose top-level IDL constructs specified with `[Exposed]` as JS data properties, however `[Global]` means a lot more (e.g. global object, named properties object, etc.). Interfaces specified with `[TargetOfExposed]` only expose top-level IDL constructs specified with `[Exposed]` and means nothing else.

This extended attribute should be used only for pseudo namespace(-ish) objects like `globalThis.chromeos`. Consult with the bindings team before you use this extended attribute.

## Deprecated Blink-specific IDL Extended Attributes

These extended attributes are _deprecated_, or are under discussion for deprecation. They should be avoided.

### [PermissiveDictionaryConversion]

Summary: `[PermissiveDictionaryConversion]` relaxes the rules about what types of values may be passed for an argument of dictionary type.

Ordinarily when passing in a value for a dictionary argument, the value must be either undefined, null, or an object. In other words, passing a boolean value like true or false must raise TypeError. The PermissiveDictionaryConversion extended attribute ignores non-object types, treating them the same as undefined and null. In order to effect this change, this extended attribute must be specified both on the dictionary type as well as the arguments of methods where it is passed.

Usage: applies to dictionaries and arguments of methods. Takes no arguments itself.

### [RuntimeCallStatsCounter]

Summary: Adding `[RuntimeCallStatsCounter=<Counter>]` as an extended attribute to an interface method or attribute results in call counts and run times of the method or attribute getter (and setter if present) using RuntimeCallStats (see Source/platform/bindings/RuntimeCallStats.h for more details about RuntimeCallStats). \<Counter\> is used to identify a group of counters that will be used to keep track of run times for a particular method/attribute.

A counter with id `k<Counter>` will keep track of the execution time and counts for methods (including the time spent in the bindings layer). For attribute getters, it is `k<Counter>_Getter` and for setters, `k<Counter>_Setter`.

Usage:

```webidl
interface Node {
  [RuntimeCallStatsCounter=NodeOwnerDocument] readonly attribute Document? ownerDocument;
  [RuntimeCallStatsCounter=NodeTextContent] attribute DOMString? textContent;
  [RuntimeCallStatsCounter=NodeHasChildNodes] boolean hasChildNodes();
}
```

The counters specified in the IDL file also need to be defined in Source/platform/bindings/RuntimeCallStats.h (under CALLBACK_COUNTERS) as follows:

```cpp
#define CALLBACK_COUNTERS(V)                         \
...                                                  \
  BINDINGS_READ_ONLY_ATTRIBUTE(V, NodeOwnerDocument) \
  BINDINGS_ATTRIBUTE(V, NodeTextContent)             \
  BINDINGS_METHOD(V, NodeHasChildNodes)
```

-------------

Derived from: [http://trac.webkit.org/wiki/WebKitIDL](http://trac.webkit.org/wiki/WebKitIDL) Licensed under [BSD](http://www.webkit.org/coding/bsd-license.html):

*** aside
BSD License
Copyright (C) 2009 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***

[CrossOriginProperties]: https://html.spec.whatwg.org/C/#crossoriginproperties-(-o-)
