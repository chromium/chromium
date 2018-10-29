# Blink IDL Extended Attributes

[TOC]

## Introduction

The main interest in extended attributes are their _semantics_: Blink implements many more extended attributes than the Web IDL standard, to specify various behavior.

The authoritative list of allowed extended attributes and values is [bindings/IDLExtendedAttributes.txt](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/bindings/IDLExtendedAttributes.txt). This is complete but not necessarily precise (there may be unused extended attributes or values), since validation is run on build, but coverage isn't checked.

Syntactically, Blink IDL extended attributes differ from standard Web IDL extended attributes in a few ways:

* trailing commas are allowed (for convenience),
* the value of a key=value pair can be a string literal, not just an identifier: `key="foo"` or `key=("foo","bar")`

Blink IDL also does not support certain recent features of the Web IDL grammar:

* Values that are not identifiers or strings are _not_ supported (the `Other` production): any non-identifier should simply be quoted (this could be changed to remove the need for quotes, but requires rather lengthy additions to the parser).

Semantically, only certain extended attributes allow lists. Similarly, only certain extended attributes allow string literals.

Extended attributes either take no value, take a required value, or take an optional value.
In the following explanations, _(i)_, _(m)_, _(s)_, _(a)_, _(p)_, _(c)_, _(d)_, and _(f)_ mean that a given extended attribute can be specified on interfaces, methods, special operations, attributes, parameters, constants, dictionaries, and callback functions respectively. For example, _(a,p)_ means that the IDL attribute can be specified on attributes and parameters.

*** note
These restrictions are not enforced by the parser: extended attributes used in unsupported contexts will simply be ignored.
***

As a rule, we do _not_ add extended attributes to the IDL that are not supported by the compiler (and are thus nops). This is because it makes the IDL misleading: looking at the IDL, it looks like it should do something, but actually doesn't, which is opaque (it requires knowledge of compiler internals). Instead, please place a comment on the preceding line, with the desired extended attribute and a FIXME referring to the relevant bug. For example (back when [Bug 358506](https://crbug.com/358506) was open):

```webidl
// FIXME: should be [MeasureAs=Foo] but [MeasureAs] not supported on constants: http://crbug.com/358506
const unsigned short bar;
```

## Naming

Extended attributes are named in UpperCamelCase, and are conventionally written as the name of the attribute within brackets, as `[ExampleExtendedAttribute]`, per [Web IDL typographic conventions](https://heycam.github.io/webidl/#conventions).

There are a few rules in naming extended attributes:

Names should be aligned with the Web IDL spec as much as possible.
Extended attributes for custom bindings are prefixed by "Custom".
Lastly, please do not confuse "_extended_ attributes", which go inside `[...]` and modify various IDL elements, and "attributes", which are of the form `attribute foo` and are interface members.

## Special cases

### Constants

Only the following (Blink-only) extended attributes apply to constants: `[DeprecateAs]`, `[MeasureAs]`, `[OriginTrialEnabled]`, `[Reflect]`, and `[RuntimeEnabled]`, and the interface extended attribute `[DoNotCheckConstants]` affects constants.

### Overloaded methods

Extended attributes mostly work normally on overloaded methods, affecting only that method (not other methods with the same name), but there are a few exceptions, due to all the methods sharing a single callback.

`[RuntimeEnabled]` works correctly on overloaded methods, both on individual overloads or (if specified on all method with that name) on the entire method ([Bug 339000](https://crbug.com/339000)).

*** note
While `[DeprecateAs]`, `[MeasureAs]` only affect callback for non-overloaded methods, the logging code is instead put in the method itself for overloaded methods, so these can be placed on the method to log in question.
***

Extended attributes that affect the callback must be on the _last_ overloaded method, though it is safest to put them on all the overloaded methods, for consistency (and in case they are rearranged or deleted). The source is [bindings/templates/methods.cpp](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/bindings/templates/methods.cpp), and currently there are no extended attribute that affect the callback (even for overloaded methods).

### Special operations (methods)

Extended attributes on special operations (methods) are largely the same as those on methods generally, though many fewer are used.

Extended attributes that apply to the whole property (not a specific operation) must be put on the _getter_, since this is always present. There are currently two of these:
`[Custom=PropertyQuery|PropertyEnumerator]` and `[NotEnumerable]`.

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

There is one interface extended attribute that only affects special operations: `[OverrideBuiltins]`.

The following extended attributes are used on special operations, as on methods generally: `[RaisesException]`.

### Partial interfaces

Extended attributes on partial interface members work as normal. However, only the following 4 extended attributes can be used on the partial interface itself; otherwise extended attributes should appear on the main interface definition:

`[Conditional]`, `[ImplementedAs]`, `[OriginTrialEnabled]` and `[RuntimeEnabled]`

3 of these are used to allow the entire partial interface to be selectively enabled or disabled: `[Conditional]`, `[OriginTrialEnabled]` and `[RuntimeEnabled]`, and function as if the extended attribute were applied to each _member_ (methods, attributes, and constants). Style-wise, if the entire partial interface should be enabled or disabled, these extended attributes should be used on the partial interface, not on each individual member; this clarifies intent and simplifies editing. However:

* If some members should not be disabled, this cannot be used on the partial interface; this is often the case for constants.
* If different members should be controlled by different flags, this must be specified individually.
* If a flag obviously applies to only one member of a single-member interface (i.e., it is named after that member), the extended attribute should be on the member.

The remaining extended attribute, `[ImplementedAs]`, is mandatory. A partial
interface must have `[ImplementedAs]` extended attribute to specify a static-only C++ class.
This is stored internally via `[PartialInterfaceImplementedAs]` (see below).

### implements

Extended attributes on members of an implemented interface work as normal. However, only the following 5 extended attributes can be used on the implemented interface itself; otherwise extended attributes should appear on the main (implementing) interface definition:

* `[LegacyTreatAsPartialInterface]` is part of an ongoing change, as implemented interfaces used to be treated internally as partial interfaces.

* `[ImplementedAs]` is only necessary for these legacy files: otherwise the class (C++) implementing (IDL) implemented interfaces does not need to be specified, as this is handled in Blink C++.

* `[OriginTrialEnabled]` behaves as for partial interfaces.

* `[RuntimeEnabled]` behaves as for partial interfaces.

* `[NoInterfaceObject]` is _always_ specified on implemented interfaces.

### Inheritance

Extended attributes are generally not inherited: only extended attributes on the interface itself are consulted. However, there are a handful of extended attributes that are inherited (applying them to an ancestor interface applies them to the descendants). These are extended attributes that affect memory management, and currently consists of `[ActiveScriptWrappable]`; the up-to-date list is [compute_dependencies.INHERITED_EXTENDED_ATTRIBUTES](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/bindings/scripts/compute_dependencies.py&q=INHERITED_EXTENDED_ATTRIBUTES).

## Standard Web IDL Extended Attributes

These are defined in the [ECMAScript-specific extended attributes](http://heycam.github.io/webidl/#es-extended-attributes) section of the [Web IDL spec](http://heycam.github.io/webidl/), and alter the binding behavior.

*** note
Unsupported: `[LenientThis]`

Undocumented: `[TreatNonObjectAsNull]`
***

### [CEReactions] _(m, a)_

Standard: [CEReactions](https://html.spec.whatwg.org/multipage/scripting.html#cereactions)

Summary: `[CEReactions]` indicates that
[custom element reactions](https://html.spec.whatwg.org/multipage/scripting.html#concept-custom-element-reaction)
are triggered for this method or attribute.

Usage: `[CEReactions]` takes no arguments.

`[CEReacionts]` doesn't work with `[Custom]`. Custom binding code should use
`blink::CEReactionsScope` if the method or the attribute has `[CEReactions]`.

Note that `blink::CEReactionsScope` must be constructed after `blink::ExceptionState`.

### [Clamp] _(a, p)_

Standard: [Clamp](https://heycam.github.io/webidl/#Clamp)

Summary: `[Clamp]` indicates that when an ECMAScript Number is converted to the IDL type, out of range values will be clamped to the range of valid values, rather than using the operators that use a modulo operation (ToInt32, ToUint32, etc.).

Usage: The `[Clamp]` extended attribute MUST NOT appear on a read only attribute, or an attribute, operation argument or dictionary member that is not of an integer type.

`[Clamp]` can be specified on writable attributes:

```webidl
interface XXX {
    [Clamp] attribute unsigned short attributeName;
};
```

`[Clamp]` can be specified on extended attributes or methods arguments:

```webidl
interface GraphicsContext {
    void setColor(octet red, octet green, octet blue);
    void setColorClamped([Clamp] octet red, [Clamp] octet green, [Clamp] octet blue);
};
```

Calling the non-`[Clamp]` version of `setColor()` uses **ToUint8()** to coerce the Numbers to octets. Hence calling `context.setColor(-1, 255, 257)` is equivalent to calling `setColor(255, 255, 1)`.

Calling the `[Clamp]` version of `setColor()` uses **clampTo()** to coerce the Numbers to octets. Hence calling `context.setColor(-1, 255, 257)` is equivalent to calling `setColorClamped(0, 255, 255)`.

### [Constructor] _(i)_

Standard: [Constructor](https://heycam.github.io/webidl/#Constructor)

Summary: `[Constructor]` indicates that the interface should have a constructor, i.e. "new XXX()".

*** note
The Blink-specific `[CallWith]` and `[RaisesException]` extended attributes, specified on an interface, add information when the constructor callback is called.
***

Usage: `[Constructor]` can be specified on interfaces:

```webidl
[
    Constructor(float x, float y, DOMString str),
]
interface XXX {
    ...
};
```

In the above, `[Constructor(float x, float y, DOMString str)]` means that the interface has a constructor and the constructor signature is `(float x, float y, DOMString str)`. Specifically, JavaScript can create a DOM object of type `XXX` by the following code:

```js
var x = new XXX(1.0, 2.0, "hello");
```

The Blink implementation must have the following method as a constructor callback:

```c++
scoped_refptr<XXX> XXX::create(float x, float y, String str)
{
    ...;
}
```

As shorthand, a constructor with no arguments can be written as `[Constructor]` instead of `[Constructor()]`.

Whether you should allow an interface to have constructor depends on the spec of the interface.

*** note
Currently `[Constructor(...)]` does not yet support optional arguments w/o defaults. It just supports optional `[Default=Undefined]`.
***

### [EnforceRange] _(a, p)_

Standard: [EnforceRange](https://heycam.github.io/webidl/#EnforceRange)

Summary: `[EnforceRange]` indicates that when an ECMAScript Number is converted to the IDL type, out of range values will result in a TypeError exception being thrown.

Usage: The `[EnforceRange]` extended attribute MUST NOT appear on a read only attribute, or an attribute, operation argument or dictionary member that is not of an integer type.

`[EnforceRange]` can be specified on writable attributes:

```webidl
interface XXX {
    [EnforceRange] attribute unsigned short attributeName;
};
```

`[EnforceRange]` can be specified on extended attributes on methods arguments:

```webidl
interface GraphicsContext {
    void setColor(octet red, octet green, octet blue);
    void setColorEnforced([EnforceRange] octet red, [EnforceRange] octet green, [EnforceRange] octet blue);
};
```

Calling the non-`[EnforceRange]` version of `setColor()` uses **ToUint8()** to coerce the Numbers to octets. Hence calling `context.setColor(-1, 255, 257)` is equivalent to calling `setColor(255, 255, 1)`.

Calling the `[EnforceRange]` version of `setColorEnforced()` with an out of range value, such as -1, 256, or Infinity will result in a `TypeError` exception.

### [Exposed] _(i, m, a, c)_

Standard: [Exposed](http://heycam.github.io/webidl/#Exposed)

Summary: Indicates on which global object or objects (e.g., Window, WorkerGlobalScope) the interface property is generated, i.e., in which global scope or scopes an interface exists. This is primarily of interest for the constructor, i.e., the [interface object Call method](https://heycam.github.io/webidl/#es-interface-call). Global context defaults to Window (the primary global scope) if not present, overridden by standard extended attribute `[NoInterfaceObject]` (the value of the property on the global object corresponding to the interface is called the **interface object**), which results in no interface property being generated.

As with `[NoInterfaceObject]` does not affect generated code for the interface itself, only the code for the corresponding global object. A partial interface is generated at build time, containing an attribute for each interface property on that global object.

All non-callback interfaces without `[NoInterfaceObject]` have a corresponding interface property on the global object. Note that in the Web IDL spec, callback interfaces with constants also have interface properties, but in Blink callback interfaces only have methods (no constants or attributes), so this is not applicable. `[Exposed]` can be used with different values to indicate on which global object or objects the property should be generated. Valid values are:

* `Window`
* [Worker](http://www.whatwg.org/specs/web-apps/current-work/multipage/workers.html#the-workerglobalscope-common-interface)
* [SharedWorker](http://www.whatwg.org/specs/web-apps/current-work/multipage/workers.html#dedicated-workers-and-the-dedicatedworkerglobalscope-interface)
* [DedicatedWorker](http://www.whatwg.org/specs/web-apps/current-work/multipage/workers.html#shared-workers-and-the-sharedworkerglobalscope-interface)
* [ServiceWorker](https://rawgithub.com/slightlyoff/ServiceWorker/master/spec/service_worker/index.html#service-worker-global-scope)

For reference, see [ECMAScript 5.1: 15.1 The Global Object](http://www.ecma-international.org/ecma-262/5.1/#sec-15.1) ([annotated](http://es5.github.io/#x15.1)), [HTML: 10 Web workers](http://www.whatwg.org/specs/web-apps/current-work/multipage/workers.html), [Web Workers](http://dev.w3.org/html5/workers/), and [Service Workers](https://rawgithub.com/slightlyoff/ServiceWorker/master/spec/service_worker/index.html) specs.

It is possible to have the global constructor generated on several interfaces by listing them, e.g. `[Exposed=(Window,WorkerGlobalScope)]`.

Usage: `[Exposed]` can be specified on interfaces that do not have the `[NoInterfaceObject]` extended attribute.

```webidl
[
    Exposed=DedicatedWorker,
] interface XXX {
    ...
};

[
    Exposed=(Window,Worker),
] interface YYY {
    ...
};
```

Exposed can also be specified with a method, attribute and constant.

As a Blink-specific extension, we allow `Exposed(Arguments)` form, such as `[Exposed(Window Feature1, DedicatedWorker Feature2)]`. You can use this form to vary the exposing global scope based on runtime enabled features. For example, `[Exposed(Window Feature1, Worker Feature2)]` exposes the qualified element to Window if "Feature1" is enabled and to Worker if "Feature2" is enabled.

### [Global] and [PrimaryGlobal] _(i)_

Standard: [Global](http://heycam.github.io/webidl/#Global)

Summary: The `[Global]` and `[PrimaryGlobal]` extended attributes can be used to give a name to one or more global interfaces, which can then be referenced by the `[Exposed]` extended attribute.

These extended attributes must either take no arguments or take an identifier list.

If the `[Global]` or `[PrimaryGlobal]` extended attribute is declared with an identifier list argument, then those identifiers are the interface’s global names; otherwise, the interface has a single global name, which is the interface's identifier.

### [HTMLConstructor]

Standard: [HTMLConstructor](https://html.spec.whatwg.org/multipage/dom.html#html-element-constructors)

Summary: HTML Elements have special constructor behavior. Interface object of given interface with the `[HTMLConstructor]` attribute will have specific behavior when called.

Usage: Must take no arguments, and must not appear on anything other than an interface. It must appear once on an interface, and the interface cannot be annotated with `[Constructor]` or `[NoInterfaceObject]` extended attributes. It must not be used on a callback interface.

### [LenientSetter] _(a)_

Standard: [LenientSetter](https://heycam.github.io/webidl/#LenientSetter)

Summary: `[LenientSetter]` indicates that a no-op setter will be generated for a readonly attribute’s accessor property. This results in erroneous assignments to the property in strict mode to be ignored rather than causing an exception to be thrown.

`[LenientSetter]` must take no arguments, and must not appear on anything other than a readonly regular attribute.

### [LegacyUnenumerableNamedProperties] _(i)_

Standard: [LegacyUnenumerableNamedProperties](https://heycam.github.io/webidl/#LegacyUnenumerableNamedProperties)

Summary: If an IDL interface [supports named properties](https://heycam.github.io/webidl/#dfn-support-named-properties), this extended attribute causes those properties not to be enumerable.

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

### [NamedConstructor] _(i)_

Standard: [NamedConstructor](https://heycam.github.io/webidl/#NamedConstructor)

Summary: If you want to allow JavaScript to create a DOM object of XXX using a different name constructor (i.e. allow JavaScript to create an XXX object using "new YYY()", where YYY != XXX), you can use `[NamedConstructor]`.

Usage: The possible usage is `[NamedConstructor=YYY(...)]`. Just as with constructors, an empty argument list can be omitted, as: `[NamedConstructor=YYY]`. `[NamedConstructor]` can be specified on interfaces. The spec allows multiple named constructors, but the Blink IDL compiler currently only supports at most one.

```webidl
[
    NamedConstructor=Audio(DOMString data),
] interface HTMLAudioElement {
    ...
};
```

The semantics are the same as `[Constructor]`, except that the name changes: JavaScript can make a DOM object by `new Audio()` instead of by `new HTMLAudioElement()`.

Whether you should allow an interface to have a named constructor or not depends on the spec of each interface.

### [NewObject] _(m)_

Standard: [NewObject](https://heycam.github.io/webidl/#NewObject)

Summary: Signals that a method that returns an object type always returns a new object or promise.

When a method returns an interface type, this extended attribute generates a test in debug mode to ensure that no wrapper object for the returned DOM object exists yet. Also see `[DoNotTestNewObject]`. When a method returns a Promise, this extended attribute currently does nothing.

### [NoInterfaceObject] _(i)_

Standard: [NoInterfaceObject](https://heycam.github.io/webidl/#NoInterfaceObject)

Summary: If the `[NoInterfaceObject]` extended attribute appears on an interface, it indicates that an interface object will not exist for the interface in the ECMAScript binding. See also the standard `[Exposed=xxx]` extended attribute; these two do _not_ change the generated code for the interface itself.

Note that every interface has a corresponding property on the ECMAScript global object, _except:_

* callback interfaces with no constants, and
* non-callback interface with the `[NoInterfaceObject]` extended attribute,

Usage: `[NoInterfaceObject]` can be specified on interfaces.

```webidl
[
    NoInterfaceObject,
] interface XXX {
    ...
};
```

Note that `[NoInterfaceObject]` **MUST** be specified on testing interfaces, as follows:

```webidl
[
    NoInterfaceObject, // testing interfaces do not appear on global objects
] interface TestingInterfaceX {
    ...
};
```

### [OverrideBuiltins] _(i)_

Standard: [OverrideBuiltins](http://heycam.github.io/webidl/#OverrideBuiltins)

Summary: Affects named property operations, making named properties shadow built-in properties of the object.

### [PutForwards] _(a)_

Standard: [PutForwards](http://heycam.github.io/webidl/#PutForwards)

Summary: Indicates that assigning to the attribute will have specific behavior. Namely, the assignment is “forwarded” to the attribute (specified by the extended attribute argument) on the object that is currently referenced by the attribute being assigned to.

Usage: Can be specified on `readonly` attributes:

```webidl
[PutForwards=href] readonly attribute Location location;
```

On setting the location attribute, the assignment will be forwarded to the Location.href attribute.

### [Replaceable] _(a)_

Standard: [Replaceable](http://heycam.github.io/webidl/#Replaceable)

Summary: `[Replaceable]` controls if a given read only regular attribute is "replaceable" or not.

Usage: `[Replaceable]` can be specified on attributes:

```webidl
interface DOMWindow {
    [Replaceable] readonly attribute long screenX;
};
```

`[Replaceable]` is _incompatible_ with `[Custom]` and `[Custom=Setter]` (because replaceable attributes have a single interface-level setter callback, rather than individual attribute-level callbacks); if you need custom binding for a replaceable attribute, remove `[Replaceable]` and `readonly`.

Intuitively, "replaceable" means that you can set a new value to the attribute without overwriting the original value. If you delete the new value, then the original value still remains.

Specifically, a writable attribute, without `[Replaceable]`, behaves as follows:

```js
window.screenX; // Evaluates to 0
window.screenX = "foo";
window.screenX; // Evaluates to "foo"
delete window.screenX;
window.screenX; // Evaluates to undefined. 0 is lost.
```

A read only attribute, with `[Replaceable]`, behaves as follows:

```js
window.screenX; // Evaluates to 0
window.screenX = "foo";
window.screenX; // Evaluates to "foo"
delete window.screenX;
window.screenX; // Evaluates to 0. 0 remains.
```

Whether `[Replaceable]` should be specified or not depends on the spec of each attribute.

### [SameObject] _(a)_

Standard: [SameObject](http://heycam.github.io/webidl/#SameObject)

Summary: Signals that a `readonly` attribute that returns an object type always returns the same object.

This attribute has no effect on code generation and should simply be used in Blink IDL files if the specification uses it. If you want the binding layer to cache the resulting object, use `[SaveSameObject]`.

### [SecureContext] _(a, i, m)_

Standard: [SecureContext](https://heycam.github.io/webidl/#SecureContext)

Summary: Interfaces and interface members with a `SecureContext` attribute are exposed only inside ["Secure Contexts"](https://w3c.github.io/webappsec-secure-contexts/).

**Non-standard:** Blink supports adding a value to the `SecureContext` attribute, which specifies a runtime-enabled flag used to control whether or not the restriction applies. This is intended for use when deprecating legacy APIs, and should not be used for new APIs.

For example: we intend to lock `window.applicationCache` to secure contexts, but need to do so in a way that allows some subset of users (enterprises) to opt out. We can do so by defining a `RestrictAppCacheToSecureContexts` runtime flag, and specifying it in IDL as follows:

```webidl
interface Window {
  ...
  [SecureContext=RestrictAppCacheToSecureContexts] readonly attribute ApplicationCache applicationCache;
  ...
}
```

### [Serializable] _(i)_

Standard: [Serializable](https://html.spec.whatwg.org/multipage/structured-data.html#serializable)

Summary: Serializable objects support being serialized, and later deserialized, for persistence in storage APIs or for passing with `postMessage()`.

```webidl
[Serializable] interface Blob {
    ...
};
```

This attribute has no effect on code generation and should simply be used in Blink IDL files if the specification uses it. Code to perform the serialization/deserialization must be added to `V8ScriptValueSerializer` for types in `core/` or `V8ScriptValueDeserializerForModules` for types in `modules/`.

### [Transferable] _(i)_

Standard: [Transferable](https://html.spec.whatwg.org/multipage/structured-data.html#transferable)

Summary: Transferable objects support being transferred across Realms with `postMessage()`.

```webidl
[Transferable] interface MessagePort {
    ...
};
```

This attribute has no effect on code generation and should simply be used in Blink IDL files if the specification uses it. Code to perform the transfer steps must be added to `V8ScriptValueSerializer` for types in `core/` or `V8ScriptValueDeserializerForModules` for types in `modules/`.

### [TreatNullAs] _(a,p)_

Standard: [TreatNullAs](https://heycam.github.io/webidl/#TreatNullAs)

Summary: `[TreatNullAs=EmptyString]` indicates that a JavaScript null is converted to `""` instead of `"null"`.

Usage: Can be specified on DOMString attributes or DOMString parameters only:

```webidl
[TreatNullAs=EmptyString] attribute DOMString str;
void func([TreatNullAs=Emptytring] DOMString str);
```

Implementation: Given `[TreatNullAs=EmptyString]`, a JavaScript null is converted to a Blink empty string, for which `String::IsEmpty()` returns true, but `String::IsNull()` return false.

### [Unforgeable] _(m,a)_

Standard: [Unforgeable](http://heycam.github.io/webidl/#Unforgeable)

Summary: Makes interface members unconfigurable and also controls where the member is defined.

Usage: Can be specified on interface methods or non-static interface attributes:

```webidl
[Unforgeable] void func();
[Unforgeable] attribute DOMString str;
```

By default, interface members are configurable (i.e. you can modify a property descriptor corresponding to the member and also you can delete the property). `[Unforgeable]` makes the member unconfiguable so that you cannot modify or delete the property corresponding to the member.

`[Unforgeable]` changes where the member is defined, too. By default, attribute getters/setters and methods are defined on a prototype chain. `[Unforgeable]` defines the member on the instance object instead of the prototype object.

### [Unscopable] _(o, a)_

Standard: [Unscopable](http://heycam.github.io/webidl/#Unscopable)

Summary: The interface member will not appear as a named property within `with` statements.

Usage: Can be specified on attributes or interfaces.

## Common Blink-specific IDL Extended Attributes

These extended attributes are widely used.

### [ActiveScriptWrappable] _(i)_

Summary: `[ActiveScriptWrappable]` indicates that a given DOM object should be kept alive as long as the DOM object has pending activities.

Usage: `[ActiveScriptWrappable]` can be specified on interfaces, and **is inherited**:

```webidl
[
    ActiveScriptWrappable,
] interface Foo {
    ...
};
```

If an interface X has `[ActiveScriptWrappable]` and an interface Y inherits the interface X, then the interface Y will also have `[ActiveScriptWrappable]`. For example

```webidl
[
    ActiveScriptWrappable,
] interface Foo {};

interface Bar : Foo {};  // inherits [ActiveScriptWrappable] from Foo
```

If a given DOM object needs to be kept alive as long as the DOM object has pending activities, you need to specify `[ActiveScriptWrappable]`. For example, `[ActiveScriptWrappable]` can be used when the DOM object is expecting events to be raised in the future.

If you use `[ActiveScriptWrappable]`, the corresponding Blink class needs to inherit ActiveScriptWrappable and override hasPendingActivity(). For example, in case of XMLHttpRequest, core/xml/XMLHttpRequest.h would look like this:

```c++
class XMLHttpRequest : public ActiveScriptWrappable<XMLHttpRequest>
{
    // Returns true if the object needs to be kept alive.
    bool hasPendingActivity() const override { return ...; }
}
```

### [PerWorldBindings] _(m, a)_

Summary: Generates faster bindings code by avoiding check for isMainWorld().

This optimization only makes sense for wrapper-types (i.e. types that have a corresponding IDL interface), as we don't need to check in which world we are for other types.

*** note
This optimization works by generating 2 separate code paths for the main world and for isolated worlds. As a consequence, using this extended attribute will increase binary size and we should refrain from overusing it.
***

### [LogActivity] _(m, a)_

Summary: logs activity, using V8PerContextData::activityLogger. Widely used. Interacts with `[PerWorldBindings]`, `[LogAllWorlds]`.

Usage:

* Valid values for attributes are: `GetterOnly`, `SetterOnly`, (no value)
* Valid values for methods are: (no value)

For methods all calls are logged, and by default for attributes all access (calls to getter or setter) are logged, but this can be restricted to just read (getter) or just write (setter).

### [CallWith] _(m, a)_, [SetterCallWith] _(a)_, [ConstructorCallWith] _(i)_

Summary: `[CallWith]` indicates that the bindings code calls the Blink implementation with additional information.

Each value changes the signature of the Blink methods by adding an additional parameter to the head of the parameter list, such as `ScriptState*` for `[CallWith=ScriptState]`.

There are also three rarely used values: `CurrentWindow`, `EnteredWindow`, `ThisValue`.

`[SetterCallWith]` applies to attributes, and only affects the signature of the setter; this is only used in Location.idl, with `CurrentWindow&EnteredWindow`.

#### [CallWith=ScriptState] _(m, a*)_

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

#### [CallWith=ExecutionContext] _(m,a)_  _deprecated_

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

_(rare CallWith values)_

#### [CallWith=CurrentWindow&EnteredWindow] _(m, a)_

`EnteredWindow` is the `Window` object that corresponds to the responsible document of the entry settings object.

#### [CallWith=ThisValue] _(m)_

`[CallWith=ThisValue]` only applies to methods in callback interfaces, and is used in only one place (CSSVariablesMapForEachCallback.idl).

IDL example:

```webidl
callback interface Example {
    [CallWith=ThisValue] boolean func(boolean a, boolean b);
};
```

C++ Blink function signature:

```c++
bool Example::func(ScriptValue thisValue, bool a, bool b);
```

*** note
`[CallWith=...]` arguments are added at the _head_ of `XXX::create(...)'s` arguments, and ` [RaisesException]`'s `ExceptionState` argument is added at the _tail_ of `XXX::create(...)`'s arguments.
***

#### [ConstructorCallWith] _(i)_

Analogous to `[CallWith]`, but applied to interfaces with constructors, and takes different values.

If `[Constructor]` is specified on an interface, `[ConstructorCallWith]` can be also specified to refine the arguments passed to the callback:

```webidl
[
    Constructor(float x, float y, DOMString str),
    ConstructorCallWith=ExecutionContext,
]
interface XXX {
    ...
};
```

Then XXX::create(...) can have the following signature

*** note
**FIXME:** outdated
***

```c++
scoped_refptr<XXX> XXX::create(ExecutionContext* context, float x, float y, String str)
{
    ...;
}
```

Be careful when you use `[ConstructorCallWith=ScriptState]`.
You should not store the passed-in ScriptState on a DOM object.
This is because if the stored ScriptState is used by some method called by a different
world (note that the DOM object is shared among multiple worlds), it leaks the ScriptState
to the world. ScriptState must be carefully maintained in a way that doesn't leak
to another world.

### [ContextEnabled] _(i)_

Summary: `[ContextEnabled]` renders the generated interface bindings unavailable by default, but also generates code which allows individual script contexts opt into installing the bindings.

Usage: `[ContextEnabled=FeatureName]`. FeatureName is an arbitrary name used to identify the feature at runtime.

```webidl
[
    ContextEnabled=MojoJS
] interface Mojo { ... };
```

When applied to an interface, the generated code for the relevant global object will include a public `installFeatureName()` method which can be called to install the interface on the global object.

Note that `[ContextEnabled]` is not mututally exclusive to `[RuntimeEnabled]`, and a feature which may be enabled by either mechanism will be enabled if the appropriate `[RuntimeEnabled]` feature is enabled; _or_ if the appropriate `[ContextEnabled]` feature is enabled; _or_ if both are enabled.

### [Custom] _(i, m, s, a, f)_

Summary: They allow you to write bindings code manually as you like: full bindings for methods and attributes, certain functions for interfaces.

Custom bindings are _strongly discouraged_. They are likely to be buggy, a source of security holes, and represent a significant maintenance burden. Before using `[Custom]`, you should doubly consider if you really need custom bindings. You are recommended to modify code generators and add specialized extended attributes or special cases if necessary to avoid using `[Custom]`.

Usage: `[Custom]` can be specified on methods or attributes. `[Custom=CallEpilogue]` can be specified on methods. `[Custom=Getter]` and `[Custom=Setter]` can be specified on attributes. `[Custom=A|B]` can be specified on interfaces, with various values (see below).

On read only attributes (that are not `[Replaceable]`), `[Custom]` is equivalent to `[Custom=Getter]` (since there is no setter) and `[Custom=Getter]` is preferred.

The bindings generator largely _ignores_ the specified type information of `[Custom]` members (signature of methods and type of attributes), but they must be valid types. It is best if the signature exactly matches the spec, but if one of the types is an interface that is not implemented, you can use object as the type instead, to indicate "unspecified object type".

`[Replaceable]` is _incompatible_ with `[Custom]` and `[Custom=Setter]` (because replaceable attributes have a single interface-level setter callback, rather than individual attribute-level callbacks); if you need custom binding for a replaceable attribute, remove `[Replaceable]` and readonly.

```webidl
[Custom] void func();
[Custom] attribute DOMString str1;
[Custom=Getter] readonly attribute DOMString str2;
[Custom=Setter] attribute DOMString str3;
```

Before explaining the details, let us clarify the relationship of these IDL attributes.

* `[Custom]` on a method indicates that you can write V8 custom bindings for the method.
* `[Custom=CallEpilogue]` on a method indicates that the normal code is generated for the method, but with an extra call to an auxiliary custom bindings callback at the end.
* `[Custom=Getter]` or `[Custom=Setter]` on an attribute means custom bindings for the attribute getter or setter.
* `[Custom]` on an attribute means custom bindings for both the getter and the setter

Methods:

```webidl
interface XXX {
  [Custom] void func();
  [Custom=CallEpilogue] void func2();
};
```

You can write custom bindings in Source/bindings/v8/custom/V8XXXCustom.cpp:

```c++
void V8XXX::funcMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ...;
}

void V8XXX::func2MethodEpilogueCustom(const v8::FunctionCallbackInfo<v8::Value>& info, V8XXX* impl)
{
    ...;
}
```

Attribute getter:

```webidl
interface XXX {
    [Custom=Getter] attribute DOMString str;
};
```

You can write custom bindings in Source/bindings/v8/custom/V8XXXCustom.cpp:

```c++
void V8XXX::strAttributeGetterCustom(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    ...;
}
```

Attribute setter:

```webidl
interface XXX {
    [Custom=Setter] attribute DOMString str;
};
```

You can write custom bindings in Source/bindings/v8/custom/V8XXXCustom.cpp:

```c++
void V8XXX::strAttributeSetterCustom(v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    ...;
}
```

`[Custom]` may also be specified on special operations:

```webidl
interface XXX {  // anonymous special operations
    [Custom] getter Node (unsigned long index);
    [Custom] setter Node (unsigned long index, Node value);
    [Custom] deleter boolean (unsigned long index);

    [Custom] getter Node (DOMString name);
    [Custom] setter Node (DOMString name, Node value);
    [Custom] deleter boolean (DOMString name);
}
interface YYY {  // special operations with identifiers
    [Custom] getter Node item(unsigned long index);
    [Custom] getter Node namedItem(DOMString name);
}
```

`[Custom]` may also be specified on callback functions:

```webidl
[Custom] callback SomeCallback = void ();
interface XXX {
    void func(SomeCallback callback);
};
```

When`[Custom]` is specified on a callback function, the code generator doesn't
generate bindings for the callback function. The binding layer uses a
`ScriptValue` instead.

#### [Custom=PropertyQuery|PropertyEnumerator] _(s)_

Summary: `[Custom=PropertyEnumerator]` allows you to write custom bindings for the case where properties of a given interface are enumerated; a custom named enumerator. There is currently only one use, and in that case it is used with `[Custom=PropertyQuery]`, since the query is also custom.

Usage: Can be specified on named property getters:

```webidl
interface XXX {
    [Custom=PropertyQuery|PropertyEnumerator] getter Foo (DOMString name);
};
```

If the property getter itself should also be custom, specify `[Custom=PropertyGetter]` (this is the default, if no arguments are given).

```webidl
interface XXX {
    [Custom=PropertyGetter|PropertyQuery|PropertyEnumerator] getter Foo (DOMString name);
};
```

You can write custom bindings as V8XXX::namedPropertyQuery(...) and V8XXX::namedPropertyEnumerator(...) in Source/bindings/v8/custom/V8XXXCustom.cpp:

```c++
v8::Local<v8::Integer> V8XXX::namedPropertyQuery(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    ...;
}

v8::Local<v8::Array> V8XXX::namedPropertyEnumerator(const v8::AccessorInfo& info)
{
    ...;
}
```

#### [Custom=LegacyCallAsFunction] _(i) _deprecated_

Summary: `[Custom=LegacyCallAsFunction]` allows you to write custom bindings for call(...) of a given interface.

Usage: `[Custom=LegacyCallAsFunction]` can be specified on interfaces:

```webidl
[
    Custom=LegacyCallAsFunction,
] interface XXX {
    ...
};
```

If you want to write custom bindings for XXX.call(...), you can use `[Custom=LegacyCallAsFunction]`.

You can write custom `V8XXX::callAsFunctionCallback(...)` in Source/bindings/v8/custom/V8XXXCustom.cpp:

```c++
v8::Local<v8::Value> V8XXX::callAsFunctionCallback(const v8::Arguments& args)
{
    ...;
}
```

### [CustomElementCallbacks] _(m, a)_

Summary: Wraps the method/accessor with a Custom Elements "callback delivery scope" which will dispatch Custom Element callbacks (createdCallback, attributeChangedCallback, etc.) before returning to script.

*** note
This attribute is only for Custom Elements V0,
and is superceded by `[CEReactions]` for V1.
***

If the method/accessor creates elements or modifies DOM nodes in any way, it should be tagged with this extended attribute. Even if you're not a Node, this may apply to you! For example [DOMTokenList.toggle](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/core/dom/dom_token_list.idl&l=34) can be reflected in the attribute of its associated element, so it needs to be tagged with CustomElementCallbacks. If the method/accessor only calls something that may modify the DOM (for example, it runs user script as a callback) you don't need to tag your method with `[CustomElementCallbacks]`; that is the responsibility of the binding that actually modifies the DOM. In general over-applying this extended attribute is safe, with one caveat:

* This extended attribute MUST NOT be used on members that operate on non-main threads, because the callback delivery scope accesses statics.
* Basically: Don't apply this extended attribute to anything that can be called from a worker.
* This criterion (accessible by workers) depends on implementation and cannot easily be checked from the IDL or C++ headers (it includes obvious cases like `[Exposed=Worker]`, where there is a constructor on the (JS) global object, but also cases where the C++ creates or accesses methods internally), so please be careful.

Usage: `[CustomElementCallbacks]` takes no arguments.

### [Default] _(p)_

Summary: `[Default]` allows one to specify the default values for optional arguments. This removes the need to have C++ overloads in the Blink implementation.

Standard: In Web IDL, [default values for optional arguments](https://heycam.github.io/webidl/#dfn-optional-argument-default-value) are written as `optional type identifier = value`. Blink supports this but not all implementations have been updated to handle overloaded functions - see [bug 258153](https://crbug.com/258153). `[Default=Undefined]` was added to all optional parameters to preserve compatibility until the C++ implementations are updated.

Usage: `[Default=Undefined]` can be specified on any optional parameter:

```webidl
interface HTMLFoo {
    void func1(int a, int b, optional int c, optional int d);
    void func2(int a, int b, [Default=Undefined] optional int c);
};
```

The parameters marked with the standard Web IDL `optional` qualifier are optional, and JavaScript can omit the parameters. Obviously, if parameter X is marked with `optional` then all subsequent parameters of X should be marked with `optional`.

The difference between `optional` and `[Default=Undefined]` optional is whether the Blink implementation requires overloaded methods or not: without a default value, the Blink implementation must have overloaded C++ functions, while with a default value, the Blink implementation only needs a single C++ function.

In case of func1(...), if JavaScript calls func1(100, 200), then HTMLFoo::func1(int a, int b) is called in Blink. If JavaScript calls func1(100, 200, 300), then HTMLFoo::func1(int a, int b, int c) is called in Blink. If JavaScript calls func1(100, 200, 300, 400), then HTMLFoo::func1(int a, int b, int c, int d) is called in Blink. In other words, if the Blink implementation has overloaded methods, you can use `optional`.

In case of func2(...) which adds `[Default=Undefined]`, if JavaScript calls func2(100, 200), then it behaves as if JavaScript called func2(100, 200, undefined). Consequently, HTMLFoo::func2(int a, int b, int c) is called in Blink. 100 is passed to a, 200 is passed to b, and 0 is passed to c. (A JavaScript `undefined` is converted to 0, following the value conversion rule in the Web IDL spec; if it were a DOMString parameter, it would end up as the string `"undefined"`.) In this way, Blink needs to just implement func2(int a, int b, int c) and needs not to implement both func2(int a, int b) and func2(int a, int b, int c).

### [DeprecateAs] _(m, a, c)_

Summary: Measures usage of a deprecated feature via UseCounter, and notifies developers about deprecation via a console warning.

`[DeprecateAs]` can be considered an extended form of `[MeasureAs]`: it both measures the feature's usage via the same UseCounter mechanism, and also sends out a warning to the console (optionally with a message) in order to inform developers that the code they've written will stop working at some point in the relatively near future.

Usage: `[DeprecateAs]` can be specified on methods, attributes, and constants.

```webidl
    [DeprecateAs=DeprecatedPrefixedAttribute] attribute Node prefixedAttribute;
    [DeprecateAs=DeprecatedPrefixedMethod] Node prefixedGetInterestingNode();
    [DeprecateAs=DeprecatedPrefixedConstant] const short DEPRECATED_PREFIXED_CONSTANT = 1;
```

The deprecation message show on the console can be specified via the [UseCounter::deprecationMessage](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/core/frame/use_counter.cc&q=UseCounter::deprecationMessage&l=615) method.

### [DoNotTestNewObject] _(m)_

Summary: Does not generate a test for `[NewObject]` in the binding layer.

When specified, does not generate a test for `[NewObject]`. Some implementation creates a new DOM object and its wrapper before passing through the binding layer. In that case, the generated test doesn't make sense. See Text.splitText() for example.

### [Measure] _(i, m, a, c)_

Summary: Measures usage of a specific feature via UseCounter.

In order to measure usage of specific features, Chrome submits anonymous statistics through the Histogram recording system for users who opt-in to sharing usage statistics. This extended attribute hooks up a specific feature to this measurement system.

Usage: `[Measure]` can be specified on interfaces, methods, attributes, and constants. When specified on an interface usage of the constructor will be measured. The generated feature name must be added to [UseCounter::Feature](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/core/frame/use_counter.h&q=%22enum%20Feature%22&sq=package:chromium&type=cs&l=61) (in [core/frame/use_counter.h](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/core/frame/use_counter.h)).

```webidl
[Measure] attribute Node interestingAttribute;
[Measure] Node getInterestingNode();
[Measure] const INTERESTING_CONSTANT = 1;
```

### [MeasureAs] _(i, m, a, c)_

Summary: Like `[Measure]`, but the feature name is provided as the extended attribute value.
This is similar to the standard `[DeprecateAs]` extended attribute, but does not display a deprecation warning.

Usage: `[MeasureAs]` can be specified on interfaces, methods, attributes, and constants. The value must match one of the enumeration values in [UseCounter::Feature](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/core/frame/use_counter.h&q=%22enum%20Feature%22&sq=package:chromium&type=cs&l=61) (in [core/frame/use_counter.h](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/core/frame/use_counter.h)).

```webidl
[MeasureAs=AttributeWeAreInterestedIn] attribute Node interestingAttribute;
[MeasureAs=MethodsAreInterestingToo] Node getInterestingNode();
[MeasureAs=EvenSomeConstantsAreInteresting] const INTERESTING_CONSTANT = 1;
```

### [NotEnumerable] _(m, a, s)_

*** note
**FIXME:** docs out of date!
***

Specification: [The spec of Writable, Enumerable and Configurable (Section 8.6.1)](http://www.ecma-international.org/publications/files/ECMA-ST/Ecma-262.pdf) - _not standard Web IDL extended attributes._

Summary: Controls the enumerability of methods and attributes.

Usage: `[NotEnumerable]` can be specified on methods and attributes

```webidl
[NotEnumerable] attribute DOMString str;
[NotEnumerable] void foo();
```

`[NotEnumerable]` indicates that the method or attribute is not enumerable.

### [OriginTrialEnabled] _(i, m, a, c)_

Summary: Like `[RuntimeEnabled]`, it controls at runtime whether bindings are exposed, but uses a different mechanism for enabling experimental features.

Usage: `[OriginTrialEnabled=FeatureName]`. FeatureName must be included in [runtime\_enabled\_features.json5](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/platform/runtime_enabled_features.json5), and is the same value that would be used with `[RuntimeEnabled]`.

```webidl
[
    OriginTrialEnabled=MediaSession
] interface MediaSession { ... };
```

When there is an active origin trial for the current execution context, the feature is enabled at runtime, and the binding would be exposed to the web. `[OriginTrialEnabled]` also includes a check for the associated runtime flag, so features can be enabled in that fashion, even without an origin trial.

`[OriginTrialEnabled]` has similar semantics to `[RuntimeEnabled]`, and is intended as a drop-in replacement. For example, `[OriginTrialEnabled]` _cannot_ be applied to arguments, see `[RuntimeEnabled]` for reasoning. The key implementation difference is that `[OriginTrialEnabled]` wraps the generated code with `if (OriginTrials::FeatureNameEnabled(...)) { ...code... }`.

For more information, see [RuntimeEnabledFeatures](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/platform/runtime_enabled_features.json5) and [OriginTrialContext](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/core/origin_trials/origin_trial_context.h).

*** note
**FIXME:** Currently, `[OriginTrialEnabled]` can only be applied to interfaces, attributes, and constants. Methods (including those generated by `iterable`, `setlike`, `maplike`, `serializer` and `stringifier`) are not supported. See [Bug 621641](https://crbug.com/621641).
***

### [RaisesException] _(i, m, a)_

Summary: Tells the code generator to append an `ExceptionState&` argument when calling the Blink implementation.

Implementations may use the methods on this parameter (e.g. `ExceptionState::throwDOMException`) to throw exceptions.

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
long XXX::count(ExceptionState& exceptionState) {
    if (...) {
        exceptionState.throwDOMException(TypeMismatchError, ...);
        return;
    }
    ...;
}

void XXX::setCount(long value, ExceptionState& exceptionState) {
    if (...) {
        exceptionState.throwDOMException(TypeMismatchError, ...);
        return;
    }
    ...;
}

void XXX::foo(ExceptionState& exceptionState) {
    if (...) {
        exceptionState.throwDOMException(TypeMismatchError, ...);
        return;
    }
    ...;
};
```

If `[RaisesException=Constructor]` is specified on an interface and `[Constructor]` is also specified then an `ExceptionState&` argument is added when calling the `XXX::create(...)` constructor callback.

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
scoped_refptr<XXX> XXX::create(float x, ExceptionState& exceptionState)
{
    ...;
    if (...) {
        exceptionState.throwDOMException(TypeMismatchError, ...);
        return nullptr;
    }
    ...;
}
```

### [Reflect] _(a)_

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

### [ReflectEmpty="value"] _(a)_

Specification: [Enumerated attributes](http://www.whatwg.org/specs/web-apps/current-work/#enumerated-attribute) - _defined in spec prose, not as an IDL extended attribute._

Summary: `[ReflectEmpty="value"]` gives the attribute keyword value to reflect when an attribute is present, but without a value; it supplements `[ReflectOnly]` and `[Reflect]`.

Usage: The possible usage is `[ReflectEmpty="value"]` in combination with `[ReflectOnly]`:

```webidl
interface HTMLMyElement {
    [Reflect, ReflectOnly="for"|"against", ReflectEmpty="for"] attribute DOMString vote;
};
```

The `[ReflectEmpty]` extended attribute specifies the value that an IDL getter for the `vote` attribute should return when the content attribute is present, but without a value (e.g., return `"for"` when accessing the `vote` IDL attribute on `<my-element vote/>`.) Its (string) literal value must be one of the possible values that the `[ReflectOnly]` extended attribute lists.

`[ReflectEmpty]` should be used if the specification for the content attribute has an empty attribute value mapped to some attribute state. For HTML, this applies to [enumerated attributes](http://www.whatwg.org/specs/web-apps/current-work/#enumerated-attribute) only.

### [ReflectInvalid="value"] _(a)_

Specification: [Limited value attributes](http://www.whatwg.org/specs/web-apps/current-work/#limited-to-only-known-values) - _defined in spec prose, not as an IDL extended attribute._

Summary: `[ReflectInvalid="value"]` gives the attribute keyword value to reflect when an attribute has an invalid/unknown value. It supplements `[ReflectOnly]` and `[Reflect]`.

Usage: The possible usage is `[ReflectInvalid="value"]` in combination with `[ReflectOnly]`:

```webidl
interface HTMLMyElement {
    [Reflect, ReflectOnly="left"|"right", ReflectInvalid="left"] attribute DOMString direction;
};
```

The `[ReflectInvalid]` extended attribute specifies the value that an IDL getter for the `direction` attribute should return when the content attribute has an unknown value (e.g., return `"left"` when accessing the `direction` IDL attribute on `<my-element direction=dont-care />`.) Its (string) literal value must be one of the possible values that the `[ReflectOnly]` extended attribute lists.

`[ReflectInvalid]` should be used if the specification for the content attribute has an _invalid value state_ defined. For HTML, this applies to [enumerated attributes](http://www.whatwg.org/specs/web-apps/current-work/#enumerated-attribute) only.

### [ReflectMissing="value"] _(a)_

Specification: [Limited value attributes](http://www.whatwg.org/specs/web-apps/current-work/#limited-to-only-known-values) - _defined in spec prose, not as an IDL extended attribute._

Summary: `[ReflectMissing="value"]` gives the attribute keyword value to reflect when an attribute isn't present. It supplements `[ReflectOnly]` and `[Reflect]`.

Usage: The possible usage is `[ReflectMissing="value"]` in combination with `[ReflectOnly]`:

```webidl
interface HTMLMyElement {
    [Reflect, ReflectOnly="ltr"|"rtl"|"auto", ReflectMissing="auto"] attribute DOMString preload;
};
```

The `[ReflectMissing]` extended attribute specifies the value that an IDL getter for the `direction` attribute should return when the content attribute is missing (e.g., return `"auto"` when accessing the `preload` IDL attribute on `<my-element>`.) Its (string) literal value must be one of the possible values that the `[ReflectOnly]` extended attribute lists.

`[ReflectMissing]` should be used if the specification for the content attribute has a _missing value state_ defined. For HTML, this applies to [enumerated attributes](http://www.whatwg.org/specs/web-apps/current-work/#enumerated-attribute) only.

### [ReflectOnly=&lt;list&gt;] _(a)_

Specification: [Limited value attributes](http://www.whatwg.org/specs/web-apps/current-work/#limited-to-only-known-values) - _defined in spec prose, not as an IDL extended attribute._

Summary: `[ReflectOnly=<list>]` indicates that a reflected string attribute should be limited to a set of allowable values; it supplements `[Reflect]`.

Usage: The possible usage is `[ReflectOnly="A1"|...|"An"]` where A1 (up to n) are the attribute values allowed. `[ReflectOnly=<list>]` is used in combination with `[Reflect]`:

```webidl
interface HTMLMyElement {
    [Reflect, ReflectOnly="on"] attribute DOMString toggle;
    [Reflect=q, ReflectOnly="first"|"second"|"third"|"fourth"] attribute DOMString quarter;
};
```

The ReflectOnly attribute limits the range of values that the attribute getter can return from its reflected attribute. If the content attribute has a value that is a case-insensitive match for one of the values given in the `ReflectOnly`'s list (using "`|`" as separator), then it will be returned. To allow attribute values that use characters that go beyond what IDL identifiers may contain, string literals are used. This is a Blink syntactic extension to extended attributes.

If there is no match, the empty string will be returned. As required by the specification, no such checking is performed when the reflected IDL attribute is set.

`[ReflectOnly=<list>]` should be used if the specification for a reflected IDL attribute says it is _"limited to only known values"_.

### [RuntimeEnabled] _(i, m, a, c)_

Summary: `[RuntimeEnabled]` wraps the generated code with `if (RuntimeEnabledFeatures::FeatureNameEnabled) { ...code... }`.

Usage: `[RuntimeEnabled=FeatureName]`. FeatureName must be included in [runtime\_enabled\_features.json5](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/platform/runtime_enabled_features.json5).

```webidl
[
    RuntimeEnabled=MediaSession
] interface MediaSession { ... };
```

Only when the feature is enabled at runtime (using a command line flag, for example, or when it is enabled only in certain platforms), the binding would be exposed to the web.

`[RuntimeEnabled]` _cannot_ be applied to arguments, as this changes signatures and method resolution and is both very confusing to reason about and implement. For example, what does it mean to mark a _required_ argument as `[RuntimeEnabled]`? You probably want to apply it only to optional arguments, which are equivalent to overloads. Thus instead apply `[RuntimeEnabled]` to the _method_, generally splitting a method in two. For example, instead of:

```webidl
foo(long x, `[RuntimeEnabled=FeatureName]` optional long y); // Don't do this!
```

do:

```webidl
// Overload can be replaced with optional if `[RuntimeEnabled]` is removed
foo(long x);
[RuntimeEnabled=FeatureName] foo(long x, long y);
```

For more information, see [RuntimeEnabledFeatures](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/blink/renderer/platform/runtime_enabled_features.json5).

### [SaveSameObject] _(a)_

Summary: Caches the resulting object and always returns the same object.

When specified, caches the resulting object and returns it in later calls so that the attribute always returns the same object. Must be accompanied with `[SameObject]`.

## Rare Blink-specific IDL Extended Attributes

These extended attributes are rarely used, generally only in one or two places. These are often replacements for `[Custom]` bindings, and may be candidates for deprecation and removal.

### [CachedAttribute] _(a)_

Summary: For performance optimization, `[CachedAttribute]` indicates that a wrapped object should be cached on a DOM object. Rarely used.

Usage: `[CachedAttribute]` can be specified on attributes, and takes a required value, generally called is*Dirty (e.g. isValueDirty):

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
void Object::setValue(Type data)
{
    m_data = data;
    m_attributeDirty = true;
}

// Called by generated binding code
bool Object::isAttributeDirty() const
{
    return m_attributeDirty;
}

// Called by generated binding code if no value cached or isAttributeDirty() returns true
ScriptValue Object::attribute(ExecutionContext* context)
{
    m_attributeDirty = false;
    return convertDataToScriptValue(m_data);
}
```

### [CheckSecurity] _(i, m, a)_

Summary: Check whether a given access is allowed or not in terms of the
same-origin security policy.

*** note
It is very important to use this attribute for interfaces and properties that
are exposed cross-origin!
***

Usage for interfaces: `[CheckSecurity=Receiver]` enables a security check for
all methods of an interface. The security check verifies that the caller still
has access to the receiver object of the method when it is invoked. This is
security-critical for interfaces that can be returned cross-origin, such as the
Location or Window interface.

```webidl
[
    CheckSecurity=Receiver,
] interface DOMWindow {
    Selection? getSelection();
};
```

Forgetting this attribute would make it possible to cache a method reference and
invoke it on a cross-origin object:

```js
var iframe = document.body.appendChild(document.createElement('iframe'));
var addEventListenerMethod = iframe.contentWindow.addEventListener;
iframe.src = 'https://example.com';
iframe.onload = function () {
  addEventListenerMethod('pointermove', function (event) {
    event.target.ownerDocument.body.innerText = 'Text from a different origin.';
  });
};
```

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

### [CrossOrigin] _(m, a)_

Summary: Allows cross-origin access to an attribute or method. Used for
implementing [CrossOriginProperties] from the spec in Location.idl and
Window.idl.

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

### [CustomConstructor] _(i)_

Summary: They allow you to write custom bindings for constructors.

Usage: They can be specified on interfaces. _Strongly discouraged._ As with `[Custom]`, it is generally better to modify the code generator. Incompatible with `[Constructor]` – you cannot mix custom constructors and generated constructors.

```webidl
[
    CustomConstructor(float x, float y, optional DOMString str),
] interface XXX {
    ...
};
```

Note that the arguments of the constructor MUST be specified so that the `length` property of the interface object is properly set, even though they do not affect the signature of the custom Blink code. Multiple `[CustomConstructor]` extended attributes are allowed; if you have overloading, this is good style, as it documents the interface, though the only effect on generated code is to change `length` (you need to write overload resolution code yourself).

Consider the following example:

```webidl
[
    CustomConstructor(float x, float y, optional DOMString str),
] interface XXX {
    ...
};
```

Then you can write custom bindings in Source/bindings/v8/custom/V8XXXConstructorCustom.cpp:

```c++
v8::Local<v8::Value> V8XXX::constructorCallback(const v8::Arguments& args)
{
   ...;
}
```

### [FlexibleArrayBufferView]

Summary: `[FlexibleArrayBufferView]` wraps a parameter that is known to be an ArrayBufferView (or a subtype of, e.g. typed arrays) with a FlexibleArrayBufferView.

The FlexibleArrayBufferView itself can then either refer to an actual ArrayBufferView or a temporary copy (for small payloads) that may even live on the stack. The idea is that copying the payload on the stack and referring to the temporary copy saves creating global handles (resulting in weak roots) in V8. Note that `[FlexibleArrayBufferView]`  will actually result in a TypedFlexibleArrayBufferView wrapper for typed arrays.

Usage: Applies to arguments of methods. See modules/webgl/WebGLRenderingContextBase.idl for an example.

### [AllowShared] _(p)_

Summary: `[AllowShared]` indicates that a parameter, which must be an ArrayBufferView (or subtype of, e.g. typed arrays), is allowed to be backed by a SharedArrayBuffer.

Usage: `[AllowShared]` must be specified on a parameter to a method:

```webidl
interface Context {
    void bufferData1([AllowShared] ArrayBufferView buffer);
    void bufferData2([AllowShared] Float32Array buffer);
}
```

A SharedArrayBuffer is a distinct type from an ArrayBuffer, but both types use ArrayBufferViews to view the data in the buffer. Most methods do not permit an ArrayBufferView that is backed by a SharedArrayBuffer, and will throw an exception. This attribute indicates that this method permits a shared ArrayBufferView.

### [PermissiveDictionaryConversion] _(p, d)_

Summary: `[PermissiveDictionaryConversion]` relaxes the rules about what types of values may be passed for an argument of dictionary type.

Ordinarily when passing in a value for a dictionary argument, the value must be either undefined, null, or an object. In other words, passing a boolean value like true or false must raise TypeError. The PermissiveDictionaryConversion extended attribute ignores non-object types, treating them the same as undefined and null. In order to effect this change, this extended attribute must be specified both on the dictionary type as well as the arguments of methods where it is passed. It exists only to eliminate certain custom bindings.

Usage: applies to dictionaries and arguments of methods. Takes no arguments itself.

### [RuntimeCallStatsCounter] _(m, a)_

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

### [URL] _(a)_

Summary: `[URL]` indicates that a given DOMString represents a URL.

Usage: `[URL]` can be specified on DOMString attributes that have `[Reflect]` extended attribute specified only:

```webidl
[Reflect, URL] attribute DOMString url;
```

You need to specify `[URL]` if a given DOMString represents a URL, since getters of URL attributes need to be realized in a special routine in Blink, i.e. `Element::getURLAttribute(...)`. If you forgot to specify `[URL]`, then the attribute getter might cause a bug.

Only used in some HTML*ELement.idl files and one other place.

## Temporary Blink-specific IDL Extended Attributes

These extended attributes are _temporary_ and are only in use while some change is in progress. Unless you are involved with the change, you can generally ignore them, and should not use them.

### [LegacyTreatAsPartialInterface] _(i)_

Summary: `[LegacyTreatAsPartialInterface]` on an interface that is the target of an `implements` statement means that the interface is treated as a partial interface, meaning members are accessed via static member functions in a separate class, rather than as instance methods on the instance object `*impl` or class methods on the C++ class implementing the (main) interface. This is legacy from original implementation of `implements`, and is being removed ([Bug 360435](https://crbug.com/360435), nbarth@).


### [CachedAccessor] _(a)_

Summary: Caches the accessor result in a private property (not directly accesible from JS). Improves accessor reads (getter) at the expense of extra memory and manual invalidation which should be trivial in most cases.


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
V8PrivateProperty::getHTMLFooBarCachedAccessor().set(context, object, newValue);
```


### [Affects] _(m, a)_

Summary: `[Affects=Nothing]` indicates that a function must not produce JS-observable side effects. Functions without this attribute are never invoked by V8 with throwOnSideEffect.

Marked functions are allowed to be nondeterministic, throw exceptions, force layout, and recalculate style, but must not set values, cache objects, or schedule execution that will be observable after the function completes. If a marked function calls into V8, it must properly handle cases when the V8 call returns an MaybeHandle.

All DOM constructors are assumed to have side effects. However, an exception can be explicitly indicated when calling constructors using the V8 API method Function::NewInstanceWithSideEffectType().

There is not yet support for marking SymbolKeyedMethodConfigurations as side-effect free. This requires additional support in V8 to whitelist Intrinsics.

Usage for attributes and operations: `[Affects=Nothing]` can be specified on an operation, or on an attribute to indicate that its getter callback is side effect free:

```webidl
interface HTMLFoo {
    [Affects=Nothing] attribute Bar bar;
    [Affects=Nothing] Bar baz();
    void removeItems();
};
```


## Discouraged Blink-specific IDL Extended Attributes

These extended attributes are _discouraged_ - they are not deprecated, but they should be avoided and removed if possible.

### [DoNotCheckConstants] _(i)_

Summary: `[DoNotCheckConstants]` indicates that constant values in an IDL file can be different from constant values in Blink implementation.

Usage: `[DoNotCheckConstants]` can be specified on interfaces:

```webidl
[
    DoNotCheckConstants,
] interface XXX {
    const unsigned short NOT_FOUND_ERR = 12345;
    const unsigned short SYNTAX_ERR = 12346;
};
```

By default (i.e. without `[DoNotCheckConstants]`), compile-time assertions are inserted to check if the constant values defined in IDL files are equal to the constant values in Blink implementation. In the above example, if NOT_FOUND_ERR were implemented as 100 in Blink, the build will fail.

*** note
Basically all constant values are defined in the spec, and thus the values in Blink implementation should be equal to the values defined in the spec. If you really want to introduce non-speced constant values and allow different values between IDL files and Blink implementation, you can specify `[DoNotCheckConstants]` to skip the compile-time assertions.
***

### [ImplementedAs] _(i, m, s, a)_

Summary: `[ImplementedAs]` specifies a method name in Blink, if the method name in an IDL file and the method name in Blink are different.

`[ImplementedAs]` is _discouraged_. Please use only if absolutely necessary: rename Blink internal names to align with IDL.

Usage: The possible usage is `[ImplementedAs=XXX]`, where XXX is a method name in Blink. `[ImplementedAs]` can be specified on interfaces, methods and attributes.

```webidl
[
    ImplementedAs=DOMPath,
] interface Path {
    [ImplementedAs=classAttribute] attribute int class;
    [ImplementedAs=deleteFunction] void delete();
};
```

Method names in Blink default to being the same as the name in an IDL file. In some cases this is not possible, e.g., `delete` is a C++ reserved word. In such cases, you can explicitly specify the method name in Blink by `[ImplementedAs]`. Generally the `[ImplementedAs]` name should be in lowerCamelCase. You should _not_ use `[ImplementedAs]` simply to avoid renaming Blink methods.

## Deprecated Blink-specific IDL Extended Attributes

These extended attributes are _deprecated_, or are under discussion for deprecation. They should be avoided.

## Internal-use Blink-specific IDL Extended Attributes

These extended attributes are added internally by the compiler, and not intended to be literally included in `.idl` files; they are documented here for clarity and completeness.

### [PartialInterfaceImplementedAs] _(m, a, c)_

Added to members of a partial interface definition (and implemented interfaces with `[LegacyTreatAsPartialInterface]`, due to [Bug 360435](https://crbug.com/360435)) when merging `.idl` files for two reasons. Firstly, these members are implemented in a separate class from the class for the (main) interface definition, so this name data is needed. This is most clearly _written_ as an extended attribute on the partial interface definition, but this is discarded during merging (only the members are merged, into flat lists of methods, attributes, and constants), and thus this extended attribute is put on the members. Secondly, members of partial interface definitions are called differently (via static member functions of the separate class, not instance methods or class methods of the main class), and thus there needs to be a flag indicating that the member comes from a partial interface definition.

## Undocumented Blink-specific IDL Extended Attributes

*** note
**FIXME:** The following need documentation:
***

* `[ImmutablePrototype]`
* `[LegacyInterfaceTypeChecking]`
* `[LogAllWorlds]`
* `[OverrideBuiltins]` :: used on named accessors
* `[PerWorldBindings]` :: interacts with `[LogActivity]`
* `[WebAgentAPI]`

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

[CrossOriginProperties]: https://html.spec.whatwg.org/multipage/browsers.html#crossoriginproperties-(-o-)
